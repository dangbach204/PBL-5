# Đối chiếu `2_light_drivers`: bản gốc và bản đã sửa

## Kết luận

Mục tiêu của các sửa đổi là đưa project cũ lên **ESP-IDF v6.0.2** (dấu vết
trong `device_firmware/2_light_drivers/build/2_light_drivers.bin`), không thay
đổi luồng nghiệp vụ của ứng dụng. Nguyên nhân build chính là ESP-IDF 6 đã bỏ
Timer Group driver cũ (`driver/timer.h`) và đã tách các peripheral driver thành
những component riêng. Hai phần cần port là `components/light_driver` và
`components/button`.

Đã xác thực lại bằng:

```sh
cd device_firmware/2_light_drivers
cmake --build build
```

Build thành công. Firmware là ảnh ESP32-C3, kích thước `0x359f0`; partition app
nhỏ nhất là `0x190000`, còn trống `0x15a610` (87%). Lưu ý đây là build lại trên
cây `build/` đã được cấu hình sẵn với ESP-IDF v6.0.2; không phải build từ một
thư mục build rỗng.

## Phạm vi đã so sánh

So sánh từng file giữa:

- Gốc: `device_firmware_original/2_light_drivers` và các component mà project
  khai báo qua `EXTRA_COMPONENT_DIRS`.
- Đã sửa: `device_firmware/2_light_drivers` và `device_firmware/components`.

Ba component được project 2 dùng là `light_driver`, `button`, `app_storage`.
`app_storage` hoàn toàn không đổi. `app_wifi` có thay đổi trong repository
nhưng không nằm trong `EXTRA_COMPONENT_DIRS` của project 2, nên không liên quan
đến build này.

## Các file project cấp ứng dụng

| Hạng mục | Kết quả đối chiếu | Ý nghĩa với build |
|---|---|---|
| `2_light_drivers/CMakeLists.txt` | Không đổi | Vẫn đăng ký ba component cục bộ: `light_driver`, `button`, `app_storage`. |
| `main/CMakeLists.txt`, `main/component.mk` | Không đổi | Danh sách source/dependency của `main` giữ nguyên. `component.mk` là dấu vết build-system cũ, nhưng không được thay đổi trong bản port. |
| `main/app_main.c`, `main/app_driver.c` | Không đổi | Logic khởi tạo storage, LED và nút GPIO không đổi. Không có thay đổi trong `main` để “chữa” lỗi API. |
| `main/include/app_priv.h`, `main/include/board_esp32c3_devkitc.h` | Không đổi | Các prototype và mapping phần cứng (nút GPIO 9) giữ nguyên. |
| `sdkconfig.defaults` | Không đổi | Vẫn chọn `esp32c3`; không cần đổi Kconfig để port hai driver. |
| `partitions.csv` | Không đổi | Vẫn gồm `sec_cert`, `nvs`, `otadata`, `phy_init`, hai OTA app 1600 KiB và `fctry`; không phải nguyên nhân/lời giải cho build. |

Trong thư mục đã sửa có thêm `sdkconfig` (file cấu hình đã được generate),
`.clangd` và `.vscode/settings.json`. Đây là metadata cấu hình IDE/build cục bộ,
không phải sửa đổi mã nguồn hoặc package dependency. Không có
`idf_component.yml`, `dependencies.lock` hay `requirements*.txt` riêng cho
project 2, tức là **không có package ESP Component Manager/Python nào được cài
thêm riêng cho project này**.

## Thay đổi bắt buộc trong `components/light_driver`

### 1. Chuyển Timer Group legacy sang GPTimer

`components/light_driver/iot_led.c` là sửa đổi quan trọng nhất (528 dòng ở bản
gốc, 848 dòng ở bản sửa; một phần tăng thêm là format/comment). Các thay đổi
chức năng là:

- Bỏ `soc/timer_group_struct.h`, `driver/timer.h`, con trỏ thanh ghi `TIMERG0`
  / `TIMERG1`, cùng các API cũ `timer_init`, `timer_start`, `timer_set_*`,
  `timer_enable_intr`, `timer_isr_register`.
- Thay cấu trúc `{ timer_group_t, timer_idx_t }` bằng
  `gptimer_handle_t`.
- Tạo timer bằng `gptimer_new_timer()` với clock APB, đếm tăng và độ phân giải
  1 MHz; đặt alarm `DUTY_SET_CYCLE * 1000` tick và
  `auto_reload_on_alarm` để tạo tick fade tuần hoàn.
- Đăng ký `fade_timercb` qua `gptimer_register_event_callbacks()`, rồi
  enable/start/stop bằng `gptimer_enable`, `gptimer_start`, `gptimer_stop`.
  Callback mới có đúng chữ ký GPTimer và không còn tự đọc/xóa/cấp lại interrupt
  của thanh ghi TIMERG.
- Khi deinit, timer được stop (nếu đang chạy), disable và
  `gptimer_del_timer()`; vì GPTimer là handle được cấp phát động.

Vì sao cần đổi: hướng dẫn migration chính thức cho ESP32-C3 nêu rằng
`driver/timer.h` đã deprecated từ v5.0 và **bị remove hoàn toàn từ v6.0**; driver
thay thế ở component `esp_driver_gptimer`, header `driver/gptimer.h`.
[Tài liệu migration ESP-IDF 6.0](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/migration-guides/release-6.x/6.0/peripherals.html).
API GPTimer chính thức cũng yêu cầu đăng ký callback trước `gptimer_enable()`;
thứ tự trong mã sửa tuân thủ điều đó và auto-reload tạo alarm tuần hoàn.
[Tài liệu GPTimer](https://docs.espressif.com/projects/esp-idf/en/v5.3/esp32/api-reference/peripherals/gptimer.html).

### 2. Không đọc register LEDC để suy ra tần số fade

Bản gốc đọc trực tiếp thanh ghi LEDC (`tick_sel`, `duty_resolution`,
`clock_divider`) để tính `freq`. Bản sửa lưu `freq_hz` trong `iot_light_t` sau
khi gọi `ledc_get_freq()` và dùng giá trị đó trong `_iot_set_fade_with_time()`.
Điều này tránh phụ thuộc vào layout thanh ghi nội bộ và vẫn giữ công thức số
cycle fade. Đây là thay đổi tương thích/ổn định, không chỉ là format.

### 3. Khai báo dependency CMake rõ ràng

`components/light_driver/CMakeLists.txt` đổi:

```cmake
# gốc
REQUIRES app_storage

# đã sửa
REQUIRES app_storage esp_driver_ledc esp_driver_gpio esp_driver_gptimer
```

Cần khai báo trực tiếp vì ESP-IDF 6 không còn để component `driver` legacy mang
public dependency đến các peripheral driver tách riêng, trong đó có LEDC và
GPTimer. Không khai báo có thể dẫn đến thiếu include/link component tùy graph
build. [Migration: Legacy Driver Dependencies Removal](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/migration-guides/release-6.x/6.0/peripherals.html).
`REQUIRES` là cơ chế ESP-IDF chuẩn để đưa component phụ thuộc vào build/link.
[Tài liệu component dependencies](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c61/api-guides/build-system-v2/component-dependencies.html).

### 4. Header và callback FreeRTOS

- `include/iot_led.h` và `include/light_driver.h` thêm `#include
  "driver/gpio.h"`. Đây là include trực tiếp cho các kiểu GPIO xuất hiện trong
  public API, thay vì vô tình dựa vào include bắc cầu từ header driver cũ.
- `light_driver.c` đổi chữ ký callback timer FreeRTOS từ `void *timer` sang
  `TimerHandle_t timer`; phù hợp prototype callback của FreeRTOS. Thân hàm không
  đổi, chỉ comment thừa được thêm.

## Thay đổi bắt buộc trong `components/button`

Mặc dù board hiện dùng `gpio_button_config`, component `button` vẫn compile cả
`button_adc.c`. Vì vậy API ADC cũ trong file đó vẫn có thể làm build hỏng.

| File | Thay đổi chức năng | Lý do |
|---|---|---|
| `button/CMakeLists.txt` | `PRIV_REQUIRES esp_adc_cal` thành `esp_adc esp_driver_gpio esp_timer`. | API ADC mới nằm trong `esp_adc`; dependency tách riêng phải được khai báo. |
| `button_adc.c` | `driver/adc.h` + `esp_adc_cal.h`, `adc1_*`, `esp_adc_cal_*` được thay bằng ADC oneshot (`adc_oneshot_*`) và calibration curve-fitting (`adc_cali_*`). | Port API ADC legacy sang API hiện hành của ESP-IDF 6. |
| `button_adc.h` | `adc1_channel_t` thành `adc_channel_t`, include `esp_adc/adc_oneshot.h`. | Đồng bộ kiểu public API với oneshot driver. |
| `button_adc.c` | Check số channel dùng `SOC_ADC_CHANNEL_NUM(ADC_UNIT_1)`; đọc nhiều mẫu, kiểm tra lỗi, sau đó calibrate; deinit xóa calibration/unit handle. | Thay các hằng/API legacy và quản lý đúng lifetime của handle mới. |

Các comment-out còn lại trong `button_adc.c`/`.h` là code cũ được giữ để tham
chiếu và các thay đổi format; chúng không cần thiết để build. Phần bắt buộc là
headers/API mới và dependency mới.

## Các thay đổi không cần thiết cho khả năng build

- Phần lớn khác biệt trong `iot_led.c` là xuống dòng/đặt ngoặc/comment nên làm
  diff lớn, nhưng không thay đổi thuật toán LED ngoài hai điểm: GPTimer và lấy
  tần số LEDC qua API.
- Các file ứng dụng `main`, root `CMakeLists.txt`, `main/CMakeLists.txt`,
  `partitions.csv`, `sdkconfig.defaults`, `app_storage` đều không đổi, nên không
  nên ghi nhận chúng là “fix build”.
- Không có package ngoài ESP-IDF được thêm. `esp_driver_ledc`,
  `esp_driver_gpio`, `esp_driver_gptimer`, `esp_adc` và `esp_timer` là component
  đi kèm ESP-IDF 6.0.2 trong môi trường build, không phải package cần cài qua
  `idf.py add-dependency`.

## Lưu ý kỹ thuật sau khi port

`fade_timercb` chạy trong ISR context. Tài liệu GPTimer yêu cầu callback không
được block và, nếu bật tùy chọn ISR-cache-safe, callback/các hàm nó gọi phải ở
IRAM. Mã hiện vẫn thực hiện cập nhật LEDC trực tiếp như mô hình ISR cũ; build đã
qua nhưng nên test fade/blink trên phần cứng, đặc biệt khi thay đổi cấu hình
cache/IRAM. [Ràng buộc callback GPTimer](https://docs.espressif.com/projects/esp-idf/en/v5.3/esp32/api-reference/peripherals/gptimer.html).

## Tóm tắt tối thiểu để tái lập port

1. Dùng ESP-IDF 6.0.2 cho target `esp32c3`.
2. Port `light_driver` từ `driver/timer.h` sang `driver/gptimer.h`, đăng ký
   callback trước enable, và khai báo `esp_driver_gptimer`/`ledc`/`gpio`.
3. Port `button_adc` sang `esp_adc` oneshot + calibration và khai báo dependency
   tương ứng, vì source ADC vẫn được biên dịch.
4. Giữ nguyên `main`, project CMake và partition nếu chỉ mục tiêu là build
   project 2 trên ESP-IDF 6.
