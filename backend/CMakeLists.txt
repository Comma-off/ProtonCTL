include(FetchContent)

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------

# nlohmann::json - vendored via FetchContent so the tree builds without a
# system package being pre-installed.
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(nlohmann_json)

find_package(CURL REQUIRED)
find_package(LibArchive REQUIRED)
find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# protonctl_core: the shared library consumed by both frontends (Kotlin via
# JNA, Flutter via dart:ffi)
# ---------------------------------------------------------------------------

add_library(protonctl_core SHARED
  src/steam_path_resolver.cpp
  src/github_client.cpp
  src/proton_repository_manager.cpp
  src/build_runner.cpp
  src/install_runner.cpp
  src/backup_manager.cpp
  src/repair_manager.cpp
  src/config_manager.cpp
  src/process_util.cpp
  src/ffi_bridge.cpp
)

target_include_directories(protonctl_core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    # backup_manager.hpp - a public header - includes <archive.h> directly,
    # so anything that links protonctl_core and includes its headers (like
    # protonctl_cli's main.cpp) needs this on its include path too, not just
    # protonctl_core's own .cpp files.
    ${LibArchive_INCLUDE_DIRS}
)

target_link_libraries(protonctl_core
  PUBLIC
    nlohmann_json::nlohmann_json
  PRIVATE
    CURL::libcurl
    ${LibArchive_LIBRARIES}
    Threads::Threads
)

target_compile_definitions(protonctl_core PRIVATE PROTONCTL_BUILDING_SHARED=1)

set_target_properties(protonctl_core PROPERTIES
  OUTPUT_NAME "protonctl_core"
  VERSION 0.1.0
  SOVERSION 0
)

# ---------------------------------------------------------------------------
# protonctl_cli: a thin debug/dev CLI so the engine can be exercised without
# the Flutter shell (useful for CI smoke tests and local debugging).
# ---------------------------------------------------------------------------

add_executable(protonctl_cli src/main.cpp)
target_link_libraries(protonctl_cli PRIVATE protonctl_core)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

if(PROTONCTL_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

install(TARGETS protonctl_core
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(DIRECTORY include/ DESTINATION include)
