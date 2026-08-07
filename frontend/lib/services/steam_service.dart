import '../ffi/protonctl_bridge.dart';
import '../models/steam_candidate.dart';

class SteamService {
  List<SteamCandidate> detectCandidates() =>
      ProtonCtlBridge.detectSteamCandidates().map(SteamCandidate.fromJson).toList();

  SteamCandidate validatePath(String path) =>
      SteamCandidate.fromJson(ProtonCtlBridge.validateSteamPath(path));

  String compatibilityToolsDir(String steamPath) => '$steamPath/compatibilitytools.d';

  String compatDataDir(String steamPath) => '$steamPath/steamapps/compatdata';
}
