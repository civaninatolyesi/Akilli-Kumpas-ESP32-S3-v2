# GUIDE-015: Test ve Doğrulama

**Hedef kitle:** Geliştirici, test eden kişi, sürüm yayınlayan kişi  
**İlgili:** [SRS §7–8](../SRS_YAZILIM_GEREKSINIM_DOKUMANI.md#7-doğrulama-yaklaşımı), [KS-19](../BILINEN_SORUNLAR.md#ks-19--otomatik-test-yok-)

Projede şu an otomatik test yoktur; doğrulama gerçek donanım üzerinde manuel senaryolarla yapılır. §5 otomatik test altyapısı için bir öneri içerir.

---

## 1. Test Ortamı

| Öğe | Gereken |
|---|---|
| Cihaz | Montajı tamamlanmış ESP32-S3 + kumpas |
| Referans | Mastar bloğu veya bilinen boyutlu parça (ör. 10,00 / 25,00 mm) |
| Ağ | 2,4 GHz WiFi, cihaz ve test bilgisayarı aynı ağda |
| Yazılım | Tarayıcı, `curl`, Python + `websocket-client` (+ `pyautogui` TC-26 için) |
| İsteğe bağlı | Logic analyzer, multimetre, USB güç ölçer |

Her test oturumu için kaydedin: tarih, firmware commit'i, kumpas modeli, test eden.

## 2. Test Senaryoları

Sonuç sütunu: ✅ Geçti · ❌ Kaldı · ⚠️ Bilinen sorun (KS) nedeniyle beklenen sapma · — Uygulanmadı.

### 2.1 Okuma ve Çözümleme

| ID | Senaryo | Adımlar | Beklenen | SRS |
|---|---|---|---|---|
| TC-01 | Sürekli okuma | Kumpası açın, 0 → 50 mm arasında 10 farklı noktada durdurun | Her durmada web arayüzü ≤ 1 s içinde kumpas ekranıyla aynı değeri gösterir | FG-OKU-01..05 |
| TC-02 | Ağ yükü altında okuma | 3 tarayıcı sekmesi + `measure_ws.py` açıkken TC-01'i tekrarlayın; ayrıca `/log` sayfası açık olsun | Değerler doğru, kaçırılan/sıçrayan değer yok | FG-OKU-06/07, FOG-PRF-04 |
| TC-03 | Pozitif değer doğruluğu | 10,00 ve 25,00 mm referansı ölçün | `mm` kumpas ekranıyla ±0,01 mm; `inch` = mm/25,4 (±0,0001) | FG-COZ-01, 04, 05 |
| TC-04 | Negatif değer | Çeneyi 20 mm açın, ZERO, 10 mm'ye kapatın | `mm ≈ -10.00`, `sign=-`, `isNegative=true` | FG-COZ-02 |
| TC-05 | Makullük sınırı | (Analiz) `MAX_ABS_MM`'i geçici olarak 20 yapıp 25 mm ölçün | 20 mm üstü değer yayınlanmaz, son geçerli değer kalır | FG-COZ-06 |

### 2.2 Filtre ve Birim

| ID | Senaryo | Adımlar | Beklenen | SRS |
|---|---|---|---|---|
| TC-06 | Kaydırma sırasında sıçrama yok | Çeneyi hızla 0 → 100 → 0 mm hareket ettirin, 20 kez | Arayüzde hiçbir zaman kumpas ekranında görülmeyen bir değer belirmez | FG-FLT-02, FOG-GVN-01 |
| TC-07 | Sıçrama gecikmesi | 10 mm'de durun, hızla 60 mm'ye götürüp durun; WS istemcisinde zaman damgalarını kaydedin | Yeni değer ~4 çerçeve periyodu içinde gelir; ara değer yayınlanmaz | FG-FLT-03 |
| TC-08 | Birim değişimi | (a) 50 mm'de mm→inç tuşuna basın. (b) 5 mm'de mm→inç tuşuna basın | (a) `unit=inch`, `mm ≈ 50.00` korunur. (b) ⚠️ KS-24: birim algılanmaz, `mm` ≈ 3,94 gösterir | FG-BRM-02..04 |

### 2.3 Ağ ve API

| ID | Senaryo | Adımlar | Beklenen | SRS |
|---|---|---|---|---|
| TC-09 | WiFi + mDNS | Cihazı açın | Seri: `WiFi baglandi` + IP + `mDNS: http://kumpas.local`; `ping kumpas.local` yanıt verir | FG-NET-01/03/06 |
| TC-10 | WiFi yokken çalışma | Router kapalıyken cihazı açın, 20 s bekleyin, router'ı açın | Seri: `timeout`; USB'den ölçüm akışı devam eder (teşhis için); router açıldıktan sonra IP ile erişim — ⚠️ KS-09: `kumpas.local` çalışmayabilir | FG-NET-02/05, FOG-GVN-03 |
| TC-11 | `/api/measure` şeması | `curl -s http://kumpas.local/api/measure` | 12 alanın hepsi, doğru tipler (SDD §6.3); geçerli JSON (`python -m json.tool`) | FG-HTTP-02 |
| TC-12 | Sayfalar ve 404 | `/`, `/log`, `/calibrate`, `/update`, `/yok` | 200 ×4, `/yok` → 404 | FG-HTTP-01/04/05 |
| TC-13 | WS yayını | §4'teki WS dinleyicisini çalıştırın; kumpası oynatın, butona basın | Her yeni değerde, her buton değişiminde ve ≤ 5 s'de bir (batarya) mesaj; `ageMs = 0` | FG-WS-01..03 |

### 2.4 Arayüz

| ID | Senaryo | Adımlar | Beklenen | SRS |
|---|---|---|---|---|
| TC-14 | Göstergeler | Ana sayfayı açın | Tüm rozetler dolu; Wi-Fi kesilince `offline` | FG-UI-01..04 |
| TC-15 | Buton logu + not | Not alanına "A1" yazın, butona basın; sonra notu ✎ ile "A1-düz" yapın | Yeni satır en üstte, not "A1", sonra "A1-düz"; not alanı temizlenir. ⚠️ KS-05: bağlantıya göre satır bırakmada oluşabilir | FG-UI-05..07 |
| TC-16 | CSV | 5 kayıt alıp CSV indirin, Excel'de açın | 6 satır (başlık + 5), eskiden yeniye, Türkçe karakterler doğru | FG-UI-08 |
| TC-17 | Mobil ve tema | Telefonda açın; sistem temasını değiştirin | Yatay kaydırma yok, okunabilir; tema uyumlu | FG-UI-10 |

### 2.5 Kalibrasyon ve OTA

| ID | Senaryo | Adımlar | Beklenen | SRS |
|---|---|---|---|---|
| TC-18 | Ofset kaydı ve kalıcılık | Ofset `-0.050` girin; kumpası oynatın; cihazı RESET'leyin | `cal=-0.050 mm`; `mm` 0,05 düşük; reset sonrası korunur | FG-KAL-01..05, FOG-GVN-06 |
| TC-19 | Hatalı ofset girişi | `curl -X POST -d "offset=abc" …/calibrate`; `curl -X POST …/calibrate` (parametresiz) | Parametresiz → 400. "abc" → ⚠️ KS-13: 0 olarak kaydedilir | FG-KAL-05/06 |
| TC-20 | Başarılı OTA | [GUIDE-009](GUIDE-009_OTA_GUNCELLEME.md) ile yükleyin | Başarı sayfası, yeniden başlatma, ofset korunur, `/log`'da `OTA: update ok` | FG-OTA-01..03/06 |
| TC-21 | Hatalı OTA | (a) `bootloader.bin` yükleyin. (b) Yükleme sırasında WiFi'yi kesin | Cihaz eski firmware ile çalışmaya devam eder; 400 sonuç sayfası (⚠️ KS-02: mesaj yanıltıcı olabilir) | FG-OTA-04, FOG-GVN-04 |

### 2.6 Batarya, Buton, Log, Python, Güvenlik

| ID | Senaryo | Adımlar | Beklenen | SRS |
|---|---|---|---|---|
| TC-22 | Batarya gerilimi | Multimetre ile LiPo gerilimini ölçün; WiFi bağlıyken `batV`'yi okuyun | Fark < 50 mV (kalibrasyonsuz), < 30 mV (GUIDE-011 sonrası) | FG-BAT-01..04 |
| TC-23 | Şarj göstergesi | USB şarjı bağlayın, 2 dk bekleyin; çıkarın | "Voltaj yükseliyor" yanar, çıkarınca söner | FG-BAT-05 |
| TC-24 | Buton debounce/polarite | Butona 20 kez basın, WS dinleyicisinde `buttonPressed` geçişlerini sayın | 20 × `false→true`, basılıyken `true`. ⚠️ KS-05: bağlantıya göre ters/hiç | FG-BTN-01..03 |
| TC-25 | Log API | `curl "…/api/log?since=0"`, sonra `since=<seq>` | İlk çağrı tüm satırlar, ikinci boş dizi; satır kimlikleri ardışık | FG-LOG-01..04 |
| TC-26 | Python scripti | Not Defteri'ni odakta tutup scripti çalıştırın, 3 kez butona basın | 3 satır `xx,xx` değeri; WS kopunca (cihaz RESET) script yeniden bağlanır | FG-PY-01..06 |
| TC-27 | Gizli bilgi | `git status`, `git log --all -- include/wifi_secrets.h` | Dosya izlenmiyor, geçmişte yok | FOG-SEC-01 |

## 3. Sürüm Öncesi Regresyon Listesi

Her `main` birleştirmesinden veya OTA dağıtımından önce asgari:

- [ ] `pio run` başarılı, Flash < %90
- [ ] TC-01, TC-03, TC-04, TC-06 (ölçüm doğruluğu)
- [ ] TC-09, TC-11, TC-13 (ağ)
- [ ] TC-15 veya TC-26 (kayıt akışı — kullanılan yönteme göre)
- [ ] TC-18 (kalibrasyon korunuyor)
- [ ] TC-20 (OTA ile yükleniyor — bir sonraki güncellemenin yapılabilmesi için kritik)
- [ ] Değişiklikle ilgili diğer senaryolar

## 4. Yardımcı Test Araçları

### WS Dinleyicisi (zaman damgalı)

```python
import json, time, websocket
t0 = time.monotonic()
def on_message(ws, m):
    d = json.loads(m)
    print(f"{time.monotonic()-t0:8.3f}s mm={d['mm']:.3f} btn={d['buttonPressed']} bat={d['batV']:.3f}")
websocket.WebSocketApp("ws://kumpas.local:81/", on_message=on_message).run_forever()
```

### JSON Şema Kontrolü

```bash
curl -s http://kumpas.local/api/measure | python -c "import json,sys; d=json.load(sys.stdin); \
req={'rawKey':int,'mm':(int,float),'inch':(int,float),'isInch':bool,'isNegative':bool,'buttonPressed':bool,'offsetMm':(int,float),'batV':(int,float),'batAdcMv':int,'batPct':int,'batCharging':bool,'ageMs':int}; \
bad=[k for k,t in req.items() if not isinstance(d.get(k),t)]; print('OK' if not bad else f'HATALI: {bad}')"
```

## 5. Otomatik Test Altyapısı Önerisi

Amaç: donanım gerektirmeyen mantığı (çözümleme, filtre, birim çıkarımı, batarya yüzdesi) bilgisayarda test etmek (KS-19).

**Adım 1 — Saf mantığı ayırın.** `decode24`, `countsToMillimeters`, `countsToInches`, `updateUnitStateByContinuity`, `batteryPercentFromVolts` ve filtre durum makinesini Arduino başlıklarına bağımlı olmayan `src/caliper_core.{h,cpp}` dosyasına taşıyın (ISR ve `loop()` bu fonksiyonları çağırmaya devam eder).

**Adım 2 — `platformio.ini`'ye yerel ortam:**

```ini
[env:native]
platform = native
test_framework = unity
build_src_filter = +<caliper_core.cpp>
```

**Adım 3 — Testler (`test/test_core/test_core.cpp`):**

```cpp
#include <unity.h>
#include "caliper_core.h"

void test_decode_positive() {
  // 12.35 mm = 2470 count, işaret 0
  auto d = decode24(2470u);
  TEST_ASSERT_FALSE(d.isNegative);
  TEST_ASSERT_EQUAL_UINT32(2470u, d.magnitude);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 12.35f, countsToMillimeters(2470, false));
}

void test_decode_negative_bit21() {
  auto d = decode24((1u << 21) | 2000u);
  TEST_ASSERT_TRUE(d.isNegative);
}

void test_battery_percent_bounds() {
  TEST_ASSERT_EQUAL_UINT8(0, batteryPercentFromVolts(2.5f));
  TEST_ASSERT_EQUAL_UINT8(100, batteryPercentFromVolts(4.5f));
  TEST_ASSERT_EQUAL_UINT8(50, batteryPercentFromVolts(3.6f));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_decode_positive);
  RUN_TEST(test_decode_negative_bit21);
  RUN_TEST(test_battery_percent_bounds);
  return UNITY_END();
}
```

**Adım 4 — Çalıştırma:** `pio test -e native`.

**Önerilen test kapsamı:** her çözümleme kombinasyonu (işaret/birim polaritesi), filtre (tek bozuk çerçeve yayınlanmaz, sıçramada 4 tekrar), birim çıkarımı (KS-24'teki sınır değerleri — 18 mm altı/üstü, 0,787× yanlış pozitif), batarya yüzdesi sınırları.

**Adım 5 — CI:** GitHub Actions'ta `pio run -e esp32s3` (derleme) + `pio test -e native` (birim test). Derleme işinde sahte bir `wifi_secrets.h` oluşturulmalıdır.

Bu altyapı kurulduğunda SRS FOG-BAK-04 ve KS-19 güncellenmelidir.

## 6. Test Raporu Şablonu

```
Tarih:            2026-__-__
Firmware commit:  _______
Kumpas modeli:    _______
Test eden:        _______

| TC | Sonuç | Not |
|----|-------|-----|
| TC-01 | | |
...
Açılan/güncellenen KS kayıtları:
```
