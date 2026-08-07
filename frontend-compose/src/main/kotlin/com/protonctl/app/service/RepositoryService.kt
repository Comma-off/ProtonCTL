package com.protonctl.app.service

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import com.protonctl.app.model.BuildJob
import com.protonctl.app.model.GitHubRelease
import com.protonctl.app.model.InstallJob
import com.protonctl.app.model.InstallStage
import com.protonctl.app.model.InstalledProtonVersion
import com.protonctl.app.model.ProtonRepository
import com.protonctl.app.model.isTerminal
import com.protonctl.app.native.ProtonCtlBridge
import com.protonctl.app.native.ProtonCtlNativeException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

/** A download's current smoothed throughput and estimated time remaining,
 * derived client-side from consecutive [InstallJob.bytesDownloaded]
 * samples - the native side only reports a byte count, not a rate. */
data class InstallSpeed(val bytesPerSecond: Double, val etaSeconds: Long?)

/**
 * Owns repository registry state, installed Proton versions, cached
 * release lists, and both async job queues (compilation, release install) -
 * polling native job status every 300ms while anything is in flight (and
 * stopping once everything's terminal) so download progress reads as live
 * rather than visibly stepping once every couple of seconds. Both
 * `BuildRunner` and `InstallRunner` run on their own background threads in
 * the C++ engine and only report progress when asked, so this has to poll
 * rather than being pushed to.
 */
class RepositoryService(compatToolsDir: String) {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Default)
    private var pollJob: Job? = null

    private data class ByteSample(val atMillis: Long, val bytesDownloaded: Long)
    private val lastByteSamples = mutableMapOf<String, ByteSample>()
    private val smoothedRates = mutableMapOf<String, Double>()

    var installSpeeds by mutableStateOf<Map<String, InstallSpeed>>(emptyMap())
        private set

    /** Keyed by installed version name (not repo, since two installed
     * builds could share a source repo at different tags). */
    var availableUpdates by mutableStateOf<Map<String, GitHubRelease>>(emptyMap())
        private set
    var checkingUpdates by mutableStateOf(false)
        private set

    var compatToolsDir: String = compatToolsDir
        private set

    var repositories by mutableStateOf<List<ProtonRepository>>(emptyList())
        private set
    var installed by mutableStateOf<List<InstalledProtonVersion>>(emptyList())
        private set
    var buildJobs by mutableStateOf<List<BuildJob>>(emptyList())
        private set
    var installJobs by mutableStateOf<List<InstallJob>>(emptyList())
        private set
    var releasesByRepo by mutableStateOf<Map<String, List<GitHubRelease>>>(emptyMap())
        private set
    var loadingRepositories by mutableStateOf(false)
        private set

    fun updateCompatToolsDir(dir: String) {
        compatToolsDir = dir
        refreshInstalled()
    }

    fun refreshAll() {
        loadingRepositories = true
        repositories = ProtonCtlBridge.listRepositories()
        loadingRepositories = false
        refreshInstalled()
        refreshJobs()
    }

    fun refreshInstalled() {
        if (compatToolsDir.isEmpty()) return
        installed = ProtonCtlBridge.listInstalled(compatToolsDir)
    }

    suspend fun addRepository(owner: String, repo: String): ProtonRepository {
        val added = ProtonCtlBridge.addRepository(owner, repo)
        repositories = repositories + added
        return added
    }

    fun removeRepository(repo: ProtonRepository) {
        ProtonCtlBridge.removeRepository(repo.owner, repo.repo)
        repositories = repositories.filterNot { it.fullName == repo.fullName }
        releasesByRepo = releasesByRepo - repo.fullName
    }

    suspend fun fetchReleases(repo: ProtonRepository): List<GitHubRelease> {
        val releases = ProtonCtlBridge.fetchReleases(repo.owner, repo.repo)
        releasesByRepo = releasesByRepo + (repo.fullName to releases)
        return releases
    }

    fun removeInstalledVersion(name: String) {
        ProtonCtlBridge.removeInstalled(compatToolsDir, name)
        refreshInstalled()
    }

    /** Checks every installed build with a known source repo against that
     * repo's latest release. One repo failing (rate limit, network, repo
     * deleted upstream) doesn't stop the rest from being checked. Installing
     * an available update goes through the normal [enqueueInstall] path,
     * which - since releases are named/versioned distinctly - naturally adds
     * a new build alongside the old one rather than overwriting it. */
    suspend fun checkForUpdates() {
        checkingUpdates = true
        try {
            val results = mutableMapOf<String, GitHubRelease>()
            for (version in installed) {
                val parts = version.sourceRepo.split("/", limit = 2)
                if (parts.size != 2 || parts[0].isEmpty() || parts[1].isEmpty()) continue
                try {
                    val update = ProtonCtlBridge.checkForUpdate(parts[0], parts[1], version.versionTag)
                    if (update != null) results[version.name] = update
                } catch (_: ProtonCtlNativeException) {
                    // Best-effort per repo - see kdoc above.
                }
            }
            availableUpdates = results
        } finally {
            checkingUpdates = false
        }
    }

    fun dismissAvailableUpdate(versionName: String) {
        availableUpdates = availableUpdates - versionName
    }

    fun enqueueBuild(repo: ProtonRepository, ref: String): String {
        val id = ProtonCtlBridge.enqueueBuild(repo, ref, compatToolsDir)
        ensurePolling()
        refreshJobs()
        return id
    }

    fun cancelBuild(jobId: String) {
        ProtonCtlBridge.cancelBuild(jobId)
        refreshJobs()
    }

    /** Queues a release download+extract on the native install worker
     * thread and returns immediately with a job id - the caller polls
     * [installJobs] for real progress instead of blocking. */
    fun enqueueInstall(repo: ProtonRepository, release: GitHubRelease): String {
        val id = ProtonCtlBridge.enqueueInstall(repo, release, compatToolsDir)
        ensurePolling()
        refreshJobs()
        return id
    }

    fun cancelInstall(jobId: String) {
        ProtonCtlBridge.cancelInstall(jobId)
        refreshJobs()
    }

    fun refreshJobs() {
        buildJobs = ProtonCtlBridge.listBuildJobs()
        installJobs = ProtonCtlBridge.listInstallJobs()
        updateInstallSpeeds()
        val allTerminal = (buildJobs.isNotEmpty() || installJobs.isNotEmpty()) &&
            buildJobs.all { it.stage.isTerminal } && installJobs.all { it.stage.isTerminal }
        if (allTerminal) refreshInstalled()
    }

    private fun updateInstallSpeeds() {
        val now = System.currentTimeMillis()
        val activeIds = installJobs.map { it.id }.toSet()
        lastByteSamples.keys.retainAll(activeIds)
        smoothedRates.keys.retainAll(activeIds)

        val next = mutableMapOf<String, InstallSpeed>()
        for (job in installJobs) {
            if (job.stage != InstallStage.DOWNLOADING) continue

            val previous = lastByteSamples[job.id]
            if (previous != null && now > previous.atMillis) {
                val elapsedSeconds = (now - previous.atMillis) / 1000.0
                val deltaBytes = job.bytesDownloaded - previous.bytesDownloaded
                if (deltaBytes >= 0) {
                    val instantRate = deltaBytes / elapsedSeconds
                    // Exponential moving average so the ETA doesn't visibly
                    // jump around between polls from ordinary jitter in
                    // chunked network reads.
                    val previousRate = smoothedRates[job.id]
                    smoothedRates[job.id] =
                        if (previousRate == null) instantRate else (0.3 * instantRate + 0.7 * previousRate)
                }
            }
            lastByteSamples[job.id] = ByteSample(now, job.bytesDownloaded)

            val rate = smoothedRates[job.id]
            if (rate != null && rate > 0) {
                val remaining = job.bytesTotal - job.bytesDownloaded
                val eta = if (job.bytesTotal > 0 && remaining > 0) (remaining / rate).toLong() else null
                next[job.id] = InstallSpeed(rate, eta)
            }
        }
        installSpeeds = next
    }

    private fun allJobsTerminal(): Boolean {
        if (buildJobs.isEmpty() && installJobs.isEmpty()) return true
        return buildJobs.all { it.stage.isTerminal } && installJobs.all { it.stage.isTerminal }
    }

    private fun ensurePolling() {
        if (pollJob?.isActive == true) return
        pollJob = scope.launch {
            do {
                delay(300)
                refreshJobs()
            } while (!allJobsTerminal())
        }
    }

    fun dispose() {
        scope.cancel()
    }
}
