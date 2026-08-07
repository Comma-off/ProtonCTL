package com.protonctl.app.model

import kotlinx.serialization.SerialName
import kotlinx.serialization.Serializable

@Serializable
enum class SteamInstallType {
    @SerialName("flatpak") FLATPAK,
    @SerialName("debian") DEBIAN,
    @SerialName("native") NATIVE,
    @SerialName("custom") CUSTOM,
    @SerialName("unknown") UNKNOWN,
}

@Serializable
data class SteamCandidate(
    val type: SteamInstallType,
    val path: String,
    val valid: Boolean,
    val label: String,
)
