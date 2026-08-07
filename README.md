# ProtonCTL
Manages Proton on Linux: installs Proton-GE releases, builds custom Proton forks from
GitHub, and backs up/restores Proton installs and wine prefixes as `.prtbak` archives.

The engine is C++ (`backend/`). The UI is Kotlin + Compose Multiplatform Desktop
(`frontend-compose/`). An earlier Flutter UI still lives at `frontend/` but isn't
actively developed - build `frontend-compose/` instead.

## Dependencies

**Backend:**
- CMake 3.20+
- A C++20 compiler
- `libcurl` (dev headers)
- `libarchive` (dev headers, built with zstd support)

**Frontend:**
- JDK 21+ on `PATH` (or `JAVA_HOME` set)
- The backend must be built first - the frontend loads it at runtime

## Building the backend

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j$(nproc)
```

Produces `build/backend/libprotonctl_core.so` (loaded by both frontends) and
`build/backend/protonctl_cli` (a dev CLI, e.g. `protonctl_cli detect-steam`).

Optional test build: add `-DPROTONCTL_BUILD_TESTS=ON` to the `cmake -S` step, then
`ctest --test-dir build --output-on-failure`.

Set a `GITHUB_TOKEN` env var to raise the GitHub API rate limit from 60 to 5000
requests/hour.

## Building the frontend

```sh
cd frontend-compose
./gradlew run
```

Every build (`run`, `createDistributable`, packaging) first copies
`../build/backend/libprotonctl_core.so` into `resources/linux-x64/`, which Compose
Desktop bundles into whatever it produces and exposes at runtime via the
`compose.application.resources.dir` system property - so the backend just needs to be
built once (see above) and everything downstream picks it up automatically, packaged
or not.

### Packaging

```sh
./gradlew packageDeb        # a .deb - installs via dpkg, auto-registers a menu entry
./gradlew packageAppImage   # a single portable .AppImage - no install needed
```

`packageAppImage` is a custom task, not a built-in Compose Desktop one - jpackage (what
Compose Desktop packaging is built on) doesn't produce the real `.AppImage` format
itself. It assembles an AppDir from `createDistributable`'s output, auto-generates the
`.desktop` file and an `AppRun` launcher script (see `build.gradle.kts`), and downloads
[`appimagetool`](https://github.com/AppImage/appimagetool) on first use to do the
actual packaging. Output: `build/appimage/ProtonCTL-x86_64.AppImage` - one file, chmod
+x and run it, nothing to install.

The app icon is `frontend-compose/icon.png` (generated from `icon.svg` via
`rsvg-convert`) - a placeholder "P" monogram, worth swapping for real branding.
