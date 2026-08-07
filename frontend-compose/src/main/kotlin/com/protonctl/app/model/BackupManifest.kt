package com.protonctl.app.model

import kotlinx.serialization.SerialName
import kotlinx.serialization.Serializable

@Serializable
data class PrtBakManifest(
    val formatVersion: String = "1",
    val protonVersion: String = "",
    val sourceRepo: String = "",
    val protonDirName: String = "",
    val winePrefixRelative: String = "",
    val dependencies: List<String> = emptyList(),
    val createdAt: String = "",
    val sha256ProtonDir: String = "",
)

@Serializable
data class CompatToolsEntry(
    val name: String,
    val sourceRepo: String = "",
    val versionTag: String = "",
)

/** Manifest for a whole-library backup - every installed Proton version
 * under compatibilitytools.d in one archive, as opposed to
 * [PrtBakManifest]'s one-version-plus-prefix scope. */
@Serializable
data class CompatToolsBackupManifest(
    val formatVersion: String = "1",
    val createdAt: String = "",
    val tools: List<CompatToolsEntry> = emptyList(),
)

@Serializable
enum class RepairIssueType {
    @SerialName("broken_symlink") BROKEN_SYMLINK,
    @SerialName("missing_runtime") MISSING_RUNTIME,
    @SerialName("corrupt_manifest") CORRUPT_MANIFEST,
    @SerialName("empty_proton_dir") EMPTY_PROTON_DIR,
    @SerialName("orphaned_prefix") ORPHANED_PREFIX,
}

@Serializable
data class RepairIssue(
    val type: RepairIssueType,
    val path: String,
    val description: String,
    val fixable: Boolean,
)
