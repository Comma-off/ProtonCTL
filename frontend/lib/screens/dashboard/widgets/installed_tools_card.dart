import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../../services/repository_service.dart';

class InstalledToolsCard extends StatelessWidget {
  const InstalledToolsCard({super.key});

  @override
  Widget build(BuildContext context) {
    final repoService = context.watch<RepositoryService>();
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
                Icon(Icons.sports_esports_outlined, color: scheme.primary),
                const SizedBox(width: 12),
                Text('Installed Proton Tools', style: Theme.of(context).textTheme.titleMedium),
                const Spacer(),
                IconButton(
                  icon: const Icon(Icons.refresh),
                  tooltip: 'Refresh',
                  onPressed: repoService.refreshInstalled,
                ),
              ],
            ),
            const Divider(height: 24),
            Expanded(
              child: repoService.installed.isEmpty
                  ? Center(
                      child: Text(
                        'No Proton builds installed yet.\nAdd a repository and install a release to get started.',
                        textAlign: TextAlign.center,
                        style: TextStyle(color: scheme.onSurfaceVariant),
                      ),
                    )
                  : ListView.builder(
                      itemCount: repoService.installed.length,
                      itemBuilder: (context, index) {
                        final v = repoService.installed[index];
                        return ListTile(
                          contentPadding: EdgeInsets.zero,
                          leading: CircleAvatar(
                            backgroundColor: scheme.primaryContainer,
                            child: Icon(Icons.check, color: scheme.onPrimaryContainer),
                          ),
                          title: Text(v.name, overflow: TextOverflow.ellipsis),
                          subtitle: Text(
                            v.sourceRepo.isEmpty ? 'Manually installed' : v.sourceRepo,
                            style: const TextStyle(fontSize: 12),
                          ),
                          trailing: IconButton(
                            icon: const Icon(Icons.delete_outline),
                            tooltip: 'Remove',
                            onPressed: () async {
                              final confirmed = await showDialog<bool>(
                                context: context,
                                builder: (context) => AlertDialog(
                                  title: const Text('Remove Proton build?'),
                                  content: Text('This deletes ${v.path} from disk.'),
                                  actions: [
                                    TextButton(
                                      onPressed: () => Navigator.of(context).pop(false),
                                      child: const Text('Cancel'),
                                    ),
                                    FilledButton(
                                      onPressed: () => Navigator.of(context).pop(true),
                                      child: const Text('Remove'),
                                    ),
                                  ],
                                ),
                              );
                              if (confirmed == true) repoService.removeInstalledVersion(v.name);
                            },
                          ),
                        );
                      },
                    ),
            ),
          ],
        ),
      ),
    );
  }
}
