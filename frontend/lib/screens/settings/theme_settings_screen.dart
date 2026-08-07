import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../theme/m3_spec.dart';
import '../../theme/scheme_variant.dart';
import '../../theme/seed_colors.dart';
import '../../theme/theme_controller.dart';

class ThemeSettingsScreen extends StatelessWidget {
  const ThemeSettingsScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final controller = context.watch<ThemeController>();
    final scheme = Theme.of(context).colorScheme;

    return Scaffold(
      appBar: AppBar(title: const Text('Theme')),
      body: ListView(
        padding: const EdgeInsets.all(20),
        children: [
          Text('Seed color', style: Theme.of(context).textTheme.titleMedium),
          const SizedBox(height: 12),
          Wrap(
            spacing: 12,
            runSpacing: 12,
            children: List.generate(kSeedColorOptions.length, (index) {
              final option = kSeedColorOptions[index];
              final selected = controller.seedColorIndex == index;
              return _SeedSwatch(
                option: option,
                selected: selected,
                onTap: () => controller.setSeedColorIndex(index),
              );
            }),
          ),
          const SizedBox(height: 32),
          Text('Spec version', style: Theme.of(context).textTheme.titleMedium),
          const SizedBox(height: 12),
          SegmentedButton<M3SpecVersion>(
            segments: M3SpecVersion.values
                .map((v) => ButtonSegment(value: v, label: Text(v.label)))
                .toList(),
            selected: {controller.specVersion},
            onSelectionChanged: (selection) => controller.setSpecVersion(selection.first),
          ),
          const SizedBox(height: 8),
          Text(
            controller.specVersion.description,
            style: Theme.of(context).textTheme.bodySmall?.copyWith(color: scheme.onSurfaceVariant),
          ),
          const SizedBox(height: 32),
          Opacity(
            opacity: controller.specVersion == M3SpecVersion.spec2025 ? 1.0 : 0.4,
            child: IgnorePointer(
              ignoring: controller.specVersion != M3SpecVersion.spec2025,
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text('Color algorithm variant', style: Theme.of(context).textTheme.titleMedium),
                  const SizedBox(height: 12),
                  ...M3SchemeVariant.values.map(
                    (variant) => RadioListTile<M3SchemeVariant>(
                      contentPadding: EdgeInsets.zero,
                      value: variant,
                      groupValue: controller.variant,
                      onChanged: (v) {
                        if (v != null) controller.setVariant(v);
                      },
                      title: Text(variant.label),
                      subtitle: Text(variant.description),
                    ),
                  ),
                  const SizedBox(height: 32),
                  Text('Contrast level', style: Theme.of(context).textTheme.titleMedium),
                  Slider(
                    value: controller.contrastLevel,
                    min: -1.0,
                    max: 1.0,
                    divisions: 20,
                    label: controller.contrastLevel.toStringAsFixed(2),
                    onChanged: controller.setContrastLevel,
                  ),
                ],
              ),
            ),
          ),
          const SizedBox(height: 24),
          Text('Appearance', style: Theme.of(context).textTheme.titleMedium),
          const SizedBox(height: 12),
          SegmentedButton<ThemeMode>(
            segments: const [
              ButtonSegment(value: ThemeMode.system, icon: Icon(Icons.brightness_auto), label: Text('System')),
              ButtonSegment(value: ThemeMode.light, icon: Icon(Icons.light_mode), label: Text('Light')),
              ButtonSegment(value: ThemeMode.dark, icon: Icon(Icons.dark_mode), label: Text('Dark')),
            ],
            selected: {controller.themeMode},
            onSelectionChanged: (selection) => controller.setThemeMode(selection.first),
          ),
          const SizedBox(height: 32),
          Text('Preview', style: Theme.of(context).textTheme.titleMedium),
          const SizedBox(height: 12),
          Card(
            elevation: 0,
            color: scheme.surfaceContainerHigh,
            shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(24)),
            child: Padding(
              padding: const EdgeInsets.all(20),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Wrap(
                    spacing: 12,
                    runSpacing: 12,
                    children: [
                      FilledButton(onPressed: () {}, child: const Text('Filled button')),
                      FilledButton.tonal(onPressed: () {}, child: const Text('Tonal button')),
                      OutlinedButton(onPressed: () {}, child: const Text('Outlined')),
                      TextButton(onPressed: () {}, child: const Text('Text button')),
                    ],
                  ),
                  const SizedBox(height: 16),
                  Wrap(
                    spacing: 8,
                    children: [
                      Chip(label: const Text('Primary'), backgroundColor: scheme.primaryContainer),
                      Chip(label: const Text('Secondary'), backgroundColor: scheme.secondaryContainer),
                      Chip(label: const Text('Tertiary'), backgroundColor: scheme.tertiaryContainer),
                      Chip(label: const Text('Error'), backgroundColor: scheme.errorContainer),
                    ],
                  ),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }
}

class _SeedSwatch extends StatelessWidget {
  const _SeedSwatch({required this.option, required this.selected, required this.onTap});

  final SeedColorOption option;
  final bool selected;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    return InkWell(
      borderRadius: BorderRadius.circular(20),
      onTap: onTap,
      child: Container(
        width: 96,
        padding: const EdgeInsets.symmetric(vertical: 12),
        decoration: BoxDecoration(
          color: option.color,
          borderRadius: BorderRadius.circular(20),
          border: selected
              ? Border.all(color: Theme.of(context).colorScheme.onSurface, width: 3)
              : null,
        ),
        child: Column(
          children: [
            Icon(selected ? Icons.check_circle : Icons.circle_outlined,
                color: option.color.computeLuminance() > 0.4 ? Colors.black87 : Colors.white),
            const SizedBox(height: 6),
            Text(
              option.name,
              textAlign: TextAlign.center,
              style: TextStyle(
                fontSize: 11,
                color: option.color.computeLuminance() > 0.4 ? Colors.black87 : Colors.white,
              ),
            ),
          ],
        ),
      ),
    );
  }
}
