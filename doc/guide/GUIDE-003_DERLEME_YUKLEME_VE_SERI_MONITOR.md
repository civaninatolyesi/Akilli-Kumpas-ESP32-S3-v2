# GUIDE-003: Derleme, Yükleme ve Seri Monitör

**Hedef kitle:** Firmware'i karta yükleyecek herkes  
**Ön koşul:** [GUIDE-001](GUIDE-001_GELISTIRME_ORTAMI_KURULUMU.md) tamamlandı, `include/wifi_secrets.h` mevcut ([GUIDE-004](GUIDE-004_WIFI_YAPILANDIRMASI.md))

---

## 1. Komut Özeti

| İş | CLI | VS Code (PlatformIO alt çubuğu) |
|---|---|---|
| Derle | `pio run` | ✓ (Build) |
| Derle + USB ile yükle | `pio run -t upload` | → (Upload) |
| Seri monitör | `pio device monitor` | 🔌 (Serial Monitor) |
| Yükle + monitör | `pio run -t upload -t monitor` | Upload and Monitor |
| Temizle | `pio run -t clean` | 🗑 (Clean) |
| Flash'ı tamamen sil | `pio run -t erase` | Project Tasks → Erase Flash |
| Bağlı cihazlar | `pio device list` | — |
| Kullanılan paket sürümleri | `pio pkg list` | — |

Windows'ta `pio` yerine `.\pio.ps1` kullanabilirsiniz (aynı argümanlar).

## 2. Derleme

```bash
pio run
```

Başarılı derlemenin sonunda bellek özeti görünür:

```
RAM:   [=         ]  ~1x.x% (used xxxxx bytes from 327680 bytes)
Flash: [=====     ]  ~xx.x% (used xxxxxx bytes from 1310720 bytes)
```

**Flash satırındaki toplam 1 310 720 bayttır** (bir OTA slotu). Kullanım %90'ı aşarsa yeni özellik eklemeden önce [ADR-015](../adr/ADR-015_TARAYICI_UZERINDEN_OTA_VE_CIFT_SLOT.md)'i gözden geçirin.

### Derleme Çıktıları (`.pio/build/esp32s3/`)

| Dosya | Kullanım |
|---|---|
| `firmware.bin` | USB veya OTA ile yüklenecek uygulama imajı |
| `firmware.elf` | Sembollü imaj — çökme izi (backtrace) çözmek için |
| `bootloader.bin` | İkinci aşama önyükleyici (USB yüklemede otomatik yazılır) |
| `partitions.bin` | Bölüm tablosu (USB yüklemede otomatik yazılır) |

## 3. USB ile Yükleme

1. Kartı USB-C veri kablosuyla bağlayın.
2. `pio run -t upload`.
3. PlatformIO portu otomatik bulur. Birden fazla port varsa `platformio.ini`'ye ekleyin:

   ```ini
   upload_port = COM7
   monitor_port = COM7
   ```

4. Yükleme başlamıyorsa kartı indirme moduna alın: **BOOT basılı → RESET bas-bırak → BOOT bırak**, sonra tekrar yükleyin. Yükleme bitince **RESET**'e basın.

> USB-CDC kullanıldığından, firmware çalışırken port numarası ile indirme modundaki port numarası farklı olabilir.

## 4. Seri Monitör

```bash
pio device monitor          # 115200 baud (platformio.ini: monitor_speed)
```

Çıkış: `Ctrl+C`. Beklenen açılış çıktısı:

```
ESP32-S3 dijital kumpas okuma basladi
DATA GPIO: 8, CLOCK GPIO: 9
WiFi baglandi: AgAdiniz  IP: 192.168.1.42
mDNS: http://kumpas.local
```

WiFi bağlanamazsa (15 s sonra):

```
WiFi baglanamadi: AgAdiniz (timeout)
```

> USB-CDC'de, monitör açılmadan önce basılan satırlar kaybolabilir. Açılış mesajlarını görmek için monitörü açtıktan sonra RESET'e basın. Seri port açık olmasa da cihaz normal çalışır.

Ölçümler seri porta **yazılmaz**; ölçüm izlemek için web arayüzü veya `/api/measure` kullanılır. Olay geçmişi için `http://kumpas.local/log`.

## 5. Flash'ı Silme

Şu durumlarda gerekir: "partition invalid" boot döngüsü, bölüm tablosu değişikliği, kalibrasyon/NVS'nin sıfırlanması.

```bash
pio run -t erase
pio run -t upload
```

> Silme kalibrasyon ofsetini de siler.

## 6. Sürüm Bilgisini Kaydetme

Firmware sürüm numarası içermez. Hangi kodun yüklü olduğunu izlemek için yüklemeden önce:

```bash
git rev-parse --short HEAD     # yüklenen commit'i not edin
pio pkg list                   # platform/kütüphane sürümlerini not edin (KS-26)
```

## 7. Sık Hatalar

| Hata | Neden | Çözüm |
|---|---|---|
| `wifi_secrets.h: No such file or directory` | Gizli dosya yok | [GUIDE-004](GUIDE-004_WIFI_YAPILANDIRMASI.md) |
| `Failed to connect to ESP32-S3: No serial data received` | Kart indirme modunda değil / yanlış port | §3 adım 4 |
| `A fatal error occurred: Could not open COMx, the port is busy` | Seri monitör açık | Monitörü kapatın |
| Boot döngüsü, `partition invalid … exceeds flash chip size` | Bölüm tablosu/flash boyutu uyumsuz | `platformio.ini` 4MB ayarlarını kontrol edin, §5 |
| `section .dram0.bss … overflowed` / flash taşması | Kod büyüdü | Slot boyutu sınırı, ADR-015 |
| `platformio.ini` ayrıştırma hatası (`File contains no section headers`) | `[env:esp32s3]` satırının başında fazladan karakter | İlk satırın tam olarak `[env:esp32s3]` olduğunu kontrol edin |
| `ModuleNotFoundError: fatfs.wrapper` | penv çakışması | [GUIDE-001 §5](GUIDE-001_GELISTIRME_ORTAMI_KURULUMU.md) |

Kablosuz güncelleme için: [GUIDE-009](GUIDE-009_OTA_GUNCELLEME.md).
