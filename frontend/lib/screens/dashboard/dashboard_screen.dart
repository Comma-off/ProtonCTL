import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../services/repository_service.dart';
import '../../services/settings_service.dart';
import '../settings/theme_settings_screen.dart';
import 'widgets/backup_actions_card.dart';
import 'widgets/compilation_queue_card.dart';
import 'widgets/installed_tools_card.dart';
import 'widgets/repository_manager_card.dart';

class DashboardScreen extends StatefulWidget {
  const DashboardScreen({super.key});

  @override
  State<DashboardScreen> createState() => _DashboardScreenState();
}

class _DashboardScreenState extends State<DashboardScreen> {
  late final RepositoryService _repositoryService;

  @override
  void initState() {
    super.initState();
    final settings = context.read<SettingsService>();
    _repositoryService = RepositoryService(compatToolsDir: settings.config.compatibilityToolsDir);
    _repositoryService.refreshAll();
  }

  @override
  void dispose() {
    _repositoryService.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return ChangeNotifierProvider<RepositoryService>.value(
      value: _repositoryService,
      child: Scaffold(
        appBar: AppBar(
          title: const Text('ProtonCTL'),
          actions: [
            IconButton(
              icon: const Icon(Icons.palette_outlined),
              tooltip: 'Theme settings',
              onPressed: () => Navigator.of(context)
                  .push(MaterialPageRoute(builder: (_) => const ThemeSettingsScreen())),
            ),
            const SizedBox(width: 8),
          ],
        ),
        body: RefreshIndicator(
          onRefresh: _repositoryService.refreshAll,
          child: LayoutBuilder(
            builder: (context, constraints) {
              final columns = constraints.maxWidth > 900 ? 2 : 1;
              return GridView.count(
                padding: const EdgeInsets.all(16),
                crossAxisCount: columns,
                childAspectRatio: columns == 2 ? 1.15 : 1.4,
                mainAxisSpacing: 16,
                crossAxisSpacing: 16,
                children: const [
                  InstalledToolsCard(),
                  RepositoryManagerCard(),
                  BackupActionsCard(),
                  CompilationQueueCard(),
                ],
              );
            },
          ),
        ),
      ),
    );
  }
}
