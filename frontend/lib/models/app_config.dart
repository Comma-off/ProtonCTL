import 'steam_candidate.dart';

class AppConfig {
  AppConfig({
    this.installType = SteamInstallType.unknown,
    this.steamPath = '',
    this.compatibilityToolsDir = '',
    this.firstStartCompleted = false,
    this.themeSpecVersion = 'spec2025',
    this.themeVariant = 'expressive',
    this.seedColorIndex = 0,
  });

  factory AppConfig.fromJson(Map<String, dynamic> json) => AppConfig(
        installType: steamInstallTypeFromJson(json['install_type'] as String? ?? 'unknown'),
        steamPath: json['steam_path'] as String? ?? '',
        compatibilityToolsDir: json['compatibility_tools_dir'] as String? ?? '',
        firstStartCompleted: json['first_start_completed'] as bool? ?? false,
        themeSpecVersion: json['theme_spec_version'] as String? ?? 'spec2025',
        themeVariant: json['theme_variant'] as String? ?? 'expressive',
        seedColorIndex: (json['seed_color_index'] as num?)?.toInt() ?? 0,
      );

  final SteamInstallType installType;
  final String steamPath;
  final String compatibilityToolsDir;
  final bool firstStartCompleted;
  final String themeSpecVersion;
  final String themeVariant;
  final int seedColorIndex;

  AppConfig copyWith({
    SteamInstallType? installType,
    String? steamPath,
    String? compatibilityToolsDir,
    bool? firstStartCompleted,
    String? themeSpecVersion,
    String? themeVariant,
    int? seedColorIndex,
  }) =>
      AppConfig(
        installType: installType ?? this.installType,
        steamPath: steamPath ?? this.steamPath,
        compatibilityToolsDir: compatibilityToolsDir ?? this.compatibilityToolsDir,
        firstStartCompleted: firstStartCompleted ?? this.firstStartCompleted,
        themeSpecVersion: themeSpecVersion ?? this.themeSpecVersion,
        themeVariant: themeVariant ?? this.themeVariant,
        seedColorIndex: seedColorIndex ?? this.seedColorIndex,
      );

  Map<String, dynamic> toJson() => {
        'install_type': steamInstallTypeToJson(installType),
        'steam_path': steamPath,
        'compatibility_tools_dir': compatibilityToolsDir,
        'first_start_completed': firstStartCompleted,
        'theme_spec_version': themeSpecVersion,
        'theme_variant': themeVariant,
        'seed_color_index': seedColorIndex,
      };
}
