import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../../models/build_job.dart';
import '../../../services/repository_service.dart';

Color _stageColor(BuildStage stage, ColorScheme scheme) {
  switch (stage) {
    case BuildStage.done:
      return scheme.primary;
    case BuildStage.failed:
    case BuildStage.cancelled:
      return scheme.error;
    default:
      return scheme.tertiary;
  }
}

String _stageLabel(BuildStage stage) {
  switch (stage) {
    case BuildStage.queued:
      return 'Queued';
    case BuildStage.cloning:
      return 'Cloning';
    case BuildStage.configuring:
      return 'Configuring';
    case BuildStage.building:
      return 'Building';
    case BuildStage.packaging:
      return 'Packaging';
    case BuildStage.done:
      return 'Done';
    case BuildStage.failed:
      return 'Failed';
    case BuildStage.cancelled:
      return 'Cancelled';
  }
}

class CompilationQueueCard extends StatelessWidget {
  const CompilationQueueCard({super.key});

  @override
  Widget build(BuildContext context) {
    final service = context.watch<RepositoryService>();
    final scheme = Theme.of(context).colorScheme;
    final jobs = service.buildJobs;

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
                Icon(Icons.precision_manufacturing_outlined, color: scheme.primary),
                const SizedBox(width: 12),
                Text('Compilation Queue', style: Theme.of(context).textTheme.titleMedium),
                const Spacer(),
                IconButton(
                  icon: const Icon(Icons.refresh),
                  tooltip: 'Refresh',
                  onPressed: service.refreshBuildJobs,
                ),
              ],
            ),
            const Divider(height: 24),
            Expanded(
              child: jobs.isEmpty
                  ? Center(
                      child: Text(
                        'No builds queued.\nUse "Build from source" on a repository to compile a custom Proton.',
                        textAlign: TextAlign.center,
                        style: TextStyle(color: scheme.onSurfaceVariant),
                      ),
                    )
                  : ListView.builder(
                      itemCount: jobs.length,
                      itemBuilder: (context, index) {
                        final job = jobs[index];
                        return Padding(
                          padding: const EdgeInsets.only(bottom: 12),
                          child: Column(
                            crossAxisAlignment: CrossAxisAlignment.start,
                            children: [
                              Row(
                                children: [
                                  Expanded(
                                    child: Text(
                                      '${job.repo.fullName}${job.ref.isEmpty ? "" : "@${job.ref}"}',
                                      overflow: TextOverflow.ellipsis,
                                      style: const TextStyle(fontWeight: FontWeight.w600),
                                    ),
                                  ),
                                  Chip(
                                    label: Text(_stageLabel(job.stage)),
                                    backgroundColor: _stageColor(job.stage, scheme).withOpacity(0.15),
                                    labelStyle: TextStyle(color: _stageColor(job.stage, scheme)),
                                    visualDensity: VisualDensity.compact,
                                  ),
                                  if (!job.isTerminal)
                                    IconButton(
                                      icon: const Icon(Icons.stop_circle_outlined),
                                      tooltip: 'Cancel',
                                      onPressed: () => service.cancelBuild(job.id),
                                    ),
                                ],
                              ),
                              const SizedBox(height: 6),
                              ClipRRect(
                                borderRadius: BorderRadius.circular(8),
                                child: LinearProgressIndicator(
                                  value: job.isTerminal ? 1.0 : job.progressPercent / 100.0,
                                  minHeight: 6,
                                  color: _stageColor(job.stage, scheme),
                                  backgroundColor: scheme.surfaceContainerHighest,
                                ),
                              ),
                              if (job.stage == BuildStage.failed && job.error.isNotEmpty)
                                Padding(
                                  padding: const EdgeInsets.only(top: 4),
                                  child: Text(
                                    job.error,
                                    style: TextStyle(color: scheme.error, fontSize: 12),
                                    maxLines: 2,
                                    overflow: TextOverflow.ellipsis,
                                  ),
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
