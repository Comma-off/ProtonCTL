package com.protonctl.app.native

import com.sun.jna.Library
import com.sun.jna.Native
import com.sun.jna.Pointer
import java.io.File

/**
 * Literal mirror of `backend/include/protonctl/ffi_bridge.h`. String-returning
 * functions return [Pointer] rather than [String] so the caller can decode
 * it *and* still pass the original pointer to [protonctl_free_string] -
 * JNA's automatic `String` return marshaling would otherwise discard the
 * native pointer we need to free.
 *
 * [ProtonCtlBridge] wraps all of this into ergonomic, JSON-decoded Kotlin
 * calls; nothing outside `native/` should touch this interface directly.
 */
internal interface ProtonCtlNative : Library {
    fun protonctl_free_string(ptr: Pointer?)
    fun protonctl_last_error(): Pointer?

    fun protonctl_detect_steam_candidates(): Pointer?
    fun protonctl_validate_steam_path(path: String): Pointer?

    fun protonctl_load_config(): Pointer?
    fun protonctl_save_config(configJson: String): Int

    fun protonctl_list_repositories(): Pointer?
    fun protonctl_add_repository(owner: String, repo: String): Pointer?
    fun protonctl_remove_repository(owner: String, repo: String): Int

    fun protonctl_fetch_releases(owner: String, repo: String): Pointer?
    fun protonctl_check_for_update(owner: String, repo: String, installedVersionTag: String): Pointer?
    fun protonctl_list_installed(compatToolsDir: String): Pointer?
    fun protonctl_remove_installed(compatToolsDir: String, name: String): Int

    fun protonctl_enqueue_build(repoJson: String, ref: String, compatToolsDir: String): Pointer?
    fun protonctl_get_build_status(jobId: String): Pointer?
    fun protonctl_list_build_jobs(): Pointer?
    fun protonctl_cancel_build(jobId: String): Int

    fun protonctl_enqueue_install(repoJson: String, releaseJson: String, compatToolsDir: String): Pointer?
    fun protonctl_get_install_status(jobId: String): Pointer?
    fun protonctl_list_install_jobs(): Pointer?
    fun protonctl_cancel_install(jobId: String): Int

    fun protonctl_create_backup(
        protonDir: String,
        prefixDir: String?,
        manifestJson: String,
        outFile: String,
        compressionLevel: Int,
    ): Int

    fun protonctl_read_manifest(prtbakFile: String): Pointer?
    fun protonctl_restore_backup(prtbakFile: String, restoreRoot: String): Int

    fun protonctl_create_library_backup(compatToolsDir: String, outFile: String, compressionLevel: Int): Int
    fun protonctl_read_library_manifest(prtbakFile: String): Pointer?
    fun protonctl_restore_library_backup(prtbakFile: String, compatToolsDir: String): Int

    fun protonctl_scan_repair(compatToolsDir: String, compatDataDir: String): Pointer?
    fun protonctl_fix_issue(issueJson: String): Int

    companion object {
        val INSTANCE: ProtonCtlNative by lazy {
            // `compose.application.resources.dir` is set by the Compose
            // Desktop Gradle plugin whenever `appResourcesRootDir` is
            // configured (build.gradle.kts) - true for both `./gradlew run`
            // and any packaged distributable/AppImage, since that's where
            // libprotonctl_core.so gets bundled. Falls back to a plain
            // library-name lookup (system paths, LD_LIBRARY_PATH) so this
            // still works if that property somehow isn't set.
            val bundled = System.getProperty("compose.application.resources.dir")
                ?.let { File(it, "libprotonctl_core.so") }
                ?.takeIf { it.exists() }

            Native.load(bundled?.absolutePath ?: "protonctl_core", ProtonCtlNative::class.java)
        }
    }
}
