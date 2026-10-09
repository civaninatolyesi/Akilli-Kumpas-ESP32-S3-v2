# GUIDE-010: Yeni Bir Kumpas Modeline Uyarlama

**Hedef kitle:** Geliştirici  
**Süre:** 30 dk – 2 saat  
**İlgili:** [ADR-004](../adr/ADR-004_ZAMAN_BOSLUGU_ILE_CERCEVE_AYIRMA.md), [ADR-006](../adr/ADR-006_KUMPAS_BIT_DUZENI_VE_ISARET_BITI.md), [ADR-008](../adr/ADR-008_SUREKLILIK_ILE_BIRIM_CIKARIMI.md), [SDD §6.2 ve §7.1](../SDD_YAZILIM_TASARIM_DOKUMANI.md#62-kumpas-çerçevesi-bit-düzeni)

---

## 1. Ne Zaman Gerekir?

- Hiç ölçüm gelmiyor ama donanım bağlantısı doğru ([GUIDE-012 §2](GUIDE-012_SORUN_GIDERME.md))
- Değerler kumpas ekranının sabit bir katı (×0,5, ×2)
- Negatif değerler pozitif görünüyor
- Değerler rastgele zıplıyor
- Farklı marka/model bir kumpas kullanılacak

## 2. Araçlar

| Araç | Zorunlu mu | Not |
|---|---|---|
| Logic analyzer + PulseView/Saleae Logic | Şiddetle önerilir | 24 MHz klon yeterli |
| Multimetre | Evet | Pinleri bulmak için |
| Seri monitör | Evet | Teşhis çıktısı için |

## 3. Zamanlama Ölçümü

Logic analyzer'ı seviye dönüştürücünün **ESP32 tarafına** bağlayın (CLOCK, DATA, GND). 1 MHz+ örnekleme ile 1 s kaydedin.

Ölçün ve tabloya yazın:

| Büyüklük | Nasıl ölçülür | Değer |
|---|---|---|
| Saat periyodu `Tclk` | Bir patlama içinde ardışık iki düşen kenar arası | ___ µs |
| Patlama başına kenar sayısı | Bir patlamadaki düşen kenarlar | ___ (24 mü, 48 mi?) |
| Çerçeve periyodu `Tf` | Ardışık iki patlamanın başlangıçları arası | ___ ms |
| Patlamalar arası sessizlik | Son kenar → sonraki ilk kenar | ___ ms |
| DATA değişim anı | DATA, CLOCK'un hangi kenarından sonra değişiyor? | yükselen / düşen |
| Kumpasın "hızlı mod"u | Bazı kumpaslar bir tuşla daha sık gönderir; tekrar ölçün | ___ |

**Ayarlama kuralı** (değişmez — ADR-004):

```
MIN_EDGE_US  <  Tclk  <  FRAME_GAP_US  <  sessizlik
```

Güvenli seçim: `MIN_EDGE_US ≈ Tclk / 4`, `FRAME_GAP_US ≈ 2·Tclk … sessizlik / 2` aralığında bir değer. [main.cpp:41-45](../../src/main.cpp#L41-L45).

**DATA örnekleme kenarı:** Kod DATA'yı CLOCK'un **düşen** kenarında okur. DATA düşen kenarda değişiyorsa (kararlı değilse) kesmeyi `RISING`'e almak gerekebilir. Transistörlü evirici kullanıyorsanız ([GUIDE-002 §4](GUIDE-002_DONANIM_BAGLANTISI.md#4-seviye-dönüştürme)) kenar ve bit polaritesi zaten ters çevrilmiş olmalıdır.

## 4. Ham Çerçeveleri Görmek (Teşhis Kodu)

Geçici olarak `loop()` içinde, `caliperReadFrame(raw)` bloğunun başına ekleyin ([main.cpp:422](../../src/main.cpp#L422)):

```cpp
  if (caliperReadFrame(raw)) {
    raw &= 0xFFFFFFu;
    // --- GEÇİCİ TEŞHİS ---
    static uint32_t lastDbg = 0;
    if (raw != lastDbg) {
      lastDbg = raw;
      Serial.printf("raw=0x%06lX mag=%lu b20=%lu b21=%lu b22=%lu b23=%lu\n",
                    (unsigned long)raw, (unsigned long)(raw & 0xFFFFFu),
                    (unsigned long)((raw >> 20) & 1u), (unsigned long)((raw >> 21) & 1u),
                    (unsigned long)((raw >> 22) & 1u), (unsigned long)((raw >> 23) & 1u));
    }
    // ---------------------
```

> Teşhis kodu commit edilmemelidir. Alternatif olarak geçici olarak [web_ui.cpp:690](../../src/web_ui.cpp#L690)'daki maskeyi `0xFFFFFF` yapıp UI'daki `raw=` rozetini izleyebilirsiniz (KS-11).

## 5. Bit Düzenini Belirleme

Kumpası **mm** modunda, ZERO'layıp aşağıdaki durumları kaydedin:

| # | Kumpas ekranı | raw | mag | b20 | b21 | b22 | b23 |
|---|---|---|---|---|---|---|---|
| 1 | 0,00 mm | | | | | | |
| 2 | +10,00 mm | | | | | | |
| 3 | +50,00 mm | | | | | | |
| 4 | −10,00 mm (çeneyi 20 mm açıp ZERO'layın, sonra 10 mm'ye kapatın) | | | | | | |
| 5 | Kumpas **inç** modunda, +0,3940 in | | | | | | |
| 6 | Kumpas inç modunda, −0,3940 in | | | | | | |

Çıkarımlar:

1. **Ölçek:** Satır 2'de `mag / 10` ≈ count/mm. 200 ise varsayılan doğru; 100 veya 400 ise `COUNTS_PER_MM`'yi değiştirin. Satır 5'te `mag / 0,394` ≈ count/in (beklenen 4000).
2. **İşaret biti:** Satır 2 ile 4 arasında değişen tek üst bit işaret bitidir → `CALIPER_SIGN_BIT`. Hiçbir üst bit değişmiyor ve `mag` çok büyükse (ör. 2²⁰'ye yakın) kumpas ikiye tümleyen gönderiyor olabilir → kodda `countsSignedFromRaw` değişikliği gerekir.
3. **Birim biti:** Satır 2 ile 5 arasında (işaret biti hariç) değişen üst bit birim bitidir → `CALIPER_UNIT_BIT` ve polaritesi. **Hiçbiri değişmiyorsa** kumpas birim biti göndermiyordur → [ADR-008](../adr/ADR-008_SUREKLILIK_ILE_BIRIM_CIKARIMI.md) "Revizyon Önerisi"ne bakın; en güvenli kullanım kumpası hep mm modunda tutmaktır.
4. **Değer genişliği:** Satır 3'te `mag` 20 bite sığıyor mu? 50 mm × 200 = 10 000 → evet. Daha geniş ölçüm aralıklı kumpaslarda `CALIPER_VALUE_BITS` gözden geçirilmeli.

Sabitler: [main.cpp:32-63](../../src/main.cpp#L32-L63).

## 6. Ölçek Hızlı Kontrolü

| Web arayüzü / kumpas ekranı oranı | Yapılacak |
|---|---|
| 2,00 | `COUNTS_PER_MM = 400` |
| 0,50 | `COUNTS_PER_MM = 100` |
| 1,27 veya 0,787 | Ölçek değil **birim** sorunu (ADR-008, KS-24) |
| Sabit fark (oran değil) | Ofset → [GUIDE-008](GUIDE-008_KALIBRASYON.md) |

## 7. 48-bit Gönderen Kumpaslar

Patlama başına 48 kenar görüyorsanız kumpas iki 24-bit kelime gönderiyordur (genellikle önce mutlak, sonra bağıl konum). Mevcut kod patlamayı iki çerçeveye böler; posta kutusu son geleni tuttuğundan loop genellikle **ikinci** kelimeyi görür. Hangi kelimenin kumpas ekranına karşılık geldiğini §5 tablosuyla belirleyin. Gerekirse ISR'de patlama içi kelime sayacı tutup yalnızca istenen kelimeyi teslim edin.

## 8. Değişiklikten Sonra

- [ ] Teşhis kodunu kaldırın.
- [ ] [GUIDE-015](GUIDE-015_TEST_VE_DOGRULAMA.md) TC-01…TC-08'i çalıştırın.
- [ ] Ölçülen zamanlama ve bit düzenini bu dosyadaki "Doğrulanmış Modeller" tablosuna ekleyin.
- [ ] Farklı bir varsayılan değer seçtiyseniz ADR-004/ADR-006'ya not ekleyin (veya yeni ADR yazın).

## 9. Doğrulanmış Modeller

| Model / satıcı | Tclk | Tf | Bit/patlama | Ölçek | İşaret biti | Birim biti | Notlar | Doğrulayan / tarih |
|---|---|---|---|---|---|---|---|---|
| Projede kullanılan kumpas | ölçülmedi (KS-17) | ölçülmedi | 24 | 200/mm, 4000/in | 21 | 23 (belirsiz) | `MIN_EDGE_US` 40→5 indirildi | — |
