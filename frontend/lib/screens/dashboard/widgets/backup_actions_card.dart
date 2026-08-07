import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../../ffi/protonctl_bridge.dart';
import '../../../models/backup_manifest.dart';
import '../../../models/proton_repository.dart';
import '../../../services/backup_service.dart';
import '../../../services/repository_service.dart';
import '../../../services/settings_service.dart';

class BackupActionsCard extends StatefulWidget {
  const BackupActionsCard({super.key});

  @override
  State<BackupActionsCard> createState() => _BackupActionsCardState();
}

class _BackupActionsCardState extends State<BackupActionsCard> {
  final BackupService _backupService = BackupService();
  bool _busy = false;

  Future<void> _showCreateBackupDialog(BuildContext context) async {
    final repoService = context.read<RepositoryService>();
    final installed = repoService.installed;
    if (installed.isEmpty) {
      ScaffoldMessenger.of(context)
          .showSnackBar(const SnackBar(content: Text('No installed Proton builds to back up yet')));
      return;
    }

    InstalledProtonVersion selected = installed.first;
    final prefixController = TextEditingController();
    final outFileController = TextEditingController(text: '${selected.name}.prtbak');

    await showDialog<void>(
      context: context,
      builder: (context) => StatefulBuilder(
        builder: (context, setState) => AlertDialog(
          title: const Text('Create backup'),
          content: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              DropdownButtonFormField<InstalledProtonVersion>(
                value: selected,
                isExpanded: true,
                decoration: const InputDecoration(labelText: 'Proton build'),
                items: installed
                    .map((v) => DropdownMenuItem(value: v, child: Text(v.name, overflow: TextOverflow.ellipsis)))
                    .toList(),
                onChanged: (v) {
                  if (v == null) return;
                  setState(() {
                    selected = v;
                    outFileController.text = '${v.name}.prtbak';
                  });
                },
              ),
              const SizedBox(height: 12),
              TextField(
                controller: prefixController,
                decoration: const InputDecoration(
                  labelText: 'Wine prefix path (optional)',
                  hintText: '.../steamapps/compatdata/<appid>/pfx',
                ),
              ),
              const SizedBox(height: 12),
              TextField(
                controller: outFileController,
                decoration: const InputDecoration(labelText: 'Output .prtbak file'),
              ),
            ],
          ),
          actions: [
            TextButton(onPressed: () => Navigator.of(context).pop(), child: const Text('Cancel')),
            FilledButton(
              onPressed: () async {
                Navigator.of(context).pop();
                setState(() => _busy = true);
                try {
                  await _backupService.createBackup(
                    protonDir: selected.path,
                    prefixDir: prefixController.text.trim().isEmpty ? null : prefixController.text.trim(),
                    manifest: PrtBakManifest(
                      protonVersion: selected.versionTag.isEmpty ? selected.name : selected.versionTag,
                      sourceRepo: selected.sourceRepo,
                    ),
                    outFile: outFileController.text.trim(),
                  );
                  if (mounted) {
                    ScaffoldMessenger.of(context)
                        .showSnackBar(const SnackBar(content: Text('Backup created')));
                  }
                } on ProtonCtlNativeException catch (e) {
                  if (mounted) {
                    ScaffoldMessenger.of(context)
                        .showSnackBar(SnackBar(content: Text('Backup failed: ${e.message}')));
                  }
                } finally {
                  if (mounted) setState(() => _busy = false);
                }
              },
              child: const Text('Create'),
            ),
          ],
        ),
      ),
    );
  }

  Future<void> _showRestoreBackupDialog(BuildContext context) async {
    final fileController = TextEditingController();
    final rootController = TextEditingController(
      text: context.read<SettingsService>().config.compatibilityToolsDir,
    );

    await showDialog<void>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Restore backup'),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            TextField(
              controller: fileController,
              decoration: const InputDecoration(labelText: '.prtbak file path'),
            ),
            const SizedBox(height: 12),
            TextField(
              controller: rootController,
              decoration: const InputDecoration(labelText: 'Restore into (compatibilitytools.d)'),
            ),
          ],
        ),
        actions: [
          TextButton(onPressed: () => Navigator.of(context).pop(), child: const Text('Cancel')),
          FilledButton(
            onPressed: () async {
              Navigator.of(context).pop();
              setState(() => _busy = true);
              try {
                await _backupService.restoreBackup(fileController.text.trim(), rootController.text.trim());
                if (mounted) {
                  context.read<RepositoryService>().refreshInstalled();
                  ScaffoldMessenger.of(context)
                      .showSnackBar(const SnackBar(content: Text('Backup restored')));
                }
              } on ProtonCtlNativeException catch (e) {
                if (mounted) {
                  ScaffoldMessenger.of(context)
                      .showSnackBar(SnackBar(content: Text('Restore failed: ${e.message}')));
                }
              } finally {
                if (mounted) setState(() => _busy = false);
              }
            },
            child: const Text('Restore'),
          ),
        ],
      ),
    );
  }

  Future<void> _runRepairScan(BuildContext context) async {
    final settings = context.read<SettingsService>().config;
    setState(() => _busy = true);
    List<RepairIssue> issues = [];
    String? error;
    try {
      issues = await _backupService.scanRepair(
        settings.compatibilityToolsDir,
        '${settings.steamPath}/steamapps/compatdata',
      );
    } on ProtonCtlNativeException catch (e) {
      error = e.message;
    } finally {
      if (mounted) setState(() => _busy = false);
    }

    if (!context.mounted) return;
    await showDialog<void>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Repair scan results'),
        content: SizedBox(
          width: 480,
          child: error != null
              ? Text(error)
              : issues.isEmpty
                  ? const Text('No issues found - everything looks healthy.')
                  : ListView(
                      shrinkWrap: true,
                      children: issues
                          .map((issue) => ListTile(
                                leading: Icon(issue.fixable ? Icons.build_circle_outlined : Icons.warning_amber),
                                title: Text(issue.path),
                                subtitle: Text(issue.description),
                                trailing: issue.fixable
                                    ? TextButton(
                                        onPressed: () {
                                          final fixed = _backupService.fixIssue(issue);
                                          if (context.mounted) {
                                            ScaffoldMessenger.of(context).showSnackBar(
                                              SnackBar(content: Text(fixed ? 'Fixed' : 'Fix failed')),
                                            );
                                          }
                                        },
                                        child: const Text('Fix'),
                                      )
                                    : null,
                              ))
                          .toList(),
                    ),
        ),
        actions: [
          TextButton(onPressed: () => Navigator.of(context).pop(), child: const Text('Close')),
        ],
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;

    return Card(
      elevation: 0,
      color: scheme.surfaceContainer,
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(28)),
      child: Padding(
        padding: const EdgeInsets.all(20),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Icon(Icons.archive_outlined, color: scheme.primary),
                const SizedBox(width: 12),
                Text('Backup & Repair', style: Theme.of(context).textTheme.titleMedium),
              ],
            ),
            const Divider(height: 24),
            Expanded(
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  FilledButton.icon(
                    onPressed: _busy ? null : () => _showCreateBackupDialog(context),
                    icon: const Icon(Icons.save_outlined),
                    label: const Text('Create .prtbak backup'),
                  ),
                  const SizedBox(height: 12),
                  OutlinedButton.icon(
                    onPressed: _busy ? null : () => _showRestoreBackupDialog(context),
                    icon: const Icon(Icons.restore_outlined),
                    label: const Text('Restore from backup'),
                  ),
                  const SizedBox(height: 12),
                  OutlinedButton.icon(
                    onPressed: _busy ? null : () => _runRepairScan(context),
                    icon: const Icon(Icons.health_and_safety_outlined),
                    label: const Text('Scan for broken installs'),
                  ),
                  if (_busy) ...[
                    const SizedBox(height: 16),
                    const CircularProgressIndicator(),
                  ],
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}
