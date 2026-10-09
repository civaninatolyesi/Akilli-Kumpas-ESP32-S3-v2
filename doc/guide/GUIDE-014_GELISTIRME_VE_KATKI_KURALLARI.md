# GUIDE-014: Geliştirme ve Katkı Kuralları

**Hedef kitle:** Kodu değiştiren herkes (insan veya AI asistanı)  
**İlgili:** [SDD §12 Tasarım Değişmezleri](../SDD_YAZILIM_TASARIM_DOKUMANI.md#12-tasarım-değişmezleri), [ADR-010](../adr/ADR-010_MODUL_AYRIMI_VE_WEB_UI_ARAYUZU.md), [ADR-023](../adr/ADR-023_DOKUMANTASYON_YAPISI_ADR_VE_GUIDE.md)

---

## 1. Kod Stili

Mevcut kodla tutarlı olun:

| Konu | Kural | Örnek |
|---|---|---|
| Girinti | 2 boşluk | |
| Derleme zamanı sabitleri | `static constexpr`, `UPPER_SNAKE_CASE` | `static constexpr uint32_t FRAME_GAP_US = 1500;` |
| Dosya kapsamlı değişkenler | `static`, `g_` öneki (main.cpp) veya önek yok (web_ui.cpp) | `g_shiftReg`, `lastMm` |
| Fonksiyonlar | `camelCase`; dışa açık olanlar `web…` önekli | `decode24`, `webUpdateButton` |
| Tipler | Sabit genişlikli tamsayılar | `uint32_t`, `uint8_t` |
| Zaman farkları | Taşmaya dayanıklı işaretsiz çıkarma | `(uint32_t)(nowMs - lastMs) >= X` |
| Yorumlar | İngilizce, kısa, "neden"i anlatan | |
| Kullanıcıya dönük metinler | Türkçe (UI, seri mesajlar, Python mesajları) | |
| Dinamik bellek | Kaçının; `String` gerekiyorsa `reserve` | |

## 2. Değişmez Kurallar (ihlal = sessiz hata)

1. **ISR'ye dokunuyorsanız:** yalnızca IRAM-güvenli çağrılar; `Serial`, `webLogf`, `String`, `new/malloc`, `digitalRead`, `micros`, `delay` **yok**. Paylaşılan değişkenler `volatile`; çok baytlı paylaşım `g_mux` içinde.
2. `web_ui.h` fonksiyonları yalnızca `setup()`/`loop()` bağlamından çağrılır.
3. `STABLE_REPEATS_NORMAL ≥ 2`.
4. JSON alanları silinmez/yeniden adlandırılmaz ([ADR-013](../adr/ADR-013_JSON_OLCUM_SOZLESMESI.md)).
5. HTML `id`'leri değiştirilirse JS de güncellenir (SRS AG-KUA-03).
6. Batarya pini ADC1'de kalır.
7. Firmware < 1 310 720 bayt.
8. `wifi_secrets.h` ve `firmware.bin` asla commit edilmez.

## 3. Sık Yapılan Değişiklikler — Tarifler

### 3.1 JSON'a ve UI'ya Yeni Alan Eklemek

Örnek: `frameCount` (alınan toplam çerçeve) alanı.

1. **`include/web_ui.h`** — yeni bildirim fonksiyonu:
   ```cpp
   void webUpdateStats(uint32_t frameCount);
   ```
2. **`src/web_ui.cpp`** — önbellek değişkeni, JSON alanı, fonksiyon:
   ```cpp
   static uint32_t lastFrameCount = 0;
   // buildMeasureJson içinde, '}' öncesine:
   json += ",\"frameCount\":" + String((unsigned long)lastFrameCount);
   // dosya sonuna:
   void webUpdateStats(uint32_t frameCount) { lastFrameCount = frameCount; }
   ```
   `json.reserve(160)` değerini gerekirse artırın.
3. **`src/main.cpp`** — değeri üretip çağırın.
4. **UI** — INDEX_HTML'e rozet (`<span class="pill" id="frames">`) ve `tick()` içine güncelleme satırı.
5. **Doküman** — [SDD §6.3](../SDD_YAZILIM_TASARIM_DOKUMANI.md#63-ölçüm-json-sözleşmesi), [GUIDE-006](GUIDE-006_HTTP_VE_WEBSOCKET_API_REFERANSI.md) alan tablosu, gerekiyorsa SRS'e gereksinim.

> Her `webUpdate*` çağrısı WS yayını tetikler. Sık değişen bir değeri her döngüde bildirmeyin; değiştiğinde veya belirli aralıklarla bildirin.

### 3.2 Yeni HTTP Uç Noktası

`webBegin()` içinde, `server.onNotFound` satırından önce:

```cpp
server.on("/api/info", HTTP_GET, []() {
  server.send(200, "application/json", "{\"fw\":\"" __DATE__ " " __TIME__ "\"}");
});
```

Değişiklik yapan (POST) uç noktalar için [ADR-022](../adr/ADR-022_KIMLIK_DOGRULAMASIZ_YEREL_AG_GUVENLIK_MODELI.md)'yi okuyun ve log satırı (`webLogf`) ekleyin.

### 3.3 Yeni Ayarlanabilir Sabit

`main.cpp` başındaki ilgili gruba `static constexpr` olarak ekleyin, yorumda birimi ve güvenli aralığı yazın, [GUIDE-013](GUIDE-013_PARAMETRE_REFERANSI.md)'e satır ekleyin.

### 3.4 Kalıcı Ayar (NVS) Eklemek

`Preferences` ile mevcut `kumpas` ad alanını kullanın; anahtar adı ≤ 15 karakter. `loadCalibration()` benzeri bir yükleyiciyi `webBegin()` başında çağırın. [SDD §6.5](../SDD_YAZILIM_TASARIM_DOKUMANI.md#65-kalıcı-depolama-nvs)'e ekleyin.

## 4. Git İş Akışı

- `main` dalı her zaman derlenebilir olmalıdır.
- Özellik/düzeltme için dal: `feat/<kısa-ad>`, `fix/<kısa-ad>`, `docs/<kısa-ad>`.
- **Commit mesajı:** [Conventional Commits](https://www.conventionalcommits.org/) biçimi önerilir (geçmişte `feat:` ve `docs:` kullanılmış):
  ```
  fix(button): invert pressed logic for button-to-GND wiring

  Closes KS-05. Button is now read as active-low with INPUT_PULLUP.
  ```
  Türler: `feat`, `fix`, `docs`, `refactor`, `perf`, `test`, `build`, `chore`. Kapsam örnekleri: `isr`, `filter`, `unit`, `web`, `ota`, `battery`, `button`, `py`.
- Bilinen bir sorunu kapatıyorsanız mesajda `KS-nn` belirtin ve [BILINEN_SORUNLAR.md](../BILINEN_SORUNLAR.md)'de durumu güncelleyin.

## 5. PR / Değişiklik Kontrol Listesi

- [ ] `pio run` uyarısız derleniyor; Flash kullanımı not edildi
- [ ] SDD §12 değişmezleri korunuyor
- [ ] İlgili [GUIDE-015](GUIDE-015_TEST_VE_DOGRULAMA.md) test senaryoları gerçek donanımda çalıştırıldı
- [ ] Davranış değiştiyse SRS gereksinimi/durumu güncellendi
- [ ] Tasarım değiştiyse SDD güncellendi
- [ ] Mimari bir seçim yapıldıysa yeni ADR yazıldı (§6)
- [ ] Yeni parametre varsa GUIDE-013 güncellendi
- [ ] Kullanıcı prosedürü değiştiyse ilgili GUIDE ve kök README güncellendi
- [ ] KS kaydı kapandıysa BILINEN_SORUNLAR.md güncellendi
- [ ] Teşhis/geçici kod kaldırıldı
- [ ] `wifi_secrets.h` / `firmware.bin` commit'te yok

## 6. Ne Zaman ADR Yazılır?

Şu sorulardan birine "evet" ise:

- Birden fazla makul seçenek vardı ve biri seçildi mi?
- Seçim geri alınması zor veya pahalı mı (protokol, veri şeması, bölüm tablosu, bağımlılık)?
- Bir risk bilinçli olarak kabul edildi mi?
- Mevcut bir ADR'nin kararı değişiyor mu?

**Süreç:**

1. `doc/adr/` altında sıradaki numarayla dosya oluşturun: `ADR-024_KISA_BASLIK.md` (Türkçe karakter yok, büyük harf, alt çizgi).
2. [doc/README.md şablonunu](../README.md#şablonlar) kullanın; durum `Önerildi`.
3. Kodla aynı PR'da gözden geçirin; birleştirilince `Kabul edildi`.
4. Eski bir kararı değiştiriyorsa eskisinin durumunu `Yerine geçildi: ADR-024` yapın — eski ADR'nin gövdesini silmeyin.
5. [doc/README.md](../README.md) dizinine satır ekleyin.

## 7. AI Asistanı ile Çalışmak

AI asistanına (Claude, Copilot vb.) iş verirken bağlam olarak şunları verin:

1. `doc/SDD_YAZILIM_TASARIM_DOKUMANI.md` (en azından §3, §8, §12)
2. İlgili ADR'ler
3. Değiştirilecek kaynak dosya(lar)

Örnek istem:

```
Bağlam: doc/SDD_YAZILIM_TASARIM_DOKUMANI.md §12 değişmezleri ve ADR-018 ekte.
Görev: KS-05'i Seçenek A ile düzelt (buton GND'ye, aktif-düşük).
Kısıtlar: JSON alanları ve HTML id'leri değişmesin; ISR'ye dokunma.
Teslim: kod değişikliği + README §5, GUIDE-002 §5, ADR-018 durum güncellemesi,
        BILINEN_SORUNLAR.md'de KS-05 → Çözüldü.
```
