# ADR-013: Ölçüm JSON Sözleşmesi — Tek Düz Nesne, Elle Üretim

| Alan | Değer |
|---|---|
| **Durum** | Kabul edildi |
| **Karar tarihi** | 2026-04-27 (`1c9ff0d`); alanlar `fdddf76` (buton, ofset) ve `2ffeeb4` (batarya) ile genişletildi |
| **Dokümante edilme** | 2026-10-10 |
| **İlgili** | [ADR-011](ADR-011_HTTP_POLLING_VE_WEBSOCKET_PUSH.md), SRS FG-HTTP-02, AG-ILT-04, SDD §6.3, [GUIDE-006](../guide/GUIDE-006_HTTP_VE_WEBSOCKET_API_REFERANSI.md) |

## Bağlam

Aynı durum bilgisi üç tüketiciye gider: tarayıcı JS'i, Python scripti ve gelecekteki üçüncü taraf istemciler. Şema basit, insan tarafından okunabilir ve dil bağımsız olmalı. Firmware tarafında JSON üretimi az bellek ve az bağımlılıkla yapılmalı.

## Karar

- Tek, düz (iç içe olmayan) bir JSON nesnesi; HTTP yanıtında ve WS mesajında **birebir aynı**.
- Alanlar ve tipler:

```
rawKey:uint  mm:number(3)  inch:number(5)  isInch:bool  isNegative:bool
buttonPressed:bool  offsetMm:number(3)  batV:number(3)  batAdcMv:uint
batPct:uint  batCharging:bool  ageMs:uint
```

- JSON, `String` birleştirme ile elle üretilir (`buildMeasureJson`), 160 bayt önceden ayrılır.
- Sayılar sabit ondalık hassasiyetle yazılır (mm 3, inç 5, gerilim 3).
- **Uyumluluk kuralı:** alan eklenebilir; mevcut alan silinemez, yeniden adlandırılamaz, tipi veya anlamı değiştirilemez. Anlam değişikliği gerekiyorsa yeni alan eklenir.

## Değerlendirilen Alternatifler

| Seçenek | Neden seçilmedi |
|---|---|
| ArduinoJson | Ek bağımlılık; 12 alanlık sabit şema için gereksiz |
| Mesaj türüne göre ayrı şemalar (`{"type":"battery",…}`) | İstemciler daha karmaşık; tek nesne "son durum" olarak her an yeterli |
| CSV/düz metin | Alan genişletmesi kırılgan |
| İç içe nesneler (`battery:{…}`) | Daha düzenli ama mevcut istemcileri bozar; düz yapı yeterli |

## Sonuçlar

**Olumlu**
- Sıfır bağımlılık, öngörülebilir bellek.
- Her mesaj kendi başına tam durum: istemci ara mesajları kaçırsa bile tutarlı.

**Olumsuz / Riskler**
- Elle üretimde kaçışlama hatası riski — şu an tüm alanlar sayı/bool olduğundan yok. Metin alanı eklenirse `appendJsonEscaped` kullanılmalı.
- `rawKey` 22 bite maskelenir; birim biti görünmez (KS-11).
- `mm` ofsetli, `inch` ofsetsiz (KS-12).
- `ageMs` ölçüm dışı güncellemeleri de sayar (KS-06).
- NaN/Inf yazılırsa geçersiz JSON olur; ölçüm katmanı `isfinite` kontrolü ile bunu önler, batarya değerleri için böyle bir kontrol yoktur (ADC her zaman sonlu değer döndürdüğünden pratikte sorun değil).

## Uygulama Notları

- [web_ui.cpp:685-704](../../src/web_ui.cpp#L685-L704)
- Tüketiciler: [web_ui.cpp:402-449](../../src/web_ui.cpp#L402-L449), [measure_ws.py:32-77](../../scripts/measure_ws.py#L32-L77)
