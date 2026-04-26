# ESP32-S3 Super Mini — Dijital Kumpas Veri Okuma

Bu proje, **DATA + CLOCK + GND** çıkışı olan (yaygın) dijital kumpasların 24‑bit senkron protokolünü ESP32‑S3 ile okuyup Seri Port’a yazdırır.

## Donanım / Bağlantı

Kumpasların çoğu **1.5V** seviyesinde sinyal üretir. ESP32‑S3 girişleri için bu seviye genelde **yetersiz** kalır; bu yüzden seviye yükseltme/izolasyon gerekir.

Önerilen basit yöntem (yaygın caliper çıkışları open‑collector/weak drive olur):

- **DATA** ve **CLOCK** hatlarını **3.3V’a pull‑up** yapın (ör. 10k)
- Kumpas hattını ESP32’ye doğrudan bağlamadan önce **NPN transistor** veya uygun **level shifter** kullanın

Minimum bağlantı:

- Kumpas GND  → ESP32 GND
- Kumpas DATA → (level shift sonrası) ESP32 GPIO8
- Kumpas CLOCK → (level shift sonrası) ESP32 GPIO9

> Not: Kumpası 3.3V ile beslemeye çalışmayın. Kumpas genelde kendi piliyle (1.5V) çalışır.

## Yazılım

- Framework: PlatformIO + Arduino
- Okuma: CLOCK düşen kenarında interrupt ile bit toplama
- Frame ayrımı: CLOCK darbeleri arasındaki boşluk süresine göre

### Pin değiştirme

[src/main.cpp](src/main.cpp) içindeki şu tanımları değiştirin:

- `CALIPER_DATA_PIN`
- `CALIPER_CLK_PIN`

### Ölçek (2x / 0.5x okuma düzeltme)

Bazı 24‑bit Çin kumpasları mm değerini **0.005mm/adım** (200 count = 1mm) olarak gönderir.
Eğer seri portta gördüğün değer kumpastaki değerin **tam 2 katı** ise, ölçek katsayısı yanlış demektir.

[src/main.cpp](src/main.cpp) içinde:

- `COUNTS_PER_MM`
- `COUNTS_PER_INCH`

değerlerini kendi kumpasına göre ayarlayabilirsin.

## Çalıştırma

PlatformIO komutları:

- Build: `pio run`
- Upload: `pio run -t upload`
- Monitor: `pio device monitor`

### Boot döngüsü / partition hatası

Eğer Seri Port’ta sürekli şu tip ROM logları görüyorsan:

`partition ... exceeds flash chip size 0x400000`

bu, kartın flash’ının **4MB** olmasına rağmen 8MB için partition yazıldığı anlamına gelir.

Bu projede 4MB’a uygun partition ayarı yapıldı. Düzeltmek için **flash’ı erase edip tekrar upload** et:

- `./pio.ps1 run -t erase`
- `./pio.ps1 run -t upload`
- `./pio.ps1 device monitor`

> Windows notu: `device monitor` açıkken upload genelde "port is busy" hatası verir.
> Upload için monitor’u durdurup (Ctrl+C / terminali kapatıp) tekrar upload et.

Seri ekranda mm ve inch değerleri yazacaktır.

## Protokol Notu

Bu kod, piyasadaki çok yaygın **24‑bit** kumpas protokolünü hedefler:

- Bit 0..19: büyüklük (count)
- Bit 20: işaret
- Bit 21: birim (0=mm, 1=inch)

Bazı modellerde ölçek/bit dizilimi farklı olabilir. Eğer okunan değerler saçma çıkarsa, model bilgini yaz — bit haritalamasını birlikte düzeltelim.
