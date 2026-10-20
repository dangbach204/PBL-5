import 'package:esp_ble_prov_dart/esp_ble_prov_dart.dart';
import 'package:flutter/material.dart';

void main() => runApp(const EspProvisioningApp());

class EspProvisioningApp extends StatelessWidget {
  const EspProvisioningApp({super.key});

  @override
  Widget build(BuildContext context) => MaterialApp(
    debugShowCheckedModeBanner: false,
    title: 'ESP Provisioning',
    theme: ThemeData(
      colorScheme: ColorScheme.fromSeed(seedColor: const Color(0xff0b6e4f)),
      useMaterial3: true,
    ),
    home: const ProvisioningPage(),
  );
}

class ProvisioningPage extends StatefulWidget {
  const ProvisioningPage({super.key});

  @override
  State<ProvisioningPage> createState() => _ProvisioningPageState();
}

class _ProvisioningPageState extends State<ProvisioningPage> {
  final _popController = TextEditingController();
  List<EspBleDevice> _devices = const [];
  List<WiFiNetwork> _networks = const [];
  EspBleProvisioner? _provisioner;
  EspBleDevice? _connectedDevice;
  String _status = 'Sẵn sàng tìm thiết bị ESP32.';
  bool _working = false;

  @override
  void dispose() {
    _popController.dispose();
    _provisioner?.disconnect();
    super.dispose();
  }

  Future<void> _findDevices() async {
    setState(() {
      _working = true;
      _devices = const [];
      _networks = const [];
      _status = 'Đang quét thiết bị BLE có tên PROV_…';
    });
    try {
      final provisioner = EspBleProvisioner(
        deviceNamePrefix: 'PROV_',
        security: Security1(pop: _popController.text),
      );
      final devices = await provisioner.scanDevices();
      if (!mounted) return;
      setState(() {
        _provisioner = provisioner;
        _devices = devices;
        _status = devices.isEmpty
            ? 'Không tìm thấy PROV_. Hãy đặt ESP32 vào provisioning mode.'
            : 'Chọn một thiết bị để kết nối.';
      });
    } catch (error) {
      _showError('Không thể quét BLE: $error');
    } finally {
      if (mounted) setState(() => _working = false);
    }
  }

  Future<void> _connect(EspBleDevice device) async {
    final provisioner = _provisioner;
    if (provisioner == null) return;
    setState(() {
      _working = true;
      _status = 'Đang kết nối ${device.name ?? device.id}…';
    });
    try {
      await provisioner.connect(device: device);
      if (mounted) setState(() => _status = 'Đang xác thực Security 1…');
      await provisioner.establishSession();
      if (mounted) setState(() => _status = 'ESP32 đang quét các mạng Wi‑Fi…');
      final networks = await provisioner.scan();
      if (!mounted) return;
      setState(() {
        _connectedDevice = device;
        _networks = networks
            .where((network) => network.ssid.isNotEmpty)
            .toList();
        _status = 'Đã kết nối. Chọn Wi‑Fi để gửi thông tin.';
      });
    } catch (error) {
      await provisioner.disconnect();
      _showError('Kết nối hoặc xác thực thất bại: $error');
    } finally {
      if (mounted) setState(() => _working = false);
    }
  }

  Future<void> _sendCredentials(WiFiNetwork network) async {
    final password = TextEditingController();
    final shouldSend = await showModalBottomSheet<bool>(
      context: context,
      isScrollControlled: true,
      builder: (context) => Padding(
        padding: EdgeInsets.fromLTRB(
          24,
          24,
          24,
          24 + MediaQuery.viewInsetsOf(context).bottom,
        ),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Text(network.ssid, style: Theme.of(context).textTheme.titleLarge),
            const SizedBox(height: 12),
            TextField(
              controller: password,
              obscureText: true,
              autofocus: true,
              decoration: const InputDecoration(
                labelText: 'Mật khẩu Wi‑Fi',
                border: OutlineInputBorder(),
              ),
            ),
            const SizedBox(height: 16),
            FilledButton(
              onPressed: () => Navigator.pop(context, true),
              child: const Text('Gửi cấu hình'),
            ),
          ],
        ),
      ),
    );
    if (shouldSend != true || !mounted) {
      password.dispose();
      return;
    }
    final provisioner = _provisioner;
    if (provisioner == null) {
      password.dispose();
      return;
    }
    setState(() {
      _working = true;
      _status = 'Đang kết nối ESP32 vào ${network.ssid}…';
    });
    try {
      final status = await provisioner.sendCredentials(
        WiFiConfig(
          ssid: network.ssid,
          passphrase: password.text,
          bssid: network.bssid,
          channel: network.channel,
        ),
      );
      if (!mounted) return;
      setState(
        () =>
            _status = 'Đã gửi cấu hình. Trạng thái ESP: ${status.state.name}.',
      );
    } catch (error) {
      _showError('Gửi Wi‑Fi thất bại: $error');
    } finally {
      password.dispose();
      if (mounted) setState(() => _working = false);
    }
  }

  void _showError(String message) {
    if (!mounted) return;
    setState(() => _status = message);
    ScaffoldMessenger.of(context)
        .showSnackBar(SnackBar(content: Text(message)));
  }

  @override
  Widget build(BuildContext context) => Scaffold(
    appBar: AppBar(title: const Text('ESP32 Provisioning')),
    body: SafeArea(
      child: ListView(
        padding: const EdgeInsets.all(20),
        children: [
          const Icon(Icons.bluetooth_searching, size: 56),
          const SizedBox(height: 12),
          Text(
            'Kết nối ESP32 với Wi‑Fi',
            textAlign: TextAlign.center,
            style: Theme.of(context).textTheme.headlineSmall,
          ),
          const SizedBox(height: 8),
          const Text(
            'Yêu cầu firmware BLE, Security 1 và tên bắt đầu bằng PROV_.',
            textAlign: TextAlign.center,
          ),
          const SizedBox(height: 24),
          TextField(
            controller: _popController,
            obscureText: true,
            decoration: const InputDecoration(
              labelText: 'Proof of Possession (PoP)',
              hintText: 'Để trống nếu firmware không dùng PoP',
              border: OutlineInputBorder(),
            ),
          ),
          const SizedBox(height: 16),
          FilledButton.icon(
            onPressed: _working ? null : _findDevices,
            icon: const Icon(Icons.search),
            label: const Text('Tìm thiết bị ESP32'),
          ),
          const SizedBox(height: 20),
          if (_working) const Center(child: CircularProgressIndicator()),
          if (_working) const SizedBox(height: 12),
          Card(
            child: Padding(
              padding: const EdgeInsets.all(14),
              child: Text(_status),
            ),
          ),
          if (_devices.isNotEmpty) ...[
            const SizedBox(height: 16),
            Text(
              'Thiết bị BLE',
              style: Theme.of(context).textTheme.titleMedium,
            ),
            ..._devices.map(
              (device) => ListTile(
                leading: const Icon(Icons.memory),
                title: Text(device.name ?? 'ESP32'),
                subtitle: Text(device.id),
                trailing: const Icon(Icons.chevron_right),
                onTap: _working ? null : () => _connect(device),
              ),
            ),
          ],
          if (_connectedDevice != null && _networks.isNotEmpty) ...[
            const SizedBox(height: 16),
            Text(
              'Wi‑Fi do ESP32 tìm thấy',
              style: Theme.of(context).textTheme.titleMedium,
            ),
            ..._networks.map(
              (network) => ListTile(
                leading: const Icon(Icons.wifi),
                title: Text(network.ssid),
                subtitle: Text('${network.rssi} dBm · Kênh ${network.channel}'),
                onTap: _working ? null : () => _sendCredentials(network),
              ),
            ),
          ],
        ],
      ),
    ),
  );
}
