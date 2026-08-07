import org.jetbrains.compose.desktop.application.dsl.TargetFormat

plugins {
    kotlin("jvm")
    id("org.jetbrains.kotlin.plugin.compose")
    id("org.jetbrains.kotlin.plugin.serialization")
    id("org.jetbrains.compose")
}

group = "com.protonctl"
version = "0.1.0"

repositories {
    google()
    mavenCentral()
}

kotlin {
    compilerOptions {
        // M3 Expressive components (MaterialExpressiveTheme, MotionScheme,
        // the new shape/typography scale) are still gated behind this
        // opt-in even though they're no longer "experimental" in the
        // colloquial sense - Material3 1.4.0 is stable, this annotation
        // just hasn't been dropped yet.
        freeCompilerArgs.add("-opt-in=androidx.compose.material3.ExperimentalMaterial3ExpressiveApi")
    }
}

dependencies {
    implementation(compose.desktop.currentOs)
    // Explicit version rather than the `compose.material3` alias: connected
    // button groups (ButtonGroup/ToggleButton/ButtonGroupDefaults) and the
    // clickable ListItem overload aren't in the alias's default version yet.
    implementation("org.jetbrains.compose.material3:material3:1.11.0-alpha07")
    implementation(compose.materialIconsExtended)

    // Material 3 Expressive dynamic color: real SPEC_2021/SPEC_2025 toggle
    // and Tonal Spot/Fidelity/Content/Expressive palette styles, driven off
    // a seed color - see theme/ProtonCtlTheme.kt.
    implementation("com.materialkolor:material-kolor:${property("materialKolor.version")}")

    // JVM <-> native bridge to backend/include/protonctl/ffi_bridge.h.
    implementation("net.java.dev.jna:jna:${property("jna.version")}")

    // File/directory pickers. On Linux this goes through the XDG Desktop
    // Portal (D-Bus, a separate process) rather than AWT's FileDialog -
    // mixing AWT's heavyweight native dialogs with Compose Desktop's
    // OpenGL-rendered surface is a known source of black/corrupted dialog
    // rendering (a real, documented Compose Multiplatform interop issue,
    // not just something we hit by chance).
    implementation("io.github.vinceglb:filekit-core:0.14.2")
    implementation("io.github.vinceglb:filekit-dialogs:0.14.2")
    implementation("io.github.vinceglb:filekit-dialogs-compose:0.14.2")

    implementation("org.jetbrains.kotlinx:kotlinx-serialization-json:1.11.0")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-core:1.11.0")
}

compose.desktop {
    application {
        mainClass = "com.protonctl.app.MainKt"

        nativeDistributions {
            targetFormats(TargetFormat.Deb)
            packageName = "protonctl"
            packageVersion = "0.1.0"
            description = "ProtonCTL - manage Proton-GE and custom Proton builds for Steam on Linux"

            // Bundles files from resources/<target>/ straight into the
            // distributable, readable at runtime via the system property
            // `compose.application.resources.dir` - this is how
            // libprotonctl_core.so gets shipped inside the app instead of
            // relying on a dev-only path into the backend's own build dir.
            // See the copyNativeLibraryForPackaging task below and
            // ProtonCtlNative.kt's loading logic.
            appResourcesRootDir.set(layout.projectDirectory.dir("resources"))

            linux {
                iconFile.set(project.file("icon.png"))
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Bundle the native engine into the app so packaged builds are self
// contained (no dev-only -Djna.library.path pointing into ../build/backend).
// ---------------------------------------------------------------------------

val nativeLibResourcesDir = layout.projectDirectory.dir("resources/linux-x64")

val copyNativeLibraryForPackaging = tasks.register<Sync>("copyNativeLibraryForPackaging") {
    val backendBuildDir = rootProject.projectDir.resolve("../build/backend")
    inputs.file(backendBuildDir.resolve("libprotonctl_core.so"))
    from(backendBuildDir) {
        include("libprotonctl_core.so")
        // The build output is a chain of symlinks (SONAME versioning); Sync
        // resolves symlinks to their real file content on copy, which is
        // exactly what we want bundled, not a dangling link.
    }
    into(nativeLibResourcesDir)

    doFirst {
        check(backendBuildDir.resolve("libprotonctl_core.so").exists()) {
            "libprotonctl_core.so not found at $backendBuildDir - build the backend first " +
                "(see repo root CMakeLists.txt) before running this task."
        }
    }
}

tasks.matching {
    // `prepareAppResources` is the Compose plugin's own task that actually
    // reads from resources/linux-x64 (both for `run` and for packaging) -
    // it has to depend on this directly, not just the tasks that trigger it.
    it.name == "run" || it.name == "createDistributable" || it.name == "prepareAppResources" ||
        it.name.startsWith("package")
}.configureEach {
    dependsOn(copyNativeLibraryForPackaging)
}

// ---------------------------------------------------------------------------
// True single-file .AppImage packaging. jpackage/Compose Desktop don't
// produce this format themselves - `TargetFormat.AppImage` in the Compose
// Gradle plugin is actually jpackage's "app-image" *directory* output, a
// different thing despite the name. This assembles a real AppDir (with an
// auto-generated .desktop file and AppRun launcher) from createDistributable's
// output and hands it to the real appimagetool.
// ---------------------------------------------------------------------------

val appImageToolFile = layout.buildDirectory.file("tools/appimagetool-x86_64.AppImage")

val downloadAppImageTool = tasks.register("downloadAppImageTool") {
    val outputFile = appImageToolFile
    outputs.file(outputFile)
    doLast {
        val dest = outputFile.get().asFile
        if (!dest.exists()) {
            dest.parentFile.mkdirs()
            val url = "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage"
            uri(url).toURL().openStream().use { input -> dest.outputStream().use { input.copyTo(it) } }
        }
        dest.setExecutable(true)
    }
}

val appDirDir = layout.buildDirectory.dir("AppDir")

val appIconFile = layout.projectDirectory.file("icon.png")

val assembleAppDir = tasks.register<Sync>("assembleAppDir") {
    dependsOn("createDistributable")

    val distributableDir = layout.buildDirectory.dir("compose/binaries/main/app/protonctl")
    into(appDirDir)
    from(distributableDir) { into("usr/lib/protonctl") }

    doLast {
        val root = appDirDir.get().asFile

        // AppRun is the entry point every AppImage runs when launched.
        root.resolve("AppRun").apply {
            writeText(
                """
                #!/bin/sh
                HERE="${'$'}(dirname "${'$'}(readlink -f "${'$'}0")")"
                exec "${'$'}HERE/usr/lib/protonctl/bin/protonctl" "${'$'}@"
                """.trimIndent() + "\n"
            )
            setExecutable(true)
        }

        // Auto-generated .desktop file - not hand maintained.
        root.resolve("protonctl.desktop").writeText(
            """
            [Desktop Entry]
            Type=Application
            Name=ProtonCTL
            Comment=Manage Proton-GE and custom Proton builds for Steam on Linux
            Exec=protonctl
            Icon=protonctl
            Categories=Utility;
            Terminal=false
            """.trimIndent() + "\n"
        )

        appIconFile.asFile.copyTo(root.resolve("protonctl.png"), overwrite = true)
    }
}

val packageAppImage = tasks.register<Exec>("packageAppImage") {
    dependsOn(assembleAppDir, downloadAppImageTool)

    val outputFile = layout.buildDirectory.file("appimage/ProtonCTL-x86_64.AppImage")
    outputs.file(outputFile)
    doFirst { outputFile.get().asFile.parentFile.mkdirs() }

    environment("ARCH", "x86_64")
    // Falls back to extracting appimagetool's own AppImage into a temp dir
    // and running it directly, since build/CI environments frequently don't
    // have FUSE available to mount AppImages the normal way.
    commandLine(
        appImageToolFile.get().asFile.absolutePath,
        "--appimage-extract-and-run",
        appDirDir.get().asFile.absolutePath,
        outputFile.get().asFile.absolutePath,
    )
}
