# **THE FINAL PROJECT --Embedded Programming Course/IMIC**

## Yêu cầu dự án
Bài tập cuối kỳ: lập trình firmware có os(freeRTOS) hoặc non-os với các tính nâng sao.
1. sử dụng uart:
* “led on”: để turn on led
* “led off”: để turn off led
* “update”: để update firmware chương trình chóp tắt led pd12 chu kì 1s
2. mỗi giây đo nhiệt độ chip một lần gửi dữ liệu nhiệt độ qua uart
3. tính nâng update firmware qua uart
	firmware1: điều khiển led thông qua uart (file bin size:
	firmware2: chớp tắt led

##Mã nguồn
Dự án non-os điều khiển chớp tắt led PD12 và update firmware bằng UART
- Truyền "led on" để bật led
- Truyền "led off" để tắt led
- Truyền "update" để chuyển sang chế độ cập nhật
- Truyền file.bin để chạy chương trình mới
- 


