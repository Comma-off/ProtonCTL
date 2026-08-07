import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../models/steam_candidate.dart';
import '../../services/settings_service.dart';
import '../../services/steam_service.dart';
import 'steam_path_step.dart';

class FirstStartWizardScreen extends StatefulWidget {
  const FirstStartWizardScreen({super.key});

  @override
  State<FirstStartWizardScreen> createState() => _FirstStartWizardScreenState();
}

class _FirstStartWizardScreenState extends State<FirstStartWizardScreen> {
  final SteamService _steamService = SteamService();
  final TextEditingController _customPathController = TextEditingController();

  int _step = 0;
  List<SteamCandidate> _candidates = [];
  SteamCandidate? _selected;
  SteamCandidate? _customValidated;
  bool _validatingCustom = false;

  @override
  void initState() {
    super.initState();
    _candidates = _steamService.detectCandidates();
    final firstValid = _candidates.where((c) => c.valid).toList();
    if (firstValid.isNotEmpty) _selected = firstValid.first;
  }

  @override
  void dispose() {
    _customPathController.dispose();
    super.dispose();
  }

  Future<void> _validateCustomPath() async {
    final path = _customPathController.text.trim();
    if (path.isEmpty) return;
    setState(() => _validatingCustom = true);
    final candidate = _steamService.validatePath(path);
    setState(() {
      _customValidated = candidate;
      _validatingCustom = false;
      if (candidate.valid) _selected = candidate;
    });
  }

  Future<void> _finish() async {
    final selected = _selected;
    if (selected == null) return;

    final settings = context.read<SettingsService>();
    await settings.completeFirstStart(
      steamPath: selected.path,
      compatibilityToolsDir: _steamService.compatibilityToolsDir(selected.path),
      installType: selected.type,
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: SafeArea(
        child: Center(
          child: ConstrainedBox(
            constraints: const BoxConstraints(maxWidth: 640),
            child: Stepper(
              currentStep: _step,
              type: StepperType.vertical,
              controlsBuilder: (context, details) => Padding(
                padding: const EdgeInsets.only(top: 16),
                child: Row(
                  children: [
                    if (details.stepIndex > 0)
                      OutlinedButton(onPressed: details.onStepCancel, child: const Text('Back')),
                    const SizedBox(width: 12),
                    FilledButton(
                      onPressed: details.stepIndex == 2 && _selected == null ? null : details.onStepContinue,
                      child: Text(details.stepIndex == 2 ? 'Finish setup' : 'Continue'),
                    ),
                  ],
                ),
              ),
              onStepContinue: () {
                if (_step < 2) {
                  setState(() => _step += 1);
                } else {
                  _finish();
                }
              },
              onStepCancel: () {
                if (_step > 0) setState(() => _step -= 1);
              },
              steps: [
                Step(
                  title: const Text('Welcome to ProtonCTL'),
                  isActive: _step >= 0,
                  state: _step > 0 ? StepState.complete : StepState.indexed,
                  content: const Align(
                    alignment: Alignment.centerLeft,
                    child: Text(
                      'PROTONCTL manages Proton-GE and custom Proton builds for your Steam library: '
                      'installing releases, compiling custom repositories, and backing up wine '
                      'prefixes as portable .prtbak archives.\n\n'
                      "Let's start by locating your Steam installation.",
                    ),
                  ),
                ),
                Step(
                  title: const Text('Locate Steam'),
                  isActive: _step >= 1,
                  state: _step > 1 ? StepState.complete : StepState.indexed,
                  content: Align(
                    alignment: Alignment.centerLeft,
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text('Detected installations', style: Theme.of(context).textTheme.labelLarge),
                        const SizedBox(height: 8),
                        ..._candidates.map(
                          (c) => Padding(
                            padding: const EdgeInsets.only(bottom: 8),
                            child: SteamCandidateTile(
                              candidate: c,
                              selected: _selected?.path == c.path,
                              onSelect: () => setState(() => _selected = c),
                            ),
                          ),
                        ),
                        const SizedBox(height: 16),
                        Text('Or specify a custom path', style: Theme.of(context).textTheme.labelLarge),
                        const SizedBox(height: 8),
                        Row(
                          children: [
                            Expanded(
                              child: TextField(
                                controller: _customPathController,
                                decoration: const InputDecoration(
                                  hintText: '/path/to/Steam',
                                  border: OutlineInputBorder(),
                                  isDense: true,
                                ),
                              ),
                            ),
                            const SizedBox(width: 8),
                            FilledButton.tonal(
                              onPressed: _validatingCustom ? null : _validateCustomPath,
                              child: _validatingCustom
                                  ? const SizedBox(
                                      width: 16,
                                      height: 16,
                                      child: CircularProgressIndicator(strokeWidth: 2),
                                    )
                                  : const Text('Check'),
                            ),
                          ],
                        ),
                        if (_customValidated != null) ...[
                          const SizedBox(height: 8),
                          SteamCandidateTile(
                            candidate: _customValidated!,
                            selected: _selected?.path == _customValidated!.path,
                            onSelect: _customValidated!.valid
                                ? () => setState(() => _selected = _customValidated)
                                : null,
                          ),
                        ],
                      ],
                    ),
                  ),
                ),
                Step(
                  title: const Text('Confirm'),
                  isActive: _step >= 2,
                  state: StepState.indexed,
                  content: Align(
                    alignment: Alignment.centerLeft,
                    child: _selected == null
                        ? const Text('Select a Steam installation to continue.')
                        : Column(
                            crossAxisAlignment: CrossAxisAlignment.start,
                            children: [
                              Text('Steam install: ${steamInstallTypeLabel(_selected!.type)}'),
                              Text(_selected!.path, style: const TextStyle(fontFamily: 'monospace')),
                              const SizedBox(height: 8),
                              Text(
                                'Compatibility tools will be installed to:\n'
                                '${_steamService.compatibilityToolsDir(_selected!.path)}',
                                style: Theme.of(context).textTheme.bodySmall,
                              ),
                            ],
                          ),
                  ),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}
