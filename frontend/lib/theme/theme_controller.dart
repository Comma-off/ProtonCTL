import 'package:flutter/material.dart';
import 'package:material_color_utilities/material_color_utilities.dart' as mcu;
import 'package:shared_preferences/shared_preferences.dart';

import 'm3_spec.dart';
import 'scheme_variant.dart';
import 'seed_colors.dart';

const String _prefSeedIndex = 'protonctl.theme.seed_index';
const String _prefSpecVersion = 'protonctl.theme.spec_version';
const String _prefVariant = 'protonctl.theme.variant';
const String _prefThemeMode = 'protonctl.theme.mode';
const String _prefContrast = 'protonctl.theme.contrast_level';

/// Owns the Material 3 Expressive theme configuration: seed color, spec
/// version (SPEC_2021/SPEC_2025), scheme variant (Tonal Spot / Fidelity /
/// Content / Expressive), contrast level, and light/dark mode.
///
/// Under [M3SpecVersion.spec2025] the color scheme is built by driving
/// `material_color_utilities`'s [mcu.DynamicScheme] subclasses directly -
/// the same mechanism Flutter's own `ColorScheme.fromSeed` uses internally,
/// just with the variant and contrast level exposed. Under
/// [M3SpecVersion.spec2021] it falls back to plain `ColorScheme.fromSeed`,
/// which only ever produces the original Tonal Spot palette.
class ThemeController extends ChangeNotifier {
  ThemeController({SharedPreferences? preferences}) : _prefs = preferences {
    _rebuild();
  }

  final SharedPreferences? _prefs;

  int _seedColorIndex = 0;
  M3SpecVersion _specVersion = M3SpecVersion.spec2025;
  M3SchemeVariant _variant = M3SchemeVariant.expressive;
  ThemeMode _themeMode = ThemeMode.system;
  double _contrastLevel = 0.0;

  late ThemeData _lightTheme;
  late ThemeData _darkTheme;

  int get seedColorIndex => _seedColorIndex;
  SeedColorOption get seedColor => kSeedColorOptions[_seedColorIndex];
  M3SpecVersion get specVersion => _specVersion;
  M3SchemeVariant get variant => _variant;
  ThemeMode get themeMode => _themeMode;
  double get contrastLevel => _contrastLevel;

  ThemeData get lightTheme => _lightTheme;
  ThemeData get darkTheme => _darkTheme;

  static Future<ThemeController> load() async {
    final prefs = await SharedPreferences.getInstance();
    final controller = ThemeController(preferences: prefs);
    controller._seedColorIndex =
        (prefs.getInt(_prefSeedIndex) ?? 0).clamp(0, kSeedColorOptions.length - 1);
    controller._specVersion = M3SpecVersion.fromName(prefs.getString(_prefSpecVersion) ?? '');
    controller._variant = M3SchemeVariant.fromName(prefs.getString(_prefVariant) ?? '');
    controller._contrastLevel = prefs.getDouble(_prefContrast) ?? 0.0;
    final modeName = prefs.getString(_prefThemeMode);
    controller._themeMode = ThemeMode.values.firstWhere(
      (m) => m.name == modeName,
      orElse: () => ThemeMode.system,
    );
    controller._rebuild();
    return controller;
  }

  void setSeedColorIndex(int index) {
    _seedColorIndex = index.clamp(0, kSeedColorOptions.length - 1);
    _prefs?.setInt(_prefSeedIndex, _seedColorIndex);
    _rebuild();
  }

  void setSpecVersion(M3SpecVersion version) {
    _specVersion = version;
    _prefs?.setString(_prefSpecVersion, version.name);
    _rebuild();
  }

  void setVariant(M3SchemeVariant variant) {
    _variant = variant;
    _prefs?.setString(_prefVariant, variant.name);
    _rebuild();
  }

  void setThemeMode(ThemeMode mode) {
    _themeMode = mode;
    _prefs?.setString(_prefThemeMode, mode.name);
    notifyListeners();
  }

  void setContrastLevel(double level) {
    _contrastLevel = level.clamp(-1.0, 1.0);
    _prefs?.setDouble(_prefContrast, _contrastLevel);
    _rebuild();
  }

  void _rebuild() {
    _lightTheme = ThemeData(useMaterial3: true, colorScheme: _buildColorScheme(Brightness.light));
    _darkTheme = ThemeData(useMaterial3: true, colorScheme: _buildColorScheme(Brightness.dark));
    notifyListeners();
  }

  ColorScheme _buildColorScheme(Brightness brightness) {
    if (_specVersion == M3SpecVersion.spec2021) {
      return ColorScheme.fromSeed(seedColor: seedColor.color, brightness: brightness);
    }

    final hct = mcu.Hct.fromInt(seedColor.color.value);
    final scheme = _buildDynamicScheme(hct, isDark: brightness == Brightness.dark);
    return _toFlutterColorScheme(scheme, brightness);
  }

  mcu.DynamicScheme _buildDynamicScheme(mcu.Hct sourceColorHct, {required bool isDark}) {
    switch (_variant) {
      case M3SchemeVariant.tonalSpot:
        return mcu.SchemeTonalSpot(
          sourceColorHct: sourceColorHct,
          isDark: isDark,
          contrastLevel: _contrastLevel,
        );
      case M3SchemeVariant.fidelity:
        return mcu.SchemeFidelity(
          sourceColorHct: sourceColorHct,
          isDark: isDark,
          contrastLevel: _contrastLevel,
        );
      case M3SchemeVariant.content:
        return mcu.SchemeContent(
          sourceColorHct: sourceColorHct,
          isDark: isDark,
          contrastLevel: _contrastLevel,
        );
      case M3SchemeVariant.expressive:
        return mcu.SchemeExpressive(
          sourceColorHct: sourceColorHct,
          isDark: isDark,
          contrastLevel: _contrastLevel,
        );
    }
  }

  ColorScheme _toFlutterColorScheme(mcu.DynamicScheme scheme, Brightness brightness) {
    Color argb(mcu.DynamicColor color) => Color(color.getArgb(scheme));

    return ColorScheme(
      brightness: brightness,
      primary: argb(mcu.MaterialDynamicColors.primary),
      onPrimary: argb(mcu.MaterialDynamicColors.onPrimary),
      primaryContainer: argb(mcu.MaterialDynamicColors.primaryContainer),
      onPrimaryContainer: argb(mcu.MaterialDynamicColors.onPrimaryContainer),
      secondary: argb(mcu.MaterialDynamicColors.secondary),
      onSecondary: argb(mcu.MaterialDynamicColors.onSecondary),
      secondaryContainer: argb(mcu.MaterialDynamicColors.secondaryContainer),
      onSecondaryContainer: argb(mcu.MaterialDynamicColors.onSecondaryContainer),
      tertiary: argb(mcu.MaterialDynamicColors.tertiary),
      onTertiary: argb(mcu.MaterialDynamicColors.onTertiary),
      tertiaryContainer: argb(mcu.MaterialDynamicColors.tertiaryContainer),
      onTertiaryContainer: argb(mcu.MaterialDynamicColors.onTertiaryContainer),
      error: argb(mcu.MaterialDynamicColors.error),
      onError: argb(mcu.MaterialDynamicColors.onError),
      errorContainer: argb(mcu.MaterialDynamicColors.errorContainer),
      onErrorContainer: argb(mcu.MaterialDynamicColors.onErrorContainer),
      surface: argb(mcu.MaterialDynamicColors.surface),
      onSurface: argb(mcu.MaterialDynamicColors.onSurface),
      surfaceDim: argb(mcu.MaterialDynamicColors.surfaceDim),
      surfaceBright: argb(mcu.MaterialDynamicColors.surfaceBright),
      surfaceContainerLowest: argb(mcu.MaterialDynamicColors.surfaceContainerLowest),
      surfaceContainerLow: argb(mcu.MaterialDynamicColors.surfaceContainerLow),
      surfaceContainer: argb(mcu.MaterialDynamicColors.surfaceContainer),
      surfaceContainerHigh: argb(mcu.MaterialDynamicColors.surfaceContainerHigh),
      surfaceContainerHighest: argb(mcu.MaterialDynamicColors.surfaceContainerHighest),
      onSurfaceVariant: argb(mcu.MaterialDynamicColors.onSurfaceVariant),
      outline: argb(mcu.MaterialDynamicColors.outline),
      outlineVariant: argb(mcu.MaterialDynamicColors.outlineVariant),
      shadow: argb(mcu.MaterialDynamicColors.shadow),
      scrim: argb(mcu.MaterialDynamicColors.scrim),
      inverseSurface: argb(mcu.MaterialDynamicColors.inverseSurface),
      onInverseSurface: argb(mcu.MaterialDynamicColors.inverseOnSurface),
      inversePrimary: argb(mcu.MaterialDynamicColors.inversePrimary),
      surfaceTint: argb(mcu.MaterialDynamicColors.surfaceTint),
    );
  }
}
