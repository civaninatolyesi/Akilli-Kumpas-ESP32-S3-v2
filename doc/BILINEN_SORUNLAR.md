# Bilinen Sorunlar ve Teknik Borç

**Sürüm:** 1.0 · **Tarih:** 2026-10-10 · **Kapsadığı kod:** commit `2e2619b`

Bu liste, dokümantasyon hazırlanırken kodun satır satır incelenmesiyle bulunan hataları, tutarsızlıkları ve riskleri içerir. Her kayıt [SRS](SRS_YAZILIM_GEREKSINIM_DOKUMANI.md) gereksinimlerine ve [SDD](SDD_YAZILIM_TASARIM_DOKUMANI.md) bölümlerine bağlanır. Bir sorun düzeltildiğinde kaydı silmeyin; **Durum** sütununu `Çözüldü (<commit>)` olarak güncelleyin.

**Önem dereceleri:** 🔴 Yüksek (yanlış ölçüm, güvenlik veya cihazın kullanılamaz hale gelmesi) · 🟠 Orta (özellik beklendiği gibi çalışmıyor) · 🟡 Düşük (kozmetik, bakım, dokümantasyon)

---

## Özet Tablo

| ID | Başlık | Önem | Alan | Durum |
|---|---|---|---|---|
| KS-01 | OTA boyut kontrolü etkisiz, sınır bölümle uyumsuz | 🟠 | OTA | Açık |
| KS-02 | OTA yazma hatasında yanıltıcı mesaj | 🟡 | OTA | Açık |
| KS-03 | OTA/kalibrasyon için kimlik doğrulama yok | 🔴 | Güvenlik | Kabul edildi (ADR-022) |
| KS-04 | WiFi şifresi firmware.bin içinde düz metin | 🟠 | Güvenlik | Açık |
| KS-05 | Buton bağlantısı ile kod mantığı uyumsuz | 🔴 | Buton | Açık |
| KS-06 | `ageMs` ölçümün yaşını göstermiyor | 🟠 | API | Açık |
| KS-07 | Tarayıcı buton logu kısa basışları kaçırabilir, kalıcı değil | 🟠 | UI | Açık |
| KS-08 | Log sayfasında "Temizle" kalıcı değil | 🟡 | UI | Açık |
| KS-09 | WiFi sonradan bağlanırsa mDNS başlamıyor | 🟠 | Ağ | Açık |
| KS-10 | Batarya sabitlerinde kullanılmayan/yanıltıcı değerler | 🟡 | Batarya | Açık |
| KS-11 | `rawKey` JSON'da birim bitini göstermiyor | 🟡 | API | Açık |
| KS-12 | Kalibrasyon ofseti yalnızca mm'ye uygulanıyor | 🟠 | Kalibrasyon | Açık |
| KS-13 | Kalibrasyon girişi doğrulanmıyor | 🟠 | Kalibrasyon | Açık |
| KS-14 | CSS tekrarları ve tanımsız değişken | 🟡 | UI | Açık |
| KS-15 | Kullanılmayan BLE klavye kütüphane iskeleti | 🟡 | Bakım | Açık |
| KS-16 | Python scripti eksik alanlarda çöküyor | 🟡 | Python | Açık |
| KS-17 | Kumpas saat periyodu hakkında çelişkili yorumlar | 🟡 | Dokümantasyon | Açık |
| KS-18 | README'de tutarsızlıklar | 🟡 | Dokümantasyon | Açık |
| KS-19 | Otomatik test yok | 🟠 | Bakım | Açık |
| KS-20 | OTA sırasında ölçüm duruyor | 🟡 | OTA | Kabul edildi |
| KS-21 | Hatalı ama açılan firmware'den otomatik geri dönüş yok | 🟠 | OTA | Açık |
| KS-22 | `spiffs` bölümü (1,44 MB) kullanılmıyor | 🟡 | Bellek | Açık |
| KS-23 | Seviye dönüştürme için pull-up önerisi riskli olabilir | 🟠 | Donanım | Açık |
| KS-24 | Birim çıkarımının algılama sınırı ve yanlış pozitif riski | 🔴 | Ölçüm | Açık |
| KS-25 | HTML sayfaları gönderilirken heap'e kopyalanıyor | 🟡 | Bellek | Açık |
| KS-26 | Kütüphane ve platform sürümleri sabitlenmemiş | 🟠 | Derleme | Açık |
| KS-27 | Ölü kod ve kullanılmayan değişkenler | 🟡 | Bakım | Açık |

---

## Ayrıntılar

### KS-01 — OTA boyut kontrolü etkisiz, sınır bölümle uyumsuz 🟠

- **Yer:** [web_ui.cpp:20](../src/web_ui.cpp#L20), [web_ui.cpp:842-848](../src/web_ui.cpp#L842-L848)
- **Gözlem:** `UPDATE_MAX_SIZE = 3 MB` iken OTA slotları 0x140000 = **1 310 720 bayt**tır. Ayrıca kontrol `UPLOAD_FILE_START` anında `upload.totalSize` ile yapılır; bu noktada henüz veri alınmadığından değer 0'dır ve kontrol hiçbir zaman tetiklenmez.
- **Etki:** Büyük dosya yazma aşamasında `Update.write` başarısız olur; kullanıcı yanıltıcı mesaj görür (KS-02). Cihaz eski firmware ile çalışmaya devam eder (güvenli hata).
- **Öneri:** `UPDATE_MAX_SIZE`'ı kaldırıp sınırı `esp_ota_get_next_update_partition(NULL)->size` ile almak; mümkünse istemciden `Content-Length` veya ayrı bir `size` alanı alıp `Update.begin(size)` çağırmak. README'deki "3 MB" ifadesini "1,25 MB" olarak düzeltmek.
- **İlgili:** FG-OTA-05, [ADR-015](adr/ADR-015_TARAYICI_UZERINDEN_OTA_VE_CIFT_SLOT.md)

### KS-02 — OTA yazma hatasında yanıltıcı mesaj 🟡

- **Yer:** [web_ui.cpp:823-827](../src/web_ui.cpp#L823-L827), [web_ui.cpp:857-861](../src/web_ui.cpp#L857-L861)
- **Gözlem:** Yazma hatası veya iptal sonrası `updateInProgress = false` olur; POST işleyicisi bu durumda "Yukleme baslamadi." gösterir. Gerçek neden yalnızca `/log`'da görülür.
- **Öneri:** Son hata metnini bir `String lastOtaError` değişkeninde tutup sonuç sayfasında göstermek.
- **İlgili:** FG-OTA-04

### KS-03 — OTA/kalibrasyon için kimlik doğrulama yok 🔴

- **Yer:** [web_ui.cpp:803-871](../src/web_ui.cpp#L803-L871)
- **Gözlem:** Aynı ağdaki herhangi bir cihaz `POST /update` ile firmware yükleyebilir veya `POST /calibrate` ile ofseti değiştirebilir.
- **Etki:** Kötü niyetli firmware; fark edilmeyen ölçüm hatası.
- **Öneri:** `server.authenticate(user, pass)` ile HTTP Basic Auth; kimlik bilgisini `wifi_secrets.h`'ye eklemek. Bkz. [ADR-022](adr/ADR-022_KIMLIK_DOGRULAMASIZ_YEREL_AG_GUVENLIK_MODELI.md) — şu an bilinçli olarak kabul edilmiş risk.
- **İlgili:** FOG-SEC-03

### KS-04 — WiFi şifresi firmware.bin içinde düz metin 🟠

- **Yer:** `include/wifi_secrets.h`, [web_ui.cpp:770](../src/web_ui.cpp#L770)
- **Gözlem:** `WIFI_SSID` ve `WIFI_PASSWORD` derleme zamanında ikili dosyaya gömülür. `strings firmware.bin` ile okunabilir.
- **Öneri:** `firmware.bin` dosyasını kimseyle paylaşmamak (dokümante edildi: [GUIDE-009](guide/GUIDE-009_OTA_GUNCELLEME.md)). Uzun vadede kimlik bilgilerini NVS'de tutup ilk açılışta AP + yapılandırma sayfası (ör. WiFiManager yaklaşımı) ile almak.
- **İlgili:** FOG-SEC-02, [ADR-016](adr/ADR-016_WIFI_STA_MDNS_VE_GIZLI_BILGI_YONETIMI.md)

### KS-05 — Buton bağlantısı ile kod mantığı uyumsuz 🔴

- **Yer:** [main.cpp:11](../src/main.cpp#L11), [main.cpp:294](../src/main.cpp#L294), [main.cpp:332-343](../src/main.cpp#L332-L343), README §5
- **Gözlem:** Pin `INPUT_PULLUP` yapılandırılır (boşta HIGH), kod ise HIGH'ı "basılı" kabul eder.
  - README'deki **buton → 3,3 V** bağlantısında pin her zaman HIGH'tır: basış hiç algılanmaz.
  - **buton → GND** bağlantısında mantık terstir: bırakma "basılı" olarak raporlanır, UI ve Python **bırakma anında** kayıt yapar ve boşta `btn=pressed` görünür.
  - Yalnızca **aktif-yüksek itme-çekme çıkışlı** bir kaynakta (ör. TTP223 dokunmatik modülü) doğru çalışır.
- **Öneri (birini seçin ve README'yi güncelleyin):**
  - A) Buton → GND, `INPUT_PULLUP`, `pressed = (digitalRead(BUTTON_PIN) == LOW)`, başlangıç değerlerini `false` yapmak. *(Önerilen — en yaygın ve gürültüye dayanıklı)*
  - B) Buton → 3,3 V, `INPUT_PULLDOWN`, mevcut mantık korunur.
- **İlgili:** FG-BTN-03, FG-UI-05, [ADR-018](adr/ADR-018_BUTON_GIRISI_VE_DEBOUNCE.md), [GUIDE-002](guide/GUIDE-002_DONANIM_BAGLANTISI.md)

### KS-06 — `ageMs` ölçümün yaşını göstermiyor 🟠

- **Yer:** [web_ui.cpp:890](../src/web_ui.cpp#L890), [web_ui.cpp:900](../src/web_ui.cpp#L900), [web_ui.cpp:913](../src/web_ui.cpp#L913)
- **Gözlem:** `lastUpdateMs` buton ve batarya güncellemelerinde de yenilenir. Batarya en geç 5 s'de bir rapor verdiği için, kumpas kapalı/kopuk olsa bile `ageMs` 5 000'i geçmez.
- **Etki:** İstemciler "kumpas veri göndermiyor" durumunu `ageMs` ile algılayamaz.
- **Öneri:** Ayrı bir `lastMeasureMs` tutup `ageMs`'i ondan hesaplamak; gerekirse genel durum için yeni bir `stateAgeMs` alanı eklemek (geriye uyumluluk için `ageMs` adı ölçüm anlamına çekilmeli).
- **İlgili:** FG-HTTP-06

### KS-07 — Tarayıcı buton logu kısa basışları kaçırabilir, kalıcı değil 🟠

- **Yer:** [web_ui.cpp:435-442](../src/web_ui.cpp#L435-L442), [web_ui.cpp:451](../src/web_ui.cpp#L451)
- **Gözlem:** UI butonu 200 ms'lik sorgu ile örnekler; iki sorgu arasında basılıp bırakılan buton görülmez. Log tarayıcı belleğindedir; sayfa yenilenirse/kapanırsa kaybolur. Birden fazla sekme farklı loglar tutar.
- **Öneri:** Ana sayfayı WS'e geçirmek (WS her değişimi push eder) veya cihaz tarafında "basış sayacı" (`pressCount`) alanı eklemek; istemci sayaç artışını kayıt olarak işler. Logu `localStorage`'da yedeklemek.
- **İlgili:** FG-UI-05, [ADR-020](adr/ADR-020_ISTEMCI_TARAFI_BUTON_LOGU_VE_CSV.md)

### KS-08 — Log sayfasında "Temizle" kalıcı değil 🟡

- **Yer:** [web_ui.cpp:530-533](../src/web_ui.cpp#L530-L533)
- **Gözlem:** Temizle `lastLogSeq = 0` yapar; sonraki sorgu tampondaki tüm satırları geri getirir.
- **Öneri:** `lastLogSeq`'i sıfırlamamak (yalnızca DOM'u temizlemek).
- **İlgili:** FG-UI-12

### KS-09 — WiFi sonradan bağlanırsa mDNS başlamıyor 🟠

- **Yer:** [web_ui.cpp:792-801](../src/web_ui.cpp#L792-L801)
- **Gözlem:** `startMdns()` yalnızca açılıştaki ilk bağlantı başarılıysa çağrılır. Açık bir yeniden bağlanma/olay işleyicisi yoktur.
- **Etki:** Router geç açılırsa cihaz IP ile erişilebilir olur ama `kumpas.local` çalışmaz; Python scripti (varsayılan URL) bağlanamaz.
- **Öneri:** `WiFi.onEvent(ARDUINO_EVENT_WIFI_STA_GOT_IP, …)` ile mDNS'i (yeniden) başlatmak ve IP'yi loglamak; `WiFi.setAutoReconnect(true)` açıkça çağırmak.
- **İlgili:** FG-NET-05

### KS-10 — Batarya sabitlerinde kullanılmayan/yanıltıcı değerler 🟡

- **Yer:** [main.cpp:19-23](../src/main.cpp#L19-L23)
- **Gözlem:** `BATTERY_SAMPLE_MS` (500) hiç kullanılmaz. `BATTERY_SAMPLE_MS_CHARGING` ve `_IDLE` ikisi de 2000'dir; yorumlar "daha sık/daha seyrek" der. `BATTERY_REPORT_MAX_INTERVAL_MS = 5000` için yorum "en az dakikada bir" der.
- **Öneri:** Kullanılmayan sabiti silmek, yorumları değerlerle eşlemek veya değerleri yorumlara göre ayarlamak (ör. şarjda 1000, boşta 5000).
- **İlgili:** FG-BAT-07

### KS-11 — `rawKey` JSON'da birim bitini göstermiyor 🟡

- **Yer:** [web_ui.cpp:690](../src/web_ui.cpp#L690)
- **Gözlem:** `lastRawKey & 0x3FFFFF` bit 22–23'ü düşürür. Birim biti 23 olduğundan UI'daki `raw=` alanı birim bitini hiç göstermez; yeni kumpas uyarlarken teşhis zorlaşır.
- **Öneri:** Maskeyi `0xFFFFFF` yapmak (alan tipi aynı kalır, değer aralığı genişler — istemciler etkilenmez).

### KS-12 — Kalibrasyon ofseti yalnızca mm'ye uygulanıyor 🟠

- **Yer:** [web_ui.cpp:884-890](../src/web_ui.cpp#L884-L890)
- **Gözlem:** `inch` ve `isNegative` ofsetsizdir. Ör. ofset −0,10 mm ve gerçek değer 0,05 mm iken `mm = −0,05` ama `isNegative = false`, `inch` = +0,00197. Ayrıca ofset değiştirildiğinde ekrandaki değer, kumpastan yeni bir stabil değer gelene kadar eski kalır.
- **Öneri:** Ofseti `webUpdateMeasurement` yerine `buildMeasureJson` içinde uygulamak; `inch = (mm+ofset)/25,4`, `isNegative = (mm+ofset) < 0`.
- **İlgili:** FG-KAL-07, [ADR-014](adr/ADR-014_KALIBRASYON_OFSETI_NVS_PREFERENCES.md)

### KS-13 — Kalibrasyon girişi doğrulanmıyor 🟠

- **Yer:** [web_ui.cpp:815](../src/web_ui.cpp#L815)
- **Gözlem:** `String::toFloat()` geçersiz metinde 0 döndürür; "abc" girişi mevcut ofseti sessizce sıfırlar. Değer aralığı sınırlanmaz (ör. 1000 mm kabul edilir).
- **Öneri:** Sayısal biçimi kontrol edip `|ofset| ≤ 5 mm` gibi bir sınır uygulamak; hatada 400 döndürmek.
- **İlgili:** FG-KAL-06

### KS-14 — CSS tekrarları ve tanımsız değişken 🟡

- **Yer:** [web_ui.cpp:97-115](../src/web_ui.cpp#L97-L115), [web_ui.cpp:583](../src/web_ui.cpp#L583), [web_ui.cpp:631](../src/web_ui.cpp#L631)
- **Gözlem:** INDEX_HTML'de `:root` bloğu üç kez tekrarlanır. CAL_HTML ve UPDATE_HTML `var(--accent-soft)` kullanır ama tanımlamaz; buton kenarlığı görünmez.
- **Öneri:** Tek `:root` bloğu; ortak CSS'i tek bir sabit/uç noktada (`/style.css`) toplamak.

### KS-15 — Kullanılmayan BLE klavye kütüphane iskeleti 🟡

- **Yer:** [lib/ESP32_BLE_Keyboard/library.json](../lib/ESP32_BLE_Keyboard/library.json)
- **Gözlem:** `library.json` `src/BleKeyboard.cpp`'ye işaret eder ama `src/` boştur; hiçbir kaynak bu kütüphaneyi içermez.
- **Öneri:** Kaldırmak ya da [ADR-021](adr/ADR-021_PYTHON_ILE_KLAVYE_EMULASYONU.md)'deki gelecek planına göre tamamlamak.

### KS-16 — Python scripti eksik alanlarda çöküyor 🟡

- **Yer:** [measure_ws.py:48-51](../scripts/measure_ws.py#L48-L51)
- **Gözlem:** `print` f-string'i `mm:.2f` ve `raw_key:06X` biçimlendirir; alanlardan biri `None` ise `TypeError` fırlar. `None` kontrolü bu satırdan sonradır. websocket-client geri çağrı istisnalarını yakalayıp `on_error`'a iletir; script çökmez ama o mesaj işlenmez.
- **Öneri:** `None` kontrolünü yazdırmadan önceye almak.
- **İlgili:** FG-PY-02

### KS-17 — Kumpas saat periyodu hakkında çelişkili yorumlar 🟡

- **Yer:** [main.cpp:43-45](../src/main.cpp#L43-L45), README §2
- **Gözlem:** Kod yorumu "yüzlerce µs", README "~1 ms" der; `MIN_EDGE_US` ise geçmişte 40 µs'den 5 µs'ye indirilmiştir (gerçek periyot muhtemelen daha kısa). Gerçek değer ölçülüp belgelenmemiştir.
- **Öneri:** Logic analyzer ölçümünü [GUIDE-010](guide/GUIDE-010_YENI_KUMPAS_MODELINE_UYARLAMA.md)'daki tabloya eklemek, yorumları güncellemek.

### KS-18 — README'de tutarsızlıklar 🟡

- README §12 `web_ui.h` için "Sadece 4 fonksiyon" der, 6 fonksiyon listeler.
- README §10 ve §11 "Maksimum firmware boyutu 3 MB" der; gerçek sınır 1,25 MB (KS-01).
- README §7 `raw` için "22-bit çerçeve" der; bu, maskeleme yüzünden doğrudur ama birim bitinin görünmediği belirtilmemiştir (KS-11).
- README §7 JSON örneğinde `batV: 3.980` için `batPct: 87` gösterilir; formüle göre %82 olmalıdır.
- README §5 buton bağlantısı kodla uyumsuzdur (KS-05).
- README §11'de "PlatformIO penv" alt bölümü, "Ölçüm gelmiyor" başlığı ile onun maddeleri arasına eklenmiştir; maddeler yanlış başlığın altında kalmıştır.
- README §18 "İki OTA slotu → bozuk firmware'de güvenli dönüş" der; bu yalnızca yarım kalan yüklemeler için geçerlidir (KS-21).
- README §18 "RAM kısıtlı (320KB)" der; ESP32-S3'te dahili SRAM 512 KB'tır (kullanılabilir heap daha azdır).

### KS-19 — Otomatik test yok 🟠

- **Gözlem:** Çözümleme, filtre ve birim çıkarımı saf fonksiyonlara yakın olmasına rağmen birim testi yoktur.
- **Öneri:** `decode24`, `countsToMillimeters`, `updateUnitStateByContinuity` ve filtreyi ayrı bir `caliper_core.{h,cpp}` dosyasına taşıyıp PlatformIO `native` ortamında Unity testleri yazmak. Taslak: [GUIDE-015 §5](guide/GUIDE-015_TEST_VE_DOGRULAMA.md#5-otomatik-test-altyapısı-önerisi).
- **İlgili:** FOG-BAK-04

### KS-20 — OTA sırasında ölçüm duruyor 🟡

- **Gözlem:** Senkron `WebServer` yükleme boyunca `loop()`'u bloklar. Kabul edilebilir (bakım işlemi), ancak operatöre belirtilmelidir.
- **Durum:** Kabul edildi; GUIDE-009'da dokümante edildi.

### KS-21 — Hatalı ama açılan firmware'den otomatik geri dönüş yok 🟠

- **Gözlem:** İki slot yalnızca **yarıda kalan** yüklemeye karşı korur. Yeni firmware açılıp hatalı çalışırsa (ör. WiFi'ye bağlanamıyorsa) eski sürüme otomatik dönülmez; firmware kendini "geçerli" olarak işaretleyen bir mekanizma içermez ve Arduino çekirdeğinin varsayılan önyükleyicisinde uygulama geri alma (rollback) özelliğine güvenilmemelidir.
- **Kurtarma:** USB ile yeniden yükleme ([GUIDE-009 §8](guide/GUIDE-009_OTA_GUNCELLEME.md)).
- **İlgili:** FOG-GVN-05

### KS-22 — `spiffs` bölümü (1,44 MB) kullanılmıyor 🟡

- **Gözlem:** Bölüm tablosu 1,44 MB `spiffs` ayırır; kod dosya sistemi kullanmaz.
- **Öneri:** Kalıcı buton logu (KS-07) veya statik dosyalar için LittleFS olarak kullanmak; ya da uygulama slotlarını büyütmek için alanı paylaştırmak.

### KS-23 — Seviye dönüştürme için pull-up önerisi riskli olabilir 🟠

- **Yer:** README §3
- **Gözlem:** README "en basit çözüm: DATA ve CLOCK'u 10 kΩ ile 3,3 V'a çekmek" önerir. Kumpas çıkışı itme-çekme ise bu, kumpasın 1,5 V beslemesine akım enjekte edebilir; açık-drenaj değilse işe yaramayabilir de. Ayrıca 1,5 V'luk "yüksek" seviye ESP32-S3'ün VIH eşiğinin (0,75 × VDD ≈ 2,5 V) altındadır.
- **Öneri:** Doğrulanmış yöntemleri kullanmak: NPN/N-MOSFET transistörlü evirici (yazılımda bit polaritesi ters çevrilir) veya karşılaştırıcı/düşük eşikli seviye dönüştürücü. Bkz. [GUIDE-002 §4](guide/GUIDE-002_DONANIM_BAGLANTISI.md#4-seviye-dönüştürme).

### KS-24 — Birim çıkarımının algılama sınırı ve yanlış pozitif riski 🔴

- **Yer:** [main.cpp:99-101](../src/main.cpp#L99-L101), [main.cpp:133-173](../src/main.cpp#L133-L173)
- **Gözlem 1 (algılanamama):** Birim biti yalnızca ilk çerçevede kullanılır. Sonrasında birim değişimi yalnızca ≥ 4 mm sapma ile algılanır; bu da kumpas ~15–19 mm'nin üzerindeyken mümkündür (SDD §4.4). Kumpas sıfıra yakınken mm/inç tuşuna basılırsa cihaz yanlış birimde kalır ve tüm ölçümler 1,27 veya 0,787 kat hatalı olur — yeniden başlatmaya kadar.
- **Gözlem 2 (yanlış pozitif):** Çene son yayınlanan değerin 0,787 (mm durumunda) katına ±0,4 mm hassasiyetle hızla kapatılıp 3–4 çerçeve sabit tutulursa birim yanlışlıkla değiştirilir (ör. 50,00 → 39,37 mm).
- **Geçici önlem:** Kumpası **her zaman mm modunda** kullanın; birim değiştirmeyin.
- **Öneri:** Kumpasın gerçekten birim biti gönderip göndermediğini tespit edin ([GUIDE-010](guide/GUIDE-010_YENI_KUMPAS_MODELINE_UYARLAMA.md)). Gönderiyorsa `g_autoUnitFix = false` yapıp bite güvenin. Göndermiyorsa birimi sabit mm kabul edip otomatik değişimi devre dışı bırakmak, sürekli otomatik tahminden daha güvenlidir.
- **İlgili:** FG-BRM-02/03, [ADR-008](adr/ADR-008_SUREKLILIK_ILE_BIRIM_CIKARIMI.md)

### KS-25 — HTML sayfaları gönderilirken heap'e kopyalanıyor 🟡

- **Yer:** [web_ui.cpp:654-668](../src/web_ui.cpp#L654-L668)
- **Gözlem:** `server.send(code, type, const char*)` içeriği geçici `String`'e kopyalar (~16 KB). Parçalanmış heap'te başarısız olabilir.
- **Öneri:** `server.send_P(200, "text/html; charset=utf-8", INDEX_HTML)` veya `sizeof` ile uzunluk verilen aşırı yüklemeyi kullanmak.

### KS-26 — Kütüphane ve platform sürümleri sabitlenmemiş 🟠

- **Yer:** [platformio.ini](../platformio.ini)
- **Gözlem:** `platform = espressif32` ve `links2004/WebSockets` sürümsüzdür. Gelecekte farklı makinede derleme farklı çekirdek/kütüphane sürümü çekebilir (ör. Arduino-ESP32 3.x API değişiklikleri).
- **Öneri:** Çalıştığı doğrulanan sürümleri `platform = espressif32@<sürüm>` ve `links2004/WebSockets@^<sürüm>` olarak sabitlemek. Kullanılan sürümler `pio pkg list` ile görülebilir ([GUIDE-003](guide/GUIDE-003_DERLEME_YUKLEME_VE_SERI_MONITOR.md)).

### KS-27 — Ölü kod ve kullanılmayan değişkenler 🟡

- `toMillimeters()`, `toInches()` ([main.cpp:261-283](../src/main.cpp#L261-L283)) çağrılmaz.
- `lastPublishedInch`, `lastPublishedIsInch`, `prevReportedAdcMv` yazılır ama okunmaz.
- `countsSignedFromRaw()` `raw24` parametresini kullanmaz.
- `g_signBit` vb. "çalışma zamanı" değişkenleri artık değişmiyor (seri komutlar `4fed220` ile kaldırıldı).
- INDEX_HTML'deki `.log-wrap` ve `.note-actions` sınıfları kullanılmaz.
- **Öneri:** Temizlik commit'i; davranış değişmez.

---

## Değişiklik Geçmişi

| Tarih | Değişiklik |
|---|---|
| 2026-10-10 | İlk liste (KS-01…KS-27). |
