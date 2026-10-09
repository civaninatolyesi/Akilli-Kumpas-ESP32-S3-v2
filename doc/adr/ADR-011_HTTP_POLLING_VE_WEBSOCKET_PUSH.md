# ADR-011: Tarayıcı için HTTP Sorgulama, Otomasyon için WebSocket Push

| Alan | Değer |
|---|---|
| **Durum** | Kabul edildi |
| **Karar tarihi** | 2026-04-27 (HTTP, `1c9ff0d`), 2026-04-28 (WS, `fdddf76`) |
| **Dokümante edilme** | 2026-10-10 |
| **İlgili** | [ADR-013](ADR-013_JSON_OLCUM_SOZLESMESI.md), [ADR-020](ADR-020_ISTEMCI_TARAFI_BUTON_LOGU_VE_CSV.md), [ADR-021](ADR-021_PYTHON_ILE_KLAVYE_EMULASYONU.md), SRS FG-HTTP-*, FG-WS-*, SDD §4.7.2–4.7.3 |

## Bağlam

İki tür istemci var:

1. **Tarayıcı:** İnsan gözü için canlı gösterim. 200 ms güncelleme algısal olarak "anlık"tır. Bağlantı kopmalarına dayanıklı ve basit olmalı.
2. **Otomasyon istemcisi (Python):** Buton basışını **kaçırmadan** ve en düşük gecikmeyle almalı; ölçüm değişimlerini olay olarak işlemeli.

## Karar

- **HTTP sorgulama:** Arduino `WebServer` port 80. Ana sayfa `GET /api/measure`'ı 200 ms'de bir çağırır. Log sayfası `GET /api/log?since=N`'i 250 ms'de bir çağırır.
- **WebSocket push:** `links2004/WebSockets` port 81. Ölçüm, buton veya batarya durumu her değiştiğinde (ve en az bir istemci bağlıysa) aynı JSON tüm istemcilere yayınlanır. İstemciden gelen mesajlar yok sayılır.
- Her iki kanal **aynı JSON şemasını** kullanır (ADR-013).

## Değerlendirilen Alternatifler

| Seçenek | Neden seçilmedi |
|---|---|
| Yalnızca HTTP sorgulama | Python için kısa buton basışları kaçabilir, gereksiz trafik |
| Yalnızca WS (tarayıcı dahil) | Yeniden bağlanma mantığı JS'de ek kod; geliştirme sırasında HTTP ile `curl` testi kolaylığı kaybolur. **Gelecekte tercih edilebilir** (KS-07'yi çözer) |
| Server-Sent Events (SSE) | Arduino `WebServer` ile uzun ömürlü yanıtlar senkron sunucuyu bloklar |
| MQTT | Ayrı broker gerektirir; "sıfır kurulum" hedefiyle çelişir |
| Async web sunucusu ile tek port | Ek bağımlılık ve görevler arası kilit ihtiyacı (ADR-009) |

## Sonuçlar

**Olumlu**
- Tarayıcı tarafı son derece basit (`fetch` + `setInterval`).
- `curl http://kumpas.local/api/measure` ile hızlı teşhis.
- Python istemcisi her olay için anında mesaj alır.

**Olumsuz / Riskler**
- Tarayıcı, 200 ms'den kısa buton basışlarını kaçırabilir (KS-07).
- İki port (80 ve 81) — güvenlik duvarı kuralları ikisine de izin vermeli.
- HTTP'deki `ageMs`, WS'de her zaman 0'dır; ayrıca ölçüm dışı güncellemeleri de sayar (KS-06).
- Her yayın tam durum gönderir (~250 bayt); bant genişliği açısından önemsiz.

## Uygulama Notları

- Rotalar: [web_ui.cpp:803-871](../../src/web_ui.cpp#L803-L871)
- WS olayları: [web_ui.cpp:743-752](../../src/web_ui.cpp#L743-L752)
- Yayın: [web_ui.cpp:884-919](../../src/web_ui.cpp#L884-L919)
- Tarayıcı sorgusu: [web_ui.cpp:402-452](../../src/web_ui.cpp#L402-L452)
