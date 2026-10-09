# ADR-006: Kumpas Bit Düzeni — 20-bit Büyüklük, Bit 21 İşaret, Bit 23 Birim

| Alan | Değer |
|---|---|
| **Durum** | Kabul edildi |
| **Karar tarihi** | 2026-04-27 (commit `1c9ff0d`; sayısal ölçek `e043d56`) |
| **Dokümante edilme** | 2026-10-10 |
| **İlgili** | [ADR-008](ADR-008_SUREKLILIK_ILE_BIRIM_CIKARIMI.md), SRS FG-COZ-01..07, SDD §4.2 ve §6.2, [GUIDE-010](../guide/GUIDE-010_YENI_KUMPAS_MODELINE_UYARLAMA.md) |

## Bağlam

"Çin tipi 24-bit kumpas protokolü" belgelenmemiştir ve üreticiden üreticiye farklılık gösterir:

- Büyüklük alanı genellikle 20 bittir.
- İşaret biti bazı modellerde bit 20, bazılarında bit 21 veya 23'tür.
- Birim (mm/inç) biti bazı modellerde bit 23'tür, bazılarında hiç yoktur.
- Ölçek mm modunda genellikle 0,005 mm/count (200 count/mm), inç modunda 0,00025 in/count'tur (4000 count/in). Bazı modeller 100 veya 400 count/mm kullanır. Mutlak değer "işaret + büyüklük" olarak gönderilir (ikiye tümleyen değil).

Projede kullanılan kumpastan gözlenen çerçevelerde negatif değerlerde bit 21'in 1 olduğu görüldü.

## Karar

| Alan | Bitler | Sabit | Değer |
|---|---|---|---|
| Büyüklük | 0–19 | `CALIPER_VALUE_BITS` | 20 |
| İşaret | 21 | `CALIPER_SIGN_BIT` | 21 (1 = negatif) |
| Birim | 23 | `CALIPER_UNIT_BIT` | 23 (1 = inç) — yalnızca ilk birim ipucu |
| Ölçek | — | `COUNTS_PER_MM`, `COUNTS_PER_INCH` | 200, 4000 |
| Makullük | — | `MAX_ABS_MM` | 500 mm |
| Üst bit reddi | 22–23 | `REJECT_IF_TOP_BITS_SET` | `false` |

Kurallar:

- İşaret-büyüklük gösterimi: `counts = isNegative ? −magnitude : magnitude`.
- Birim biti işaret bitiyle aynı konuma ayarlanırsa birim biti yok sayılır (negatif değerlerin "inç" sanılmasını önlemek için).
- Bit 20 ve 22 karşılaştırma/filtre anahtarına dahil edilmez; bu bitlerdeki değişimler yeni ölçüm üretmez.
- Polarite bayrakları (`g_signBitMeansNegativeWhen1`, `g_unitBitMeansInchWhen1`) ters mantıklı varyantlar için tutulur.

## Değerlendirilen Alternatifler

| Seçenek | Neden seçilmedi |
|---|---|
| İşaret bit 20 (bazı kaynaklarda) | Bu kumpasta negatifler bit 21'de gözlendi |
| İkiye tümleyen 24-bit yorumu | Gözlenen çerçevelerle uyuşmadı |
| Çalışma zamanında seri komutlarla bit düzeni seçimi | `1c9ff0d`–`a4d6615` arası vardı; `4fed220` ile kaldırıldı (bkz. ADR-008) — üretim kodunu sade tutmak için |
| Üst bitleri reddetmek (`true`) | Bit 23 meşru olarak 1 olabildiğinden geçerli çerçeveler atılıyordu |

## Sonuçlar

**Olumlu**
- Tüm bit düzeni `main.cpp` başında, yorumlarıyla birlikte tek yerde.
- Farklı kumpasa uyarlama genellikle 1–3 sabit değişikliğidir.

**Olumsuz / Riskler**
- Doğru bit düzeni deneme-yanılma gerektirir; [GUIDE-010](../guide/GUIDE-010_YENI_KUMPAS_MODELINE_UYARLAMA.md) adım adım yöntem verir.
- `rawKey` JSON'da 22 bite maskelendiğinden bit 23 UI'da görünmez (KS-11) — uyarlama sırasında seri log veya maske değişikliği gerekir.

## Uygulama Notları

- Sabitler: [main.cpp:32-67](../../src/main.cpp#L32-L67)
- `decode24`: [main.cpp:244-252](../../src/main.cpp#L244-L252)
- Dönüşüm: [main.cpp:112-131](../../src/main.cpp#L112-L131)
