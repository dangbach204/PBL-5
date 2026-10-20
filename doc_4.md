# Project 4: Network Config

Mục đích của project 4 là minh hoạ cách cấu hình Wi-Fi cho thiết bị ESP32-C3 bằng provisioning, đồng thời vẫn giữ các chức năng nền của bài mẫu: in log `Hello World` liên tục, điều khiển đèn bằng nút vật lý, và cho phép người dùng provision thiết bị qua `ESP BLE Provisioning` hoặc `ESP SoftAP Provisioning`.

Tài liệu gốc của project cũng mô tả đúng mục tiêu này: đây là ví dụ cho phần “thiết bị cấu hình Wi-Fi network” trong sách, với các chức năng chính là `Hello World`, điều khiển đèn, và cấu hình kết nối router qua app provisioning.

## Tóm tắt thay đổi để build được

So với bản gốc trong `device_firmware_original/4_network_config`, bản đã sửa trong `device_firmware/4_network_config` có các thay đổi quan trọng sau:

| Khu vực                                                                                | Bản gốc                                                                                                                           | Bản đã sửa                                                                                                                                                                                                                           | Vì sao phải đổi                                                                                                                                                                                                                                                                                                                 |
| -------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `CMakeLists.txt` ở root project                                                        | Thêm `qrcode` từ `IDF_PATH/examples/common_components/qrcode` vào `EXTRA_COMPONENT_DIRS`                                          | Bỏ đường dẫn `qrcode` khỏi `EXTRA_COMPONENT_DIRS`                                                                                                                                                                                    | QR code không còn được kéo bằng đường dẫn ví dụ nội bộ nữa mà được khai báo như dependency của component bằng Component Manager. Cách này rõ ràng hơn, có version pin và tự kéo dependency con. Theo ESP-IDF Build System, component manager sẽ resolve dependency từ manifest và tạo `dependencies.lock`/`managed_components`. |
| `main/idf_component.yml`                                                               | Không có                                                                                                                          | Thêm `espressif/network_provisioning` và `espressif/qrcode`                                                                                                                                                                          | Đây là phần bắt buộc để CMake biết cần tải và link các component provisioning mới. `network_provisioning` cung cấp API provision Wi-Fi/Thread qua protocomm; `qrcode` cung cấp bộ tạo QR code dùng trong provisioning flow.                                                                                                     |
| `main/app_main.c`                                                                      | Dùng header và API kiểu cũ: `wifi_provisioning/manager.h`, `wifi_provisioning/scheme_ble.h`, `wifi_prov_mgr_*`, `WIFI_PROV_EVENT` | Đổi sang `network_provisioning/manager.h`, `network_provisioning/scheme_ble.h`, `network_prov_mgr_*`, `NETWORK_PROV_EVENT`                                                                                                           | Header cũ không khớp với component mới nên sẽ fail ở bước compile/link. API mới đúng với component `network_provisioning` trong ESP-IDF hiện tại.                                                                                                                                                                               |
| `main/app_main.c`                                                                      | Không có manifest component manager                                                                                               | Có `wifi_prov_mgr_initialize()` dùng `network_prov_mgr_init`, `network_prov_mgr_is_wifi_provisioned`, `network_prov_mgr_start_provisioning`, `network_prov_mgr_endpoint_create/register`, và in QR code bằng `esp_qrcode_generate()` | Đây là luồng provisioning mới: chọn BLE scheme, tạo endpoint custom, in QR cho app quét, rồi deinit provisioning manager khi đã provision xong.                                                                                                                                                                                 |
| `partitions.csv`                                                                       | Giống nhau                                                                                                                        | Không đổi                                                                                                                                                                                                                            | Partition table hiện tại đã đủ cho NVS, OTA, PHY init và factory data. Không cần sửa để build project 4.                                                                                                                                                                                                                        |
| `main/app_driver.c`, `main/include/app_priv.h`, `main/include/board_esp32c3_devkitc.h` | Giống nhau                                                                                                                        | Không đổi                                                                                                                                                                                                                            | Các file này vẫn phục vụ phần điều khiển đèn và nút bấm thông qua shared components, không ảnh hưởng tới provisioning flow.                                                                                                                                                                                                     |
| `managed_components/`                                                                  | Không có                                                                                                                          | Có `espressif__network_provisioning`, `espressif__qrcode`, `espressif__cjson`                                                                                                                                                        | Đây là thư mục sinh ra khi Component Manager resolve manifest. `cjson` là dependency kéo theo của `network_provisioning`.                                                                                                                                                                                                       |
| `dependencies.lock`                                                                    | Không có                                                                                                                          | Có                                                                                                                                                                                                                                   | File này khóa chính xác version dependency đã resolve. Tài liệu ESP-IDF ghi rõ file này được tạo tự động và không nên chỉnh tay.                                                                                                                                                                                                |

## Chi tiết từng sửa đổi

### 1) Đổi nguồn cung cấp QR code component

Bản gốc thêm QR code component bằng cách trỏ trực tiếp tới `IDF_PATH/examples/common_components/qrcode` trong `EXTRA_COMPONENT_DIRS`. Bản đã sửa bỏ đường dẫn này và chuyển sang khai báo dependency trong `main/idf_component.yml`.

Lý do:

- ESP-IDF Component Manager khuyến nghị khai báo dependency trong manifest `idf_component.yml`.
- Khi CMake configure project, component manager sẽ tải dependency, tạo `dependencies.lock`, và đặt component trong `managed_components`.
- `qrcode` là component chính thức của Espressif, dùng để sinh QR cho flow provisioning.

Tài liệu tham chiếu:

- [IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html)
- [QR Code generator component](https://github.com/espressif/idf-extra-components/tree/master/qrcode)

### 2) Đổi provisioning API từ `wifi_provisioning` sang `network_provisioning`

Trong `main/app_main.c`, toàn bộ include và tên hàm provisioning được đổi theo API mới:

- `#include "wifi_provisioning/manager.h"` -> `#include "network_provisioning/manager.h"`
- `#include "wifi_provisioning/scheme_ble.h"` -> `#include "network_provisioning/scheme_ble.h"`
- `wifi_prov_mgr_*` -> `network_prov_mgr_*`
- `WIFI_PROV_EVENT` -> `NETWORK_PROV_EVENT`

Lý do:

- Unified Provisioning của ESP-IDF là framework chính thức để cấu hình Wi-Fi/Thread qua protocomm.
- Tài liệu chính thức mô tả rõ framework này hỗ trợ provisioning qua Wi-Fi (SoftAP) hoặc Bluetooth LE, hỗ trợ security scheme, custom endpoint, và example code nằm trong component `network_provisioning`.
- Nếu giữ include cũ thì build sẽ không tìm thấy header và symbol tương ứng trong cấu hình hiện tại.

Tài liệu tham chiếu:

- [Provisioning API](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/provisioning/index.html)
- [Unified Provisioning](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/provisioning/provisioning.html)
- [Network Provisioning component](https://github.com/espressif/idf-extra-components/tree/master/network_provisioning)

### 3) Giữ QR flow và custom endpoint để phục vụ provisioning thực tế

Phần provisioning trong `app_main.c` vẫn giữ logic:

- khởi tạo provisioning manager với BLE scheme
- dùng security level 1 và PoP `abcd1234`
- set custom BLE service UUID
- tạo endpoint `custom-data`
- start provisioning
- register handler cho endpoint custom
- in QR code để app provisioning quét được

Điểm khác biệt là toàn bộ flow này chạy trên component mới, nên việc thêm dependency là bắt buộc để link được `network_provisioning` và `qrcode`.

Tài liệu tham chiếu:

- [Unified Provisioning - Overview](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/provisioning/provisioning.html)
- [Network Provisioning README](https://github.com/espressif/idf-extra-components/tree/master/network_provisioning)

### 4) `partitions.csv` không đổi

File `partitions.csv` giữa hai bản không thay đổi. Điều này hợp lý vì provisioning flow chỉ cần NVS để lưu trạng thái Wi-Fi/provisioning, còn layout OTA, PHY init và factory partition đã có sẵn.

Kết luận: không cần sửa partition table để build được project 4.

## Các file không đổi nhưng vẫn liên quan

- `main/CMakeLists.txt` không đổi. Component `main` vẫn đăng ký `app_main.c` và `app_driver.c` như cũ.
- `main/app_driver.c`, `main/include/app_priv.h`, `main/include/board_esp32c3_devkitc.h` không đổi.
- `device_firmware/components/` dùng chung cho các project trong bộ sách vẫn giữ nguyên; project 4 vẫn phụ thuộc vào `light_driver`, `button`, và `app_storage` qua `EXTRA_COMPONENT_DIRS`.

## Vì sao bản sửa mới build được

1. Header và symbol provisioning khớp với component thật sự được build: `network_provisioning`.
2. QR code được lấy từ dependency chính thức của Component Manager thay vì phụ thuộc vào đường dẫn ví dụ nội bộ.
3. `dependencies.lock` khóa version đã resolve, giúp build lặp lại được và tránh lệch version giữa máy dev khác nhau.
4. `managed_components` được sinh tự động bởi ESP-IDF, nên không phải tự chép source component vào project.

## Artefact sinh ra khi cấu hình/build

- `dependencies.lock`: file khoá dependency, không chỉnh tay.
- `sdkconfig`: cấu hình build hiện tại.
- `sdkconfig.old`: bản sao cấu hình trước đó.
- `managed_components/`: nơi lưu component do Component Manager tải về.

## Tài liệu tham chiếu ngắn

- [ESP-IDF Build System](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html)
- [IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html)
- [Provisioning API](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/provisioning/index.html)
- [Unified Provisioning](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/provisioning/provisioning.html)
