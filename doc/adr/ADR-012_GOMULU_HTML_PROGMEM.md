# ADR-012: Web Arayüzünün Firmware İçine Gömülmesi (Raw String / PROGMEM)

| Alan | Değer |
|---|---|
| **Durum** | Kabul edildi |
| **Karar tarihi** | 2026-04-27 (commit `1c9ff0d`) |
| **Dokümante edilme** | 2026-10-10 |
| **İlgili** | [ADR-011](ADR-011_HTTP_POLLING_VE_WEBSOCKET_PUSH.md), SRS AG-KUA-02, FOG-KAY-02, SDD §4.8, [KS-14](../BILINEN_SORUNLAR.md#ks-14--css-tekrarları-ve-tanımsız-değişken-), [KS-25](../BILINEN_SORUNLAR.md#ks-25--html-sayfaları-gönderilirken-heape-kopyalanıyor-) |

## Bağlam

Arayüz atölyede, çoğu zaman internet erişimi olmayan bir yerel ağda açılacak. Kurulumu kolay olmalı: tek bir `firmware.bin` cihazın tamamını güncellemeli. Dosya sistemi kullanmak ayrı bir yükleme adımı (`uploadfs`) ve OTA'da ayrı imaj demektir.

## Karar

- Dört sayfa (`INDEX_HTML`, `LOG_HTML`, `CAL_HTML`, `UPDATE_HTML`) `web_ui.cpp` içinde C++ raw string literal (`R"HTML(… )HTML"`) olarak, `static const char … PROGMEM` dizileri halinde tutulur.
- HTML, CSS ve JS aynı dosyada, **harici kaynak (CDN, font, kütüphane) olmadan** yazılır.
- Sayfalar `text/html; charset=utf-8` ile sunulur; arayüz dili Türkçe'dir.
- Açık/koyu tema `color-scheme: light dark` ile tarayıcıya bırakılır; 640 px altı için medya sorgusu vardır.

| Sayfa | Boyut |
|---|---|
| INDEX_HTML | 15 898 B |
| LOG_HTML | 3 874 B |
| CAL_HTML | 2 110 B |
| UPDATE_HTML | 1 937 B |

## Değerlendirilen Alternatifler

| Seçenek | Neden seçilmedi |
|---|---|
| SPIFFS/LittleFS'te dosyalar | Ayrı yükleme adımı; OTA sırasında UI ile firmware uyumsuz kalabilir |
| Gzip'lenmiş, derleme betiğiyle üretilen başlık dosyası | Daha küçük ve düzenli; ama derleme betiği gerektirir. **Gelecekte değerlendirilebilir** |
| CDN'den çerçeve (Bootstrap, Vue) | İnternetsiz çalışmaz |

## Sonuçlar

**Olumlu**
- Tek imaj = tutarlı firmware + UI.
- İnternetsiz tam işlev.

**Olumsuz / Riskler**
- ESP32'de `PROGMEM` etkisizdir (sabit veriler zaten flash'a eşlenir); ancak `server.send(…, const char*)` içeriği geçici `String`'e kopyalar → ana sayfa için istek başına ~16 KB heap (KS-25). `send_P` kullanılmalı.
- HTML'i C++ dosyasında düzenlemek sözdizimi vurgulama ve biçimlendirme desteğini azaltır.
- CSS tekrarları birikmiş durumda (KS-14); her sayfa kendi stilini taşır.
- HTML `id`'leri JS ile sıkı bağlıdır; değiştirirken SRS AG-KUA-03 listesine bakılmalı.

## Uygulama Notları

- [web_ui.cpp:89-652](../../src/web_ui.cpp#L89-L652)
- Raw string sonlandırıcı `)HTML"` olduğundan HTML içinde `)HTML"` dizisi geçmemelidir.
