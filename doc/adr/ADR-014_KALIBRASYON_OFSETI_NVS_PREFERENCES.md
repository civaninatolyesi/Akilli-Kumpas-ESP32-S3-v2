# ADR-014: Kalibrasyonun Tek Ofset Olarak NVS'de (Preferences) Saklanması

| Alan | Değer |
|---|---|
| **Durum** | Kabul edildi |
| **Karar tarihi** | 2026-04-28 (commit `fdddf76`) |
| **Dokümante edilme** | 2026-10-10 |
| **İlgili** | [ADR-010](ADR-010_MODUL_AYRIMI_VE_WEB_UI_ARAYUZU.md), SRS FG-KAL-*, SDD §4.7.4 ve §6.5, [GUIDE-008](../guide/GUIDE-008_KALIBRASYON.md), KS-12, KS-13 |

## Bağlam

Ucuz kumpasların sıfır noktası zamanla kayabilir veya kullanıcı belirli bir referansa (ör. mastar bloğu) göre sabit bir düzeltme uygulamak isteyebilir. Düzeltme cihaz yeniden başladığında kaybolmamalıdır. Ölçek hatası (count/mm) ise model özelliğidir ve derleme zamanı sabitiyle çözülür (ADR-006).

## Karar

- Kalibrasyon **tek bir toplamsal ofset** (mm) olarak modellenir: `gösterilen_mm = ham_mm + ofset`.
- Ofset, ESP32 NVS'de Arduino `Preferences` ile saklanır: ad alanı `kumpas`, anahtar `offset_mm`, tip `float`, varsayılan 0.
- Açılışta `webBegin()` içinde salt-okunur modda yüklenir; `POST /calibrate` ile yazılır.
- Form adımı 0,001 mm'dir; sıfırlama ayrı bir formla `offset=0` gönderir.
- Ofset web katmanında uygulanır; ölçüm katmanı ve birim çıkarımı ham değerle çalışır.

## Değerlendirilen Alternatifler

| Seçenek | Neden seçilmedi |
|---|---|
| Kazanç + ofset (iki noktalı kalibrasyon) | Kumpasın kendi ölçeği zaten doğru kabul edilir; kullanıcı arayüzü karmaşıklaşır. İhtiyaç olursa `gain` anahtarı eklenebilir |
| Ofseti derleme zamanı sabiti yapmak | Her ayar için yeniden derleme |
| EEPROM emülasyonu | `Preferences` daha güvenli (aşınma dengeleme, atomik yazım) |
| "Şu anki değeri sıfır yap" (tare) düğmesi | Kumpasın kendi ZERO tuşu bu işi görüyor; gelecekte kolayca eklenebilir |

## Sonuçlar

**Olumlu**
- Güç kesilse de korunur; `/api/measure` içinde `offsetMm` olarak görünür (izlenebilirlik).
- Ölçüm katmanından bağımsız.

**Olumsuz / Riskler**
- Yalnızca `mm`'ye uygulanır; `inch` ve `isNegative` ofsetsiz (KS-12).
- Ofset, kumpastan **yeni** bir stabil değer gelene kadar ekrandaki değere yansımaz.
- Giriş doğrulaması yok: geçersiz metin ofseti 0 yapar, büyük değerler kabul edilir (KS-13).
- Kimlik doğrulama yok (KS-03).
- `pio run -t erase` ofseti siler.

## Uygulama Notları

- [web_ui.cpp:21-22](../../src/web_ui.cpp#L21-L22), [web_ui.cpp:754-765](../../src/web_ui.cpp#L754-L765), [web_ui.cpp:809-820](../../src/web_ui.cpp#L809-L820), [web_ui.cpp:886](../../src/web_ui.cpp#L886)
