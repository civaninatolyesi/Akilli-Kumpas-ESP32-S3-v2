# ADR-004: Zaman Boşluğu ile Çerçeve Ayırma ve Gürültü Eşiği

| Alan | Değer |
|---|---|
| **Durum** | Kabul edildi (değerler revize edildi) |
| **Karar tarihi** | 2026-04-26 (`0be7619`), revizyon 2026-04-26 (`e043d56`) |
| **Dokümante edilme** | 2026-10-10 |
| **İlgili** | [ADR-003](ADR-003_KESME_TABANLI_KUMPAS_OKUMA.md), [ADR-007](ADR-007_TEKRAR_TABANLI_STABILIZASYON_FILTRESI.md), SRS FG-OKU-03/04, SDD §7.1, [KS-17](../BILINEN_SORUNLAR.md#ks-17--kumpas-saat-periyodu-hakkında-çelişkili-yorumlar-) |

## Bağlam

Kumpas protokolünde başlangıç biti, senkron kelimesi veya çip seçme sinyali yoktur. Bir çerçevenin nerede başladığını gösteren tek ipucu, iki veri patlaması arasında CLOCK hattının uzun süre sessiz kalmasıdır. Ayrıca uzun kablolar ve seviye dönüştürücüler CLOCK hattında kısa sıçramalar (glitch) üretebilir; her sıçrama sahte bir bit demektir.

## Karar

ISR'de her düşen kenar için bir önceki kenara olan süre (`gap`) hesaplanır:

| Koşul | Eylem | Sabit |
|---|---|---|
| `gap < MIN_EDGE_US` | Kenar gürültü kabul edilir, bit okunmaz | `MIN_EDGE_US = 5` µs |
| `gap > FRAME_GAP_US` | Yeni çerçeve başlar: bit sayacı ve yazmaç sıfırlanır, ardından bu kenarın biti okunur | `FRAME_GAP_US = 1500` µs |
| Aradaki değerler | Normal bit | — |

**Değişmez:** `MIN_EDGE_US < saat periyodu < FRAME_GAP_US < çerçeveler arası sessizlik`.

### Değer Tarihçesi

| Commit | `FRAME_GAP_US` | `MIN_EDGE_US` | `REJECT_IF_TOP_BITS_SET` |
|---|---|---|---|
| `0be7619` (ilk) | 3000 | 40 | `true` |
| `e043d56` ve sonrası | **1500** | **5** | **`false`** |

`MIN_EDGE_US`'in 40'tan 5'e indirilmesi, gerçek saat periyodunun 40 µs'ye yakın/altında olduğu ve geçerli kenarların atıldığı izlenimini verir. `FRAME_GAP_US`'in yarıya indirilmesi senkronizasyonun daha çabuk yakalanmasını sağlar. Üst bit reddi, bit 23'ün bu kumpasta meşru olarak 1 olabildiği (birim/başka bayrak) anlaşılınca kapatıldı.

## Değerlendirilen Alternatifler

| Seçenek | Neden seçilmedi |
|---|---|
| Sabit sayıda bit sayıp senkronu hiç sıfırlamamak | Tek kaçan/fazla bit kalıcı kaymaya yol açar |
| Donanım zamanlayıcısıyla "sessizlik zaman aşımı" kesmesi | Ek kesme ve durum; aynı sonucu bir sonraki kenarda gap hesabı veriyor |
| Gürültüyü RC filtreyle donanımda bastırmak | Tamamlayıcıdır, yerine geçmez; yazılım eşiği maliyetsiz |

## Sonuçlar

**Olumlu**
- Kendiliğinden senkronize olur: açılışta veya bozulma sonrası en geç bir çerçeve kaybedilir.
- Kısa sıçramaların çoğu elenir.

**Olumsuz / Riskler**
- Kumpasın "hızlı mod"unda çerçeveler arası boşluk 1500 µs'den kısaysa çerçeveler birleşir. Kumpasın yavaş/hızlı mod boşlukları ölçülmelidir.
- Atılan gürültü kenarı da `g_lastEdgeUs`'i günceller; bir glitch'ten hemen sonra gelen gerçek kenar da atılabilir (çerçeve bozulur, ADR-007 filtresi eler).
- 48-bit gönderen kumpaslarda patlama içinde boşluk olmadığı için iki ardışık 24-bit çerçeve çıkar (SDD §7.1).

## Uygulama Notları

- Sabitler: [main.cpp:39-51](../../src/main.cpp#L39-L51)
- Uygulama: [main.cpp:186-200](../../src/main.cpp#L186-L200)
- Ayar yöntemi: [GUIDE-010](../guide/GUIDE-010_YENI_KUMPAS_MODELINE_UYARLAMA.md) §3

## Doğrulama

Logic analyzer ile saat periyodu ve çerçeve boşluğu ölçülüp değişmezin sağlandığı gösterilmelidir (KS-17 kapatılınca ölçüm değerleri bu ADR'ye eklenecek).
