# So Sánh Project 3 Wi-Fi Connection

Phạm vi so sánh:

- Bản gốc: device_firmware_original/3_wifi_connection
- Bản đã sửa: device_firmware/3_wifi_connection

Kết luận ngắn:

- Khác biệt nguồn thực sự giữa hai bản rất ít.
- Phần ảnh hưởng trực tiếp đến chạy Wi-Fi chỉ là thay SSID và mật khẩu trong main/app_main.c.
- Các file build cốt lõi như CMakeLists.txt, main/CMakeLists.txt, headers, partitions.csv, sdkconfig.defaults và thư mục components dùng chung không có thay đổi nội dung giữa hai bản.
- File sdkconfig ở bản đã sửa là file sinh ra sau khi chạy cấu hình/build, không phải một chỉnh sửa source thủ công.

## Những thay đổi thực tế

| File                                              | Trạng thái                 | Nội dung thay đổi                                                                                                                                |
| ------------------------------------------------- | -------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------ |
| device_firmware/3_wifi_connection/main/app_main.c | Có thay đổi                | Thay giá trị mặc định YOUR-SSID và YOUR-PASS bằng SSID và mật khẩu thực tế: Five Elders và 15072004.                                             |
| device_firmware/3_wifi_connection/sdkconfig       | Mới xuất hiện ở bản đã sửa | Đây là file cấu hình do ESP-IDF sinh ra sau khi build/configure. Nội dung chính cho project này là target esp32c3 và bật custom partition table. |

### Chi tiết ở main/app_main.c

Trong bản gốc, Wi-Fi được khai báo bằng placeholder:

- LIGHT_ESP_WIFI_SSID = YOUR-SSID
- LIGHT_ESP_WIFI_PASS = YOUR-PASS

Trong bản đã sửa, hai giá trị này được thay bằng thông tin mạng thật.

Lý do phải đổi:

- Nếu giữ placeholder thì firmware vẫn biên dịch được, nhưng lúc chạy sẽ không thể kết nối vào AP thực.
- Đây là điều kiện vận hành chứ không phải điều kiện biên dịch. Nói cách khác, sửa này cần để thiết bị hoạt động đúng, không phải để CMake hoặc compiler chấp nhận mã nguồn.

## Những file không đổi nhưng đã kiểm tra

Các file sau có nội dung giống nhau giữa hai bản:

- device_firmware_original/3_wifi_connection/CMakeLists.txt và device_firmware/3_wifi_connection/CMakeLists.txt
- device_firmware_original/3_wifi_connection/main/CMakeLists.txt và device_firmware/3_wifi_connection/main/CMakeLists.txt
- device_firmware_original/3_wifi_connection/main/app_driver.c và device_firmware/3_wifi_connection/main/app_driver.c
- device_firmware_original/3_wifi_connection/main/include/app_priv.h và device_firmware/3_wifi_connection/main/include/app_priv.h
- device_firmware_original/3_wifi_connection/main/include/board_esp32c3_devkitc.h và device_firmware/3_wifi_connection/main/include/board_esp32c3_devkitc.h
- device_firmware_original/3_wifi_connection/partitions.csv và device_firmware/3_wifi_connection/partitions.csv
- device_firmware_original/3_wifi_connection/sdkconfig.defaults và device_firmware/3_wifi_connection/sdkconfig.defaults

Ý nghĩa của việc không đổi này:

- Project đã có sẵn cấu trúc ESP-IDF đúng chuẩn: top-level CMake gọi project.cmake, còn component main đăng ký source và include directory đúng cách.
- main/app_driver.c vẫn dùng các component chung như light_driver và iot_button, nên không phát sinh yêu cầu sửa CMake ở project 3.
- partitions.csv vẫn giữ cấu hình OTA hai slot, NVS, PHY init và partition factory, nên không cần chỉnh lại layout flash cho project 3.

## Phần components liên quan đến project 3

Trong device_firmware/components, project 3 đang dùng các component dùng chung sau:

- app_storage
- button
- light_driver

Các component này được project lấy qua EXTRA_COMPONENT_DIRS trong CMake cấp project. Nội dung CMake đó cũng không thay đổi giữa hai bản, nên không có sửa build nào ở khu vực này.

Điểm cần lưu ý:

- app_wifi có tồn tại trong thư mục components dùng chung, nhưng project 3 không thấy gọi trực tiếp trong main/app_main.c hoặc main/app_driver.c.
- Vì vậy, nếu chỉ xét project 3, app_wifi không phải phần tạo ra khác biệt giữa hai cây thư mục này.

## Vì sao các file build không cần đổi

ESP-IDF build system vốn đã hỗ trợ đúng kiểu project này:

- CMakeLists.txt cấp project dùng EXTRA_COMPONENT_DIRS để CMake tìm component ở thư mục ngoài main.
- Component CMake dùng idf_component_register để khai báo source và include path.
- Tài liệu ESP-IDF mô tả rõ project có thể dùng thư mục components hoặc EXTRA_COMPONENT_DIRS, và file sdkconfig là file cấu hình được tạo hoặc điều chỉnh bởi build system.

Nói cách khác:

- Không có dấu hiệu cho thấy project 3 cần sửa CMakeLists.txt, headers hoặc partitions.csv để build được.
- Bản đã sửa chủ yếu là thay thông tin Wi-Fi thực tế để firmware chạy đúng sau khi build.

## Dẫn Chứng Tài Liệu

- ESP-IDF Build System: mô tả project có thể dùng thư mục components, main, và EXTRA_COMPONENT_DIRS; đồng thời sdkconfig là file cấu hình project do build system quản lý.
  - https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-guides/build-system.html
- ESP-IDF Build System: phần Project CMakeLists File và Component CMakeLists Files, giải thích project.cmake, EXTRA_COMPONENT_DIRS, và idf_component_register.
  - https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-guides/build-system.html#project-cmakelists-file
  - https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-guides/build-system.html#component-cmakelists-files
- ESP-IDF Partition Tables: giải thích custom partition table CSV, offset 0x8000, các partition nvs, phy, ota, và cách build system kiểm tra kích thước app.
  - https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-guides/partition-tables.html

## Tóm tắt cuối

Nếu chỉ xét project 3, thay đổi giữa hai bản là:

- Có: đổi SSID/mật khẩu trong main/app_main.c.
- Có: thêm file sdkconfig do build sinh ra ở bản đã sửa.
- Không có: thay đổi ở CMakeLists.txt, headers, partitions.csv, sdkconfig.defaults, và các component dùng chung.

Vì vậy, nguyên nhân chính để firmware dùng được thực tế là cung cấp thông tin Wi-Fi thật; còn phần build đã được cấu trúc sẵn đúng chuẩn ESP-IDF từ trước.
