# ADR-015: Tarayıcı Üzerinden OTA ve 4 MB Çift Slot Bölüm Tablosu

| Alan | Değer |
|---|---|
| **Durum** | Kabul edildi |
| **Karar tarihi** | 2026-04-28 (commit `fdddf76`) |
| **Dokümante edilme** | 2026-10-10 |
| **İlgili** | [ADR-001](ADR-001_DONANIM_PLATFORMU_ESP32_S3_SUPER_MINI.md), [ADR-022](ADR-022_KIMLIK_DOGRULAMASIZ_YEREL_AG_GUVENLIK_MODELI.md), SRS FG-OTA-*, KIS-06, SDD §4.7.5 ve §6.6, [GUIDE-009](../guide/GUIDE-009_OTA_GUNCELLEME.md), KS-01, KS-02, KS-20, KS-21 |

## Bağlam

Cihaz kumpasa monte edildikten sonra USB portuna erişim zorlaşır. Firmware güncellemesi, geliştirici bilgisayarında PlatformIO olmayan bir kullanıcı tarafından da yapılabilmeli. Kart 4 MB flash'a sahiptir ve varsayılan 8 MB bölüm tabloları bu kartta açılmaz.

## Karar

1. **Özel bölüm tablosu** `partitions_ota_4mb.csv`:

   | Ad | Ofset | Boyut |
   |---|---|---|
   | nvs | 0x9000 | 20 KB |
   | otadata | 0xE000 | 8 KB |
   | app0 (ota_0) | 0x10000 | 1,25 MB |
   | app1 (ota_1) | 0x150000 | 1,25 MB |
   | spiffs | 0x290000 | 1,44 MB |

2. **Tarayıcı tabanlı yükleme:** `GET /update` dosya seçme formu; `POST /update` multipart yükleme. Arduino `Update` sınıfı ile pasif slota parça parça yazma; `Update.end(true)` ile doğrulama ve önyükleme slotunu değiştirme; başarıda 400 ms sonra `ESP.restart()`.
3. Kimlik doğrulama yok (ADR-022).
4. ArduinoOTA (espota/PlatformIO `upload_protocol = espota`) **kullanılmaz**.

## Değerlendirilen Alternatifler

| Seçenek | Neden seçilmedi |
|---|---|
| ArduinoOTA | Kullanıcının bilgisayarında PlatformIO/Arduino IDE gerektirir; ek UDP portu |
| Sunucudan çekme (HTTP pull) | Güncelleme sunucusu gerektirir |
| Varsayılan `default.csv` (≈1,2 MB slot + SPIFFS) | Benzer; ama 4 MB'ye açıkça uyan ve ileride ayarlanabilen özel tablo tercih edildi |
| Tek slot + fabrika bölümü | Yarım kalan güncelleme cihazı kurtarma moduna düşürür |
| `min_spiffs` benzeri (≈1,9 MB slot) | Mevcut firmware 1,25 MB'ye sığıyor; dosya sistemi için alan ayrıldı (henüz kullanılmıyor, KS-22) |

## Sonuçlar

**Olumlu**
- Kablosuz, araçsız güncelleme.
- Yükleme yarıda kesilirse (ağ, güç) aktif slot değişmez; cihaz eski firmware ile açılır.

**Olumsuz / Riskler**
- `UPDATE_MAX_SIZE` (3 MB) slot boyutuyla uyumsuz ve kontrolün kendisi etkisiz (KS-01). Gerçek sınır 1 310 720 bayttır.
- Yazma hatasında kullanıcıya "Yukleme baslamadi." gösterilir (KS-02).
- Yükleme boyunca `loop()` bloklanır; ölçüm durur (KS-20).
- Yeni firmware açılır ama hatalı çalışırsa otomatik geri dönüş yoktur (KS-21). Ör. yanlış `wifi_secrets.h` ile derlenen firmware cihazı ağdan düşürür → USB ile kurtarma gerekir.
- MD5/imza doğrulaması yok; yalnızca ESP imaj başlığı doğrulanır.
- `firmware.bin` WiFi şifresini içerir (KS-04).

## Uygulama Notları

- [partitions_ota_4mb.csv](../../partitions_ota_4mb.csv), [platformio.ini](../../platformio.ini)
- [web_ui.cpp:18-20](../../src/web_ui.cpp#L18-L20), [web_ui.cpp:611-683](../../src/web_ui.cpp#L611-L683), [web_ui.cpp:821-870](../../src/web_ui.cpp#L821-L870)
