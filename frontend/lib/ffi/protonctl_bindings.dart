import 'dart:ffi' as ffi;
import 'dart:io';

import 'package:ffi/ffi.dart';

typedef _FreeStringNative = ffi.Void Function(ffi.Pointer<Utf8>);
typedef _FreeStringDart = void Function(ffi.Pointer<Utf8>);

typedef _NoArgStringNative = ffi.Pointer<Utf8> Function();
typedef _NoArgStringDart = ffi.Pointer<Utf8> Function();

typedef _OneStringInStringOutNative = ffi.Pointer<Utf8> Function(ffi.Pointer<Utf8>);
typedef _OneStringInStringOutDart = ffi.Pointer<Utf8> Function(ffi.Pointer<Utf8>);

typedef _TwoStringInStringOutNative = ffi.Pointer<Utf8> Function(ffi.Pointer<Utf8>, ffi.Pointer<Utf8>);
typedef _TwoStringInStringOutDart = ffi.Pointer<Utf8> Function(ffi.Pointer<Utf8>, ffi.Pointer<Utf8>);

typedef _ThreeStringInStringOutNative = ffi.Pointer<Utf8> Function(
    ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>);
typedef _ThreeStringInStringOutDart = ffi.Pointer<Utf8> Function(
    ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>);

typedef _OneStringInIntOutNative = ffi.Int32 Function(ffi.Pointer<Utf8>);
typedef _OneStringInIntOutDart = int Function(ffi.Pointer<Utf8>);

typedef _TwoStringInIntOutNative = ffi.Int32 Function(ffi.Pointer<Utf8>, ffi.Pointer<Utf8>);
typedef _TwoStringInIntOutDart = int Function(ffi.Pointer<Utf8>, ffi.Pointer<Utf8>);

typedef _CreateBackupNative = ffi.Int32 Function(
    ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Int32);
typedef _CreateBackupDart = int Function(
    ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, int);

/// Thin, literal mirror of `protonctl/ffi_bridge.h`. Every function here
/// returns raw FFI types (`Pointer<Utf8>`, `int`); [ProtonCtlBridge] wraps
/// these into ergonomic, JSON-decoded, Future-based Dart calls.
class ProtonCtlNativeLibrary {
  factory ProtonCtlNativeLibrary() => _instance ??= ProtonCtlNativeLibrary._(_open());

  ProtonCtlNativeLibrary._(this._lib) {
    freeString = _lib.lookupFunction<_FreeStringNative, _FreeStringDart>('protonctl_free_string');
    lastError = _lib.lookupFunction<_NoArgStringNative, _NoArgStringDart>('protonctl_last_error');

    detectSteamCandidates = _lib
        .lookupFunction<_NoArgStringNative, _NoArgStringDart>('protonctl_detect_steam_candidates');
    validateSteamPath = _lib
        .lookupFunction<_OneStringInStringOutNative, _OneStringInStringOutDart>('protonctl_validate_steam_path');

    loadConfig = _lib.lookupFunction<_NoArgStringNative, _NoArgStringDart>('protonctl_load_config');
    saveConfig =
        _lib.lookupFunction<_OneStringInIntOutNative, _OneStringInIntOutDart>('protonctl_save_config');

    listRepositories =
        _lib.lookupFunction<_NoArgStringNative, _NoArgStringDart>('protonctl_list_repositories');
    addRepository = _lib
        .lookupFunction<_TwoStringInStringOutNative, _TwoStringInStringOutDart>('protonctl_add_repository');
    removeRepository = _lib
        .lookupFunction<_TwoStringInIntOutNative, _TwoStringInIntOutDart>('protonctl_remove_repository');

    fetchReleases = _lib
        .lookupFunction<_TwoStringInStringOutNative, _TwoStringInStringOutDart>('protonctl_fetch_releases');
    installRelease = _lib.lookupFunction<_ThreeStringInStringOutNative, _ThreeStringInStringOutDart>(
        'protonctl_install_release');
    listInstalled = _lib
        .lookupFunction<_OneStringInStringOutNative, _OneStringInStringOutDart>('protonctl_list_installed');
    removeInstalled = _lib
        .lookupFunction<_TwoStringInIntOutNative, _TwoStringInIntOutDart>('protonctl_remove_installed');

    enqueueBuild = _lib.lookupFunction<_ThreeStringInStringOutNative, _ThreeStringInStringOutDart>(
        'protonctl_enqueue_build');
    getBuildStatus = _lib
        .lookupFunction<_OneStringInStringOutNative, _OneStringInStringOutDart>('protonctl_get_build_status');
    listBuildJobs =
        _lib.lookupFunction<_NoArgStringNative, _NoArgStringDart>('protonctl_list_build_jobs');
    cancelBuild =
        _lib.lookupFunction<_OneStringInIntOutNative, _OneStringInIntOutDart>('protonctl_cancel_build');

    createBackup =
        _lib.lookupFunction<_CreateBackupNative, _CreateBackupDart>('protonctl_create_backup');
    readManifest = _lib
        .lookupFunction<_OneStringInStringOutNative, _OneStringInStringOutDart>('protonctl_read_manifest');
    restoreBackup = _lib
        .lookupFunction<_TwoStringInIntOutNative, _TwoStringInIntOutDart>('protonctl_restore_backup');

    scanRepair = _lib
        .lookupFunction<_TwoStringInStringOutNative, _TwoStringInStringOutDart>('protonctl_scan_repair');
    fixIssue =
        _lib.lookupFunction<_OneStringInIntOutNative, _OneStringInIntOutDart>('protonctl_fix_issue');
  }

  static ProtonCtlNativeLibrary? _instance;
  final ffi.DynamicLibrary _lib;

  static ffi.DynamicLibrary _open() {
    if (Platform.isLinux) return ffi.DynamicLibrary.open('libprotonctl_core.so');
    if (Platform.isWindows) return ffi.DynamicLibrary.open('protonctl_core.dll');
    if (Platform.isMacOS) return ffi.DynamicLibrary.open('libprotonctl_core.dylib');
    throw UnsupportedError('PROTONCTL has no native engine build for this platform');
  }

  late final _FreeStringDart freeString;
  late final _NoArgStringDart lastError;

  late final _NoArgStringDart detectSteamCandidates;
  late final _OneStringInStringOutDart validateSteamPath;

  late final _NoArgStringDart loadConfig;
  late final _OneStringInIntOutDart saveConfig;

  late final _NoArgStringDart listRepositories;
  late final _TwoStringInStringOutDart addRepository;
  late final _TwoStringInIntOutDart removeRepository;

  late final _TwoStringInStringOutDart fetchReleases;
  late final _ThreeStringInStringOutDart installRelease;
  late final _OneStringInStringOutDart listInstalled;
  late final _TwoStringInIntOutDart removeInstalled;

  late final _ThreeStringInStringOutDart enqueueBuild;
  late final _OneStringInStringOutDart getBuildStatus;
  late final _NoArgStringDart listBuildJobs;
  late final _OneStringInIntOutDart cancelBuild;

  late final _CreateBackupDart createBackup;
  late final _OneStringInStringOutDart readManifest;
  late final _TwoStringInIntOutDart restoreBackup;

  late final _TwoStringInStringOutDart scanRepair;
  late final _OneStringInIntOutDart fixIssue;
}
