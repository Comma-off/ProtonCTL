package com.protonctl.app.service

import com.protonctl.app.model.SteamCandidate
import com.protonctl.app.native.ProtonCtlBridge

class SteamService {
    fun detectCandidates(): List<SteamCandidate> = ProtonCtlBridge.detectSteamCandidates()

    fun validatePath(path: String): SteamCandidate = ProtonCtlBridge.validateSteamPath(path)

    fun compatibilityToolsDir(steamPath: String): String = "$steamPath/compatibilitytools.d"

    fun compatDataDir(steamPath: String): String = "$steamPath/steamapps/compatdata"
}
