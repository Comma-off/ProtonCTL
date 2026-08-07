/// Which Material 3 color pipeline to use. `material_color_utilities`
/// (as published) exposes a single, fixed dynamic-color algorithm with no
/// "spec version" flag of its own, so PROTONCTL draws this distinction itself:
///
/// - `spec2021` reproduces the original Material You launch behavior: a
///   single Tonal Spot palette via [ColorScheme.fromSeed], with no variant
///   or contrast-level control (that picker didn't exist yet in 2021).
/// - `spec2025` drives `material_color_utilities`'s [DynamicScheme]
///   directly, unlocking the scheme-variant and contrast-level controls
///   that shipped with the 2024/2025 "Expressive" color updates.
enum M3SpecVersion {
  spec2021,
  spec2025;

  String get label => this == M3SpecVersion.spec2025 ? 'SPEC 2025 (Expressive)' : 'SPEC 2021 (Baseline)';

  String get description => this == M3SpecVersion.spec2025
      ? 'Full dynamic color control: choose a scheme variant and contrast level.'
      : "Material You's original 2021 launch behavior: a single Tonal Spot palette, "
          'no variant or contrast controls.';

  static M3SpecVersion fromName(String name) =>
      M3SpecVersion.values.firstWhere((v) => v.name == name, orElse: () => M3SpecVersion.spec2025);
}
