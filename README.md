# Bộ test Arduino và ESP32 bằng Arduino IDE

Bộ sketch này dùng để kiểm tra nhanh bo ESP32, đo hiệu năng tương đối, đồng thời có benchmark và stress test chạy được trên Arduino AVR. Mỗi thư mục là một sketch độc lập; mở file `.ino` cùng tên thư mục trong Arduino IDE.

## Chuẩn bị Arduino IDE

1. Cài gói board **esp32 by Espressif Systems** trong Boards Manager.
2. Chọn đúng bo hoặc chọn một cấu hình ESP32 tương thích với bo của bạn.
3. Chọn đúng cổng USB và mở Serial Monitor ở **115200 baud**.
4. Nếu upload không bắt đầu, giữ nút **BOOT**, bấm Upload, rồi thả BOOT khi Arduino IDE hiện `Connecting...`.

> ESP32 thường, S2/S3, C3/C6/H2 có sơ đồ chân khác nhau. Hãy sửa các hằng `LED_PIN`, `GPIO_OUT_PIN`, `GPIO_IN_PIN`, `ADC_PIN`, `SDA_PIN`, `SCL_PIN` theo bo của bạn. Không dùng chân flash, USB hoặc chân strap nếu chưa kiểm tra sơ đồ bo.

## Thứ tự nên chạy

1. `01_chip_info_blink`: xác nhận chip, flash, heap, PSRAM và LED.
2. `02_gpio_loopback`: kiểm tra GPIO bằng một dây nối qua điện trở 1 kΩ.
3. `03_adc_test`: kiểm tra ADC với GND, 3V3 qua cầu phân áp, hoặc biến trở.
4. `04_wifi_scan`: kiểm tra radio Wi-Fi và mức tín hiệu RSSI.
5. `05_i2c_scanner`: tìm cảm biến/màn hình I2C nếu có gắn thêm.
6. `06_benchmark`: đo số học nguyên, số thực, tốc độ RAM và `digitalWrite()`.
7. `07_coremark`: chạy EEMBC CoreMark 1.0 chuẩn để đo hiệu năng CPU.
8. `08_portable_benchmark`: benchmark CPU, π/`double` và RAM dùng chung cho Arduino AVR và ESP32.
9. `09_portable_stress_test`: tải CPU + kiểm tra RAM trong 10 phút; ESP32 hai core sẽ tải cả hai core.
10. `10_coremark_dual_oled`: 5 lượt CoreMark: single-core SRAM/PSRAM, dual-core SRAM/PSRAM và dual-core hybrid SRAM+PSRAM. Bo không có PSRAM chỉ chạy 2 lượt SRAM. OLED SH1106 I²C `0x3C` hiện điểm CoreMark/MHz; nó ngừng cập nhật trong các lượt dual-core để không chen vào vùng đo.

## Cách đọc kết quả

- Flash/PSRAM phải gần với thông số người bán công bố. Dung lượng hiển thị có thể khác đôi chút do cách quy đổi và phân vùng.
- `GPIO loopback: PASS` ở cả mức LOW và HIGH nghĩa là cặp chân hoạt động cơ bản.
- ADC nối GND phải gần 0 mV. Điện áp đo bằng ADC ESP32 chỉ mang tính tham khảo nếu chưa hiệu chuẩn.
- Wi-Fi quét thấy mạng và RSSI hợp lý (ví dụ khoảng -30 đến -80 dBm) cho thấy radio hoạt động.
- Benchmark chủ yếu dùng để so sánh giữa các cấu hình CPU/compiler/board. Không coi số vòng lặp là một chuẩn công nghiệp tuyệt đối.
- Với CoreMark, kết quả đạt yêu cầu khi có dòng `Correct operation validated`, thời gian chạy ít nhất 10 giây và dòng cuối có dạng `CoreMark 1.0 : <điểm> / ...`. Điểm càng cao càng nhanh.
- Sketch CoreMark dùng một context, không cần cài thư viện ngoài. Chỉ so sánh điểm khi tần số CPU, phiên bản Arduino-ESP32 và tùy chọn biên dịch tương đương.
- CoreMark single/dual-core dùng thư viện Adafruit SH110X/GFX/BusIO cho OLED; điểm CoreMark/MHz được tính bằng CoreMark/s chia MHz của mỗi core. Xem hướng dẫn nối dây và menu Arduino IDE trong `10_coremark_dual_oled/README.md`.
- Portable benchmark không tính thời gian Serial, TFT, Wi-Fi hoặc Bluetooth. So sánh `double` chỉ có ý nghĩa khi `sizeof(double)` giống nhau.
- Stress test báo `PASS` khi không phát hiện lỗi trong vùng RAM thử và phép tính vẫn hữu hạn; đây không phải chứng nhận toàn bộ phần cứng.
- Nếu benchmark báo lỗi RAM/PSRAM, chạy lại với nguồn USB và cáp tốt trước khi kết luận bo hỏng.

## Lưu ý an toàn

- GPIO của ESP32 dùng mức logic **3,3 V**; không đưa 5 V trực tiếp vào chân.
- Khi test loopback, nối hai GPIO qua điện trở khoảng **1 kΩ** để hạn chế rủi ro nếu cấu hình nhầm hai chân cùng là output trái mức.
- ADC không được vượt quá 3,3 V. Muốn đo nguồn cao hơn phải dùng cầu phân áp.
