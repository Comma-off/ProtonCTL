package com.protonctl.app.ui.wizard

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.Error
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Icon
import androidx.compose.material3.ListItem
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
import com.protonctl.app.model.SteamCandidate
import com.protonctl.app.service.SettingsService
import com.protonctl.app.service.SteamService
import kotlinx.coroutines.launch

@Composable
fun FirstStartWizard(settingsService: SettingsService) {
    val steamService = remember { SteamService() }
    val scope = rememberCoroutineScope()

    var step by remember { mutableStateOf(0) }
    val candidates = remember { steamService.detectCandidates() }
    var selected by remember { mutableStateOf(candidates.firstOrNull { it.valid }) }
    var customPath by remember { mutableStateOf("") }
    var customValidated by remember { mutableStateOf<SteamCandidate?>(null) }
    var validating by remember { mutableStateOf(false) }

    Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.TopCenter) {
        Column(
            modifier = Modifier
                .widthIn(max = 640.dp)
                .fillMaxWidth()
                .verticalScroll(rememberScrollState())
                .padding(32.dp),
        ) {
            Text("Welcome to ProtonCTL", style = MaterialTheme.typography.headlineSmall)
            Spacer(Modifier.height(8.dp))
            Text(
                "PROTONCTL manages Proton-GE and custom Proton builds for your Steam library: " +
                    "installing releases, compiling custom repositories, and backing up wine " +
                    "prefixes as portable .prtbak archives.",
                style = MaterialTheme.typography.bodyMedium,
            )
            Spacer(Modifier.height(24.dp))

            if (step >= 1) {
                Text("Locate Steam", style = MaterialTheme.typography.titleMedium)
                Spacer(Modifier.height(12.dp))

                candidates.forEach { candidate ->
                    SteamCandidateTile(
                        candidate = candidate,
                        selected = selected?.path == candidate.path,
                        onSelect = if (candidate.valid) {
                            { selected = candidate }
                        } else null,
                    )
                    Spacer(Modifier.height(8.dp))
                }

                Text("Or specify a custom path", style = MaterialTheme.typography.labelLarge)
                Spacer(Modifier.height(8.dp))
                Row(verticalAlignment = Alignment.CenterVertically) {
                    OutlinedTextField(
                        value = customPath,
                        onValueChange = { customPath = it },
                        modifier = Modifier.weight(1f),
                        placeholder = { Text("/path/to/Steam") },
                        singleLine = true,
                    )
                    Spacer(Modifier.width(8.dp))
                    Button(
                        enabled = !validating && customPath.isNotBlank(),
                        onClick = {
                            scope.launch {
                                validating = true
                                val candidate = steamService.validatePath(customPath.trim())
                                customValidated = candidate
                                if (candidate.valid) selected = candidate
                                validating = false
                            }
                        },
                    ) {
                        if (validating) {
                            CircularProgressIndicator(modifier = Modifier.height(16.dp), strokeWidth = 2.dp)
                        } else {
                            Text("Check")
                        }
                    }
                }

                customValidated?.let { candidate ->
                    Spacer(Modifier.height(8.dp))
                    SteamCandidateTile(
                        candidate = candidate,
                        selected = selected?.path == candidate.path,
                        onSelect = if (candidate.valid) {
                            { selected = candidate }
                        } else null,
                    )
                }

                Spacer(Modifier.height(24.dp))
            }

            if (step >= 2) {
                Text("Confirm", style = MaterialTheme.typography.titleMedium)
                Spacer(Modifier.height(12.dp))
                selected?.let { candidate ->
                    Text("Steam install: ${candidate.label}")
                    Text(candidate.path, style = MaterialTheme.typography.bodySmall)
                    Spacer(Modifier.height(8.dp))
                    Text(
                        "Compatibility tools will be installed to:\n" +
                            steamService.compatibilityToolsDir(candidate.path),
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                } ?: Text("Select a Steam installation to continue.")
                Spacer(Modifier.height(24.dp))
            }

            Row(horizontalArrangement = Arrangement.End, modifier = Modifier.fillMaxWidth()) {
                if (step > 0) {
                    OutlinedButton(onClick = { step -= 1 }) { Text("Back") }
                    Spacer(Modifier.width(12.dp))
                }
                Button(
                    enabled = step < 2 || selected != null,
                    onClick = {
                        if (step < 2) {
                            step += 1
                        } else {
                            val candidate = selected ?: return@Button
                            settingsService.completeFirstStart(
                                steamPath = candidate.path,
                                compatibilityToolsDir = steamService.compatibilityToolsDir(candidate.path),
                                installType = candidate.type,
                            )
                        }
                    },
                ) {
                    Text(if (step == 2) "Finish setup" else "Continue")
                }
            }
        }
    }
}

@Composable
private fun SteamCandidateTile(
    candidate: SteamCandidate,
    selected: Boolean,
    onSelect: (() -> Unit)?,
) {
    val scheme = MaterialTheme.colorScheme
    Card(
        onClick = { onSelect?.invoke() },
        enabled = onSelect != null,
        colors = CardDefaults.cardColors(
            containerColor = if (selected) scheme.primaryContainer else scheme.surfaceContainerHigh,
        ),
    ) {
        ListItem(
            headlineContent = { Text(candidate.label) },
            supportingContent = { Text(candidate.path, style = MaterialTheme.typography.bodySmall) },
            trailingContent = {
                if (candidate.valid) {
                    Icon(Icons.Filled.CheckCircle, contentDescription = "Valid", tint = scheme.primary)
                } else {
                    Icon(Icons.Filled.Error, contentDescription = "Invalid", tint = scheme.error)
                }
            },
            colors = androidx.compose.material3.ListItemDefaults.colors(containerColor = Color.Transparent),
        )
    }
}
