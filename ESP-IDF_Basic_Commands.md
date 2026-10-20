# ESP-IDF – Các lệnh cơ bản

Tài liệu nhanh các lệnh thường dùng khi làm việc với **ESP-IDF**.

> Ví dụ bên dưới dùng `idf.py` và có thể chạy trong terminal sau khi đã export môi trường ESP-IDF.

---

## 1. Kiểm tra ESP-IDF

### Kiểm tra phiên bản ESP-IDF

```bash
idf.py --version
```

### Kiểm tra lệnh `idf.py`

```bash
which idf.py
```

### Kiểm tra các target được hỗ trợ

```bash
idf.py --list-targets
```

---

## 2. Set Target

Chọn chip/SoC mà project sử dụng.

### ESP32

```bash
idf.py set-target esp32
```

### ESP32-S2

```bash
idf.py set-target esp32s2
```

### ESP32-S3

```bash
idf.py set-target esp32s3
```

### ESP32-C3

```bash
idf.py set-target esp32c3
```

### ESP32-C6

```bash
idf.py set-target esp32c6
```

### ESP32-H2

```bash
idf.py set-target esp32h2
```

Lệnh này sẽ cập nhật cấu hình target của project.

---

## 3. Build Project

Biên dịch project:

```bash
idf.py build
```

Nếu build thành công, các file firmware sẽ được tạo trong thư mục:

```text
build/
```

### Build và tự động chọn target

Thông thường nên set target trước:

```bash
idf.py set-target esp32c3
idf.py build
```

---

## 4. Clean

### Xóa các file build trung gian

```bash
idf.py clean
```

Lệnh này xóa các file build nhưng giữ lại một số cấu hình của project.

### Full Clean

Xóa toàn bộ thư mục `build`:

```bash
idf.py fullclean
```

Sau đó build lại:

```bash
idf.py build
```

> Khi gặp lỗi build khó hiểu hoặc thay đổi cấu hình lớn, có thể dùng `fullclean`.

---

## 5. Menuconfig

Mở giao diện cấu hình ESP-IDF:

```bash
idf.py menuconfig
```

Tại đây có thể cấu hình:

- CPU frequency
- Flash size
- Partition table
- FreeRTOS
- Wi-Fi
- Bluetooth
- Logging
- Bootloader
- Component configuration
- Và nhiều tùy chọn khác

Sau khi chỉnh sửa, chọn:

```text
Save
```

để lưu cấu hình vào:

```text
sdkconfig
```

---

## 6. Xóa cấu hình hiện tại

Xóa file cấu hình `sdkconfig`:

```bash
rm sdkconfig
```

Sau đó chạy lại:

```bash
idf.py menuconfig
```

Hoặc build lại để ESP-IDF tạo cấu hình mặc định:

```bash
idf.py build
```

> Cẩn thận khi xóa `sdkconfig` vì toàn bộ cấu hình project có thể bị reset.

---

## 7. Flash Firmware

### Flash vào ESP

```bash
idf.py flash
```

Lệnh này thường sẽ:

1. Build nếu cần
2. Xác định các binary cần flash
3. Ghi firmware vào flash của ESP

### Chỉ định cổng serial

Ví dụ:

```bash
idf.py -p /dev/ttyUSB0 flash
```

Hoặc:

```bash
idf.py -p /dev/ttyACM0 flash
```

Trên Windows:

```powershell
idf.py -p COM3 flash
```

---

## 8. Build + Flash

Có thể thực hiện trực tiếp:

```bash
idf.py build flash
```

Hoặc thường dùng:

```bash
idf.py build flash monitor
```

---

## 9. Erase Flash

Xóa toàn bộ nội dung flash:

```bash
idf.py erase-flash
```

Sau đó có thể flash lại:

```bash
idf.py flash
```

### Xóa flash rồi build + flash

```bash
idf.py erase-flash flash
```

Hoặc:

```bash
idf.py erase-flash build flash
```

> `erase-flash` khác `clean`:  
> - `clean` → xóa file build trên máy tính.
> - `erase-flash` → xóa dữ liệu đang nằm trong chip ESP.

---

## 10. Monitor

Mở Serial Monitor:

```bash
idf.py monitor
```

Hoặc chỉ định port:

```bash
idf.py -p /dev/ttyUSB0 monitor
```

Có thể kết hợp build, flash và monitor:

```bash
idf.py build flash monitor
```

### Thoát monitor

Thông thường:

```text
Ctrl + ]
```

---

## 11. Flash + Monitor

Sau khi firmware đã build:

```bash
idf.py flash monitor
```

Đây là một trong những lệnh thường dùng nhất khi phát triển firmware.

---

## 12. Xem Log

Sau khi chạy:

```bash
idf.py monitor
```

ESP-IDF sẽ hiển thị log từ chương trình:

```text
I (1234) MAIN: Hello World!
I (1235) WIFI: Connecting...
E (2000) WIFI: Connection failed
```

Các mức log thường gặp:

```text
E = Error
W = Warning
I = Info
D = Debug
V = Verbose
```

---

## 13. Build + Flash + Monitor

Workflow phổ biến:

```bash
idf.py build flash monitor
```

Nếu cần xóa flash trước:

```bash
idf.py erase-flash build flash monitor
```

---

## 14. Partition Table

Mở cấu hình:

```bash
idf.py menuconfig
```

Sau đó tìm:

```text
Partition Table
```

Có thể chọn partition table mặc định hoặc custom.

Các file liên quan thường nằm ở:

```text
partitions.csv
```

Nếu project sử dụng partition table custom, có thể cấu hình trong `menuconfig`.

---

## 15. Chọn Port

### Linux

Liệt kê USB serial:

```bash
ls /dev/ttyUSB*
```

```bash
ls /dev/ttyACM*
```

Hoặc:

```bash
ls /dev/serial/by-id/
```

Cách cuối thường dễ xác định chính xác ESP nào đang được kết nối.

### Windows

Mở Device Manager và kiểm tra:

```text
Ports (COM & LPT)
```

Ví dụ:

```text
COM3
COM5
```

Sau đó:

```bash
idf.py -p COM3 flash
```

---

## 16. Xem thiết bị USB trên Linux

```bash
lsusb
```

Xem log kernel:

```bash
dmesg | tail
```

Nếu không có quyền đọc `dmesg`, có thể dùng:

```bash
sudo dmesg | tail
```

Theo dõi thiết bị khi cắm/rút:

```bash
sudo dmesg --follow
```

---

## 17. Set Baud Rate

Flash với baud rate cụ thể:

```bash
idf.py -p /dev/ttyUSB0 -b 921600 flash
```

Ví dụ baud thấp hơn:

```bash
idf.py -p /dev/ttyUSB0 -b 115200 flash
```

Monitor với baud rate:

```bash
idf.py -p /dev/ttyUSB0 monitor -b 115200
```

---

## 18. Kiểm tra Project

### Liệt kê component

```bash
idf.py reconfigure
```

### Reconfigure project

```bash
idf.py reconfigure
```

Lệnh này hữu ích khi thay đổi:

- `CMakeLists.txt`
- Component
- `sdkconfig`
- Cấu hình build

---

## 19. Size Firmware

Xem thông tin kích thước firmware:

```bash
idf.py size
```

Chi tiết hơn:

```bash
idf.py size-components
```

Và:

```bash
idf.py size-files
```

Các lệnh này giúp kiểm tra flash/RAM đang được sử dụng bởi từng component.

---

## 20. Python Environment

Kiểm tra Python:

```bash
python3 --version
```

ESP-IDF thường sử dụng Python environment riêng.

Kiểm tra phiên bản IDF:

```bash
idf.py --version
```

---

# 21. Các lệnh thường dùng nhất

Nếu chỉ cần nhớ những lệnh quan trọng nhất:

| Công việc | Lệnh |
|---|---|
| Kiểm tra IDF | `idf.py --version` |
| Chọn ESP target | `idf.py set-target esp32c3` |
| Build | `idf.py build` |
| Clean | `idf.py clean` |
| Full clean | `idf.py fullclean` |
| Config | `idf.py menuconfig` |
| Reconfigure | `idf.py reconfigure` |
| Flash | `idf.py flash` |
| Xóa flash | `idf.py erase-flash` |
| Monitor | `idf.py monitor` |
| Build + Flash | `idf.py build flash` |
| Build + Flash + Monitor | `idf.py build flash monitor` |
| Xem size | `idf.py size` |
| Xem size component | `idf.py size-components` |

---

# 22. Workflow ESP-IDF cơ bản

## Project mới

Ví dụ sử dụng ESP32-C3:

```bash
cd my_project

idf.py set-target esp32c3

idf.py menuconfig

idf.py build

idf.py -p /dev/ttyUSB0 flash

idf.py -p /dev/ttyUSB0 monitor
```

Hoặc gộp lại:

```bash
idf.py set-target esp32c3
idf.py menuconfig
idf.py -p /dev/ttyUSB0 build flash monitor
```

---

# 23. Khi code bị lỗi build

Có thể thử theo thứ tự:

```bash
idf.py clean
idf.py build
```

Nếu vẫn lỗi:

```bash
idf.py fullclean
idf.py build
```

Nếu nghi ngờ cấu hình:

```bash
idf.py menuconfig
```

Nếu muốn reset hoàn toàn cấu hình:

```bash
rm sdkconfig
idf.py reconfigure
idf.py build
```

---

# 24. Khi ESP chạy firmware cũ

Nếu đã flash firmware mới nhưng ESP có vẻ vẫn chạy firmware cũ:

```bash
idf.py erase-flash
idf.py build flash monitor
```

> Lưu ý: `erase-flash` sẽ xóa **toàn bộ flash**, bao gồm dữ liệu ứng dụng, NVS, cấu hình Wi-Fi và các dữ liệu khác được lưu trên flash.

---

# 25. Một số lệnh nhanh cho ESP32-C3

### Build

```bash
idf.py build
```

### Flash

```bash
idf.py -p /dev/ttyUSB0 flash
```

### Monitor

```bash
idf.py -p /dev/ttyUSB0 monitor
```

### Build + Flash + Monitor

```bash
idf.py -p /dev/ttyUSB0 build flash monitor
```

### Erase + Build + Flash + Monitor

```bash
idf.py -p /dev/ttyUSB0 erase-flash build flash monitor
```

---

# 26. Cheat Sheet

```bash
# Kiểm tra ESP-IDF
idf.py --version

# Chọn target
idf.py set-target esp32c3

# Cấu hình
idf.py menuconfig

# Reconfigure
idf.py reconfigure

# Build
idf.py build

# Clean
idf.py clean

# Full clean
idf.py fullclean

# Flash
idf.py -p /dev/ttyUSB0 flash

# Erase toàn bộ flash
idf.py -p /dev/ttyUSB0 erase-flash

# Monitor
idf.py -p /dev/ttyUSB0 monitor

# Build + Flash
idf.py -p /dev/ttyUSB0 build flash

# Build + Flash + Monitor
idf.py -p /dev/ttyUSB0 build flash monitor

# Xem kích thước firmware
idf.py size
idf.py size-components
idf.py size-files
```

---

## Ghi nhớ

```text
clean
  ↓
Xóa file build trên PC

erase-flash
  ↓
Xóa dữ liệu trong flash của ESP

menuconfig
  ↓
Thay đổi cấu hình project

build
  ↓
Biên dịch source code → firmware

flash
  ↓
Ghi firmware → ESP

monitor
  ↓
Xem UART log từ ESP
```
