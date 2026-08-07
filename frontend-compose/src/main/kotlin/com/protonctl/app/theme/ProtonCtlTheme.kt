package com.protonctl.app.theme

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import com.materialkolor.DynamicMaterialExpressiveTheme
import com.materialkolor.PaletteStyle
import com.materialkolor.dynamiccolor.ColorSpec
import com.protonctl.app.model.AppConfig

enum class AppThemeMode { SYSTEM, LIGHT, DARK }

/**
 * Holds the Material 3 Expressive theme configuration: seed color, spec
 * version, palette style, contrast level, and light/dark mode. Seed
 * color/spec version/palette style round-trip through the native
 * [AppConfig] (`~/.config/protonctl/config.json`); contrast level and theme
 * mode are session-only for now.
 *
 * Unlike the earlier Flutter build of this app, [ColorSpec.SpecVersion]
 * and [PaletteStyle] here are `material-kolor`'s real types - both axes
 * are independently meaningful under either spec version, so nothing
 * needs to be disabled/greyed-out based on which one is picked.
 */
class ThemeController(
    seedColorIndex: Int = 0,
    specVersion: ColorSpec.SpecVersion = ColorSpec.SpecVersion.SPEC_2025,
    paletteStyle: PaletteStyle = PaletteStyle.Expressive,
    themeMode: AppThemeMode = AppThemeMode.SYSTEM,
    contrastLevel: Double = 0.0,
) {
    var seedColorIndex by mutableStateOf(seedColorIndex.coerceIn(0, SeedColorOptions.lastIndex))
    var specVersion by mutableStateOf(specVersion)
    var paletteStyle by mutableStateOf(paletteStyle)
    var themeMode by mutableStateOf(themeMode)
    var contrastLevel by mutableStateOf(contrastLevel)

    val seedColor get() = SeedColorOptions[seedColorIndex].color

    companion object {
        /** Reads persisted theme fields off [AppConfig], tolerating values
         * written by the earlier Flutter build (e.g. `"spec2025"`,
         * `"expressive"`) as well as this build's own enum names. */
        fun fromAppConfig(config: AppConfig): ThemeController = ThemeController(
            seedColorIndex = config.seedColorIndex,
            specVersion = parseSpecVersion(config.themeSpecVersion),
            paletteStyle = parsePaletteStyle(config.themeVariant),
        )

        private fun parseSpecVersion(raw: String): ColorSpec.SpecVersion = when {
            raw.equals("SPEC_2021", ignoreCase = true) || raw.equals("spec2021", ignoreCase = true) ->
                ColorSpec.SpecVersion.SPEC_2021
            else -> ColorSpec.SpecVersion.SPEC_2025
        }

        private fun parsePaletteStyle(raw: String): PaletteStyle =
            PaletteStyle.entries.firstOrNull { it.name.equals(raw, ignoreCase = true) }
                ?: PaletteStyle.Expressive
    }
}

fun ThemeController.toAppConfigPatch(base: AppConfig): AppConfig = base.copy(
    seedColorIndex = seedColorIndex,
    themeSpecVersion = specVersion.name,
    themeVariant = paletteStyle.name,
)

@Composable
fun ProtonCtlTheme(controller: ThemeController, content: @Composable () -> Unit) {
    val isDark = when (controller.themeMode) {
        AppThemeMode.SYSTEM -> isSystemInDarkTheme()
        AppThemeMode.LIGHT -> false
        AppThemeMode.DARK -> true
    }

    DynamicMaterialExpressiveTheme(
        seedColor = controller.seedColor,
        isDark = isDark,
        style = controller.paletteStyle,
        specVersion = controller.specVersion,
        contrastLevel = controller.contrastLevel,
        animate = true,
        content = content,
    )
}
