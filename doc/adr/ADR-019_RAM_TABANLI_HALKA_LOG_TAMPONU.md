# ADR-019: RAM Tabanlı Halka Log Tamponu ve Artımlı Log API'si

| Alan | Değer |
|---|---|
| **Durum** | Kabul edildi |
| **Karar tarihi** | 2026-05-28 (commit `2ffeeb4`) |
| **Dokümante edilme** | 2026-10-10 |
| **İlgili** | SRS FG-LOG-*, FG-UI-11/12, SDD §4.7.6 ve §6.4, [GUIDE-012](../guide/GUIDE-012_SORUN_GIDERME.md), KS-08 |

## Bağlam

Cihaz kumpasa monte edilip USB'den ayrıldığında seri konsol erişilemez. WiFi zaman aşımı, OTA hataları, kalibrasyon değişiklikleri gibi olayların uzaktan görülebilmesi gerekiyordu.

## Karar

- `webLogf(fmt, …)`: printf biçimli, `vsnprintf` ile 96 baytlık yerel tampona.
- Depolama: `char logLines[100][96]` statik halka tampon (9 600 B DRAM). En eski satırın üzerine yazılır.
- Her satıra 1'den başlayan monoton artan kimlik (`id`); toplam sayaç `logSeq`.
- `GET /api/log?since=N`: `id > N` olan satırlar + güncel `seq`. İstemci `seq`'i saklayıp bir sonraki istekte gönderir → yalnızca yeni satırlar aktarılır.
- `/log` sayfası 250 ms'de bir sorgular, satırları `textContent` ile ekler.
- JSON'da `"`, `\`, `\n`, `\r`, `\t` kaçışlanır; diğer kontrol karakterleri atılır.
- Seri porta yazılan mesajlar (`Serial.printf`) ile web logu ayrıdır; önemli olaylar her ikisine de yazılır.

## Değerlendirilen Alternatifler

| Seçenek | Neden seçilmedi |
|---|---|
| Flash'ta kalıcı log (LittleFS) | Flash aşınması; yeniden başlatma sonrası log nadiren gerekli |
| Syslog/UDP | Alıcı sunucu gerektirir |
| Seri çıktıyı WS'e yönlendirmek | Seri trafik yoğun olduğunda WS'i doldurur; yapılandırılmamış metin |
| Dinamik `std::deque<String>` | Heap parçalanması |

## Sonuçlar

**Olumlu**
- Sabit, öngörülebilir bellek; heap yok.
- Artımlı API sayesinde düşük trafik.
- Açılıştan itibaren (WiFi'den önce bile) olaylar toplanır.

**Olumsuz / Riskler**
- Yeniden başlatmada log kaybolur — çökme nedenini görmek için seri konsol gerekir.
- Satırlar 95 karakterde kesilir.
- Zaman damgası yok (`millis()` eklenmesi önerilir).
- `/log`'daki "Temizle" `lastLogSeq = 0` yaptığından satırlar geri gelir (KS-08).
- `webLogf` ISR'den çağrılamaz (yığın tamponu, `vsnprintf`).

## Uygulama Notları

- [web_ui.cpp:39-87](../../src/web_ui.cpp#L39-L87), [web_ui.cpp:711-740](../../src/web_ui.cpp#L711-L740), [web_ui.cpp:458-561](../../src/web_ui.cpp#L458-L561)
