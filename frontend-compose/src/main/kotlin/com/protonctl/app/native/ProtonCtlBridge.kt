package com.protonctl.app.native

import com.protonctl.app.model.AppConfig
import com.protonctl.app.model.BuildJob
import com.protonctl.app.model.CompatToolsBackupManifest
import com.protonctl.app.model.GitHubRelease
import com.protonctl.app.model.InstallJob
import com.protonctl.app.model.InstalledProtonVersion
import com.protonctl.app.model.PrtBakManifest
import com.protonctl.app.model.ProtonRepository
import com.protonctl.app.model.RepairIssue
import com.protonctl.app.model.SteamCandidate
import com.sun.jna.Pointer
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import kotlinx.serialization.ExperimentalSerializationApi
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonNamingStrategy
import kotlinx.serialization.encodeToString

class ProtonCtlNativeException(message: String) : Exception(message)

@OptIn(ExperimentalSerializationApi::class)
private val json = Json {
    ignoreUnknownKeys = true
    namingStrategy = JsonNamingStrategy.SnakeCase
}

private fun Pointer?.takeStringAndFree(): String? {
    if (this == null) return null
    val value = getString(0, "UTF-8")
    ProtonCtlNative.INSTANCE.protonctl_free_string(this)
    return value
}

private fun Pointer?.requireStringAndFree(): String =
    takeStringAndFree() ?: throw ProtonCtlNativeException(lastError())

private fun lastError(): String =
    ProtonCtlNative.INSTANCE.protonctl_last_error()?.getString(0, "UTF-8")?.ifEmpty { "unknown native error" }
        ?: "unknown native error"

private fun requireOk(rc: Int) {
    if (rc != 0) throw ProtonCtlNativeException(lastError())
}

/**
 * High-level, JSON-decoded facade over `protonctl_core`. Fast/local calls run
 * on the calling thread; network- or IO-heavy calls are `suspend` and
 * dispatched on [Dispatchers.IO] so the Compose UI thread never blocks on
 * a GitHub request, archive extraction, or directory walk.
 */
object ProtonCtlBridge {
    private val native get() = ProtonCtlNative.INSTANCE

    // --- steam / config (fast, local) --------------------------------------

    fun detectSteamCandidates(): List<SteamCandidate> =
        json.decodeFromString(native.protonctl_detect_steam_candidates().requireStringAndFree())

    fun validateSteamPath(path: String): SteamCandidate =
        json.decodeFromString(native.protonctl_validate_steam_path(path).requireStringAndFree())

    fun loadConfig(): AppConfig =
        json.decodeFromString(native.protonctl_load_config().requireStringAndFree())

    fun saveConfig(config: AppConfig) {
        requireOk(native.protonctl_save_config(json.encodeToString(config)))
    }

    // --- repositories ---------------------------------------------------------

    fun listRepositories(): List<ProtonRepository> =
        json.decodeFromString(native.protonctl_list_repositories().requireStringAndFree())

    suspend fun addRepository(owner: String, repo: String): ProtonRepository =
        withContext(Dispatchers.IO) {
            json.decodeFromString(native.protonctl_add_repository(owner, repo).requireStringAndFree())
        }

    fun removeRepository(owner: String, repo: String) {
        requireOk(native.protonctl_remove_repository(owner, repo))
    }

    suspend fun fetchReleases(owner: String, repo: String): List<GitHubRelease> =
        withContext(Dispatchers.IO) {
            json.decodeFromString(native.protonctl_fetch_releases(owner, repo).requireStringAndFree())
        }

    /** Null return means already up to date (or no releases/unknown repo) -
     * a real native-side error still throws [ProtonCtlNativeException], same
     * as everything else here. */
    suspend fun checkForUpdate(owner: String, repo: String, installedVersionTag: String): GitHubRelease? =
        withContext(Dispatchers.IO) {
            val body = native.protonctl_check_for_update(owner, repo, installedVersionTag).requireStringAndFree()
            json.decodeFromString<GitHubRelease?>(body)
        }

    fun listInstalled(compatToolsDir: String): List<InstalledProtonVersion> =
        json.decodeFromString(native.protonctl_list_installed(compatToolsDir).requireStringAndFree())

    fun removeInstalled(compatToolsDir: String, name: String) {
        requireOk(native.protonctl_remove_installed(compatToolsDir, name))
    }

    // --- compilation queue -----------------------------------------------------

    fun enqueueBuild(repo: ProtonRepository, ref: String, compatToolsDir: String): String =
        native.protonctl_enqueue_build(json.encodeToString(repo), ref, compatToolsDir).requireStringAndFree()

    fun getBuildStatus(jobId: String): BuildJob? =
        native.protonctl_get_build_status(jobId).takeStringAndFree()?.let { json.decodeFromString(it) }

    fun listBuildJobs(): List<BuildJob> =
        json.decodeFromString(native.protonctl_list_build_jobs().requireStringAndFree())

    fun cancelBuild(jobId: String) {
        native.protonctl_cancel_build(jobId)
    }

    // --- release install (async, real progress) ---------------------------------

    fun enqueueInstall(repo: ProtonRepository, release: GitHubRelease, compatToolsDir: String): String =
        native.protonctl_enqueue_install(
            json.encodeToString(repo),
            json.encodeToString(release),
            compatToolsDir,
        ).requireStringAndFree()

    fun getInstallStatus(jobId: String): InstallJob? =
        native.protonctl_get_install_status(jobId).takeStringAndFree()?.let { json.decodeFromString(it) }

    fun listInstallJobs(): List<InstallJob> =
        json.decodeFromString(native.protonctl_list_install_jobs().requireStringAndFree())

    fun cancelInstall(jobId: String) {
        native.protonctl_cancel_install(jobId)
    }

    // --- backup / repair ----------------------------------------------------------

    suspend fun createBackup(
        protonDir: String,
        prefixDir: String?,
        manifest: PrtBakManifest,
        outFile: String,
        compressionLevel: Int = 6,
    ) = withContext(Dispatchers.IO) {
        requireOk(
            native.protonctl_create_backup(protonDir, prefixDir, json.encodeToString(manifest), outFile, compressionLevel)
        )
    }

    fun readManifest(prtbakFile: String): PrtBakManifest =
        json.decodeFromString(native.protonctl_read_manifest(prtbakFile).requireStringAndFree())

    suspend fun restoreBackup(prtbakFile: String, restoreRoot: String) = withContext(Dispatchers.IO) {
        requireOk(native.protonctl_restore_backup(prtbakFile, restoreRoot))
    }

    // --- whole-library backup (every installed Proton version at once) ----------

    suspend fun createLibraryBackup(compatToolsDir: String, outFile: String, compressionLevel: Int = 6) =
        withContext(Dispatchers.IO) {
            requireOk(native.protonctl_create_library_backup(compatToolsDir, outFile, compressionLevel))
        }

    fun readLibraryManifest(prtbakFile: String): CompatToolsBackupManifest =
        json.decodeFromString(native.protonctl_read_library_manifest(prtbakFile).requireStringAndFree())

    suspend fun restoreLibraryBackup(prtbakFile: String, compatToolsDir: String) =
        withContext(Dispatchers.IO) {
            requireOk(native.protonctl_restore_library_backup(prtbakFile, compatToolsDir))
        }

    suspend fun scanRepair(compatToolsDir: String, compatDataDir: String): List<RepairIssue> =
        withContext(Dispatchers.IO) {
            json.decodeFromString(native.protonctl_scan_repair(compatToolsDir, compatDataDir).requireStringAndFree())
        }

    fun fixIssue(issue: RepairIssue): Boolean =
        native.protonctl_fix_issue(json.encodeToString(issue)) == 0
}
