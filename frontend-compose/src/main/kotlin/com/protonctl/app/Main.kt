package com.protonctl.app

import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Window
import androidx.compose.ui.window.WindowState
import androidx.compose.ui.window.application
import com.protonctl.app.ui.App

fun main() = application {
    Window(
        onCloseRequest = ::exitApplication,
        title = "ProtonCTL",
        state = WindowState(width = 1120.dp, height = 760.dp),
    ) {
        App()
    }
}
