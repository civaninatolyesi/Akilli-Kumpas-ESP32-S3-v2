# GUIDE-012: Sorun Giderme

**Hedef kitle:** Herkes  
**Yaklaşım:** Belirtiyi bulun → olası nedenleri sırayla eleyin. Her adımda hangi aracı kullanacağınız belirtilmiştir.

**Teşhis araçları:**

| Araç | Ne gösterir |
|---|---|
| Seri monitör ([GUIDE-003 §4](GUIDE-003_DERLEME_YUKLEME_VE_SERI_MONITOR.md)) | Açılış, WiFi, IP, çökme izleri |
| `http://kumpas.local/log` | WiFi, kalibrasyon, OTA olayları |
| `curl http://kumpas.local/api/measure` | Ham durum JSON'u |
| Web arayüzü rozetleri | `raw`, `unit`, `sign`, `btn`, `cal` |
| Logic analyzer | Kumpas sinyali |

---

## 1. Derleme ve Yükleme

| Belirti | Olası neden | Çözüm |
|---|---|---|
| `wifi_secrets.h: No such file` | Gizli dosya yok | [GUIDE-004 §2](GUIDE-004_WIFI_YAPILANDIRMASI.md) |
| `platformio.ini` ayrıştırma hatası | İlk satır `[env:esp32s3]` değil (fazladan karakter) | Dosyayı düzeltin |
| `ModuleNotFoundError: fatfs.wrapper` | penv çakışması | [GUIDE-001 §5](GUIDE-001_GELISTIRME_ORTAMI_KURULUMU.md) |
| Port bulunamıyor | Şarj kablosu, sürücü, indirme modu | Veri kablosu; BOOT+RESET |
| `port is busy` | Seri monitör açık | Kapatın |
| Boot döngüsü, `partition invalid` | Flash boyutu/bölüm uyumsuz | `pio run -t erase` → upload |
| Derleme sonrası farklı makinede derlenmiyor | Platform/kütüphane sürümü farkı (KS-26) | `pio pkg list` çıktılarını karşılaştırın |

## 2. Ölçüm Gelmiyor (`--` veya hiç değişmiyor)

Sırayla:

1. **Kumpas açık mı?** Bazı kumpaslar otomatik kapanır; kapalıyken veri göndermez.
2. **GND ortak mı?** Multimetre ile devamlılık.
3. **Seviye dönüştürme çalışıyor mu?** ESP32 tarafında logic analyzer veya osiloskopla 0/3,3 V darbeler görülmeli ([GUIDE-002 §4](GUIDE-002_DONANIM_BAGLANTISI.md#4-seviye-dönüştürme)).
4. **DATA ve CLOCK yer değiştirmiş mi?** İki kabloyu değiştirip deneyin.
5. **Evirici kullanıyorsanız** kesme kenarı ve bit polaritesi yazılımda çevrildi mi?
6. **Zamanlama sabitleri uygun mu?** `MIN_EDGE_US < Tclk < FRAME_GAP_US < sessizlik` ([GUIDE-010 §3](GUIDE-010_YENI_KUMPAS_MODELINE_UYARLAMA.md)).
7. **Değer 500 mm sınırını aşıyor mu?** Yanlış ölçek/bit düzeni çok büyük değer üretip `MAX_ABS_MM` ile atılıyor olabilir → [GUIDE-010 §4](GUIDE-010_YENI_KUMPAS_MODELINE_UYARLAMA.md) teşhis kodu.
8. **Değer hiç durmuyor mu?** Filtre aynı değeri 2 kez ardışık görmeden yayınlamaz; gürültülü sinyal her çerçeveyi farklı kılar. Teşhis koduyla `raw`'ın sürekli değişip değişmediğine bakın.

## 3. Ölçüm Yanlış

| Belirti | Neden | Çözüm |
|---|---|---|
| Değer ekranın 2 katı / yarısı | Ölçek | `COUNTS_PER_MM` 400/100 — [GUIDE-010 §6](GUIDE-010_YENI_KUMPAS_MODELINE_UYARLAMA.md) |
| Değer ekranın 1,27 / 0,787 katı | Birim çıkarımı yanlış birimde (KS-24) | Kumpası mm moduna alın, cihazı RESET'leyin |
| Negatif değerler pozitif görünüyor | İşaret biti | `CALIPER_SIGN_BIT` 20/23 deneyin |
| Sabit küçük fark | Ofset | `cal=` rozetini kontrol edin; [GUIDE-008](GUIDE-008_KALIBRASYON.md) |
| Rastgele sıçramalar | Gürültü, senkron kaybı | Kabloları kısaltın, GND'yi güçlendirin; `MIN_EDGE_US`'i biraz artırın |
| `unit=` kendiliğinden değişiyor | Yanlış pozitif birim değişimi (KS-24) | `g_autoUnitFix = false` ve kumpası mm'de kullanın |
| Değer gecikmeli güncelleniyor | Filtre büyük sıçramada 4 çerçeve bekler | Normal davranış ([ADR-007](../adr/ADR-007_TEKRAR_TABANLI_STABILIZASYON_FILTRESI.md)) |
| Ofset değiştirdim ama değer değişmedi | Ofset yeni ölçümde uygulanır | Kumpası hafifçe oynatın |

## 4. Ağ

| Belirti | Neden | Çözüm |
|---|---|---|
| Seri: `WiFi baglanamadi … (timeout)` | Yanlış SSID/şifre, 5 GHz ağ, sinyal zayıf | [GUIDE-004 §1](GUIDE-004_WIFI_YAPILANDIRMASI.md) |
| IP ile açılıyor, `kumpas.local` açılmıyor | Windows mDNS, VPN, multicast engeli, WiFi geç bağlandı (KS-09) | IP kullanın; cihazı yeniden başlatın |
| Sayfa açılıyor, durum sürekli `offline` | Bağlantı kesik/yavaş | Sinyal gücü, router |
| Sayfa çok yavaş | Çok sayıda açık sekme/istemci | Fazla sekmeleri kapatın |
| Telefon erişemiyor, bilgisayar erişiyor | Telefon mobil veride / farklı ağda / misafir ağında | Aynı ağa bağlayın |

## 5. Buton

| Belirti | Neden | Çözüm |
|---|---|---|
| `btn=` hiç değişmiyor | Buton 3,3 V'a bağlı + dahili pull-up (KS-05) | [GUIDE-002 §5](GUIDE-002_DONANIM_BAGLANTISI.md) |
| Boşta `btn=pressed`, kayıt bırakınca oluyor | Buton GND'ye bağlı, mantık ters (KS-05) | Kod düzeltmesi ([ADR-018](../adr/ADR-018_BUTON_GIRISI_VE_DEBOUNCE.md)) |
| Tarayıcı bazı basışları kaydetmiyor | 200 ms yoklama (KS-07) | Butonu biraz uzun tutun; Python/WS kullanın |
| Çift kayıt | Buton sıçraması 20 ms'den uzun | Donanım RC (100 nF) veya debounce süresini artırın |

## 6. Batarya

| Belirti | Neden | Çözüm |
|---|---|---|
| %0 / anlamsız gerilim | Bölücü bağlı değil | [GUIDE-002 §6](GUIDE-002_DONANIM_BAGLANTISI.md) |
| Gerilim multimetreden farklı | Direnç toleransı | [GUIDE-011 §2](GUIDE-011_BATARYA_OLCUMU_AYARI.md) |
| Gerilim titriyor | Yüksek empedans, kondansatör yok | 100 nF ekleyin |
| "Voltaj yükseliyor" şarj yokken yanıyor | Eğilim tahmini (yük değişimi, sıcaklık) | [GUIDE-011 §4](GUIDE-011_BATARYA_OLCUMU_AYARI.md) |

## 7. OTA

Bkz. [GUIDE-009 §7–8](GUIDE-009_OTA_GUNCELLEME.md).

## 8. Python Scripti

Bkz. [GUIDE-007 §9](GUIDE-007_PYTHON_OTOMASYON_SCRIPTI.md).

## 9. Çökme / Yeniden Başlama

1. Seri monitörü açık tutun, çökmeyi bekleyin.
2. `Guru Meditation Error` ve `Backtrace: 0x… 0x…` satırlarını kopyalayın.
3. Çözümleyin:

   ```bash
   ~/.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-addr2line -pfiaC -e .pio/build/esp32s3/firmware.elf 0x4200xxxx 0x4037xxxx
   ```

   veya `platformio.ini`'ye `monitor_filters = esp32_exception_decoder` ekleyin.
4. ISR içinde yeni bir çağrı eklediyseniz `Cache disabled but cached memory region accessed` hatası IRAM kuralının ihlalidir ([SDD §12](../SDD_YAZILIM_TASARIM_DOKUMANI.md#12-tasarım-değişmezleri)).

## 10. Destek İsterken Paylaşılacak Bilgiler

- Kumpas modeli ve fotoğrafı
- Firmware commit'i (`git rev-parse --short HEAD`)
- Seri monitör açılış çıktısı
- `/api/measure` çıktısı ve `/log` içeriği
- Mümkünse logic analyzer ekran görüntüsü
- **Paylaşmayın:** `wifi_secrets.h`, `firmware.bin`
