enum SteamInstallType { flatpak, debian, native, custom, unknown }

SteamInstallType steamInstallTypeFromJson(String value) {
  switch (value) {
    case 'flatpak':
      return SteamInstallType.flatpak;
    case 'debian':
      return SteamInstallType.debian;
    case 'native':
      return SteamInstallType.native;
    case 'custom':
      return SteamInstallType.custom;
    default:
      return SteamInstallType.unknown;
  }
}

String steamInstallTypeToJson(SteamInstallType type) => type.name;

class SteamCandidate {
  SteamCandidate({
    required this.type,
    required this.path,
    required this.valid,
    required this.label,
  });

  factory SteamCandidate.fromJson(Map<String, dynamic> json) => SteamCandidate(
        type: steamInstallTypeFromJson(json['type'] as String? ?? 'unknown'),
        path: json['path'] as String? ?? '',
        valid: json['valid'] as bool? ?? false,
        label: json['label'] as String? ?? '',
      );

  final SteamInstallType type;
  final String path;
  final bool valid;
  final String label;
}
