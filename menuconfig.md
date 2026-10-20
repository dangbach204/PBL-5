# Menuconfig đã thực hiện trong dự án 7_insights

Tài liệu này tổng hợp các cấu hình đã được chọn trong `menuconfig` và các giá trị tương ứng trong file `sdkconfig` của dự án.

## 1. Target và build

- `CONFIG_IDF_CMAKE=y`
- `CONFIG_IDF_TARGET_ARCH_RISCV=y`
- `CONFIG_IDF_TARGET="esp32c3"`
- `CONFIG_IDF_TARGET_ESP32C3=y`

## 2. Flash và partition

- `CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y`
- `CONFIG_PARTITION_TABLE_CUSTOM=y`
- `CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"`
- `CONFIG_PARTITION_TABLE_FILENAME="partitions.csv"`
- `CONFIG_PARTITION_TABLE_OFFSET=0x8000`
- `CONFIG_PARTITION_TABLE_MD5=y`

## 3. Bluetooth / BLE

- `CONFIG_BT_ENABLED=y`
- `CONFIG_BTDM_CTRL_MODE_BLE_ONLY=y`
- `CONFIG_BT_NIMBLE_ENABLED=y`

## 4. MbedTLS

- `CONFIG_MBEDTLS_DYNAMIC_BUFFER=y`
- `CONFIG_MBEDTLS_DYNAMIC_FREE_PEER_CERT=y`
- `CONFIG_MBEDTLS_DYNAMIC_FREE_CONFIG_DATA=y`

## 5. FreeRTOS

- `CONFIG_FREERTOS_TIMER_TASK_STACK_DEPTH=3120`

## 6. ESP Insights

- `CONFIG_ESP_INSIGHTS_ENABLED=y`

## 7. Coredump

- `CONFIG_ESP32_ENABLE_COREDUMP=y`
- `CONFIG_ESP32_ENABLE_COREDUMP_TO_FLASH=y`
- `CONFIG_ESP32_COREDUMP_DATA_FORMAT_ELF=y`
- `CONFIG_ESP32_COREDUMP_CHECKSUM_CRC32=y`
- `CONFIG_ESP32_CORE_DUMP_MAX_TASKS_NUM=64`
- `CONFIG_ESP32_CORE_DUMP_STACK_SIZE=1024`

## 8. Diagnostics / metrics

- `CONFIG_DIAG_ENABLE_METRICS=y`
- `CONFIG_DIAG_ENABLE_HEAP_METRICS=y`
- `CONFIG_DIAG_ENABLE_WIFI_METRICS=y`
- `CONFIG_DIAG_ENABLE_VARIABLES=y`
- `CONFIG_DIAG_ENABLE_NETWORK_VARIABLES=y`

## 9. Tổng kết ngắn gọn

Dự án này chủ yếu cấu hình cho:

- Vi điều khiển: ESP32-C3
- Bộ nhớ flash: 4MB
- Bảng phân vùng tùy chỉnh: `partitions.csv`
- BLE-only mode với NimBLE
- ESP Insights bật để thu thập dữ liệu thời gian thực
- Coredump lưu vào flash
- Metrics/diagnostics cho heap, Wi-Fi và biến hệ thống

## 10. Nguồn tham khảo

Các giá trị trên được lấy trực tiếp từ file cấu hình:

- [device_firmware/7_insights/sdkconfig](sdkconfig)
- [device_firmware/7_insights/sdkconfig.defaults](sdkconfig.defaults)
