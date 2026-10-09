# ADR-007: Tekrar Tabanlı Stabilizasyon Filtresi

| Alan | Değer |
|---|---|
| **Durum** | Kabul edildi |
| **Karar tarihi** | 2026-04-27 (commit `1c9ff0d`) |
| **Dokümante edilme** | 2026-10-10 |
| **İlgili** | [ADR-004](ADR-004_ZAMAN_BOSLUGU_ILE_CERCEVE_AYIRMA.md), [ADR-008](ADR-008_SUREKLILIK_ILE_BIRIM_CIKARIMI.md), SRS FG-FLT-01..04, FOG-GVN-01, SDD §4.3 |

## Bağlam

ISR seviyesinde gürültü eşiği ve çerçeve senkronu olsa da bozuk çerçeveler yine oluşabilir: senkron kaybı, tek bit hatası, kumpas kaydırılırken ara değerler. Ölçüm sisteminde **yanlış bir değerin bir kez bile** yayınlanması (ör. Excel'e yazılması), birkaç on milisaniyelik gecikmeden çok daha zararlıdır. Kumpas aynı değeri sürekli tekrar ettiği için tekrar doğrulaması doğal bir doğruluk testi sunar.

Ortalama/medyan gibi sayısal filtreler ise ölçüm aletinde istenmez: kumpasın kendi gösterdiği değerden farklı bir "ara değer" üretirler.

## Karar

1. Ham çerçeve 24 bite maskelenir ve **anahtar** = değer bitleri + işaret biti + birim biti olarak alınır (bit 20 ve 22 yok sayılır).
2. Aynı anahtar ardışık en az `STABLE_REPEATS_NORMAL = 2` kez görülmeden yayınlanmaz.
3. Yeni değer son yayınlanandan `JUMP_MM_THRESHOLD = 0,50 mm` fazla farklıysa `STABLE_REPEATS_ON_JUMP = 4` ardışık tekrar gerekir.
4. Son yayınlanan anahtar tekrar yayınlanmaz (değişim tabanlı yayın).
5. `isfinite(mm)` ve `|mm| ≤ MAX_ABS_MM` sağlanmazsa yayınlanmaz.
6. Değer **asla** ortalanmaz; yayınlanan değer kumpasın gönderdiği count'un birebir karşılığıdır.

## Değerlendirilen Alternatifler

| Seçenek | Neden seçilmedi |
|---|---|
| Filtresiz | Tek bozuk çerçeve ekranda/Excel'de sıçrama olarak görünür |
| Hareketli ortalama / medyan | Kumpas ekranıyla uyuşmayan değerler; gecikme; çözünürlük kaybı |
| Sabit 3+ tekrar | Küçük değişimlerde gereksiz gecikme |
| Çerçeve içi eşlik/CRC | Protokolde yok |

## Sonuçlar

**Olumlu**
- Tek bozuk çerçeve matematiksel olarak yayınlanamaz (aynı bozulmanın iki kez ardışık oluşması gerekir).
- Büyük sıçramalar (kaydırma sırasında yakalanan geçici değerler, birim çıkarımı geçişleri) ekstra doğrulanır.
- Kumpas durduğunda tipik gecikme yalnızca ~1 ek çerçeve periyodudur.

**Olumsuz / Riskler**
- Kumpas sürekli hareket ederken (hiçbir değer iki kez tekrarlanmazsa) hiç yayın olmaz. Ölçüm aleti için kabul edilebilir: ölçüm, çene durduğunda alınır.
- Sıçramadan sonraki ilk yayın ~4 çerçeve periyodu gecikir.
- `pendingCount < required` dalındaki `return`, o turdaki `delay(2)`'yi atlar (işlevsel etkisi yok).

## Uygulama Notları

- Sabitler: [main.cpp:103-106](../../src/main.cpp#L103-L106)
- Anahtar maskesi: [main.cpp:108-110](../../src/main.cpp#L108-L110)
- Filtre: [main.cpp:422-470](../../src/main.cpp#L422-L470)

## Doğrulama

[GUIDE-015](../guide/GUIDE-015_TEST_VE_DOGRULAMA.md) TC-06 (kaydırma sırasında sıçrama olmaması), TC-07 (sıçrama sonrası gecikme).
