package com.protonctl.app.theme

import androidx.compose.ui.graphics.Color

data class SeedColorOption(val displayName: String, val color: Color)

val SeedColorOptions = listOf(
    SeedColorOption("Proton Violet", Color(0xFF7C4DFF)),
    SeedColorOption("Steam Blue", Color(0xFF2A6DF4)),
    SeedColorOption("GE Ember", Color(0xFFFF6D3F)),
    SeedColorOption("Wine Crimson", Color(0xFFB3261E)),
    SeedColorOption("Terminal Green", Color(0xFF00C853)),
    SeedColorOption("Slate Teal", Color(0xFF00696D)),
)
