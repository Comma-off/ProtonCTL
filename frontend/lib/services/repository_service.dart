import 'dart:async';

import 'package:flutter/foundation.dart';

import '../ffi/protonctl_bridge.dart';
import '../models/build_job.dart';
import '../models/proton_repository.dart';

/// Owns repository registry state, installed Proton versions, cached
/// release lists, and the compilation queue - polling native build jobs
/// on a timer since `BuildRunner` on the C++ side runs them on its own
/// background thread and only reports progress when asked.
class RepositoryService extends ChangeNotifier {
  RepositoryService({required String compatToolsDir}) : _compatToolsDir = compatToolsDir;

  String _compatToolsDir;
  String get compatToolsDir => _compatToolsDir;

  List<ProtonRepository> repositories = [];
  List<InstalledProtonVersion> installed = [];
  final Map<String, List<GitHubRelease>> releasesByRepo = {};
  List<BuildJob> buildJobs = [];

  Timer? _pollTimer;
  bool loadingRepositories = false;

  void updateCompatToolsDir(String dir) {
    _compatToolsDir = dir;
    refreshInstalled();
  }

  Future<void> refreshAll() async {
    loadingRepositories = true;
    notifyListeners();
    repositories = ProtonCtlBridge.listRepositories().map(ProtonRepository.fromJson).toList();
    loadingRepositories = false;
    refreshInstalled();
    refreshBuildJobs();
    notifyListeners();
  }

  void refreshInstalled() {
    if (_compatToolsDir.isEmpty) return;
    installed = ProtonCtlBridge.listInstalled(_compatToolsDir)
        .map(InstalledProtonVersion.fromJson)
        .toList();
    notifyListeners();
  }

  Future<ProtonRepository> addRepository(String owner, String repo) async {
    final json = await ProtonCtlBridge.addRepository(owner, repo);
    final added = ProtonRepository.fromJson(json);
    repositories.add(added);
    notifyListeners();
    return added;
  }

  void removeRepository(ProtonRepository repo) {
    ProtonCtlBridge.removeRepository(repo.owner, repo.repo);
    repositories.removeWhere((r) => r.fullName == repo.fullName);
    releasesByRepo.remove(repo.fullName);
    notifyListeners();
  }

  Future<List<GitHubRelease>> fetchReleases(ProtonRepository repo) async {
    final releases = await ProtonCtlBridge.fetchReleases(repo.owner, repo.repo);
    final parsed = releases.map(GitHubRelease.fromJson).toList();
    releasesByRepo[repo.fullName] = parsed;
    notifyListeners();
    return parsed;
  }

  Future<void> installRelease(ProtonRepository repo, GitHubRelease release) async {
    await ProtonCtlBridge.installRelease(repo.toJson(), release.toJson(), _compatToolsDir);
    refreshInstalled();
  }

  void removeInstalledVersion(String name) {
    ProtonCtlBridge.removeInstalled(_compatToolsDir, name);
    refreshInstalled();
  }

  String enqueueBuild(ProtonRepository repo, String ref) {
    final id = ProtonCtlBridge.enqueueBuild(repo.toJson(), ref, _compatToolsDir);
    _ensurePolling();
    refreshBuildJobs();
    return id;
  }

  void cancelBuild(String jobId) {
    ProtonCtlBridge.cancelBuild(jobId);
    refreshBuildJobs();
  }

  void refreshBuildJobs() {
    buildJobs = ProtonCtlBridge.listBuildJobs().map(BuildJob.fromJson).toList();
    notifyListeners();
    if (buildJobs.isNotEmpty && buildJobs.every((j) => j.isTerminal)) {
      refreshInstalled();
    }
  }

  void _ensurePolling() {
    _pollTimer ??= Timer.periodic(const Duration(seconds: 2), (_) => refreshBuildJobs());
  }

  @override
  void dispose() {
    _pollTimer?.cancel();
    super.dispose();
  }
}
