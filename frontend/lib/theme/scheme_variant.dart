/// Color generation algorithm applied on top of the seed color's HCT hue
/// to derive the full tonal palette set. Only meaningful under
/// [M3SpecVersion.spec2025]; each case maps 1:1 to a `material_color_utilities`
/// `Scheme*` subclass in [ThemeController].
enum M3SchemeVariant {
  tonalSpot('Tonal Spot', 'Balanced, low-chroma palettes - the M3 default.'),
  fidelity('Fidelity', 'Preserves the seed color as closely as tone allows.'),
  content('Content', 'Derives secondary/tertiary hues from the seed like an artwork palette.'),
  expressive('Expressive', 'Wider hue shifts and higher chroma for a bolder look.');

  const M3SchemeVariant(this.label, this.description);

  final String label;
  final String description;

  static M3SchemeVariant fromName(String name) =>
      M3SchemeVariant.values.firstWhere((v) => v.name == name, orElse: () => M3SchemeVariant.expressive);
}
