# ADR-003: Kumpas Verisinin Kesme (ISR) Tabanlı Okunması

| Alan | Değer |
|---|---|
| **Durum** | Kabul edildi |
| **Karar tarihi** | 2026-04-26 (commit `0be7619`) |
| **Dokümante edilme** | 2026-10-10 |
| **İlgili** | [ADR-004](ADR-004_ZAMAN_BOSLUGU_ILE_CERCEVE_AYIRMA.md), [ADR-005](ADR-005_ISR_ANA_DONGU_VERI_PAYLASIMI.md), [ADR-009](ADR-009_TEK_SUPER_DONGU_MIMARISI.md), SRS FG-OKU-01..08, SDD §4.1 |

## Bağlam

Kumpas, CLOCK ve DATA hatlarıyla senkron seri veri yayınlar. Ana denetleyici (ESP32) **köle/dinleyici** konumundadır: saati kumpas üretir, yeniden gönderim veya el sıkışma yoktur. Tek bir kenarın kaçırılması tüm çerçeveyi kaydırır.

Ana döngü ise HTTP istekleri, WebSocket, batarya ADC okuması gibi süresi değişken işler yapar; tek bir HTTP yanıtı (16 KB ana sayfa) milisaniyeler sürebilir.

## Karar

CLOCK hattı (GPIO9) **düşen kenar kesmesi** ile dinlenir. Her kesmede:

1. Zaman damgası `esp_timer_get_time()` ile alınır (Arduino `micros()` değil).
2. DATA hattı `gpio_get_level()` ile okunur (Arduino `digitalRead()` değil).
3. Bit, LSB-önce sırayla kaydırma yazmacına eklenir.
4. 24 bit dolunca çerçeve ana döngüye teslim edilir (ADR-005).

ISR `ARDUINO_ISR_ATTR` (IRAM_ATTR) ile işaretlenir ve kısa tutulur.

## Değerlendirilen Alternatifler

| Seçenek | Açıklama | Neden seçilmedi |
|---|---|---|
| `loop()` içinde yoklama (polling) | CLOCK seviyesini sürekli okumak | Ağ işleri sırasında kenarlar kaçar; `delay(2)` bile tüm çerçeveyi kaçırmaya yeter |
| Ayrı FreeRTOS görevi + sıkı yoklama | Bir çekirdeği yoklamaya ayırmak | CPU israfı, WiFi görevleriyle öncelik çatışması, karmaşıklık |
| SPI çevre birimi (slave modu) | Donanım kaydırma yazmacı | Kumpas CS sinyali üretmez; çerçeve sınırı yalnızca boşlukla belli; SPI slave yapılandırması karmaşık |
| RMT/PCNT çevre birimi | Donanımsal darbe yakalama | DATA'yı CLOCK'a göre örneklemek için uygun değil / gereksiz karmaşık |
| **GPIO kesmesi** | Her düşen kenarda kısa ISR | **Seçildi** — basit, deterministik, yeterli hız |

## Sonuçlar

**Olumlu**
- Ana döngü ne kadar meşgul olursa olsun bitler kaçmaz.
- Kod 40 satırın altında; donanım bağımlılığı yalnızca iki GPIO.

**Olumsuz / Riskler**
- Her saat kenarında bir kesme: saniyede (24 × çerçeve sayısı) kesme — ihmal edilebilir yük.
- ISR içinde flash'taki fonksiyon çağrılırsa ve o sırada flash önbelleği kapalıysa (NVS yazımı, OTA) çökme riski. `gpio_get_level` ve `esp_timer_get_time` IRAM-güvenli kabul edilir; ISR'ye yeni çağrı eklerken bu değişmez korunmalıdır (SDD §12).
- DATA, kenardan hemen sonra (kesme gecikmesi kadar sonra) örneklenir. Kumpas DATA'yı düşen kenardan çok kısa süre sonra değiştiriyorsa kayma olabilir; logic analyzer ile doğrulanmalıdır.

## Uygulama Notları

- ISR: [main.cpp:186-215](../../src/main.cpp#L186-L215)
- Kurulum: [main.cpp:217-227](../../src/main.cpp#L217-L227), `attachInterrupt(clkPin, onCaliperClockFalling, FALLING)`
- ISR, `setup()` içinde `webBegin()`'den **önce** etkinleştirilir; WiFi bağlanırken de çerçeveler toplanır.

## Doğrulama

- [GUIDE-015](../guide/GUIDE-015_TEST_VE_DOGRULAMA.md) TC-01 (sürekli okuma), TC-02 (ağ yükü altında okuma).
