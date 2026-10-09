# ADR-009: Tek Süper Döngü Mimarisi (Ek FreeRTOS Görevi Yok)

| Alan | Değer |
|---|---|
| **Durum** | Kabul edildi |
| **Karar tarihi** | 2026-04-26 (`0be7619`), ağ katmanı eklenirken korundu (`1c9ff0d`) |
| **Dokümante edilme** | 2026-10-10 |
| **İlgili** | [ADR-003](ADR-003_KESME_TABANLI_KUMPAS_OKUMA.md), [ADR-005](ADR-005_ISR_ANA_DONGU_VERI_PAYLASIMI.md), [ADR-010](ADR-010_MODUL_AYRIMI_VE_WEB_UI_ARAYUZU.md), SRS KIS-03, SDD §3.4 |

## Bağlam

Sistemde zamanlama açısından kritik tek iş kumpas bitlerini yakalamaktır ve bu ISR ile çözülmüştür (ADR-003). Geri kalan işler (filtre, buton, batarya, HTTP, WS) milisaniye toleranslıdır. Arduino çatısı `setup()/loop()` modelini ve `loopTask` adlı tek bir FreeRTOS görevini sağlar.

## Karar

- Tüm uygulama mantığı `loop()` içinde sırayla çalışır: `webLoop()` → buton → batarya (zaman dilimli) → çerçeve işleme → `delay(2)`.
- Ek görev, kuyruk, mutex veya zamanlayıcı geri çağrısı kullanılmaz.
- Durum, fonksiyon içi `static` değişkenlerde ve dosya kapsamlı `static` değişkenlerde tutulur.
- `delay(2)` boşta CPU'yu bırakır ve görev izleyicisini (task watchdog) besler.

## Değerlendirilen Alternatifler

| Seçenek | Neden seçilmedi |
|---|---|
| Ölçüm görevi + ağ görevi | Paylaşılan durum için kilitler gerekir; `WebServer` thread-safe değildir; kazanım küçük |
| Async web sunucusu (ESPAsyncWebServer) | Geri çağrılar ayrı görevde çalışır → paylaşılan duruma kilit gerekir; ek bağımlılık |
| Zamanlayıcı ile batarya örnekleme | `millis()` tabanlı zaman dilimleme yeterli |

## Sonuçlar

**Olumlu**
- Web katmanındaki durum değişkenleri tek görevden erişildiği için kilit gerekmez.
- Okuması ve hata ayıklaması kolay, deterministik sıra.

**Olumsuz / Riskler**
- Bloklayıcı her iş tüm döngüyü durdurur: açılışta WiFi bağlanma (≤ 15 s), OTA yükleme (KS-20). Bu sürelerde buton ve batarya güncellenmez; kumpas çerçeveleri ISR'de toplanmaya devam eder ama yalnızca sonuncusu korunur.
- `loop()` içinde 22 adet `static` değişken var; fonksiyon büyüdükçe okunabilirlik düşüyor. Bileşenleri (batarya, buton, filtre) ayrı fonksiyon/dosyalara bölmek önerilir (KS-19 ile birlikte).

## Uygulama Notları

- [main.cpp:304-473](../../src/main.cpp#L304-L473)
