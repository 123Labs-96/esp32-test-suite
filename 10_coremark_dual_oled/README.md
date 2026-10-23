# CoreMark single/dual-core + OLED SH1106 cho ESP32

Mở `10_coremark_dual_oled.ino` trong Arduino IDE. Sketch chạy trên ESP32
WROOM classic (LX6, flash 4 MB) và ESP32-S3 N16R8 (LX7, flash 16 MB,
PSRAM 8 MB). OLED mặc định là **SH1106 128×64 I²C địa chỉ 0x3C**.

## Lắp OLED

| OLED | ESP32 WROOM classic | ESP32-S3 N16R8 |
| --- | --- | --- |
| VCC | 3V3 | 3V3 |
| GND | GND | GND |
| SDA | GPIO 21 | GPIO 8 |
| SCL | GPIO 22 | GPIO 9 |

Hai chân của S3 là ví dụ thường dùng; nếu bo của bạn đã gán chúng cho phần
cứng khác, sửa `OLED_SDA` và `OLED_SCL` ở đầu sketch. Tránh GPIO 19/20 khi
dùng USB native trên S3. Nếu OLED là SH1107 hoặc độ phân giải khác 128×64,
cần đổi lớp `Adafruit_SH1106G` và bố cục hiển thị.

## Thư viện

Trong Arduino IDE, cài **esp32 by Espressif Systems** ở Boards Manager.
Sketch này dùng API Task Watchdog của Arduino-ESP32 **3.x**; không dùng
bản core 2.x. Phiên bản đã biên dịch kiểm tra là **3.3.12**.
Trong Library Manager, cài **Adafruit SH110X** và các dependency được IDE gợi ý:
**Adafruit GFX Library** và **Adafruit BusIO**. Serial Monitor: **115200 baud**.

## Cấu hình Arduino IDE

| Tools | ESP32 WROOM 4 MB | ESP32-S3 N16R8 |
| --- | --- | --- |
| Board | ESP32 Dev Module | ESP32S3 Dev Module |
| CPU Frequency | 240 MHz | 240 MHz |
| Flash Size | 4 MB | 16 MB |
| PSRAM | Disabled / không có | OPI PSRAM (8 MB R8) |
| Flash Mode | QIO nếu bo/flash hỗ trợ; DIO nếu boot lỗi | QIO 80MHz cho WROOM-1 N16R8; OPI 80MHz cho WROOM-2 N16R8V |
| Flash Frequency | 80 MHz nếu ổn định; thử 40 MHz nếu lỗi | Nằm ngay trong lựa chọn Flash Mode ở board S3 |
| Core Debug Level | None | None |
| Partition Scheme | Bất kỳ phân vùng 4 MB đủ sketch | Bất kỳ phân vùng 16 MB đủ sketch |

Chọn `Tools > Arduino Runs On` và `Events Run On` theo cấu hình mặc định
của board; sketch tự ghim worker single-core vào core 1, còn dual-core vào
core 0 và core 1. Tăng
Upload Speed chỉ giảm thời gian nạp, không tăng tốc benchmark. Không cần ép
xung quá 240 MHz.

Với S3 dùng cổng USB native, có thể chọn **USB CDC On Boot: Enabled** để
xem Serial Monitor qua USB. Nếu bo dùng chip USB-UART riêng, chọn đúng cổng
UART của nó; tùy chọn USB CDC không làm CPU nhanh hơn.

Nếu S3 N16R8 có mã **WROOM-2 N16R8V**, flash là OPI; chọn đúng Flash Mode
theo chữ in trên module. Với **WROOM-1 N16R8**, flash QSPI và PSRAM OPI.
Chọn sai PSRAM/flash mode có thể làm board khởi động lại. Khi chạy, Serial
phải báo khoảng `PSRAM: 8 MiB`; nếu là 0, kiểm tra lại Tools > PSRAM.

## Bài đo và kết quả

- Lượt 1: **single-core SRAM**, một context trên core 1.
- Lượt 2 (nếu có PSRAM): **single-core PSRAM**, một context trên core 1.
- Lượt 3: **dual-core SRAM**, hai context trên core 0 và core 1.
- Lượt 4 (nếu có PSRAM): **dual-core PSRAM**, cả hai context ở PSRAM.
- Lượt 5 (nếu có PSRAM): **dual-core hybrid**, context 0 ở SRAM/core 0 và
  context 1 ở PSRAM/core 1.
- WROOM không có PSRAM chỉ chạy lượt 1 và 3; ba lượt còn lại hiện `N/A`.
- Mỗi context dùng bộ dữ liệu chuẩn 2.000 byte, seed `0, 0, 0x66`.
- Chỉ bộ dữ liệu của CoreMark được đặt theo SRAM/PSRAM; code, stack, OLED
  buffer và các cấu trúc phụ vẫn ở vùng mặc định của Arduino-ESP32.
- Worker và semaphore được tạo **trước** vùng đo; thời gian tạo/xóa task
  không còn nằm trong phép đo. Serial chỉ in báo cáo sau vùng đo.
- Mỗi lượt tự hiệu chuẩn để vùng đo dài ít nhất 10 giây. Hai lượt single-core
  có thanh tiến độ thời gian **ước tính** cập nhật mỗi 2 giây từ core 0; worker
  chạy ở core 1 nên OLED không tranh core tính toán.
- Trước mỗi lượt dual-core, OLED hiện tên bài đo và tiến độ theo **số lượt đã
  xong**, rồi ngừng cập nhật hoàn toàn cho đến khi phép đo kết thúc. Đây là
  đánh đổi có chủ đích: không có tiến độ thời gian chạy trực tiếp khi cả hai
  core đang được đo, nhưng OLED không gửi I²C/chen CPU trong vùng tính giờ.
- Trong lúc chạy, sketch tạm tăng thời hạn Task Watchdog lên 120 giây rồi
  khôi phục mặc định sau khi đo. Idle task vẫn được đăng ký với watchdog;
  không gọi `disableCore0WDT()`/`disableCore1WDT()` vì Arduino-ESP32 3.x
  có thể spam `esp_task_wdt_reset(...): task not found` khi gỡ idle task.
- Muốn kiểm tra sai lệch do OLED/khởi tạo giao diện, đổi `ENABLE_OLED = false`
  và chạy lại để lấy điểm Serial-only. Serial cũng không in trong vùng đo
  bình thường.
- Chỉ coi `PASS` là hợp lệ khi CoreMark in `Correct operation validated`,
  không có lỗi, và thời gian vùng đo từ 10 giây trở lên. Dòng chuẩn trên
  Serial có dạng `CoreMark 1.0 : <điểm> / ... / N:FreeRTOS-pinned`.

### Đơn vị điểm

- `CoreMark/s` là số iteration/giây do CoreMark gốc in ra.
- `CoreMark/MHz = CoreMark/s ÷ MHz của **mỗi** core`. Với lượt hai nhân,
  tử số là throughput **gộp hai context**; không chia thêm cho 2. Nếu muốn
  hiệu suất trung bình mỗi nhân, lấy điểm dual-core này chia 2.
- `SRAM 2-core scaling` là điểm dual-core SRAM chia điểm single-core SRAM.
- OLED hiện năm điểm `CoreMark/MHz` theo thứ tự `1C SRAM`, `1C PSRAM`,
  `2C SRAM`, `2C PSRAM`, `2C MIX`; Serial hiện cả hai đơn vị, trạng thái
  PASS/FAIL và tỷ lệ scaling. Chỉ so sánh giữa hai bo khi CPU MHz, board
  package, flash mode và trạng thái OLED được ghi rõ.

Sketch kiểm tra CRC của **performance run**. Các điểm `CoreMark/MHz` ở đây là
phép chuẩn hóa thực dụng cho việc so hai bo, **không phải điểm EEMBC được
chứng nhận**. Nếu muốn nộp điểm theo quy tắc EEMBC, cần chạy thêm cấu hình
validation seed `0x3415, 0x3415, 0x66`, ghi compiler flags, vị trí và tỉ số
tần số bộ nhớ/cache so với CPU; đặc biệt phải mô tả lượt PSRAM riêng.

CoreMark đo các thuật toán số nguyên (linked list, matrix, state machine),
không phải bài benchmark dấu phẩy động. Hai bo có khác kiến trúc CPU và bộ
nhớ; ghi lại phiên bản `esp32 by Espressif Systems`, CPU/flash/PSRAM setting
khi so điểm.

## Nguồn mã

`core_list_join.c`, `core_main.c`, `core_matrix.c`, `core_state.c`,
`core_util.c`, `coremark.h` và `LICENSE.md` sao chép nguyên từ bộ EEMBC
CoreMark 1.0 trong sketch `07_coremark` của suite này (EEMBC commit
`1f483d5b8316753a742cbf5590caf5bd0a4e4777`). Chỉ `core_portme.c`,
`core_portme.h` và file `.ino` là phần port/giao diện mới.

Nguồn: https://github.com/eembc/coremark
