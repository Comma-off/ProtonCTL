package com.protonctl.app.service

import com.protonctl.app.model.CompatToolsBackupManifest
import com.protonctl.app.model.PrtBakManifest
import com.protonctl.app.model.RepairIssue
import com.protonctl.app.native.ProtonCtlBridge

class BackupService {
    suspend fun createBackup(
        protonDir: String,
        prefixDir: String?,
        manifest: PrtBakManifest,
        outFile: String,
        compressionLevel: Int = 6,
    ) = ProtonCtlBridge.createBackup(protonDir, prefixDir, manifest, outFile, compressionLevel)

    fun readManifest(prtbakFile: String): PrtBakManifest = ProtonCtlBridge.readManifest(prtbakFile)

    suspend fun restoreBackup(prtbakFile: String, restoreRoot: String) =
        ProtonCtlBridge.restoreBackup(prtbakFile, restoreRoot)

    /** Backs up every installed Proton version under [compatToolsDir] into
     * one archive, so the whole collection can be imported into another
     * instance later via [restoreLibraryBackup]. */
    suspend fun createLibraryBackup(compatToolsDir: String, outFile: String, compressionLevel: Int = 6) =
        ProtonCtlBridge.createLibraryBackup(compatToolsDir, outFile, compressionLevel)

    fun readLibraryManifest(prtbakFile: String): CompatToolsBackupManifest =
        ProtonCtlBridge.readLibraryManifest(prtbakFile)

    suspend fun restoreLibraryBackup(prtbakFile: String, compatToolsDir: String) =
        ProtonCtlBridge.restoreLibraryBackup(prtbakFile, compatToolsDir)

    suspend fun scanRepair(compatToolsDir: String, compatDataDir: String): List<RepairIssue> =
        ProtonCtlBridge.scanRepair(compatToolsDir, compatDataDir)

    fun fixIssue(issue: RepairIssue): Boolean = ProtonCtlBridge.fixIssue(issue)
}
