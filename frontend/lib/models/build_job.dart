import 'proton_repository.dart';

enum BuildStage { queued, cloning, configuring, building, packaging, done, failed, cancelled }

BuildStage buildStageFromJson(String value) =>
    BuildStage.values.firstWhere((s) => s.name == value, orElse: () => BuildStage.queued);

class BuildJob {
  BuildJob({
    required this.id,
    required this.repo,
    required this.ref,
    required this.stage,
    required this.progressPercent,
    required this.logLines,
    required this.error,
    required this.resultPath,
  });

  factory BuildJob.fromJson(Map<String, dynamic> json) => BuildJob(
        id: json['id'] as String? ?? '',
        repo: ProtonRepository.fromJson(json['repo'] as Map<String, dynamic>? ?? const {}),
        ref: json['ref'] as String? ?? '',
        stage: buildStageFromJson(json['stage'] as String? ?? 'queued'),
        progressPercent: (json['progress_percent'] as num?)?.toInt() ?? 0,
        logLines: ((json['log_lines'] as List?) ?? []).cast<String>(),
        error: json['error'] as String? ?? '',
        resultPath: json['result_path'] as String? ?? '',
      );

  final String id;
  final ProtonRepository repo;
  final String ref;
  final BuildStage stage;
  final int progressPercent;
  final List<String> logLines;
  final String error;
  final String resultPath;

  bool get isTerminal =>
      stage == BuildStage.done || stage == BuildStage.failed || stage == BuildStage.cancelled;
}
