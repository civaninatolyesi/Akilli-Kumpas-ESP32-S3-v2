# ADR-008: Süreklilik ile mm/İnç Birim Çıkarımı (ve Seri Prob Aracının Kaldırılması)

| Alan | Değer |
|---|---|
| **Durum** | Kabul edildi — **revizyon önerilir** (bkz. KS-24) |
| **Karar tarihi** | 2026-04-27 (`1c9ff0d` çıkarım, `a4d6615` prob eklendi, `4fed220` prob kaldırıldı) |
| **Dokümante edilme** | 2026-10-10 |
| **İlgili** | [ADR-006](ADR-006_KUMPAS_BIT_DUZENI_VE_ISARET_BITI.md), [ADR-007](ADR-007_TEKRAR_TABANLI_STABILIZASYON_FILTRESI.md), SRS FG-BRM-01..04, SDD §4.4 ve §5.6, [KS-24](../BILINEN_SORUNLAR.md#ks-24--birim-çıkarımının-algılama-sınırı-ve-yanlış-pozitif-riski-) |

## Bağlam

Kumpaslar mm/inç tuşuyla birim değiştirir ve count ölçeği değişir (200 count/mm ↔ 4000 count/in). Kullanılan kumpasta birim bitinin konumu belirsizdi: geliştirme sırasında seri port üzerinden "mm örneği al / inç örneği al / bit farkını analiz et" komutlarıyla çalışan bir **prob aracı** yazıldı (`a4d6615`). Araç, bit 20–23'te mm ve inç örnekleri arasında güvenilir bir fark bulamadığı durumlar için "bu kumpas ayrı bir birim bayrağı göndermiyor olabilir; süreklilik tabanlı çıkarım pratik seçenektir" sonucunu üretiyordu.

## Karar

1. Birim bitinin (varsayılan 23) değeri yalnızca **ilk** çerçevede başlangıç birimi olarak kullanılır.
2. Sonrasında birim, fiziksel süreklilikten çıkarılır: her yeni değer hem mevcut hem alternatif birimle mm'ye çevrilir ve son yayınlanan mm değeriyle karşılaştırılır.
3. **Güçlü ipucu:** mevcut yorum ≥ `UNIT_SWITCH_MIN_BAD_MM` (4,0 mm) yanlış **ve** alternatif yorum ≤ `UNIT_SWITCH_MAX_GOOD_MM` (0,40 mm) doğru.
4. Birim yalnızca `UNIT_SWITCH_CONFIRM` (3) ardışık güçlü ipucundan sonra değişir.
5. `g_autoUnitFix = true` varsayılan; `false` yapılırsa birim ilk değerde sabit kalır.
6. Seri prob/yerleşim seçme araçları üretim kodundan kaldırılır (`4fed220`, −385 satır).

## Değerlendirilen Alternatifler

| Seçenek | Artı | Eksi |
|---|---|---|
| Birim bitine her çerçevede güvenmek | Basit, anında | Bit yoksa/gürültülüyse birim titrer |
| Birimi sabit mm kabul etmek | En öngörülebilir | Kullanıcı inç'e geçerse değerler 0,787× hatalı |
| **Süreklilik çıkarımı** | Bit olmadan da çoğu geçişi yakalar | Algılama sınırı ve nadir yanlış pozitif (aşağıda) |
| Seri prob aracını üretimde tutmak | Sahada teşhis | 385 satır karmaşıklık, son kullanıcı için anlamsız |

## Sonuçlar

**Olumlu**
- Gürültülü birim biti birimi titretmez.
- Ölçüm ≥ ~19 mm iken yapılan birim değişimleri 3–4 çerçevede düzeltilir.

**Olumsuz / Riskler (analiz: SDD §4.4)**
- **Algılama sınırı:** Yanlış yorumlanan değer doğrunun 0,787 veya 1,27 katıdır; 4 mm sapma için çene en az ~14,8–18,8 mm açık olmalıdır. Sıfıra yakın yapılan birim değişimi algılanmaz ve sonraki hareketler "sürekli" göründüğü için yanlış birim **kalıcı** olur.
- **Yanlış pozitif:** Çene son değerin 0,787 katına (±0,4 mm) hızla kapatılıp sabit tutulursa birim yanlışlıkla değişir.
- Birim biti gerçekten varsa bile ilk çerçeveden sonra kullanılmaz.

## Revizyon Önerisi

1. Kumpasın birim biti gönderip göndermediği [GUIDE-010 §5](../guide/GUIDE-010_YENI_KUMPAS_MODELINE_UYARLAMA.md) ile kesinleştirilmeli.
2. **Bit varsa:** `g_autoUnitFix = false` ve birim her çerçevede bitten alınmalı (filtre anahtarı zaten birim bitini içerdiğinden titreme 2 tekrar filtresiyle bastırılır).
3. **Bit yoksa:** birimi mm'ye sabitlemek ve kullanıcıya "kumpas mm modunda kullanılmalı" uyarısı vermek, kısmen çalışan bir tahminden daha güvenlidir.

Bu öneri uygulanınca yeni bir ADR (ADR-0xx) bu kararı "Yerine geçildi" durumuna getirmelidir.

## Uygulama Notları

- Durum ve sabitler: [main.cpp:85-101](../../src/main.cpp#L85-L101)
- Algoritma: [main.cpp:133-173](../../src/main.cpp#L133-L173)
- Çağrı yeri: [main.cpp:448](../../src/main.cpp#L448)
