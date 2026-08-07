class ProtonRepository {
  ProtonRepository({
    required this.owner,
    required this.repo,
    required this.displayName,
    this.isBuiltin = false,
    this.buildCommandOverride = '',
  });

  factory ProtonRepository.fromJson(Map<String, dynamic> json) => ProtonRepository(
        owner: json['owner'] as String? ?? '',
        repo: json['repo'] as String? ?? '',
        displayName: json['display_name'] as String? ?? '',
        isBuiltin: json['is_builtin'] as bool? ?? false,
        buildCommandOverride: json['build_command_override'] as String? ?? '',
      );

  final String owner;
  final String repo;
  final String displayName;
  final bool isBuiltin;
  final String buildCommandOverride;

  String get fullName => '$owner/$repo';

  Map<String, dynamic> toJson() => {
        'owner': owner,
        'repo': repo,
        'display_name': displayName,
        'is_builtin': isBuiltin,
        'build_command_override': buildCommandOverride,
      };
}

class GitHubAsset {
  GitHubAsset({
    required this.name,
    required this.browserDownloadUrl,
    required this.size,
    this.contentType = '',
  });

  factory GitHubAsset.fromJson(Map<String, dynamic> json) => GitHubAsset(
        name: json['name'] as String? ?? '',
        browserDownloadUrl: json['browser_download_url'] as String? ?? '',
        size: (json['size'] as num?)?.toInt() ?? 0,
        contentType: json['content_type'] as String? ?? '',
      );

  final String name;
  final String browserDownloadUrl;
  final int size;
  final String contentType;

  Map<String, dynamic> toJson() => {
        'name': name,
        'browser_download_url': browserDownloadUrl,
        'size': size,
        'content_type': contentType,
      };
}

class GitHubRelease {
  GitHubRelease({
    required this.tagName,
    required this.name,
    required this.body,
    required this.publishedAt,
    required this.prerelease,
    required this.assets,
  });

  factory GitHubRelease.fromJson(Map<String, dynamic> json) => GitHubRelease(
        tagName: json['tag_name'] as String? ?? '',
        name: json['name'] as String? ?? '',
        body: json['body'] as String? ?? '',
        publishedAt: json['published_at'] as String? ?? '',
        prerelease: json['prerelease'] as bool? ?? false,
        assets: ((json['assets'] as List?) ?? [])
            .map((a) => GitHubAsset.fromJson(a as Map<String, dynamic>))
            .toList(),
      );

  final String tagName;
  final String name;
  final String body;
  final String publishedAt;
  final bool prerelease;
  final List<GitHubAsset> assets;

  Map<String, dynamic> toJson() => {
        'tag_name': tagName,
        'name': name,
        'body': body,
        'published_at': publishedAt,
        'prerelease': prerelease,
        'assets': assets.map((a) => a.toJson()).toList(),
      };
}

class InstalledProtonVersion {
  InstalledProtonVersion({
    required this.name,
    required this.path,
    required this.sourceRepo,
    required this.versionTag,
    required this.installedAt,
  });

  factory InstalledProtonVersion.fromJson(Map<String, dynamic> json) => InstalledProtonVersion(
        name: json['name'] as String? ?? '',
        path: json['path'] as String? ?? '',
        sourceRepo: json['source_repo'] as String? ?? '',
        versionTag: json['version_tag'] as String? ?? '',
        installedAt: json['installed_at'] as String? ?? '',
      );

  final String name;
  final String path;
  final String sourceRepo;
  final String versionTag;
  final String installedAt;
}
