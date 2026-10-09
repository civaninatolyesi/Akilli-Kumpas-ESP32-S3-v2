# ADR-005: ISR–Ana Döngü Veri Paylaşımı: Tek Slotlu Posta Kutusu ve portMUX

| Alan | Değer |
|---|---|
| **Durum** | Kabul edildi |
| **Karar tarihi** | 2026-04-26 (commit `0be7619`) |
| **Dokümante edilme** | 2026-10-10 |
| **İlgili** | [ADR-003](ADR-003_KESME_TABANLI_KUMPAS_OKUMA.md), [ADR-009](ADR-009_TEK_SUPER_DONGU_MIMARISI.md), SRS FG-OKU-05..07, SDD §4.1 |

## Bağlam

ISR tamamlanmış bir 24-bit çerçeveyi ana döngüye iletmelidir. ESP32-S3 çift çekirdeklidir; ISR ve `loopTask` farklı çekirdeklerde çalışabilir. Basit `volatile` değişkenler tek başına, "çerçeve değeri" ile "çerçeve var" bayrağının tutarlı okunmasını garanti etmez.

İletilmesi gereken veri yalnızca **en güncel** ölçümdür: kumpas aynı değeri sürekli tekrarlar, eski bir çerçeveyi işlemenin değeri yoktur.

## Karar

- **Tek slotlu posta kutusu:** `g_lastFrame` (değer) + `g_hasFrame` (bayrak).
- ISR, 24. bitte `portENTER_CRITICAL_ISR(&g_mux)` içinde ikisini birlikte yazar.
- Ana döngü `caliperReadFrame()` ile önce bayrağı kilitsiz kontrol eder (hızlı yol), varsa `portENTER_CRITICAL(&g_mux)` içinde değeri kopyalayıp bayrağı temizler.
- Loop yetişemezse yeni çerçeve eskinin **üzerine yazılır** (son gelen kazanır).

## Değerlendirilen Alternatifler

| Seçenek | Neden seçilmedi |
|---|---|
| FreeRTOS kuyruğu (`xQueueSendFromISR`) | Fazla ağır; kuyrukta biriken eski değerler gecikme yaratır |
| Halka tampon (N çerçeve) | Filtre için ardışık çerçeveler zaten loop'ta sayılıyor; birikme gereksiz |
| Yalnızca `volatile` (kilitsiz) | Çift çekirdekte değer/bayrak tutarsızlığı riski |
| Kesmeleri kapatmak (`noInterrupts`) | Yalnızca o çekirdeği etkiler; çok çekirdekte yetersiz |
| `std::atomic<uint32_t>` ile bayrak + değeri tek kelimeye paketleme | Mümkün (24 bit veri + 1 bit bayrak 32 bite sığar) ama okunabilirlik düşer; gelecekte değerlendirilebilir |

## Sonuçlar

**Olumlu**
- Minimal, deterministik, ISR süresine etkisi önemsiz.
- Gecikme her zaman en fazla bir çerçeve.

**Olumsuz / Riskler**
- Loop uzun süre bloklanırsa (OTA, WiFi bağlanma) aradaki çerçeveler kaybolur. Bu kabul edilebilir; ancak filtre "ardışık" tekrarları sayarken aslında araya kaybolmuş çerçeveler girmiş olabilir. Kumpas aynı değeri tekrar ettiğinden pratik etkisi yoktur.
- `portMUX` bir spinlock'tur; kritik bölgeler birkaç komutla sınırlı tutulmalıdır.

## Uygulama Notları

- Tanımlar: [main.cpp:177-182](../../src/main.cpp#L177-L182)
- Yazma: [main.cpp:206-214](../../src/main.cpp#L206-L214)
- Okuma: [main.cpp:229-236](../../src/main.cpp#L229-L236)
