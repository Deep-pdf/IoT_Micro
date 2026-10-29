# Retro-Go cho ESP32-S3 N16R8 + GMT024 + MAX98357A

Đây là bản tùy biến từ dự án mã nguồn mở [Retro-Go](https://github.com/ducalex/retro-go), dành cho bo ESP32-S3 N16R8 có khe microSD tích hợp, màn hình GMT024-08-SPI8P 2.4 inch dùng ST7789 và ampli I2S MAX98357A. Mã nguồn nền là commit `4ced120669750ca7228fd0414211430c1d923166`.

Project giữ giấy phép và thông tin tác giả của Retro-Go trong `COPYING` và `README.md`. Không kèm ROM thương mại.

## Phần cứng đã kiểm tra

### Màn hình GMT024 SPI

| Chân màn hình | ESP32-S3 |
|---|---:|
| SCL/SCK | GPIO10 |
| SDA/MOSI | GPIO11 |
| RST | GPIO12 |
| DC | GPIO13 |
| CS | GPIO14 |
| BL | 3V3 trực tiếp |
| VCC | 3V3 |
| GND | GND |

`BL` đang cấp thẳng 3V3 nên phần mềm không điều chỉnh được độ sáng đèn nền.

### MAX98357A

| Chân MAX98357A | ESP32-S3 |
|---|---:|
| DIN | GPIO3 |
| BCLK | GPIO46 |
| LRC/WS | GPIO9 |
| VIN | 5V hoặc nguồn phù hợp với module |
| GND | GND chung |

### Nút bấm

Mỗi nút nối giữa GPIO và GND; firmware bật điện trở kéo lên nội bộ.

| Nút | ESP32-S3 |
|---|---:|
| UP | GPIO42 |
| DOWN | GPIO2 |
| LEFT | GPIO1 |
| RIGHT | GPIO41 |
| A | GPIO21 |
| B | GPIO47 |
| START | GPIO48 |
| SELECT | GPIO45 |

- Mở menu trong game: `START + SELECT`.
- Mở tùy chọn nhanh: `SELECT + A`.
- Khôi phục về launcher khi lỗi vòng lặp khởi động: giữ `DOWN` lúc bật nguồn.

GPIO45 và GPIO46 là chân strap của ESP32-S3. Không gắn thêm điện trở kéo lên/kéo xuống mạnh vào hai chân này. MAX98357A có đầu vào trở kháng cao nên GPIO46 có thể dùng làm BCLK sau khi khởi động.

### Khe microSD tích hợp

Firmware dùng SDMMC 1-bit:

| Tín hiệu SD | ESP32-S3 |
|---|---:|
| CLK | GPIO39 |
| CMD | GPIO38 |
| DATA/D0 | GPIO40 |

Định dạng thẻ FAT32. Các thư mục ROM ở gốc thẻ gồm `roms/nes`, `roms/gb`, `roms/gbc`, `roms/gw`, `roms/sms`, `roms/gg`, `roms/col`, `roms/pce` và `roms/lnx`.

## Hệ máy trong bản build mặc định

- NES/Famicom
- Game Boy và Game Boy Color
- Game & Watch
- Sega Master System, SG-1000 và Game Gear
- ColecoVision
- PC Engine/TurboGrafx-16 HuCard
- Atari Lynx

SNES bị loại khỏi target này để giảm dung lượng và tránh hiệu năng thấp. Project nguồn gốc còn có các app Mega Drive, DOOM và MSX, nhưng script mặc định chỉ đóng gói `launcher` và `retro-core`, giống firmware V7 đã kiểm tra.

## Chuẩn bị môi trường

Khuyến nghị ESP-IDF 4.4.8, là phiên bản đã dùng để kiểm tra project này. Cài ESP-IDF theo tài liệu chính thức của Espressif, sau đó mở terminal ESP-IDF hoặc chạy:

```sh
. /duong-dan/esp-idf/export.sh
```

Kiểm tra:

```sh
idf.py --version
python3 --version
```

## Biên dịch firmware

Tại thư mục project:

```sh
./build-gmt024-s3.sh
```

Hoặc chạy thủ công:

```sh
python3 rg_tool.py --target=gmt024-s3 clean launcher retro-core
python3 rg_tool.py --target=gmt024-s3 build-img launcher retro-core
```

File kết quả có dạng:

```text
retro-go_<version>_gmt024-s3.img
```

Thư mục `prebuilt` trong gói chia sẻ chứa firmware V7 đã biên dịch sẵn để đối chiếu và thử phần cứng.

## Nạp firmware

File `.img` là ảnh ghép hoàn chỉnh và luôn nạp tại địa chỉ `0x0`:

```sh
esptool.py --chip esp32s3 write_flash --flash_mode dio --flash_size 16MB 0x0 retro-go_*_gmt024-s3.img
```

Nếu dùng Espressif Web Flasher/esptool-js:

1. Chọn file `.img`.
2. Đặt địa chỉ `0x0`.
3. Flash mode `DIO`, flash size `16MB`.
4. Erase Flash rồi Program.

## Thay đổi chân hoặc phần cứng

File chính cần sửa:

```text
components/retro-go/targets/gmt024-s3/config.h
```

Các nhóm cấu hình quan trọng:

- `RG_GAMEPAD_GPIO_MAP`: chân nút.
- `RG_GPIO_LCD_*`: chân SPI màn hình.
- `RG_GPIO_SND_I2S_*`: chân MAX98357A.
- `RG_GPIO_SDSPI_*`: chân SDMMC; tên macro được giữ theo kiến trúc Retro-Go.
- `RG_SCREEN_SPEED`: tốc độ SPI màn hình. Dùng `SPI_MASTER_FREQ_20M` nếu dây dài hoặc màn bị nhiễu.
- `RG_SCREEN_ROTATION` và `RG_SCREEN_RGB_BGR`: chiều và thứ tự màu.
- `RG_SCREEN_INIT()`: chuỗi khởi tạo ST7789.

Thiết lập ESP32-S3 N16R8 và flash 16 MB nằm tại:

```text
components/retro-go/targets/gmt024-s3/sdkconfig
components/retro-go/targets/gmt024-s3/env.py
```

Sau mỗi lần đổi cấu hình target, nên chạy `clean` rồi build lại.

## Giấy phép

Các thay đổi của project này được phân phối cùng điều khoản GPLv2 của Retro-Go. Xem `COPYING` và phần License trong `README.md` để biết ngoại lệ giấy phép của một số emulator.
