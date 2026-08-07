package com.protonctl.app.ui.dashboard

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.DeleteOutline
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.SportsEsports
import androidx.compose.material.icons.filled.Update
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.ListItem
import androidx.compose.material3.ListItemDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Button
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import com.protonctl.app.model.GitHubRelease
import com.protonctl.app.model.InstalledProtonVersion
import com.protonctl.app.model.ProtonRepository
import com.protonctl.app.service.RepositoryService
import com.protonctl.app.util.formatRelativeDate
import kotlinx.coroutines.launch

@Composable
fun InstalledToolsCard(repositoryService: RepositoryService) {
    var pendingRemoval by remember { mutableStateOf<InstalledProtonVersion?>(null) }
    var pendingUpdate by remember { mutableStateOf<InstalledProtonVersion?>(null) }
    val scope = rememberCoroutineScope()

    Card(
        modifier = Modifier.fillMaxWidth().height(420.dp),
        shape = MaterialTheme.shapes.extraLarge,
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainer),
    ) {
        Column(modifier = Modifier.fillMaxSize().padding(20.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Icon(Icons.Filled.SportsEsports, contentDescription = null, tint = MaterialTheme.colorScheme.primary)
                Text(
                    "Installed Proton Tools",
                    style = MaterialTheme.typography.titleMedium,
                    modifier = Modifier.padding(start = 12.dp).weight(1f),
                )
                if (repositoryService.checkingUpdates) {
                    CircularProgressIndicator(modifier = Modifier.size(20.dp))
                } else {
                    IconButton(onClick = { scope.launch { repositoryService.checkForUpdates() } }) {
                        Icon(Icons.Filled.Update, contentDescription = "Check for updates")
                    }
                }
                IconButton(onClick = { repositoryService.refreshInstalled() }) {
                    Icon(Icons.Filled.Refresh, contentDescription = "Refresh")
                }
            }

            if (repositoryService.installed.isEmpty()) {
                Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                    Text(
                        "No Proton builds installed yet.\nAdd a repository and install a release to get started.",
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
            } else {
                LazyColumn(modifier = Modifier.fillMaxSize()) {
                    items(repositoryService.installed, key = { it.name }) { version ->
                        val installedAt = formatRelativeDate(version.installedAt)
                        val update = repositoryService.availableUpdates[version.name]
                        ListItem(
                            headlineContent = { Text(version.name, maxLines = 1) },
                            supportingContent = {
                                Column {
                                    Text(
                                        buildString {
                                            append(version.sourceRepo.ifEmpty { "Manually installed" })
                                            if (installedAt.isNotEmpty()) append(" · $installedAt")
                                        },
                                        maxLines = 1,
                                    )
                                    if (update != null) {
                                        Text(
                                            "Update available: ${update.tagName}",
                                            maxLines = 1,
                                            color = MaterialTheme.colorScheme.primary,
                                        )
                                    }
                                }
                            },
                            leadingContent = {
                                Icon(
                                    Icons.Filled.CheckCircle,
                                    contentDescription = null,
                                    tint = MaterialTheme.colorScheme.primary,
                                )
                            },
                            trailingContent = {
                                Row(verticalAlignment = Alignment.CenterVertically) {
                                    if (update != null) {
                                        TextButton(onClick = { pendingUpdate = version }) { Text("Update") }
                                    }
                                    IconButton(onClick = { pendingRemoval = version }) {
                                        Icon(Icons.Filled.DeleteOutline, contentDescription = "Remove")
                                    }
                                }
                            },
                            colors = ListItemDefaults.colors(containerColor = Color.Transparent),
                        )
                    }
                }
            }
        }
    }

    pendingRemoval?.let { version ->
        AlertDialog(
            onDismissRequest = { pendingRemoval = null },
            title = { Text("Remove Proton build?") },
            text = { Text("This deletes ${version.path} from disk.") },
            confirmButton = {
                Button(onClick = {
                    repositoryService.removeInstalledVersion(version.name)
                    pendingRemoval = null
                }) { Text("Remove") }
            },
            dismissButton = {
                OutlinedButton(onClick = { pendingRemoval = null }) { Text("Cancel") }
            },
        )
    }

    pendingUpdate?.let { version ->
        val update: GitHubRelease? = repositoryService.availableUpdates[version.name]
        if (update == null) {
            pendingUpdate = null
        } else {
            AlertDialog(
                onDismissRequest = { pendingUpdate = null },
                title = { Text("Install update?") },
                text = {
                    Text(
                        "${update.tagName} is available for ${version.sourceRepo}. It will be installed " +
                            "as a new, separate build alongside ${version.name} - your current install " +
                            "won't be modified or removed.",
                    )
                },
                confirmButton = {
                    Button(onClick = {
                        val parts = version.sourceRepo.split("/", limit = 2)
                        val repo = ProtonRepository(owner = parts[0], repo = parts[1], displayName = parts[1])
                        repositoryService.enqueueInstall(repo, update)
                        repositoryService.dismissAvailableUpdate(version.name)
                        pendingUpdate = null
                    }) { Text("Install") }
                },
                dismissButton = {
                    OutlinedButton(onClick = { pendingUpdate = null }) { Text("Cancel") }
                },
            )
        }
    }
}
