import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../../ffi/protonctl_bridge.dart';
import '../../../models/proton_repository.dart';
import '../../../services/repository_service.dart';

class RepositoryManagerCard extends StatelessWidget {
  const RepositoryManagerCard({super.key});

  Future<void> _showAddRepositoryDialog(BuildContext context, RepositoryService service) async {
    final ownerController = TextEditingController();
    final repoController = TextEditingController();
    final formKey = GlobalKey<FormState>();
    String? error;

    await showDialog<void>(
      context: context,
      builder: (context) => StatefulBuilder(
        builder: (context, setState) => AlertDialog(
          title: const Text('Add custom repository'),
          content: Form(
            key: formKey,
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                TextFormField(
                  controller: ownerController,
                  decoration: const InputDecoration(labelText: 'Owner', hintText: 'e.g. GloriousEggroll'),
                  validator: (v) => (v == null || v.trim().isEmpty) ? 'Required' : null,
                ),
                const SizedBox(height: 12),
                TextFormField(
                  controller: repoController,
                  decoration: const InputDecoration(labelText: 'Repository', hintText: 'e.g. proton-ge-custom'),
                  validator: (v) => (v == null || v.trim().isEmpty) ? 'Required' : null,
                ),
                if (error != null) ...[
                  const SizedBox(height: 12),
                  Text(error!, style: TextStyle(color: Theme.of(context).colorScheme.error)),
                ],
              ],
            ),
          ),
          actions: [
            TextButton(onPressed: () => Navigator.of(context).pop(), child: const Text('Cancel')),
            FilledButton(
              onPressed: () async {
                if (!(formKey.currentState?.validate() ?? false)) return;
                try {
                  await service.addRepository(ownerController.text.trim(), repoController.text.trim());
                  if (context.mounted) Navigator.of(context).pop();
                } on ProtonCtlNativeException catch (e) {
                  setState(() => error = e.message);
                }
              },
              child: const Text('Add'),
            ),
          ],
        ),
      ),
    );
  }

  Future<void> _showReleasesSheet(
    BuildContext context,
    RepositoryService service,
    ProtonRepository repo,
  ) async {
    await showModalBottomSheet<void>(
      context: context,
      isScrollControlled: true,
      builder: (context) => DraggableScrollableSheet(
        expand: false,
        initialChildSize: 0.7,
        builder: (context, scrollController) => _ReleasesSheetContent(
          repo: repo,
          service: service,
          scrollController: scrollController,
        ),
      ),
    );
  }

  Future<void> _showBuildFromSourceDialog(
    BuildContext context,
    RepositoryService service,
    ProtonRepository repo,
  ) async {
    final refController = TextEditingController();
    await showDialog<void>(
      context: context,
      builder: (context) => AlertDialog(
        title: Text('Build ${repo.fullName} from source'),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text('Clones the repository and runs its build pipeline '
                '(build.sh, autogen.sh+configure+make, or a bare Makefile), '
                'then packages the output into compatibilitytools.d.'),
            const SizedBox(height: 12),
            TextField(
              controller: refController,
              decoration: const InputDecoration(
                labelText: 'Branch / tag / commit (optional)',
                hintText: 'default branch',
              ),
            ),
          ],
        ),
        actions: [
          TextButton(onPressed: () => Navigator.of(context).pop(), child: const Text('Cancel')),
          FilledButton(
            onPressed: () {
              service.enqueueBuild(repo, refController.text.trim());
              Navigator.of(context).pop();
              ScaffoldMessenger.of(context).showSnackBar(
                const SnackBar(content: Text('Build queued - see Compilation Queue for progress')),
              );
            },
            child: const Text('Queue build'),
          ),
        ],
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final service = context.watch<RepositoryService>();
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
                Icon(Icons.source_outlined, color: scheme.primary),
                const SizedBox(width: 12),
                Text('Repository Manager', style: Theme.of(context).textTheme.titleMedium),
                const Spacer(),
                IconButton(
                  icon: const Icon(Icons.add),
                  tooltip: 'Add repository',
                  onPressed: () => _showAddRepositoryDialog(context, service),
                ),
              ],
            ),
            const Divider(height: 24),
            Expanded(
              child: service.loadingRepositories
                  ? const Center(child: CircularProgressIndicator())
                  : ListView.builder(
                      itemCount: service.repositories.length,
                      itemBuilder: (context, index) {
                        final repo = service.repositories[index];
                        return ListTile(
                          contentPadding: EdgeInsets.zero,
                          leading: const Icon(Icons.folder_zip_outlined),
                          title: Text(repo.fullName),
                          subtitle: Text(repo.isBuiltin ? 'Built-in' : 'Custom'),
                          trailing: Row(
                            mainAxisSize: MainAxisSize.min,
                            children: [
                              IconButton(
                                icon: const Icon(Icons.build_outlined),
                                tooltip: 'Build from source',
                                onPressed: () => _showBuildFromSourceDialog(context, service, repo),
                              ),
                              IconButton(
                                icon: const Icon(Icons.cloud_download_outlined),
                                tooltip: 'View releases',
                                onPressed: () => _showReleasesSheet(context, service, repo),
                              ),
                              if (!repo.isBuiltin)
                                IconButton(
                                  icon: const Icon(Icons.delete_outline),
                                  tooltip: 'Remove',
                                  onPressed: () => service.removeRepository(repo),
                                ),
                            ],
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

class _ReleasesSheetContent extends StatefulWidget {
  const _ReleasesSheetContent({
    required this.repo,
    required this.service,
    required this.scrollController,
  });

  final ProtonRepository repo;
  final RepositoryService service;
  final ScrollController scrollController;

  @override
  State<_ReleasesSheetContent> createState() => _ReleasesSheetContentState();
}

class _ReleasesSheetContentState extends State<_ReleasesSheetContent> {
  List<GitHubRelease>? _releases;
  String? _error;
  String? _installingTag;

  @override
  void initState() {
    super.initState();
    _load();
  }

  Future<void> _load() async {
    try {
      final releases = await widget.service.fetchReleases(widget.repo);
      if (mounted) setState(() => _releases = releases);
    } on ProtonCtlNativeException catch (e) {
      if (mounted) setState(() => _error = e.message);
    }
  }

  Future<void> _install(GitHubRelease release) async {
    setState(() => _installingTag = release.tagName);
    try {
      await widget.service.installRelease(widget.repo, release);
      if (mounted) {
        Navigator.of(context).pop();
        ScaffoldMessenger.of(context)
            .showSnackBar(SnackBar(content: Text('Installed ${release.tagName}')));
      }
    } on ProtonCtlNativeException catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text('Install failed: ${e.message}')));
      }
    } finally {
      if (mounted) setState(() => _installingTag = null);
    }
  }

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.all(20),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text('Releases for ${widget.repo.fullName}', style: Theme.of(context).textTheme.titleLarge),
          const SizedBox(height: 12),
          Expanded(
            child: _error != null
                ? Center(child: Text(_error!))
                : _releases == null
                    ? const Center(child: CircularProgressIndicator())
                    : ListView.builder(
                        controller: widget.scrollController,
                        itemCount: _releases!.length,
                        itemBuilder: (context, index) {
                          final release = _releases![index];
                          final busy = _installingTag == release.tagName;
                          return ListTile(
                            title: Text(release.tagName),
                            subtitle: Text('${release.assets.length} asset(s)'
                                '${release.prerelease ? " - prerelease" : ""}'),
                            trailing: FilledButton.tonal(
                              onPressed: busy ? null : () => _install(release),
                              child: busy
                                  ? const SizedBox(
                                      width: 16, height: 16, child: CircularProgressIndicator(strokeWidth: 2))
                                  : const Text('Install'),
                            ),
                          );
                        },
                      ),
          ),
        ],
      ),
    );
  }
}
