import 'package:flutter/foundation.dart';

import '../ffi/protonctl_bridge.dart';
import '../models/app_config.dart';
import '../models/steam_candidate.dart';

/// Wraps the native `AppConfig` (Steam path, compatibilitytools.d
/// location, first-start flag, persisted theme choices) so the rest of
/// the app can read/write it as a normal Dart object instead of poking
/// JSON through the FFI bridge directly.
class SettingsService extends ChangeNotifier {
  AppConfig _config = AppConfig();
  AppConfig get config => _config;

  Future<void> load() async {
    _config = AppConfig.fromJson(ProtonCtlBridge.loadConfig());
    notifyListeners();
  }

  Future<void> update(AppConfig Function(AppConfig current) updater) async {
    _config = updater(_config);
    ProtonCtlBridge.saveConfig(_config.toJson());
    notifyListeners();
  }

  Future<void> completeFirstStart({
    required String steamPath,
    required String compatibilityToolsDir,
    required SteamInstallType installType,
  }) async {
    await update((c) => c.copyWith(
          steamPath: steamPath,
          compatibilityToolsDir: compatibilityToolsDir,
          installType: installType,
          firstStartCompleted: true,
        ));
  }
}
