class PrtBakManifest {
  PrtBakManifest({
    this.formatVersion = '1',
    required this.protonVersion,
    this.sourceRepo = '',
    this.protonDirName = '',
    this.winePrefixRelative = '',
    this.dependencies = const [],
    this.createdAt = '',
    this.sha256ProtonDir = '',
  });

  factory PrtBakManifest.fromJson(Map<String, dynamic> json) => PrtBakManifest(
        formatVersion: json['format_version'] as String? ?? '1',
        protonVersion: json['proton_version'] as String? ?? '',
        sourceRepo: json['source_repo'] as String? ?? '',
        protonDirName: json['proton_dir_name'] as String? ?? '',
        winePrefixRelative: json['wine_prefix_relative'] as String? ?? '',
        dependencies: ((json['dependencies'] as List?) ?? []).cast<String>(),
        createdAt: json['created_at'] as String? ?? '',
        sha256ProtonDir: json['sha256_proton_dir'] as String? ?? '',
      );

  final String formatVersion;
  final String protonVersion;
  final String sourceRepo;
  final String protonDirName;
  final String winePrefixRelative;
  final List<String> dependencies;
  final String createdAt;
  final String sha256ProtonDir;

  Map<String, dynamic> toJson() => {
        'format_version': formatVersion,
        'proton_version': protonVersion,
        'source_repo': sourceRepo,
        'proton_dir_name': protonDirName,
        'wine_prefix_relative': winePrefixRelative,
        'dependencies': dependencies,
        'created_at': createdAt,
        'sha256_proton_dir': sha256ProtonDir,
      };
}

enum RepairIssueType { brokenSymlink, missingRuntime, corruptManifest, emptyProtonDir, orphanedPrefix }

RepairIssueType _repairIssueTypeFromJson(String value) {
  switch (value) {
    case 'broken_symlink':
      return RepairIssueType.brokenSymlink;
    case 'missing_runtime':
      return RepairIssueType.missingRuntime;
    case 'corrupt_manifest':
      return RepairIssueType.corruptManifest;
    case 'empty_proton_dir':
      return RepairIssueType.emptyProtonDir;
    case 'orphaned_prefix':
      return RepairIssueType.orphanedPrefix;
    default:
      return RepairIssueType.missingRuntime;
  }
}

String _repairIssueTypeToJson(RepairIssueType type) {
  switch (type) {
    case RepairIssueType.brokenSymlink:
      return 'broken_symlink';
    case RepairIssueType.missingRuntime:
      return 'missing_runtime';
    case RepairIssueType.corruptManifest:
      return 'corrupt_manifest';
    case RepairIssueType.emptyProtonDir:
      return 'empty_proton_dir';
    case RepairIssueType.orphanedPrefix:
      return 'orphaned_prefix';
  }
}

class RepairIssue {
  RepairIssue({
    required this.type,
    required this.path,
    required this.description,
    required this.fixable,
  });

  factory RepairIssue.fromJson(Map<String, dynamic> json) => RepairIssue(
        type: _repairIssueTypeFromJson(json['type'] as String? ?? ''),
        path: json['path'] as String? ?? '',
        description: json['description'] as String? ?? '',
        fixable: json['fixable'] as bool? ?? false,
      );

  final RepairIssueType type;
  final String path;
  final String description;
  final bool fixable;

  Map<String, dynamic> toJson() => {
        'type': _repairIssueTypeToJson(type),
        'path': path,
        'description': description,
        'fixable': fixable,
      };
}
