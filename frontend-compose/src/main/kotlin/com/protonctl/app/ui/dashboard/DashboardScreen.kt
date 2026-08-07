package com.protonctl.app.ui.dashboard

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Palette
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExtendedFloatingActionButton
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.protonctl.app.service.RepositoryService
import com.protonctl.app.service.SettingsService
import com.protonctl.app.theme.ThemeController
import com.protonctl.app.ui.settings.ThemeSettingsScreen

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun DashboardScreen(settingsService: SettingsService, themeController: ThemeController) {
    val repositoryService = remember { RepositoryService(settingsService.config.compatibilityToolsDir) }
    var showSettings by remember { mutableStateOf(false) }
    var showAddRepositoryDialog by remember { mutableStateOf(false) }

    DisposableEffect(Unit) {
        repositoryService.refreshAll()
        onDispose { repositoryService.dispose() }
    }

    if (showSettings) {
        ThemeSettingsScreen(
            themeController = themeController,
            settingsService = settingsService,
            onBack = { showSettings = false },
        )
        return
    }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("ProtonCTL") },
                actions = {
                    IconButton(onClick = { showSettings = true }) {
                        Icon(Icons.Filled.Palette, contentDescription = "Theme settings")
                    }
                },
            )
        },
        floatingActionButton = {
            // The single most common action on this screen - per M3 guidance,
            // a FAB rather than one more icon buried in a card header.
            ExtendedFloatingActionButton(
                text = { Text("Add repository") },
                icon = { Icon(Icons.Filled.Add, contentDescription = null) },
                onClick = { showAddRepositoryDialog = true },
            )
        },
    ) { padding ->
        LazyVerticalGrid(
            columns = GridCells.Adaptive(minSize = 420.dp),
            modifier = Modifier.padding(padding).padding(16.dp),
            contentPadding = PaddingValues(bottom = 16.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp),
            horizontalArrangement = Arrangement.spacedBy(16.dp),
        ) {
            item { InstalledToolsCard(repositoryService) }
            item {
                RepositoryManagerCard(
                    repositoryService = repositoryService,
                    showAddDialog = showAddRepositoryDialog,
                    onDismissAddDialog = { showAddRepositoryDialog = false },
                )
            }
            item { BackupActionsCard(settingsService, repositoryService) }
            item { CompilationQueueCard(repositoryService) }
        }
    }
}
