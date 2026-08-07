import 'package:flutter/material.dart';

/// One of the six predefined seed colors users can pick from in the theme
/// settings screen. Chosen to evoke the app's domain (Proton/Wine/Steam on
/// Linux) rather than generic swatches.
class SeedColorOption {
  const SeedColorOption({required this.name, required this.color});

  final String name;
  final Color color;
}

const List<SeedColorOption> kSeedColorOptions = <SeedColorOption>[
  SeedColorOption(name: 'Proton Violet', color: Color(0xFF7C4DFF)),
  SeedColorOption(name: 'Steam Blue', color: Color(0xFF2A6DF4)),
  SeedColorOption(name: 'GE Ember', color: Color(0xFFFF6D3F)),
  SeedColorOption(name: 'Wine Crimson', color: Color(0xFFB3261E)),
  SeedColorOption(name: 'Terminal Green', color: Color(0xFF00C853)),
  SeedColorOption(name: 'Slate Teal', color: Color(0xFF00696D)),
];
