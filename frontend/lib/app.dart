import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import 'screens/dashboard/dashboard_screen.dart';
import 'screens/first_start_wizard/wizard_screen.dart';
import 'services/settings_service.dart';
import 'theme/theme_controller.dart';

class ProtonCtlApp extends StatelessWidget {
  const ProtonCtlApp({super.key});

  @override
  Widget build(BuildContext context) {
    final theme = context.watch<ThemeController>();
    final settings = context.watch<SettingsService>();

    return MaterialApp(
      title: 'ProtonCTL',
      debugShowCheckedModeBanner: false,
      themeMode: theme.themeMode,
      theme: theme.lightTheme,
      darkTheme: theme.darkTheme,
      home: settings.config.firstStartCompleted
          ? const DashboardScreen()
          : const FirstStartWizardScreen(),
    );
  }
}
