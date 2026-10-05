# Portable benchmark

Mở `08_portable_benchmark.ino`, chọn bo rồi upload. Không cần thư viện ngoài.
Serial Monitor dùng 115200 baud.

Sketch đo bốn nhóm tải, mỗi nhóm khoảng 1,5 giây:

- vòng lặp số nguyên 32 bit;
- phép toán `float`;
- số lần tính π bằng 16 số hạng BBP;
- tốc độ `memcpy` trên một buffer cố định.

Chỉ so sánh hai kết quả khi dùng cùng phiên bản code, cùng tần số CPU và cùng
tùy chọn compiler. Trên AVR, `double` có thể chỉ rộng 4 byte; sketch luôn in
`sizeof(double)` để phát hiện khác biệt này.

