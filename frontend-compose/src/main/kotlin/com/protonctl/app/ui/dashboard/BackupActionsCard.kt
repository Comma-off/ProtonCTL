package com.protonctl.app.ui.dashboard

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Archive
import androidx.compose.material.icons.filled.BuildCircle
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.CloudUpload
import androidx.compose.material.icons.filled.ErrorOutline
import androidx.compose.material.icons.filled.FolderOpen
import androidx.compose.material.icons.filled.HealthAndSafety
import androidx.compose.material.icons.filled.Inventory2
import androidx.compose.material.icons.filled.Restore
import androidx.compose.material.icons.filled.Save
import androidx.compose.material.icons.filled.WarningAmber
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.ExposedDropdownMenuBox
import androidx.compose.material3.ExposedDropdownMenuDefaults
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
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
import androidx.compose.ui.unit.dp
import com.protonctl.app.model.InstalledProtonVersion
import com.protonctl.app.model.PrtBakManifest
import com.protonctl.app.model.RepairIssue
import com.protonctl.app.native.ProtonCtlNativeException
import com.protonctl.app.service.BackupService
import com.protonctl.app.service.RepositoryService
import com.protonctl.app.service.SettingsService
import io.github.vinceglb.filekit.dialogs.FileKitDialogSettings
import io.github.vinceglb.filekit.dialogs.FileKitType
import io.github.vinceglb.filekit.dialogs.compose.rememberDirectoryPickerLauncher
import io.github.vinceglb.filekit.dialogs.compose.rememberFilePickerLauncher
import io.github.vinceglb.filekit.dialogs.compose.rememberFileSaverLauncher
import kotlinx.coroutines.launch

private data class StatusMessage(val text: String, val isError: Boolean)

// M3's error color role is meant to be paired with errorContainer/onErrorContainer
// for anything more prominent than field-level supporting text - a plain
// default-colored Text() doesn't visually distinguish "it worked" from "it
// failed" at a glance, which is what this is for.
@Composable
private fun StatusBanner(status: StatusMessage) {
    Surface(
        modifier = Modifier.fillMaxWidth().padding(top = 12.dp),
        shape = MaterialTheme.shapes.medium,
        color = if (status.isError) MaterialTheme.colorScheme.errorContainer else MaterialTheme.colorScheme.primaryContainer,
        contentColor = if (status.isError) MaterialTheme.colorScheme.onErrorContainer else MaterialTheme.colorScheme.onPrimaryContainer,
    ) {
        Row(
            modifier = Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 12.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Icon(
                if (status.isError) Icons.Filled.ErrorOutline else Icons.Filled.CheckCircle,
                contentDescription = null,
            )
            Spacer(Modifier.width(12.dp))
            Text(status.text, style = MaterialTheme.typography.bodySmall)
        }
    }
}

@Composable
fun BackupActionsCard(settingsService: SettingsService, repositoryService: RepositoryService) {
    val backupService = remember { BackupService() }
    val scope = rememberCoroutineScope()

    var showCreateDialog by remember { mutableStateOf(false) }
    var showRestoreDialog by remember { mutableStateOf(false) }
    var showLibraryBackupDialog by remember { mutableStateOf(false) }
    var showLibraryRestoreDialog by remember { mutableStateOf(false) }
    var repairResults by remember { mutableStateOf<List<RepairIssue>?>(null) }
    var repairError by remember { mutableStateOf<String?>(null) }
    var busy by remember { mutableStateOf(false) }
    var status by remember { mutableStateOf<StatusMessage?>(null) }

    Card(
        modifier = Modifier.fillMaxWidth().height(420.dp),
        shape = MaterialTheme.shapes.extraLarge,
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainer),
    ) {
        Column(modifier = Modifier.fillMaxSize().padding(20.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Icon(Icons.Filled.Archive, contentDescription = null, tint = MaterialTheme.colorScheme.primary)
                Text(
                    "Backup & Repair",
                    style = MaterialTheme.typography.titleMedium,
                    modifier = Modifier.padding(start = 12.dp),
                )
            }
            Spacer(Modifier.height(24.dp))

            Column(
                modifier = Modifier.fillMaxSize().verticalScroll(rememberScrollState()),
                verticalArrangement = Arrangement.Center,
                horizontalAlignment = Alignment.CenterHorizontally,
            ) {
                Button(
                    onClick = { showCreateDialog = true },
                    enabled = !busy,
                    modifier = Modifier.fillMaxWidth(0.8f),
                ) {
                    Icon(Icons.Filled.Save, contentDescription = null)
                    Spacer(Modifier.width(8.dp))
                    Text("Create .tar.gz backup")
                }
                Spacer(Modifier.height(12.dp))
                OutlinedButton(
                    onClick = { showRestoreDialog = true },
                    enabled = !busy,
                    modifier = Modifier.fillMaxWidth(0.8f),
                ) {
                    Icon(Icons.Filled.Restore, contentDescription = null)
                    Spacer(Modifier.width(8.dp))
                    Text("Restore from backup")
                }
                Spacer(Modifier.height(12.dp))
                OutlinedButton(
                    onClick = { showLibraryBackupDialog = true },
                    enabled = !busy,
                    modifier = Modifier.fillMaxWidth(0.8f),
                ) {
                    Icon(Icons.Filled.Inventory2, contentDescription = null)
                    Spacer(Modifier.width(8.dp))
                    Text("Back up entire library")
                }
                Spacer(Modifier.height(12.dp))
                OutlinedButton(
                    onClick = { showLibraryRestoreDialog = true },
                    enabled = !busy,
                    modifier = Modifier.fillMaxWidth(0.8f),
                ) {
                    Icon(Icons.Filled.CloudUpload, contentDescription = null)
                    Spacer(Modifier.width(8.dp))
                    Text("Import Proton library")
                }
                Spacer(Modifier.height(12.dp))
                OutlinedButton(
                    enabled = !busy,
                    modifier = Modifier.fillMaxWidth(0.8f),
                    onClick = {
                        scope.launch {
                            busy = true
                            repairError = null
                            try {
                                repairResults = backupService.scanRepair(
                                    settingsService.config.compatibilityToolsDir,
                                    "${settingsService.config.steamPath}/steamapps/compatdata",
                                )
                            } catch (e: ProtonCtlNativeException) {
                                repairError = e.message
                                repairResults = emptyList()
                            } finally {
                                busy = false
                            }
                        }
                    },
                ) {
                    Icon(Icons.Filled.HealthAndSafety, contentDescription = null)
                    Spacer(Modifier.width(8.dp))
                    Text("Scan for broken installs")
                }
                if (busy) {
                    Spacer(Modifier.height(16.dp))
                    CircularProgressIndicator()
                }
                status?.let { StatusBanner(it) }
            }
        }
    }

    if (showCreateDialog) {
        CreateBackupDialog(
            installed = repositoryService.installed,
            onDismiss = { showCreateDialog = false },
            onCreate = { protonDir, prefixDir, manifest, outFile ->
                scope.launch {
                    busy = true
                    try {
                        backupService.createBackup(protonDir, prefixDir, manifest, outFile)
                        status = StatusMessage("Backup created", isError = false)
                    } catch (e: ProtonCtlNativeException) {
                        status = StatusMessage("Backup failed: ${e.message}", isError = true)
                    } finally {
                        busy = false
                    }
                }
                showCreateDialog = false
            },
        )
    }

    if (showRestoreDialog) {
        RestoreBackupDialog(
            defaultRestoreRoot = settingsService.config.compatibilityToolsDir,
            onDismiss = { showRestoreDialog = false },
            onRestore = { prtbakFile, restoreRoot ->
                scope.launch {
                    busy = true
                    try {
                        backupService.restoreBackup(prtbakFile, restoreRoot)
                        repositoryService.refreshInstalled()
                        status = StatusMessage("Backup restored", isError = false)
                    } catch (e: ProtonCtlNativeException) {
                        status = StatusMessage("Restore failed: ${e.message}", isError = true)
                    } finally {
                        busy = false
                    }
                }
                showRestoreDialog = false
            },
        )
    }

    if (showLibraryBackupDialog) {
        CreateLibraryBackupDialog(
            installedCount = repositoryService.installed.size,
            onDismiss = { showLibraryBackupDialog = false },
            onCreate = { outFile ->
                scope.launch {
                    busy = true
                    try {
                        backupService.createLibraryBackup(settingsService.config.compatibilityToolsDir, outFile)
                        status = StatusMessage("Library backup created", isError = false)
                    } catch (e: ProtonCtlNativeException) {
                        status = StatusMessage("Library backup failed: ${e.message}", isError = true)
                    } finally {
                        busy = false
                    }
                }
                showLibraryBackupDialog = false
            },
        )
    }

    if (showLibraryRestoreDialog) {
        RestoreLibraryBackupDialog(
            defaultCompatToolsDir = settingsService.config.compatibilityToolsDir,
            onDismiss = { showLibraryRestoreDialog = false },
            onRestore = { prtbakFile, compatToolsDir ->
                scope.launch {
                    busy = true
                    try {
                        backupService.restoreLibraryBackup(prtbakFile, compatToolsDir)
                        repositoryService.refreshInstalled()
                        status = StatusMessage("Proton library imported", isError = false)
                    } catch (e: ProtonCtlNativeException) {
                        status = StatusMessage("Import failed: ${e.message}", isError = true)
                    } finally {
                        busy = false
                    }
                }
                showLibraryRestoreDialog = false
            },
        )
    }

    if (repairResults != null || repairError != null) {
        RepairResultsDialog(
            issues = repairResults ?: emptyList(),
            error = repairError,
            backupService = backupService,
            onDismiss = { repairResults = null; repairError = null },
        )
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun CreateBackupDialog(
    installed: List<InstalledProtonVersion>,
    onDismiss: () -> Unit,
    onCreate: (protonDir: String, prefixDir: String?, manifest: PrtBakManifest, outFile: String) -> Unit,
) {
    if (installed.isEmpty()) {
        AlertDialog(
            onDismissRequest = onDismiss,
            title = { Text("No Proton builds") },
            text = { Text("No installed Proton builds to back up yet.") },
            confirmButton = { TextButton(onClick = onDismiss) { Text("OK") } },
        )
        return
    }

    var selected by remember { mutableStateOf(installed.first()) }
    var expanded by remember { mutableStateOf(false) }
    var prefixDir by remember { mutableStateOf("") }
    var outFile by remember { mutableStateOf("${selected.name}.tar.gz") }

    val prefixDirPicker = rememberDirectoryPickerLauncher { picked ->
        picked?.let { prefixDir = it.file.absolutePath }
    }
    val outFileSaver = rememberFileSaverLauncher(dialogSettings = FileKitDialogSettings.createDefault()) { saved ->
        saved?.let { outFile = it.file.absolutePath }
    }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Create backup") },
        text = {
            Column {
                ExposedDropdownMenuBox(expanded = expanded, onExpandedChange = { expanded = it }) {
                    OutlinedTextField(
                        value = selected.name,
                        onValueChange = {},
                        readOnly = true,
                        label = { Text("Proton build") },
                        trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded = expanded) },
                        modifier = Modifier.fillMaxWidth().widthIn(min = 320.dp),
                    )
                    ExposedDropdownMenu(
                        expanded = expanded,
                        onDismissRequest = { expanded = false },
                    ) {
                        installed.forEach { version ->
                            DropdownMenuItem(
                                text = { Text(version.name) },
                                onClick = {
                                    selected = version
                                    outFile = "${version.name}.tar.gz"
                                    expanded = false
                                },
                            )
                        }
                    }
                }
                Spacer(Modifier.height(12.dp))
                OutlinedTextField(
                    value = prefixDir,
                    onValueChange = { prefixDir = it },
                    label = { Text("Wine prefix path (optional)") },
                    placeholder = { Text(".../steamapps/compatdata/<appid>/pfx") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                    trailingIcon = {
                        IconButton(onClick = { prefixDirPicker.launch() }) {
                            Icon(Icons.Filled.FolderOpen, contentDescription = "Browse")
                        }
                    },
                )
                Spacer(Modifier.height(12.dp))
                OutlinedTextField(
                    value = outFile,
                    onValueChange = { outFile = it },
                    label = { Text("Output .tar.gz file") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                    trailingIcon = {
                        IconButton(onClick = {
                            outFileSaver.launch(suggestedName = outFile, allowedExtensions = setOf("tar.gz"))
                        }) { Icon(Icons.Filled.FolderOpen, contentDescription = "Browse") }
                    },
                )
            }
        },
        confirmButton = {
            Button(onClick = {
                onCreate(
                    selected.path,
                    prefixDir.trim().ifEmpty { null },
                    PrtBakManifest(
                        protonVersion = selected.versionTag.ifEmpty { selected.name },
                        sourceRepo = selected.sourceRepo,
                    ),
                    outFile.trim(),
                )
            }) { Text("Create") }
        },
        dismissButton = { OutlinedButton(onClick = onDismiss) { Text("Cancel") } },
    )
}

@Composable
private fun RestoreBackupDialog(
    defaultRestoreRoot: String,
    onDismiss: () -> Unit,
    onRestore: (prtbakFile: String, restoreRoot: String) -> Unit,
) {
    var prtbakFile by remember { mutableStateOf("") }
    var restoreRoot by remember { mutableStateOf(defaultRestoreRoot) }

    val prtbakFilePicker = rememberFilePickerLauncher(type = FileKitType.File("tar.gz")) { picked ->
        picked?.let { prtbakFile = it.file.absolutePath }
    }
    val restoreRootPicker = rememberDirectoryPickerLauncher { picked ->
        picked?.let { restoreRoot = it.file.absolutePath }
    }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Restore backup") },
        text = {
            Column {
                OutlinedTextField(
                    value = prtbakFile,
                    onValueChange = { prtbakFile = it },
                    label = { Text(".tar.gz file path") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                    trailingIcon = {
                        IconButton(onClick = { prtbakFilePicker.launch() }) {
                            Icon(Icons.Filled.FolderOpen, contentDescription = "Browse")
                        }
                    },
                )
                Spacer(Modifier.height(12.dp))
                OutlinedTextField(
                    value = restoreRoot,
                    onValueChange = { restoreRoot = it },
                    label = { Text("Restore into (compatibilitytools.d)") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                    trailingIcon = {
                        IconButton(onClick = { restoreRootPicker.launch() }) {
                            Icon(Icons.Filled.FolderOpen, contentDescription = "Browse")
                        }
                    },
                )
            }
        },
        confirmButton = {
            Button(onClick = { onRestore(prtbakFile.trim(), restoreRoot.trim()) }) { Text("Restore") }
        },
        dismissButton = { OutlinedButton(onClick = onDismiss) { Text("Cancel") } },
    )
}

@Composable
private fun CreateLibraryBackupDialog(
    installedCount: Int,
    onDismiss: () -> Unit,
    onCreate: (outFile: String) -> Unit,
) {
    if (installedCount == 0) {
        AlertDialog(
            onDismissRequest = onDismiss,
            title = { Text("No Proton builds") },
            text = { Text("No installed Proton builds to back up yet.") },
            confirmButton = { TextButton(onClick = onDismiss) { Text("OK") } },
        )
        return
    }

    var outFile by remember { mutableStateOf("protonctl-library.tar.gz") }

    val outFileSaver = rememberFileSaverLauncher(dialogSettings = FileKitDialogSettings.createDefault()) { saved ->
        saved?.let { outFile = it.file.absolutePath }
    }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Back up entire library") },
        text = {
            Column {
                Text(
                    "Packs all $installedCount installed Proton build(s) under compatibilitytools.d " +
                        "into one archive, so you can bring your whole collection to another instance " +
                        "with \"Import Proton library\".",
                    style = MaterialTheme.typography.bodyMedium,
                )
                Spacer(Modifier.height(12.dp))
                OutlinedTextField(
                    value = outFile,
                    onValueChange = { outFile = it },
                    label = { Text("Output .tar.gz file") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                    trailingIcon = {
                        IconButton(onClick = {
                            outFileSaver.launch(suggestedName = outFile, allowedExtensions = setOf("tar.gz"))
                        }) { Icon(Icons.Filled.FolderOpen, contentDescription = "Browse") }
                    },
                )
            }
        },
        confirmButton = {
            Button(onClick = { onCreate(outFile.trim()) }) { Text("Create") }
        },
        dismissButton = { OutlinedButton(onClick = onDismiss) { Text("Cancel") } },
    )
}

@Composable
private fun RestoreLibraryBackupDialog(
    defaultCompatToolsDir: String,
    onDismiss: () -> Unit,
    onRestore: (prtbakFile: String, compatToolsDir: String) -> Unit,
) {
    var prtbakFile by remember { mutableStateOf("") }
    var compatToolsDir by remember { mutableStateOf(defaultCompatToolsDir) }

    val prtbakFilePicker = rememberFilePickerLauncher(type = FileKitType.File("tar.gz")) { picked ->
        picked?.let { prtbakFile = it.file.absolutePath }
    }
    val compatToolsDirPicker = rememberDirectoryPickerLauncher { picked ->
        picked?.let { compatToolsDir = it.file.absolutePath }
    }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Import Proton library") },
        text = {
            Column {
                Text(
                    "Restores every Proton build from a whole-library backup into " +
                        "compatibilitytools.d, adding to (or overwriting) whatever's already there.",
                    style = MaterialTheme.typography.bodyMedium,
                )
                Spacer(Modifier.height(12.dp))
                OutlinedTextField(
                    value = prtbakFile,
                    onValueChange = { prtbakFile = it },
                    label = { Text(".tar.gz library file path") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                    trailingIcon = {
                        IconButton(onClick = { prtbakFilePicker.launch() }) {
                            Icon(Icons.Filled.FolderOpen, contentDescription = "Browse")
                        }
                    },
                )
                Spacer(Modifier.height(12.dp))
                OutlinedTextField(
                    value = compatToolsDir,
                    onValueChange = { compatToolsDir = it },
                    label = { Text("Restore into (compatibilitytools.d)") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                    trailingIcon = {
                        IconButton(onClick = { compatToolsDirPicker.launch() }) {
                            Icon(Icons.Filled.FolderOpen, contentDescription = "Browse")
                        }
                    },
                )
            }
        },
        confirmButton = {
            Button(onClick = { onRestore(prtbakFile.trim(), compatToolsDir.trim()) }) { Text("Import") }
        },
        dismissButton = { OutlinedButton(onClick = onDismiss) { Text("Cancel") } },
    )
}

@Composable
private fun RepairResultsDialog(
    issues: List<RepairIssue>,
    error: String?,
    backupService: BackupService,
    onDismiss: () -> Unit,
) {
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Repair scan results") },
        text = {
            Column(modifier = Modifier.widthIn(min = 420.dp).height(320.dp)) {
                when {
                    error != null -> Text(error)
                    issues.isEmpty() -> Text("No issues found - everything looks healthy.")
                    else -> LazyColumn {
                        items(issues, key = { it.path }) { issue ->
                            Row(
                                modifier = Modifier.fillMaxWidth().padding(vertical = 6.dp),
                                verticalAlignment = Alignment.CenterVertically,
                            ) {
                                Icon(
                                    if (issue.fixable) Icons.Filled.BuildCircle else Icons.Filled.WarningAmber,
                                    contentDescription = null,
                                )
                                Column(modifier = Modifier.weight(1f).padding(start = 12.dp)) {
                                    Text(issue.path, maxLines = 1)
                                    Text(issue.description, style = MaterialTheme.typography.bodySmall)
                                }
                                if (issue.fixable) {
                                    TextButton(onClick = { backupService.fixIssue(issue) }) { Text("Fix") }
                                }
                            }
                        }
                    }
                }
            }
        },
        confirmButton = {},
        dismissButton = { OutlinedButton(onClick = onDismiss) { Text("Close") } },
    )
}
