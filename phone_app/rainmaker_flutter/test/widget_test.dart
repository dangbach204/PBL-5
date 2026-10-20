// This is a basic Flutter widget test.
//
// To perform an interaction with a widget in your test, use the WidgetTester
// utility in the flutter_test package. For example, you can send tap and scroll
// gestures. You can also use WidgetTester to find child widgets in the widget
// tree, read text, and verify that the values of widget properties are correct.

import 'package:flutter_test/flutter_test.dart';

import 'package:rainmaker_flutter/main.dart';

void main() {
  testWidgets('shows the ESP provisioning screen', (WidgetTester tester) async {
    await tester.pumpWidget(const EspProvisioningApp());

    expect(find.text('Kết nối ESP32 với Wi‑Fi'), findsOneWidget);
    expect(find.text('Tìm thiết bị ESP32'), findsOneWidget);
  });
}
