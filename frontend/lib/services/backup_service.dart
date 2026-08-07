import '../ffi/protonctl_bridge.dart';
import '../models/backup_manifest.dart';

class BackupService {
  Future<void> createBackup({
    required String protonDir,
    String? prefixDir,
    required PrtBakManifest manifest,
    required String outFile,
    int zstdLevel = 19,
  }) =>
      ProtonCtlBridge.createBackup(
        protonDir: protonDir,
        prefixDir: prefixDir,
        manifest: manifest.toJson(),
        outFile: outFile,
        zstdLevel: zstdLevel,
      );

  PrtBakManifest readManifest(String prtbakFile) =>
      PrtBakManifest.fromJson(ProtonCtlBridge.readManifest(prtbakFile));

  Future<void> restoreBackup(String prtbakFile, String restoreRoot) =>
      ProtonCtlBridge.restoreBackup(prtbakFile, restoreRoot);

  Future<List<RepairIssue>> scanRepair(String compatToolsDir, String compatDataDir) async {
    final issues = await ProtonCtlBridge.scanRepair(compatToolsDir, compatDataDir);
    return issues.map(RepairIssue.fromJson).toList();
  }

  bool fixIssue(RepairIssue issue) {
    try {
      ProtonCtlBridge.fixIssue(issue.toJson());
      return true;
    } on ProtonCtlNativeException {
      return false;
    }
  }
}
