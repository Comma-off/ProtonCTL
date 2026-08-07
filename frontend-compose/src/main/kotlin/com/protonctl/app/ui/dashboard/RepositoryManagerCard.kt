package com.protonctl.app.ui.dashboard

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Build
import androidx.compose.material.icons.filled.CloudDownload
import androidx.compose.material.icons.filled.DeleteOutline
import androidx.compose.material.icons.filled.Source
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.ListItem
import androidx.compose.material3.ListItemDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
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
import com.protonctl.app.model.ProtonRepository
import com.protonctl.app.native.ProtonCtlNativeException
import com.protonctl.app.service.RepositoryService
import com.protonctl.app.util.formatRelativeDate
import kotlinx.coroutines.launch

@Composable
fun RepositoryManagerCard(
    repositoryService: RepositoryService,
    showAddDialog: Boolean,
    onDismissAddDialog: () -> Unit,
) {
    var releasesFor by remember { mutableStateOf<ProtonRepository?>(null) }
    var buildFor by remember { mutableStateOf<ProtonRepository?>(null) }

    Card(
        modifier = Modifier.fillMaxWidth().height(420.dp),
        shape = MaterialTheme.shapes.extraLarge,
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainer),
    ) {
        Column(modifier = Modifier.fillMaxSize().padding(20.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Icon(Icons.Filled.Source, contentDescription = null, tint = MaterialTheme.colorScheme.primary)
                Text(
                    "Repository Manager",
                    style = MaterialTheme.typography.titleMedium,
                    modifier = Modifier.padding(start = 12.dp).weight(1f),
                )
            }

            if (repositoryService.loadingRepositories) {
                CircularProgressIndicator(modifier = Modifier.padding(top = 24.dp))
            } else {
                LazyColumn(modifier = Modifier.fillMaxSize()) {
                    items(repositoryService.repositories, key = { it.fullName }) { repo ->
                        ListItem(
                            headlineContent = { Text(repo.fullName) },
                            supportingContent = { Text(if (repo.isBuiltin) "Built-in" else "Custom") },
                            trailingContent = {
                                Row(verticalAlignment = Alignment.CenterVertically) {
                                    IconButton(onClick = { buildFor = repo }) {
                                        Icon(Icons.Filled.Build, contentDescription = "Build from source")
                                    }
                                    IconButton(onClick = { releasesFor = repo }) {
                                        Icon(Icons.Filled.CloudDownload, contentDescription = "View releases")
                                    }
                                    if (!repo.isBuiltin) {
                                        IconButton(onClick = { repositoryService.removeRepository(repo) }) {
                                            Icon(Icons.Filled.DeleteOutline, contentDescription = "Remove")
                                        }
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

    if (showAddDialog) {
        AddRepositoryDialog(
            repositoryService = repositoryService,
            onDismiss = onDismissAddDialog,
        )
    }

    releasesFor?.let { repo ->
        ReleasesDialog(repo = repo, repositoryService = repositoryService, onDismiss = { releasesFor = null })
    }

    buildFor?.let { repo ->
        BuildFromSourceDialog(repo = repo, repositoryService = repositoryService, onDismiss = { buildFor = null })
    }
}

@Composable
private fun AddRepositoryDialog(repositoryService: RepositoryService, onDismiss: () -> Unit) {
    var owner by remember { mutableStateOf("") }
    var repo by remember { mutableStateOf("") }
    var error by remember { mutableStateOf<String?>(null) }
    var busy by remember { mutableStateOf(false) }
    val scope = rememberCoroutineScope()

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Add custom repository") },
        text = {
            Column {
                OutlinedTextField(
                    value = owner,
                    onValueChange = { owner = it },
                    label = { Text("Owner") },
                    placeholder = { Text("e.g. GloriousEggroll") },
                    singleLine = true,
                )
                androidx.compose.foundation.layout.Spacer(Modifier.height(12.dp))
                OutlinedTextField(
                    value = repo,
                    onValueChange = { repo = it },
                    label = { Text("Repository") },
                    placeholder = { Text("e.g. proton-ge-custom") },
                    singleLine = true,
                )
                error?.let {
                    androidx.compose.foundation.layout.Spacer(Modifier.height(12.dp))
                    Text(it, color = MaterialTheme.colorScheme.error)
                }
            }
        },
        confirmButton = {
            Button(
                enabled = !busy && owner.isNotBlank() && repo.isNotBlank(),
                onClick = {
                    scope.launch {
                        busy = true
                        try {
                            repositoryService.addRepository(owner.trim(), repo.trim())
                            onDismiss()
                        } catch (e: ProtonCtlNativeException) {
                            error = e.message
                        } finally {
                            busy = false
                        }
                    }
                },
            ) { Text("Add") }
        },
        dismissButton = { OutlinedButton(onClick = onDismiss) { Text("Cancel") } },
    )
}

@Composable
private fun ReleasesDialog(repo: ProtonRepository, repositoryService: RepositoryService, onDismiss: () -> Unit) {
    var releases by remember { mutableStateOf<List<GitHubRelease>?>(null) }
    var error by remember { mutableStateOf<String?>(null) }

    androidx.compose.runtime.LaunchedEffect(repo.fullName) {
        try {
            releases = repositoryService.fetchReleases(repo)
        } catch (e: ProtonCtlNativeException) {
            error = e.message
        }
    }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Releases for ${repo.fullName}") },
        text = {
            Column(modifier = Modifier.widthIn(min = 420.dp).height(360.dp)) {
                when {
                    error != null -> Text(error!!)
                    releases == null -> CircularProgressIndicator()
                    releases!!.isEmpty() -> Text("No releases found.")
                    else -> LazyColumn {
                        items(releases!!, key = { it.tagName }) { release ->
                            val publishedAt = formatRelativeDate(release.publishedAt)
                            ListItem(
                                headlineContent = { Text(release.tagName) },
                                supportingContent = {
                                    Text(
                                        buildString {
                                            append("${release.assets.size} asset(s)")
                                            if (release.prerelease) append(" - prerelease")
                                            if (publishedAt.isNotEmpty()) append(" · $publishedAt")
                                        },
                                    )
                                },
                                trailingContent = {
                                    Button(
                                        onClick = {
                                            repositoryService.enqueueInstall(repo, release)
                                            onDismiss()
                                        },
                                    ) { Text("Install") }
                                },
                                colors = ListItemDefaults.colors(containerColor = Color.Transparent),
                            )
                        }
                    }
                }
            }
        },
        confirmButton = {},
        dismissButton = { OutlinedButton(onClick = onDismiss) { Text("Close") } },
    )
}

@Composable
private fun BuildFromSourceDialog(repo: ProtonRepository, repositoryService: RepositoryService, onDismiss: () -> Unit) {
    var ref by remember { mutableStateOf("") }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Build ${repo.fullName} from source") },
        text = {
            Column {
                Text(
                    "Clones the repository and runs its build pipeline (build.sh, " +
                        "autogen.sh+configure+make, or a bare Makefile), then packages the " +
                        "output into compatibilitytools.d.",
                    style = MaterialTheme.typography.bodyMedium,
                )
                androidx.compose.foundation.layout.Spacer(Modifier.height(12.dp))
                OutlinedTextField(
                    value = ref,
                    onValueChange = { ref = it },
                    label = { Text("Branch / tag / commit (optional)") },
                    placeholder = { Text("default branch") },
                    singleLine = true,
                )
            }
        },
        confirmButton = {
            Button(onClick = {
                repositoryService.enqueueBuild(repo, ref.trim())
                onDismiss()
            }) { Text("Queue build") }
        },
        dismissButton = { OutlinedButton(onClick = onDismiss) { Text("Cancel") } },
    )
}
