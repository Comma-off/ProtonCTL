package com.protonctl.app.model

import kotlinx.serialization.Serializable

@Serializable
data class ProtonRepository(
    val owner: String,
    val repo: String,
    val displayName: String,
    val isBuiltin: Boolean = false,
    val buildCommandOverride: String = "",
) {
    val fullName: String get() = "$owner/$repo"
}

@Serializable
data class GitHubAsset(
    val name: String,
    val browserDownloadUrl: String,
    val size: Long = 0,
    val contentType: String = "",
)

@Serializable
data class GitHubRelease(
    val tagName: String,
    val name: String,
    val body: String = "",
    val publishedAt: String = "",
    val prerelease: Boolean = false,
    val assets: List<GitHubAsset> = emptyList(),
)

@Serializable
data class InstalledProtonVersion(
    val name: String,
    val path: String,
    val sourceRepo: String = "",
    val versionTag: String = "",
    val installedAt: String = "",
)
