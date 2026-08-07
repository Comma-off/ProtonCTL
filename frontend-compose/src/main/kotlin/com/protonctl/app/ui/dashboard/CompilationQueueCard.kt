package com.protonctl.app.ui.dashboard

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.CloudDownload
import androidx.compose.material.icons.filled.PrecisionManufacturing
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.StopCircle
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ColorScheme
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.SuggestionChip
import androidx.compose.material3.SuggestionChipDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import com.protonctl.app.model.BuildJob
import com.protonctl.app.model.BuildStage
import com.protonctl.app.model.InstallJob
import com.protonctl.app.model.InstallStage
import com.protonctl.app.model.isTerminal
import com.protonctl.app.service.InstallSpeed
import com.protonctl.app.service.RepositoryService
import kotlin.math.roundToInt

private fun buildStageColor(stage: BuildStage, scheme: ColorScheme): Color = when (stage) {
    BuildStage.DONE -> scheme.primary
    BuildStage.FAILED, BuildStage.CANCELLED -> scheme.error
    else -> scheme.tertiary
}

private fun buildStageLabel(stage: BuildStage): String = when (stage) {
    BuildStage.QUEUED -> "Queued"
    BuildStage.CLONING -> "Cloning"
    BuildStage.CONFIGURING -> "Configuring"
    BuildStage.BUILDING -> "Building"
    BuildStage.PACKAGING -> "Packaging"
    BuildStage.DONE -> "Done"
    BuildStage.FAILED -> "Failed"
    BuildStage.CANCELLED -> "Cancelled"
}

private fun installStageColor(stage: InstallStage, scheme: ColorScheme): Color = when (stage) {
    InstallStage.DONE -> scheme.primary
    InstallStage.FAILED, InstallStage.CANCELLED -> scheme.error
    else -> scheme.tertiary
}

private fun installStageLabel(stage: InstallStage): String = when (stage) {
    InstallStage.QUEUED -> "Queued"
    InstallStage.DOWNLOADING -> "Downloading"
    InstallStage.EXTRACTING -> "Extracting"
    InstallStage.DONE -> "Done"
    InstallStage.FAILED -> "Failed"
    InstallStage.CANCELLED -> "Cancelled"
}

private fun formatBytes(bytes: Long): String {
    if (bytes <= 0) return "0 MB"
    val mb = bytes / (1024.0 * 1024.0)
    return "${(mb * 10).roundToInt() / 10.0} MB"
}

private fun formatEta(etaSeconds: Long): String = when {
    etaSeconds < 60 -> "${etaSeconds}s left"
    etaSeconds < 3600 -> "${etaSeconds / 60}m ${etaSeconds % 60}s left"
    else -> "${etaSeconds / 3600}h ${(etaSeconds % 3600) / 60}m left"
}

@Composable
fun CompilationQueueCard(repositoryService: RepositoryService) {
    val installJobs = repositoryService.installJobs
    val buildJobs = repositoryService.buildJobs

    Card(
        modifier = Modifier.fillMaxWidth().height(420.dp),
        shape = MaterialTheme.shapes.extraLarge,
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainer),
    ) {
        Column(modifier = Modifier.fillMaxSize().padding(20.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Icon(
                    Icons.Filled.PrecisionManufacturing,
                    contentDescription = null,
                    tint = MaterialTheme.colorScheme.primary,
                )
                Text(
                    "Task Queue",
                    style = MaterialTheme.typography.titleMedium,
                    modifier = Modifier.padding(start = 12.dp).weight(1f),
                )
                IconButton(onClick = { repositoryService.refreshJobs() }) {
                    Icon(Icons.Filled.Refresh, contentDescription = "Refresh")
                }
            }

            if (installJobs.isEmpty() && buildJobs.isEmpty()) {
                Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                    Text(
                        "Nothing queued.\nInstall a release or build a repository from source to see " +
                            "progress here.",
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
            } else {
                LazyColumn(modifier = Modifier.fillMaxSize()) {
                    items(installJobs, key = { "install-${it.id}" }) { job ->
                        InstallJobRow(
                            job = job,
                            speed = repositoryService.installSpeeds[job.id],
                            onCancel = { repositoryService.cancelInstall(job.id) },
                        )
                        Spacer(Modifier.height(12.dp))
                    }
                    items(buildJobs, key = { "build-${it.id}" }) { job ->
                        BuildJobRow(job = job, onCancel = { repositoryService.cancelBuild(job.id) })
                        Spacer(Modifier.height(12.dp))
                    }
                }
            }
        }
    }
}

@Composable
private fun InstallJobRow(job: InstallJob, speed: InstallSpeed?, onCancel: () -> Unit) {
    val scheme = MaterialTheme.colorScheme
    val color = installStageColor(job.stage, scheme)

    Column(modifier = Modifier.fillMaxWidth()) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Icon(Icons.Filled.CloudDownload, contentDescription = null, modifier = Modifier.height(16.dp))
            Spacer(Modifier.width(8.dp))
            Column(modifier = Modifier.weight(1f)) {
                Text(
                    "${job.repo.fullName} ${job.release.tagName}",
                    maxLines = 1,
                    style = MaterialTheme.typography.bodyMedium,
                )
                if (job.stage == InstallStage.DOWNLOADING && job.bytesTotal > 0) {
                    val remaining = formatBytes(job.bytesTotal - job.bytesDownloaded)
                    val detail = buildString {
                        append("${formatBytes(job.bytesDownloaded)} / ${formatBytes(job.bytesTotal)}")
                        append(" · $remaining left")
                        if (speed != null) {
                            append(" · ${formatBytes(speed.bytesPerSecond.toLong())}/s")
                            if (speed.etaSeconds != null) append(" · ${formatEta(speed.etaSeconds)}")
                        }
                    }
                    Text(detail, style = MaterialTheme.typography.bodySmall, color = scheme.onSurfaceVariant)
                }
            }
            SuggestionChip(
                onClick = {},
                label = { Text(installStageLabel(job.stage)) },
                colors = SuggestionChipDefaults.suggestionChipColors(containerColor = color.copy(alpha = 0.15f)),
            )
            if (!job.stage.isTerminal) {
                IconButton(onClick = onCancel) {
                    Icon(Icons.Filled.StopCircle, contentDescription = "Cancel")
                }
            }
        }
        Spacer(Modifier.height(6.dp))
        LinearProgressIndicator(
            progress = { if (job.stage.isTerminal) 1f else job.progressPercent / 100f },
            modifier = Modifier.fillMaxWidth().height(6.dp),
            color = color,
            trackColor = scheme.surfaceContainerHighest,
        )
        if (job.stage == InstallStage.FAILED && job.error.isNotEmpty()) {
            Text(job.error, color = scheme.error, style = MaterialTheme.typography.bodySmall, maxLines = 2)
        }
    }
}

@Composable
private fun BuildJobRow(job: BuildJob, onCancel: () -> Unit) {
    val scheme = MaterialTheme.colorScheme
    val color = buildStageColor(job.stage, scheme)

    Column(modifier = Modifier.fillMaxWidth()) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(
                "${job.repo.fullName}${if (job.ref.isEmpty()) "" else "@${job.ref}"}",
                maxLines = 1,
                modifier = Modifier.weight(1f),
                style = MaterialTheme.typography.bodyMedium,
            )
            SuggestionChip(
                onClick = {},
                label = { Text(buildStageLabel(job.stage)) },
                colors = SuggestionChipDefaults.suggestionChipColors(containerColor = color.copy(alpha = 0.15f)),
            )
            if (!job.stage.isTerminal) {
                IconButton(onClick = onCancel) {
                    Icon(Icons.Filled.StopCircle, contentDescription = "Cancel")
                }
            }
        }
        Spacer(Modifier.height(6.dp))
        LinearProgressIndicator(
            progress = { if (job.stage.isTerminal) 1f else job.progressPercent / 100f },
            modifier = Modifier.fillMaxWidth().height(6.dp),
            color = color,
            trackColor = scheme.surfaceContainerHighest,
        )
        if (job.stage == BuildStage.FAILED && job.error.isNotEmpty()) {
            Text(
                job.error,
                color = scheme.error,
                style = MaterialTheme.typography.bodySmall,
                maxLines = 2,
            )
        }
    }
}
