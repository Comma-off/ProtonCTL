package com.protonctl.app.model

import kotlinx.serialization.SerialName
import kotlinx.serialization.Serializable

@Serializable
enum class BuildStage {
    @SerialName("queued") QUEUED,
    @SerialName("cloning") CLONING,
    @SerialName("configuring") CONFIGURING,
    @SerialName("building") BUILDING,
    @SerialName("packaging") PACKAGING,
    @SerialName("done") DONE,
    @SerialName("failed") FAILED,
    @SerialName("cancelled") CANCELLED,
}

val BuildStage.isTerminal: Boolean
    get() = this == BuildStage.DONE || this == BuildStage.FAILED || this == BuildStage.CANCELLED

@Serializable
data class BuildJob(
    val id: String,
    val repo: ProtonRepository,
    val ref: String = "",
    val stage: BuildStage = BuildStage.QUEUED,
    val progressPercent: Int = 0,
    val logLines: List<String> = emptyList(),
    val error: String = "",
    val resultPath: String = "",
)
