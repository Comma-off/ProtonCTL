package com.protonctl.app.model

import kotlinx.serialization.Serializable

@Serializable
data class AppConfig(
    val installType: SteamInstallType = SteamInstallType.UNKNOWN,
    val steamPath: String = "",
    val compatibilityToolsDir: String = "",
    val firstStartCompleted: Boolean = false,
    val themeSpecVersion: String = "spec2025",
    val themeVariant: String = "expressive",
    val seedColorIndex: Int = 0,
)
