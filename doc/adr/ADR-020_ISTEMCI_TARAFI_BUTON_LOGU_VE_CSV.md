# ADR-020: Buton Logunun Tarayıcıda Tutulması ve CSV Dışa Aktarma

| Alan | Değer |
|---|---|
| **Durum** | Kabul edildi |
| **Karar tarihi** | 2026-05-28 (`2ffeeb4`); not düzenleme 2026-06-03 (`7649d60`) |
| **Dokümante edilme** | 2026-10-10 |
| **İlgili** | [ADR-011](ADR-011_HTTP_POLLING_VE_WEBSOCKET_PUSH.md), [ADR-018](ADR-018_BUTON_GIRISI_VE_DEBOUNCE.md), SRS FG-UI-05..09, SDD §4.8, [GUIDE-005](../guide/GUIDE-005_WEB_ARAYUZU_KULLANIMI.md), KS-07 |

## Bağlam

Operatör bir parti parçayı ölçerken her ölçümü butona basarak kaydetmek, isteğe bağlı not (parça no, konum) eklemek ve sonunda listeyi Excel'e almak istiyor. Python scripti doğrudan Excel'e yazabiliyor ama her bilgisayarda Python kurulu değil.

## Karar

- Kayıt listesi **tarayıcı sekmesinin belleğinde** (DOM tablosu) tutulur; cihazda saklanmaz.
- Kayıt tetikleyicisi: `/api/measure` yanıtlarında `buttonPressed`'in `false → true` geçişi. İlk yanıt yalnızca başlangıç durumu olarak kullanılır.
- Her satır: tarayıcı saatiyle `HH:MM:SS.mmm`, mm (2 ondalık), inç (4 ondalık), not.
- Üstteki "Not" alanı bir sonraki kayda eklenir ve temizlenir; her satırın notu ✎ ile sonradan düzenlenebilir.
- En fazla 200 satır; en yenisi üstte.
- **CSV:** eskiden yeniye sıralı, başlık `Zaman,mm,in,Not`, her hücre çift tırnaklı (`"` → `""`), UTF-8 BOM (Excel'in Türkçe karakterleri doğru açması için), dosya adı `buton-logu-YYYYMMDD-HHMMSS.csv`. Oluşturma tamamen istemcide (`Blob` + geçici `<a download>`).
- Not metni `textContent` ile yazılır (XSS önlemi).

## Değerlendirilen Alternatifler

| Seçenek | Neden seçilmedi |
|---|---|
| Logu cihazda (RAM/flash) tutmak | Firmware karmaşıklığı; flash aşınması; ilk sürümde gerekmedi |
| `localStorage` yedeği | Basit ve faydalı — **önerilen iyileştirme** |
| Sunucu tarafı CSV uç noktası | Cihazda log olmadığı için anlamsız |
| Noktalı virgüllü CSV (Türkçe Excel) | Virgül + tırnak + BOM, modern Excel'de çoğunlukla doğru açılır; sorun olursa GUIDE-005'te "Veri → Metinden" yöntemi anlatılır |

## Sonuçlar

**Olumlu**
- Ek yazılım gerektirmeden telefondan bile kayıt ve CSV.
- Cihaz kaynaklarını kullanmaz.

**Olumsuz / Riskler**
- Sekme kapanır/yenilenirse kayıtlar kaybolur.
- 200 ms sorgu aralığından kısa basışlar kaçabilir (KS-07).
- Her sekme bağımsız log tutar; iki sekme açıksa kayıtlar ikisine de düşer.
- Zaman damgası bilgisayarın saatidir, cihazın değil.
- Mevcut buton mantığıyla (KS-05) kayıt basma yerine bırakma anında oluşabilir.
- Kayıtlı `mm` ofsetlidir, `inch` ofsetsizdir (KS-12).

## Uygulama Notları

- [web_ui.cpp:234-262](../../src/web_ui.cpp#L234-L262) (HTML), [web_ui.cpp:279-395](../../src/web_ui.cpp#L279-L395) (JS), [web_ui.cpp:435-442](../../src/web_ui.cpp#L435-L442) (kenar algılama)
