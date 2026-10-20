# Project 6 - Performance Optimize

## Mục đích của project 6

Project 6 là ví dụ tối ưu hiệu năng cho một thiết bị ESP32-C3 dùng ESP-RainMaker. Mục tiêu chính của bài này là làm cho thiết bị:

1. Khởi động nhanh hơn.
2. Firmware nhỏ gọn hơn để vừa với flash của board.
3. Vẫn hỗ trợ các tính năng của RainMaker như cấu hình mạng, điều khiển Light và OTA.
4. Giữ được tối ưu công suất trong chế độ Light off.

Theo mô tả gốc trong README của ví dụ, project này được dùng trong chương về quản lý nguồn và tối ưu low-power, và có các nội dung tối ưu như rút ngắn thời gian khởi động, giảm kích thước firmware và giảm công suất tiêu thụ. Xem [README gốc của project 6](device_firmware_original/6_project_optimize/README_cn.md) và [README bản sửa](device_firmware/6_project_optimize/README_cn.md).

## Kết luận nhanh

Sau khi đối chiếu `device_firmware_original/6_project_optimize` với `device_firmware/6_project_optimize`, phần code nguồn trong `main/*.c` và `main/include/*.h` gần như không đổi. Những thay đổi làm cho project build được nằm chủ yếu ở:

1. `CMakeLists.txt` cấp project.
2. `partitions.csv`.
3. Các artefact sinh ra bởi build như `managed_components/`, `dependencies.lock`, `sdkconfig`, `build/` và `sec_cert.bin`.

Binary build hiện tại của project khoảng 1.3 MB, nên partition cũ 960 KB không còn đủ chỗ cho app.

## So sánh chi tiết

| Nhóm file                                                         | Trạng thái so với bản gốc | Ý nghĩa thực tế                                                                   |
| ----------------------------------------------------------------- | ------------------------- | --------------------------------------------------------------------------------- |
| `main/app_main.c`, `main/app_driver.c`, `main/app_pm.c`           | Không thay đổi            | Logic ứng dụng chính giữ nguyên, không phải chỗ cần sửa để build thành công.      |
| `main/include/app_priv.h`, `main/include/board_esp32c3_devkitc.h` | Không thay đổi            | Header của app và mapping phần cứng vẫn như bản gốc.                              |
| `main/CMakeLists.txt`                                             | Không thay đổi            | Cách nhúng `server.crt`, khai báo source và macro `DEVELOPMENT_BOARD` giữ nguyên. |
| `CMakeLists.txt` cấp project                                      | Có thay đổi               | Đây là chỗ quan trọng nhất cho build system và component resolution.              |
| `partitions.csv`                                                  | Có thay đổi               | Đổi layout flash để app lớn hơn vẫn fit partition.                                |
| `managed_components/`, `dependencies.lock`                        | Mới sinh ra ở bản sửa     | Do IDF Component Manager tải và khóa version các component phụ thuộc.             |
| `build/`, `sdkconfig`, `sdkconfig.old`, `sec_cert.bin`            | Mới sinh ra ở bản sửa     | Đây là artefact build, không phải code nguồn cần sửa tay.                         |

## Những sửa đổi cụ thể và lý do

### 1. `CMakeLists.txt` cấp project

File gốc của project 6 đã khai báo `EXTRA_COMPONENT_DIRS` khá rộng, bao gồm cả các component RainMaker phụ trợ như `esp_schedule`, `json_generator`, `json_parser`, `qrcode`, `rmaker_common`. Ở bản sửa, phần này được rút gọn còn các component cục bộ và `esp_rainmaker` chính, còn các phần phụ trợ được để cho cơ chế quản lý dependency của ESP-IDF tự resolve qua `managed_components`.

Điều này phù hợp với tài liệu ESP-IDF Build System:

1. ESP-IDF sẽ tìm component trong `IDF_PATH/components`, `PROJECT_DIR/components` và `EXTRA_COMPONENT_DIRS`.
2. Khi Component Manager được bật, nó sẽ resolve dependency, tải các managed components và tạo `dependencies.lock`.
3. Thư mục `managed_components/` là nơi lưu các component đã được Component Manager tải về, không nên sửa tay.

Xem tài liệu chính thức: [ESP-IDF Build System](https://docs.espressif.com/projects/esp-idf/en/v6.0/esp32/api-guides/build-system.html).

Tóm lại, thay đổi này làm cấu hình build gọn hơn và bền hơn: chỉ cần khai báo component gốc `esp_rainmaker`, còn các component phụ thuộc sẽ được chốt version trong `dependencies.lock`.

### 2. `partitions.csv`

Partition table là thay đổi quan trọng thứ hai. Bản gốc dùng layout nhỏ hơn, với hai OTA app partition kích thước `960K` và các offset để trống, cho phép tool tự tính toán. Bản sửa đổi sang layout có offset cụ thể và tăng kích thước hai OTA partition lên `0x180000` mỗi slot.

Lý do phải đổi là vì app hiện tại đã lớn hơn partition cũ. Binary build hiện tại của project khoảng `1.3 MB`, trong khi slot cũ chỉ `960 KB`. ESP-IDF cũng có cơ chế kiểm tra kích thước binary so với partition app và sẽ báo lỗi nếu app không fit vào bất kỳ app partition nào.

Tài liệu tham chiếu:

1. [ESP-IDF Partition Tables](https://docs.espressif.com/projects/esp-idf/en/v6.0/esp32/api-guides/partition-tables.html)
2. Phần "Partition Size Checks" trong cùng tài liệu, mô tả việc build sẽ fail nếu app binary quá lớn.

Layout mới cũng giữ lại `sec_cert`, `nvs`, `phy_init` và `fctry`, tức là vẫn đảm bảo:

1. OTA vẫn hoạt động.
2. Chứng chỉ bảo mật có chỗ lưu riêng.
3. NVS và PHY data vẫn có vùng riêng, đúng với yêu cầu của RainMaker và Wi-Fi stack.

### 3. `main/*.c` và `main/include/*.h`

Không có sửa đổi giữa hai bản ở các file này.

Điều này quan trọng vì nó cho thấy vấn đề build không nằm ở logic ứng dụng. Ứng dụng vẫn dùng cùng một luồng:

1. `app_main.c` khởi tạo app.
2. `app_driver.c` xử lý driver/Light.
3. `app_pm.c` xử lý tối ưu power management.
4. `app_priv.h` và `board_esp32c3_devkitc.h` vẫn là header nội bộ và mapping phần cứng cũ.

### 4. `main/CMakeLists.txt`

Không có thay đổi giữa hai bản.

File này vẫn chỉ:

1. Khai báo các source `app_main.c`, `app_pm.c`, `app_driver.c`.
2. Thêm `server.crt` bằng `target_add_binary_data`.
3. Truyền macro `DEVELOPMENT_BOARD` vào build.

Nghĩa là không cần sửa gì ở `main` để làm project build được, vì phần quyết định vẫn nằm ở project-level CMake và partition table.

### 5. `managed_components/` và `dependencies.lock`

Đây là phần mới xuất hiện ở bản sửa và là dấu hiệu cho thấy project đang dùng IDF Component Manager.

`dependencies.lock` ghi lại version chính xác của các managed components đang được dùng trong project. Đây là file do ESP-IDF sinh ra tự động, không nên chỉnh tay.

Các direct dependencies quan trọng trong bản build hiện tại gồm:

1. `espressif/cbor`
2. `espressif/esp_schedule`
3. `espressif/esp_secure_cert_mgr`
4. `espressif/json_generator`
5. `espressif/json_parser`
6. `espressif/mdns`
7. `espressif/network_provisioning`
8. `espressif/qrcode`
9. `espressif/rmaker_common`

Ngoài ra còn có các dependency kéo theo như `cjson`, `esp_daylight`, `jsmn`, `mqtt`, `rmaker_cmd_resp`, `rmaker_common_events`, `rmaker_console`, `rmaker_system_ctrl`, `rmaker_time_sync`, `rmaker_work_queue`.

Tài liệu tham chiếu:

1. [ESP-IDF Build System](https://docs.espressif.com/projects/esp-idf/en/v6.0/esp32/api-guides/build-system.html)
2. Mục mô tả `managed_components` và `dependencies.lock` trong trang này.

### 6. `components/`

Không có component nguồn riêng mới trong thư mục project 6. Project vẫn dùng các component dùng chung của workspace như `app_storage`, `app_wifi`, `button`, `light_driver` thông qua cơ chế component path của ESP-IDF.

Ý nghĩa của cách làm này là code dùng chung được tách khỏi từng project, giảm lặp lại và giúp các project RainMaker khác reuse lại cùng bộ driver/UI/storage.

## Vì sao phải đổi để build được

Ngắn gọn thì có hai lý do chính:

1. ESP-IDF cần thấy đúng component graph để link được RainMaker và các dependency của nó. Bản sửa tận dụng cơ chế `EXTRA_COMPONENT_DIRS` và `managed_components` thay vì hardcode toàn bộ dependency phụ trợ.
2. Firmware thực tế lớn hơn partition cũ. Với binary hiện tại khoảng 1.3 MB, hai slot OTA 960 KB không thể chứa app, nên phải nới `partitions.csv` để build và flash hợp lệ.

## Tài liệu tham khảo

1. [ESP-IDF Build System](https://docs.espressif.com/projects/esp-idf/en/v6.0/esp32/api-guides/build-system.html)
2. [ESP-IDF Partition Tables](https://docs.espressif.com/projects/esp-idf/en/v6.0/esp32/api-guides/partition-tables.html)
3. [README gốc project 6](device_firmware_original/6_project_optimize/README_cn.md)
4. [README bản sửa project 6](device_firmware/6_project_optimize/README_cn.md)
