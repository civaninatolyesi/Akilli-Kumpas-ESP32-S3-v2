# GUIDE-009: OTA (Kablosuz) Firmware Güncelleme

**Hedef kitle:** Kurulumcu, geliştirici  
**İlgili:** [ADR-015](../adr/ADR-015_TARAYICI_UZERINDEN_OTA_VE_CIFT_SLOT.md), [ADR-022](../adr/ADR-022_KIMLIK_DOGRULAMASIZ_YEREL_AG_GUVENLIK_MODELI.md), [KS-01](../BILINEN_SORUNLAR.md#ks-01--ota-boyut-kontrolü-etkisiz-sınır-bölümle-uyumsuz-), [KS-02](../BILINEN_SORUNLAR.md#ks-02--ota-yazma-hatasında-yanıltıcı-mesaj-), [KS-20](../BILINEN_SORUNLAR.md#ks-20--ota-sırasında-ölçüm-duruyor-), [KS-21](../BILINEN_SORUNLAR.md#ks-21--hatalı-ama-açılan-firmwareden-otomatik-geri-dönüş-yok-)

---

## 1. Nasıl Çalışır?

Flash'ta iki uygulama slotu vardır (`app0`, `app1`, her biri 1,25 MB). Cihaz birinden çalışırken yeni firmware diğerine yazılır. Yazma eksiksiz bitip doğrulanınca önyükleme seçimi (`otadata`) yeni slota çevrilir ve cihaz yeniden başlar. Yükleme yarıda kesilirse cihaz eski slottan açılmaya devam eder.

## 2. Ön Koşullar

- [ ] Cihaz ve bilgisayar aynı ağda; `http://kumpas.local/` açılıyor
- [ ] Yeni firmware **aynı** `wifi_secrets.h` ile derlendi (veya ağ değişikliği bilinçli ve USB erişimi var — [GUIDE-004 §5](GUIDE-004_WIFI_YAPILANDIRMASI.md))
- [ ] Batarya ile çalışıyorsa şarj > %30 (güncelleme sırasında güç kesilmemeli)
- [ ] Ölçüm almıyorsunuz (güncelleme boyunca ölçüm durur — KS-20)

## 3. Firmware Hazırlama

```bash
pio run
```

1. Çıktının sonundaki **Flash** satırını kontrol edin: kullanılan bayt **< 1 310 720** olmalı.
2. Dosya: `.pio/build/esp32s3/firmware.bin`
3. Hangi commit'ten derlendiğini not edin: `git rev-parse --short HEAD`

> README ve kod "3 MB sınır" der; gerçek sınır **1,25 MB**'dir (KS-01).

## 4. Tarayıcıdan Yükleme

1. `http://kumpas.local/update`
2. **Dosya Seç** → `firmware.bin` (`bootloader.bin` veya `partitions.bin` **değil**).
3. **Yükle**. Sayfa yükleme bitene kadar bekler (dosya boyutu ve ağa göre 10–60 s). Sekmeyi kapatmayın.
4. Sonuç:
   - **Basarili — "Guncelleme tamamlandi. Cihaz yeniden baslatiliyor..."** → ~5–15 s sonra cihaz yeniden ağa bağlanır.
   - **Basarisiz** + hata metni → cihaz eski firmware ile çalışmaya devam eder. Ayrıntı için `/log`.

## 5. Komut Satırından Yükleme

```bash
curl -F "firmware=@.pio/build/esp32s3/firmware.bin" http://kumpas.local/update
```

PowerShell (Windows 10+ `curl.exe` yerleşiktir):

```powershell
curl.exe -F "firmware=@.pio/build/esp32s3/firmware.bin" http://kumpas.local/update
```

## 6. Doğrulama

1. Ana sayfa açılıyor, durum `online`.
2. `/log` → yeni açılış satırları (`Boot: main setup`, `WiFi: connected …`).
3. Kalibrasyon korunmuş (`cal=` önceki değer).
4. Kumpası hareket ettirip ölçümün güncellendiğini görün.
5. Değişikliğe özgü test senaryolarını çalıştırın ([GUIDE-015](GUIDE-015_TEST_VE_DOGRULAMA.md)).

## 7. Hata Durumları

| Mesaj / Belirti | Neden | Çözüm |
|---|---|---|
| "Yukleme baslamadi." | `Update.begin` başarısız **veya** yazma sırasında hata oldu (mesaj yanıltıcı — KS-02) | `/log`'daki `OTA: …` satırına bakın |
| `/log`: `OTA: write failed …` | Dosya slottan büyük, ağ kesintisi | Boyutu kontrol edin, tekrar deneyin |
| `/log`: `OTA: end failed …` / "Wrong Magic Byte" | Yanlış dosya (ör. `.elf`, `bootloader.bin`) veya bozuk dosya | Doğru `firmware.bin`'i seçin |
| Sayfa yanıt vermiyor | WiFi kopması | Cihaz eski firmware ile çalışıyor olmalı; tekrar deneyin |
| Güncelleme başarılı ama cihaz ağa dönmüyor | Yeni firmware'de WiFi bilgileri yanlış veya firmware hatalı (otomatik geri dönüş yok — KS-21) | §8 |

## 8. Kurtarma (USB ile)

1. Kartı USB ile bağlayın.
2. Seri monitörde açılış çıktısını inceleyin ([GUIDE-003 §4](GUIDE-003_DERLEME_YUKLEME_VE_SERI_MONITOR.md)).
3. Bilinen iyi bir commit'e dönün ve yükleyin:

   ```bash
   git checkout <iyi-commit>
   pio run -t upload
   ```

4. Kart açılmıyorsa (boot döngüsü): BOOT + RESET ile indirme moduna alın, `pio run -t erase` ve ardından `pio run -t upload`. (Kalibrasyon silinir — yeniden girin.)

## 9. Güvenlik

- `/update` kimlik doğrulama içermez: ağdaki herkes firmware yükleyebilir. Cihazı yalnızca güvenilir ağlarda kullanın ([ADR-022](../adr/ADR-022_KIMLIK_DOGRULAMASIZ_YEREL_AG_GUVENLIK_MODELI.md)).
- `firmware.bin` WiFi şifresini düz metin olarak içerir — **paylaşmayın, e-postayla göndermeyin, depoya eklemeyin** (KS-04).
