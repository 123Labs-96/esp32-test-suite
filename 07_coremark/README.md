# CoreMark trong Arduino IDE

Sketch này chứa mã thuật toán CoreMark 1.0 từ kho chính thức
[EEMBC CoreMark](https://github.com/eembc/coremark), commit
`1f483d5b8316753a742cbf5590caf5bd0a4e4777` (2025-05-01).

Các file thuật toán `core_list_join.c`, `core_main.c`, `core_matrix.c`,
`core_state.c`, `core_util.c` và `coremark.h` được giữ nguyên.
Chỉ `07_coremark.ino` và `core_portme.*` là lớp port cho Arduino-ESP32.

## Chạy benchmark

1. Mở `07_coremark.ino` bằng Arduino IDE.
2. Chọn đúng bo ESP32.
3. Đặt CPU Frequency về mức bạn muốn đo; ghi lại mức này khi so sánh.
4. Upload và mở Serial Monitor ở 115200 baud.
5. Đợi đến khi thấy `Correct operation validated` và dòng `CoreMark 1.0 : ...`.

Không cần cài thư viện CoreMark bên ngoài.

## Điều kiện của phép đo này

- Performance run chuẩn: seed 0, 0, 0x66 và tổng dữ liệu 2000 byte.
- Một context (một tác vụ đo); không cộng điểm hai core.
- Tự hiệu chỉnh số vòng chạy; kết quả chính thức chỉ hợp lệ khi thời gian đo
  ít nhất 10 giây.
- Điểm chính là `Iterations/Sec`; số càng cao càng nhanh.
- Chỉ so sánh khi CPU frequency, phiên bản Arduino-ESP32, trình biên dịch,
  chế độ tối ưu và vị trí bộ nhớ tương đương.
- Hãy chạy 3 lần sau một lần khởi động nguội và lấy trung bình. Tắt Wi-Fi,
  Bluetooth và các tác vụ nền khác để giảm nhiễu.
