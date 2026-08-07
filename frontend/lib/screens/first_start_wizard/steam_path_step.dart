import 'package:flutter/material.dart';

import '../../models/steam_candidate.dart';

String steamInstallTypeLabel(SteamInstallType type) {
  switch (type) {
    case SteamInstallType.flatpak:
      return 'Flatpak';
    case SteamInstallType.debian:
      return 'Debian / derivative';
    case SteamInstallType.native:
      return 'Native';
    case SteamInstallType.custom:
      return 'Custom';
    case SteamInstallType.unknown:
      return 'Detected';
  }
}

IconData steamInstallTypeIcon(SteamInstallType type) {
  switch (type) {
    case SteamInstallType.flatpak:
      return Icons.widgets_outlined;
    case SteamInstallType.debian:
      return Icons.terminal_outlined;
    case SteamInstallType.native:
      return Icons.desktop_windows_outlined;
    case SteamInstallType.custom:
      return Icons.folder_open_outlined;
    case SteamInstallType.unknown:
      return Icons.link_outlined;
  }
}

/// Renders one detected Steam install candidate as a selectable card,
/// disabling it when [SteamCandidate.valid] is false so users can't pick
/// a path that doesn't actually contain a `steamapps` folder.
class SteamCandidateTile extends StatelessWidget {
  const SteamCandidateTile({
    super.key,
    required this.candidate,
    required this.selected,
    required this.onSelect,
  });

  final SteamCandidate candidate;
  final bool selected;
  final VoidCallback? onSelect;

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    final enabled = candidate.valid && onSelect != null;

    return Card(
      elevation: 0,
      color: selected ? scheme.primaryContainer : scheme.surfaceContainerHigh,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(20),
        side: selected ? BorderSide(color: scheme.primary, width: 2) : BorderSide.none,
      ),
      child: ListTile(
        enabled: enabled,
        onTap: enabled ? onSelect : null,
        leading: Icon(
          steamInstallTypeIcon(candidate.type),
          color: selected ? scheme.onPrimaryContainer : scheme.onSurfaceVariant,
        ),
        title: Text(
          candidate.label,
          style: TextStyle(fontWeight: FontWeight.w600, color: selected ? scheme.onPrimaryContainer : null),
        ),
        subtitle: Text(candidate.path, style: const TextStyle(fontSize: 12)),
        trailing: candidate.valid
            ? Icon(Icons.check_circle, color: scheme.primary)
            : Tooltip(
                message: 'No steamapps/ folder found at this path',
                child: Icon(Icons.error_outline, color: scheme.error),
              ),
      ),
    );
  }
}
