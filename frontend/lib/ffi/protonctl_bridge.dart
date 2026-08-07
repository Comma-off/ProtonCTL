import 'dart:convert';
import 'dart:ffi' as ffi;

import 'package:ffi/ffi.dart';
import 'package:flutter/foundation.dart';

import 'protonctl_bindings.dart';

class ProtonCtlNativeException implements Exception {
  ProtonCtlNativeException(this.message);
  final String message;

  @override
  String toString() => 'ProtonCtlNativeException: $message';
}

/// Describes one native call to make from inside a background isolate.
/// Kept as a plain, transferable data class so [compute] can send it
/// across the isolate boundary.
class _NativeCall {
  const _NativeCall(this.op, this.args, {this.intArg = 0});
  final String op;
  final List<String?> args;
  final int intArg;
}

String? _takeString(ffi.Pointer<Utf8> ptr, ProtonCtlNativeLibrary lib) {
  if (ptr == ffi.nullptr) return null;
  final value = ptr.toDartString();
  lib.freeString(ptr);
  return value;
}

String _requireString(ffi.Pointer<Utf8> ptr, ProtonCtlNativeLibrary lib) {
  final value = _takeString(ptr, lib);
  if (value == null) {
    throw ProtonCtlNativeException(lib.lastError().toDartString());
  }
  return value;
}

/// Runs on a background isolate spawned by [compute]; reopens the native
/// library (symbols are process-global, so this is cheap) and performs
/// whichever blocking/network-bound call `call.op` names.
dynamic _isolateDispatch(_NativeCall call) {
  final lib = ProtonCtlNativeLibrary();

  ffi.Pointer<Utf8> s(String? v) => (v ?? '').toNativeUtf8();

  switch (call.op) {
    case 'addRepository':
      final owner = s(call.args[0]);
      final repo = s(call.args[1]);
      final result = _requireString(lib.addRepository(owner, repo), lib);
      calloc.free(owner);
      calloc.free(repo);
      return result;

    case 'fetchReleases':
      final owner = s(call.args[0]);
      final repo = s(call.args[1]);
      final result = _requireString(lib.fetchReleases(owner, repo), lib);
      calloc.free(owner);
      calloc.free(repo);
      return result;

    case 'installRelease':
      final repoJson = s(call.args[0]);
      final releaseJson = s(call.args[1]);
      final compatDir = s(call.args[2]);
      final result = _requireString(lib.installRelease(repoJson, releaseJson, compatDir), lib);
      calloc.free(repoJson);
      calloc.free(releaseJson);
      calloc.free(compatDir);
      return result;

    case 'createBackup':
      final protonDir = s(call.args[0]);
      final prefixDir = s(call.args[1]);
      final manifestJson = s(call.args[2]);
      final outFile = s(call.args[3]);
      final rc = lib.createBackup(protonDir, prefixDir, manifestJson, outFile, call.intArg);
      calloc.free(protonDir);
      calloc.free(prefixDir);
      calloc.free(manifestJson);
      calloc.free(outFile);
      if (rc != 0) throw ProtonCtlNativeException(lib.lastError().toDartString());
      return null;

    case 'restoreBackup':
      final prtbakFile = s(call.args[0]);
      final restoreRoot = s(call.args[1]);
      final rc = lib.restoreBackup(prtbakFile, restoreRoot);
      calloc.free(prtbakFile);
      calloc.free(restoreRoot);
      if (rc != 0) throw ProtonCtlNativeException(lib.lastError().toDartString());
      return null;

    case 'scanRepair':
      final compatToolsDir = s(call.args[0]);
      final compatDataDir = s(call.args[1]);
      final result = _requireString(lib.scanRepair(compatToolsDir, compatDataDir), lib);
      calloc.free(compatToolsDir);
      calloc.free(compatDataDir);
      return result;

    default:
      throw ArgumentError('Unknown native op: ${call.op}');
  }
}

/// High-level, JSON-decoded, Future-based facade over the native
/// `protonctl_core` engine. Fast/local calls run synchronously on the calling
/// isolate; network- or IO-heavy calls are dispatched to a background
/// isolate via [compute] so the UI never blocks on a GitHub request,
/// archive extraction, or directory walk.
class ProtonCtlBridge {
  ProtonCtlBridge._();
  static final ProtonCtlNativeLibrary _lib = ProtonCtlNativeLibrary();

  // --- steam / config (fast, local) -----------------------------------

  static List<Map<String, dynamic>> detectSteamCandidates() {
    final json = _requireString(_lib.detectSteamCandidates(), _lib);
    return (jsonDecode(json) as List).cast<Map<String, dynamic>>();
  }

  static Map<String, dynamic> validateSteamPath(String path) {
    final pathPtr = path.toNativeUtf8();
    final result = _requireString(_lib.validateSteamPath(pathPtr), _lib);
    calloc.free(pathPtr);
    return jsonDecode(result) as Map<String, dynamic>;
  }

  static Map<String, dynamic> loadConfig() {
    final json = _requireString(_lib.loadConfig(), _lib);
    return jsonDecode(json) as Map<String, dynamic>;
  }

  static void saveConfig(Map<String, dynamic> config) {
    final jsonPtr = jsonEncode(config).toNativeUtf8();
    final rc = _lib.saveConfig(jsonPtr);
    calloc.free(jsonPtr);
    if (rc != 0) throw ProtonCtlNativeException(_lib.lastError().toDartString());
  }

  // --- repositories -----------------------------------------------------

  static List<Map<String, dynamic>> listRepositories() {
    final json = _requireString(_lib.listRepositories(), _lib);
    return (jsonDecode(json) as List).cast<Map<String, dynamic>>();
  }

  static Future<Map<String, dynamic>> addRepository(String owner, String repo) async {
    final json = await compute(_isolateDispatch, _NativeCall('addRepository', [owner, repo])) as String;
    return jsonDecode(json) as Map<String, dynamic>;
  }

  static void removeRepository(String owner, String repo) {
    final ownerPtr = owner.toNativeUtf8();
    final repoPtr = repo.toNativeUtf8();
    final rc = _lib.removeRepository(ownerPtr, repoPtr);
    calloc.free(ownerPtr);
    calloc.free(repoPtr);
    if (rc != 0) throw ProtonCtlNativeException(_lib.lastError().toDartString());
  }

  static Future<List<Map<String, dynamic>>> fetchReleases(String owner, String repo) async {
    final json = await compute(_isolateDispatch, _NativeCall('fetchReleases', [owner, repo])) as String;
    return (jsonDecode(json) as List).cast<Map<String, dynamic>>();
  }

  static Future<Map<String, dynamic>> installRelease(
    Map<String, dynamic> repo,
    Map<String, dynamic> release,
    String compatToolsDir,
  ) async {
    final json = await compute(
      _isolateDispatch,
      _NativeCall('installRelease', [jsonEncode(repo), jsonEncode(release), compatToolsDir]),
    ) as String;
    return jsonDecode(json) as Map<String, dynamic>;
  }

  static List<Map<String, dynamic>> listInstalled(String compatToolsDir) {
    final dirPtr = compatToolsDir.toNativeUtf8();
    final json = _requireString(_lib.listInstalled(dirPtr), _lib);
    calloc.free(dirPtr);
    return (jsonDecode(json) as List).cast<Map<String, dynamic>>();
  }

  static void removeInstalled(String compatToolsDir, String name) {
    final dirPtr = compatToolsDir.toNativeUtf8();
    final namePtr = name.toNativeUtf8();
    final rc = _lib.removeInstalled(dirPtr, namePtr);
    calloc.free(dirPtr);
    calloc.free(namePtr);
    if (rc != 0) throw ProtonCtlNativeException(_lib.lastError().toDartString());
  }

  // --- compilation queue --------------------------------------------------

  static String enqueueBuild(Map<String, dynamic> repo, String ref, String compatToolsDir) {
    final repoPtr = jsonEncode(repo).toNativeUtf8();
    final refPtr = ref.toNativeUtf8();
    final dirPtr = compatToolsDir.toNativeUtf8();
    final id = _requireString(_lib.enqueueBuild(repoPtr, refPtr, dirPtr), _lib);
    calloc.free(repoPtr);
    calloc.free(refPtr);
    calloc.free(dirPtr);
    return id;
  }

  static Map<String, dynamic>? getBuildStatus(String jobId) {
    final idPtr = jobId.toNativeUtf8();
    final resultPtr = _lib.getBuildStatus(idPtr);
    calloc.free(idPtr);
    final json = _takeString(resultPtr, _lib);
    return json == null ? null : jsonDecode(json) as Map<String, dynamic>;
  }

  static List<Map<String, dynamic>> listBuildJobs() {
    final json = _requireString(_lib.listBuildJobs(), _lib);
    return (jsonDecode(json) as List).cast<Map<String, dynamic>>();
  }

  static void cancelBuild(String jobId) {
    final idPtr = jobId.toNativeUtf8();
    _lib.cancelBuild(idPtr);
    calloc.free(idPtr);
  }

  // --- backup / repair -----------------------------------------------------

  static Future<void> createBackup({
    required String protonDir,
    String? prefixDir,
    required Map<String, dynamic> manifest,
    required String outFile,
    int zstdLevel = 19,
  }) async {
    await compute(
      _isolateDispatch,
      _NativeCall(
        'createBackup',
        [protonDir, prefixDir ?? '', jsonEncode(manifest), outFile],
        intArg: zstdLevel,
      ),
    );
  }

  static Map<String, dynamic> readManifest(String prtbakFile) {
    final pathPtr = prtbakFile.toNativeUtf8();
    final json = _requireString(_lib.readManifest(pathPtr), _lib);
    calloc.free(pathPtr);
    return jsonDecode(json) as Map<String, dynamic>;
  }

  static Future<void> restoreBackup(String prtbakFile, String restoreRoot) async {
    await compute(_isolateDispatch, _NativeCall('restoreBackup', [prtbakFile, restoreRoot]));
  }

  static Future<List<Map<String, dynamic>>> scanRepair(
      String compatToolsDir, String compatDataDir) async {
    final json = await compute(
      _isolateDispatch,
      _NativeCall('scanRepair', [compatToolsDir, compatDataDir]),
    ) as String;
    return (jsonDecode(json) as List).cast<Map<String, dynamic>>();
  }

  static void fixIssue(Map<String, dynamic> issue) {
    final issuePtr = jsonEncode(issue).toNativeUtf8();
    final rc = _lib.fixIssue(issuePtr);
    calloc.free(issuePtr);
    if (rc != 0) throw ProtonCtlNativeException(_lib.lastError().toDartString());
  }
}
