package com.protonctl.app.ui.settings

import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.core.Spring
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.spring
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.BrightnessAuto
import androidx.compose.material.icons.filled.Check
import androidx.compose.material.icons.filled.DarkMode
import androidx.compose.material.icons.filled.LightMode
import androidx.compose.material3.ButtonGroupDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.ListItem
import androidx.compose.material3.ListItemDefaults
import androidx.compose.material3.MaterialShapes
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Slider
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.ToggleButton
import androidx.compose.material3.ToggleButtonDefaults
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Matrix
import androidx.compose.ui.graphics.Outline
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.Shape
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.graphics.luminance
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.role
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.Density
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.dp
import androidx.graphics.shapes.Morph
import com.materialkolor.PaletteStyle
import com.materialkolor.dynamiccolor.ColorSpec
import com.protonctl.app.service.SettingsService
import com.protonctl.app.theme.AppThemeMode
import com.protonctl.app.theme.SeedColorOptions
import com.protonctl.app.theme.ThemeController
import com.protonctl.app.theme.toAppConfigPatch

private fun paletteStyleDescription(style: PaletteStyle): String = when (style) {
    PaletteStyle.TonalSpot -> "Balanced, low-chroma palettes - the M3 default."
    PaletteStyle.Fidelity -> "Preserves the seed color as closely as tone allows."
    PaletteStyle.Content -> "Derives secondary/tertiary hues from the seed like an artwork palette."
    PaletteStyle.Expressive -> "Wider hue shifts and higher chroma for a bolder look."
    else -> "Playful, source-color-detached palette."
}

private val PickerVariants =
    listOf(PaletteStyle.TonalSpot, PaletteStyle.Fidelity, PaletteStyle.Content, PaletteStyle.Expressive)

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ThemeSettingsScreen(
    themeController: ThemeController,
    settingsService: SettingsService,
    onBack: () -> Unit,
) {
    fun persist() {
        settingsService.update { themeController.toAppConfigPatch(it) }
    }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Theme") },
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Back")
                    }
                },
            )
        },
    ) { padding ->
        LazyColumn(
            modifier = Modifier.fillMaxSize().padding(padding),
            contentPadding = PaddingValues(20.dp),
        ) {
            item {
                Text("Seed color", style = MaterialTheme.typography.titleMedium)
                Spacer(Modifier.height(12.dp))
                Row(horizontalArrangement = Arrangement.spacedBy(16.dp)) {
                    SeedColorOptions.forEachIndexed { index, option ->
                        SeedSwatch(
                            name = option.displayName,
                            color = option.color,
                            selected = themeController.seedColorIndex == index,
                            onClick = {
                                themeController.seedColorIndex = index
                                persist()
                            },
                        )
                    }
                }
                Spacer(Modifier.height(32.dp))

                Text("Spec version", style = MaterialTheme.typography.titleMedium)
                Spacer(Modifier.height(12.dp))
                ConnectedChoiceGroup(
                    options = listOf(ColorSpec.SpecVersion.SPEC_2021, ColorSpec.SpecVersion.SPEC_2025),
                    selected = themeController.specVersion,
                    onSelect = {
                        themeController.specVersion = it
                        persist()
                    },
                    label = {
                        if (it == ColorSpec.SpecVersion.SPEC_2021) "SPEC 2021 (Baseline)" else "SPEC 2025 (Expressive)"
                    },
                )
                Spacer(Modifier.height(32.dp))

                Text("Color algorithm variant", style = MaterialTheme.typography.titleMedium)
            }

            items(PickerVariants) { style ->
                val selected = themeController.paletteStyle == style
                // M3 Expressive's list update calls for a segmented visual
                // style with an improved selection treatment - each option
                // is its own rounded, separated container instead of a flat
                // continuous row, with the selected one getting a
                // highlighted container color rather than relying on the
                // radio dot alone.
                val containerColor by animateColorAsState(
                    targetValue = if (selected) {
                        MaterialTheme.colorScheme.secondaryContainer
                    } else {
                        MaterialTheme.colorScheme.surfaceContainerHigh
                    },
                    label = "paletteStyleContainer",
                )
                Surface(
                    onClick = {
                        themeController.paletteStyle = style
                        persist()
                    },
                    shape = MaterialTheme.shapes.large,
                    color = containerColor,
                    modifier = Modifier.fillMaxWidth().padding(vertical = 4.dp),
                ) {
                    ListItem(
                        headlineContent = { Text(style.name) },
                        supportingContent = { Text(paletteStyleDescription(style)) },
                        leadingContent = {
                            RadioButton(
                                selected = selected,
                                onClick = {
                                    themeController.paletteStyle = style
                                    persist()
                                },
                            )
                        },
                        colors = ListItemDefaults.colors(containerColor = Color.Transparent),
                    )
                }
            }

            item {
                Spacer(Modifier.height(16.dp))
                Text("Contrast level", style = MaterialTheme.typography.titleMedium)
                Slider(
                    value = themeController.contrastLevel.toFloat(),
                    onValueChange = { themeController.contrastLevel = it.toDouble() },
                    onValueChangeFinished = { persist() },
                    valueRange = -1f..1f,
                    steps = 19,
                )
                Spacer(Modifier.height(24.dp))

                Text("Appearance", style = MaterialTheme.typography.titleMedium)
                Spacer(Modifier.height(12.dp))
                ConnectedChoiceGroup(
                    options = listOf(AppThemeMode.SYSTEM, AppThemeMode.LIGHT, AppThemeMode.DARK),
                    selected = themeController.themeMode,
                    onSelect = { themeController.themeMode = it },
                    label = { it.name.lowercase().replaceFirstChar { c -> c.uppercase() } },
                    icon = {
                        when (it) {
                            AppThemeMode.SYSTEM -> Icons.Filled.BrightnessAuto
                            AppThemeMode.LIGHT -> Icons.Filled.LightMode
                            AppThemeMode.DARK -> Icons.Filled.DarkMode
                        }
                    },
                )
            }
        }
    }
}

/**
 * A connected button group for exclusive choice among 2-4 options - the M3
 * Expressive replacement for `SegmentedButton`/`SingleChoiceSegmentedButtonRow`,
 * which are being deprecated in favor of this. Buttons visually merge into
 * one shape (square-ish leading/trailing ends, rounded middle) rather than
 * sitting as separate, evenly-rounded chips side by side.
 */
@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun <T> ConnectedChoiceGroup(
    options: List<T>,
    selected: T,
    onSelect: (T) -> Unit,
    label: (T) -> String,
    icon: ((T) -> ImageVector)? = null,
) {
    FlowRow(horizontalArrangement = Arrangement.spacedBy(ButtonGroupDefaults.ConnectedSpaceBetween)) {
        options.forEachIndexed { index, option ->
            ToggleButton(
                checked = option == selected,
                onCheckedChange = { onSelect(option) },
                shapes = when (index) {
                    0 -> ButtonGroupDefaults.connectedLeadingButtonShapes()
                    options.lastIndex -> ButtonGroupDefaults.connectedTrailingButtonShapes()
                    else -> ButtonGroupDefaults.connectedMiddleButtonShapes()
                },
                modifier = Modifier.semantics { role = Role.RadioButton },
            ) {
                icon?.let {
                    Icon(it(option), contentDescription = null)
                    Spacer(Modifier.size(ToggleButtonDefaults.IconSpacing))
                }
                Text(label(option))
            }
        }
    }
}

// MaterialShapes' predefined RoundedPolygons (unlike a raw RoundedPolygon()
// constructor call) are normalized to span [0, 1] on each axis, centered on
// (0.5, 0.5) - verified directly by measuring MaterialShapes.Circle and
// Cookie4Sided's actual cubic coordinates, since assuming the generic
// [-1, 1]-centered-on-origin convention (as Android's official Morph sample
// uses for its own hand-built shapes) rendered these particular shapes at
// roughly a quarter of the intended size. A plain scale maps that [0, 1]
// space directly onto the pixel size with no translation needed.
private class MorphShape(private val morph: Morph, private val progress: Float) : Shape {
    override fun createOutline(size: Size, layoutDirection: LayoutDirection, density: Density): Outline {
        val path = Path()
        var first = true
        morph.forEachCubic(progress.coerceIn(0f, 1f)) { cubic ->
            if (first) {
                path.moveTo(cubic.anchor0X, cubic.anchor0Y)
                first = false
            }
            path.cubicTo(
                cubic.control0X, cubic.control0Y,
                cubic.control1X, cubic.control1Y,
                cubic.anchor1X, cubic.anchor1Y,
            )
        }
        path.close()

        val matrix = Matrix()
        matrix.scale(size.width, size.height)
        path.transform(matrix)
        return Outline.Generic(path)
    }
}

/** A circular seed-color swatch that fluidly morphs into a squircle (M3's
 * Cookie4Sided shape) with a fading-in checkmark when selected - the same
 * pattern Android's own wallpaper/theme color picker uses, rather than a
 * labeled rounded-square chip or an instant shape swap. No visible label -
 * `name` is exposed to screen readers only. */
@Composable
private fun SeedSwatch(name: String, color: Color, selected: Boolean, onClick: () -> Unit) {
    val contentColor = if (color.luminance() > 0.4f) Color.Black.copy(alpha = 0.87f) else Color.White
    val morph = remember { Morph(MaterialShapes.Circle, MaterialShapes.Cookie4Sided) }
    val progress by animateFloatAsState(
        targetValue = if (selected) 1f else 0f,
        animationSpec = spring(dampingRatio = Spring.DampingRatioMediumBouncy, stiffness = Spring.StiffnessMedium),
        label = "seedSwatchShape",
    )
    val shape = MorphShape(morph, progress)

    Box(
        modifier = Modifier
            .size(56.dp)
            .clip(shape)
            .background(color)
            .then(
                if (selected) {
                    Modifier.border(3.dp, MaterialTheme.colorScheme.onSurface, shape)
                } else {
                    Modifier
                }
            )
            .semantics { contentDescription = name }
            .clickable(onClick = onClick),
        contentAlignment = Alignment.Center,
    ) {
        if (progress > 0f) {
            Icon(
                Icons.Filled.Check,
                contentDescription = "Selected",
                tint = contentColor,
                modifier = Modifier.graphicsLayer { alpha = progress.coerceIn(0f, 1f) },
            )
        }
    }
}
