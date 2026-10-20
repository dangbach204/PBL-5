# Mục đích project 5: RainMaker

Project 5 là mẫu RainMaker dùng để điều khiển một thiết bị đèn từ xa bằng ESP RainMaker App. Mục tiêu chính của project là cho phép:

1. Provision Wi-Fi cho ESP32-C3.
2. Điều khiển trạng thái đèn bằng nút vật lý và bằng cloud.
3. Hỗ trợ OTA firmware update.

Đây là sample trong phần “điều khiển thiết bị trên cloud” của cuốn sách, nên phần lõi của bài toán không nằm ở giao diện mà ở việc ghép đúng RainMaker SDK, provisioning stack, OTA và partition layout để build được trên ESP-IDF hiện tại.

## So sánh bản gốc và bản đã sửa

### 1) Phần `main/` hầu như không đổi

Mình không thấy khác biệt logic trong:

- [device_firmware/5_rainmaker/main/app_main.c](device_firmware/5_rainmaker/main/app_main.c)
- [device_firmware/5_rainmaker/main/app_driver.c](device_firmware/5_rainmaker/main/app_driver.c)
- [device_firmware/5_rainmaker/main/CMakeLists.txt](device_firmware/5_rainmaker/main/CMakeLists.txt)
- [device_firmware/5_rainmaker/main/include/app_priv.h](device_firmware/5_rainmaker/main/include/app_priv.h)
- [device_firmware/5_rainmaker/main/include/board_esp32c3_devkitc.h](device_firmware/5_rainmaker/main/include/board_esp32c3_devkitc.h)
- [device_firmware/5_rainmaker/main/server.crt](device_firmware/5_rainmaker/main/server.crt)

Kết luận: phần xử lý RainMaker trong `main` giữ nguyên, nên các sửa đổi để build được chủ yếu nằm ở dependency, component API và partition table.

`sdkconfig.defaults` cũng không đổi. Các file `sdkconfig` và `sdkconfig.old` chỉ xuất hiện ở bản sửa vì là file cấu hình sinh ra sau khi cấu hình/build.

### 2) Root `CMakeLists.txt` chuyển từ trỏ tay sang dependency được khóa

Trong bản gốc, [device_firmware_original/5_rainmaker/CMakeLists.txt](device_firmware_original/5_rainmaker/CMakeLists.txt) còn khai báo thủ công các component RainMaker phụ trợ qua `EXTRA_COMPONENT_DIRS`. Ở bản sửa, [device_firmware/5_rainmaker/CMakeLists.txt](device_firmware/5_rainmaker/CMakeLists.txt) đã rút gọn lại, chỉ giữ `esp_rainmaker`.

Ý nghĩa của thay đổi này:

- Trước đây project phụ thuộc trực tiếp vào cây source `esp-rainmaker` ngoài repo.
- Bản sửa chuyển các phần còn lại sang cơ chế component manager và khóa version bằng [device_firmware/5_rainmaker/dependencies.lock](device_firmware/5_rainmaker/dependencies.lock).
- Cách này giảm phụ thuộc vào việc clone đúng cấu trúc thư mục của repo ngoài và làm build ổn định hơn.

### 3) Gói/phụ thuộc RainMaker được khóa rõ ràng hơn

File [device_firmware/5_rainmaker/dependencies.lock](device_firmware/5_rainmaker/dependencies.lock) cho thấy project đang pin các package như:

- `espressif/network_provisioning`
- `espressif/qrcode`
- `espressif/esp_schedule`
- `espressif/json_generator`
- `espressif/json_parser`
- `espressif/rmaker_common`
- `espressif/esp_secure_cert_mgr`
- `espressif/mdns`
- `espressif/cbor`
- `espressif/cjson`
- `espressif/mqtt`
- `idf 6.0.2`

Điểm quan trọng là `dependencies.lock` giúp chốt đúng bộ version đang build được, tránh lỗi lệch API giữa RainMaker, ESP-IDF và component manager.

### 4) Các thay đổi trong `components/` để khớp API mới và build được

#### `components/app_wifi`

- [device_firmware/components/app_wifi/CMakeLists.txt](device_firmware/components/app_wifi/CMakeLists.txt) đổi dependency từ `wifi_provisioning` sang `network_provisioning`, và thêm `nvs_flash`.
- [device_firmware/components/app_wifi/app_wifi.c](device_firmware/components/app_wifi/app_wifi.c) chuyển sang các API `network_prov_mgr_*`, `network_prov_scheme_ble`, `network_prov_scheme_softap`, đồng thời dùng `esp_netif` và `nvs_flash`.
- [device_firmware/components/app_wifi/idf_component.yml](device_firmware/components/app_wifi/idf_component.yml) khai báo `espressif/network_provisioning` và `espressif/qrcode` để component manager kéo đúng package.

Lý do phải đổi: README của project yêu cầu riêng repo RainMaker và `RAIMAKER_PATH` để lấy các thành phần liên quan, còn RainMaker docs mô tả firmware SDK của RainMaker là bộ SDK dựa trên ESP-IDF, có provisioning, cloud connectivity và OTA. Với bộ API mới, `network_provisioning` là đường build phù hợp hơn so với cách ghép provisioning cũ.

#### `components/button`

- [device_firmware/components/button/CMakeLists.txt](device_firmware/components/button/CMakeLists.txt) đổi `PRIV_REQUIRES` từ `esp_adc_cal` sang `esp_adc`, `esp_driver_gpio`, `esp_timer`.
- [device_firmware/components/button/include/button_adc.h](device_firmware/components/button/include/button_adc.h) đổi từ `driver/adc.h` sang `esp_adc/adc_oneshot.h`, và chuyển kiểu `adc1_channel_t` sang `adc_channel_t`.
- [device_firmware/components/button/button_adc.c](device_firmware/components/button/button_adc.c) cũng chuyển sang ADC oneshot API và loại bỏ các đoạn dùng `esp_adc_cal` cũ.

Lý do phải đổi: API ADC legacy không còn là lựa chọn tốt cho ESP-IDF mới. Dùng `adc_oneshot` và các driver `esp_driver_*` giúp component này biên dịch ổn trên bộ SDK hiện tại.

#### `components/light_driver`

- [device_firmware/components/light_driver/CMakeLists.txt](device_firmware/components/light_driver/CMakeLists.txt) thêm `esp_driver_ledc`, `esp_driver_gpio`, `esp_driver_gptimer`.
- [device_firmware/components/light_driver/light_driver.c](device_firmware/components/light_driver/light_driver.c) đổi callback timer từ `void *` sang `TimerHandle_t`.

Lý do phải đổi: light driver phụ thuộc LEDC và timer của FreeRTOS/driver mới, nên cần khai báo đúng driver để link được và tránh lỗi kiểu hàm callback khi build.

### 5) `partitions.csv` được chỉnh để OTA image có đủ chỗ

Trong bản gốc, [device_firmware_original/5_rainmaker/partitions.csv](device_firmware_original/5_rainmaker/partitions.csv) dùng kích thước OTA là `1600K`. Bản sửa ở [device_firmware/5_rainmaker/partitions.csv](device_firmware/5_rainmaker/partitions.csv) đã đổi thành `1728K`, đồng thời dời `fctry` từ `0x340000` lên `0x380000`.

Các điểm còn giữ lại:

- `sec_cert` vẫn tồn tại ở `0xd000`
- `nvs` và `otadata` vẫn giữ cấu trúc cho provisioning/OTA
- Flash vẫn là 4 MB theo `sdkconfig`

Vì sao phải đổi: project này vẫn là mẫu RainMaker có OTA, nhưng bộ component và SDK mới làm image nặng hơn. Tăng kích thước OTA slot giúp binary mới fit trong flash mà không phải bỏ OTA.

### 6) Những gì không đổi trong project 5

- `main` không thay đổi logic.
- `main` headers không thay đổi.
- `sdkconfig.defaults` không thay đổi.
- `server.crt` không thay đổi.
- `partitions.csv` là file đã đổi, còn `sdkconfig`/`sdkconfig.old` chỉ là file sinh ra sau cấu hình/build.

## Tài liệu đối chiếu

- [README_cn.md](device_firmware/5_rainmaker/README_cn.md) ghi rõ project RainMaker cần clone repo `esp-rainmaker` riêng và set `RAIMAKER_PATH` trước khi build.
- [RainMaker docs](https://rainmaker.espressif.com/docs/) mô tả RainMaker là một bộ IoT stack đầy đủ với firmware SDK, provisioning, cloud backend và OTA.
- [Get Started](https://rainmaker.espressif.com/docs/get-started.html) và [OTA docs](https://rainmaker.espressif.com/docs/ota.html) là hai tài liệu liên quan trực tiếp đến provisioning và firmware upgrade của project này.
