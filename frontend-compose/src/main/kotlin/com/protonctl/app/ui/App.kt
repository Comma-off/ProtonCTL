package com.protonctl.app.ui

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import com.protonctl.app.service.SettingsService
import com.protonctl.app.theme.ProtonCtlTheme
import com.protonctl.app.theme.ThemeController
import com.protonctl.app.ui.dashboard.DashboardScreen
import com.protonctl.app.ui.wizard.FirstStartWizard

@Composable
fun App() {
    val settingsService = remember { SettingsService() }
    var isLoaded by remember { mutableStateOf(false) }

    LaunchedEffect(Unit) {
        settingsService.load()
        isLoaded = true
    }

    if (!isLoaded) {
        Surface {
            Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                CircularProgressIndicator()
            }
        }
        return
    }

    val themeController = remember { ThemeController.fromAppConfig(settingsService.config) }

    ProtonCtlTheme(themeController) {
        Surface(color = MaterialTheme.colorScheme.background) {
            if (settingsService.config.firstStartCompleted) {
                DashboardScreen(settingsService = settingsService, themeController = themeController)
            } else {
                FirstStartWizard(settingsService = settingsService)
            }
        }
    }
}
