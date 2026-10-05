# Portable stress test

Mở `09_portable_stress_test.ino`, chọn bo rồi upload. Không cần thư viện ngoài.
Serial Monitor dùng 115200 baud.

Mặc định bài test chạy 10 phút, tải CPU bằng số nguyên và `float`, đồng thời
ghi/đọc/xác minh một vùng RAM. Với ESP32 hai core, sketch tạo thêm một task tải
core còn lại. Báo cáo trạng thái xuất hiện mỗi 5 giây.

Kết quả `PASS` nghĩa là bài test không phát hiện sai mẫu RAM hoặc giá trị số
thực bất thường. Nó không chứng minh toàn bộ RAM hay mọi ngoại vi đều hoàn hảo.

Muốn chạy đến khi reset, đổi:

```cpp
constexpr uint32_t STRESS_DURATION_MINUTES = 0;
```

Chạy ở xung mặc định và nguồn USB ổn định. Dừng ngay nếu bo nóng bất thường,
có mùi lạ, liên tục reset hoặc báo brownout.

