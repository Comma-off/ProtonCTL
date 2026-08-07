package com.protonctl.app.service

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import com.protonctl.app.model.AppConfig
import com.protonctl.app.model.SteamInstallType
import com.protonctl.app.native.ProtonCtlBridge

/** Wraps the native [AppConfig] so the rest of the app reads/writes it as
 * plain Compose state instead of poking JSON through the bridge directly. */
class SettingsService {
    var config by mutableStateOf(AppConfig())
        private set

    fun load() {
        config = ProtonCtlBridge.loadConfig()
    }

    fun update(updater: (AppConfig) -> AppConfig) {
        config = updater(config)
        ProtonCtlBridge.saveConfig(config)
    }

    fun completeFirstStart(steamPath: String, compatibilityToolsDir: String, installType: SteamInstallType) {
        update {
            it.copy(
                steamPath = steamPath,
                compatibilityToolsDir = compatibilityToolsDir,
                installType = installType,
                firstStartCompleted = true,
            )
        }
    }
}
