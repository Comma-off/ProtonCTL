package com.protonctl.app.model

import kotlinx.serialization.SerialName
import kotlinx.serialization.Serializable

@Serializable
enum class InstallStage {
    @SerialName("queued") QUEUED,
    @SerialName("downloading") DOWNLOADING,
    @SerialName("extracting") EXTRACTING,
    @SerialName("done") DONE,
    @SerialName("failed") FAILED,
    @SerialName("cancelled") CANCELLED,
}

val InstallStage.isTerminal: Boolean
    get() = this == InstallStage.DONE || this == InstallStage.FAILED || this == InstallStage.CANCELLED

@Serializable
data class InstallJob(
    val id: String,
    val repo: ProtonRepository,
    val release: GitHubRelease,
    val stage: InstallStage = InstallStage.QUEUED,
    val progressPercent: Int = 0,
    val bytesDownloaded: Long = 0,
    val bytesTotal: Long = 0,
    val error: String = "",
    val result: InstalledProtonVersion? = null,
)
