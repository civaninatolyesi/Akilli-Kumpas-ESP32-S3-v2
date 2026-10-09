# ADR-017: Gerilim Bölücü ile Batarya Ölçümü ve Eğilim Tabanlı Şarj Çıkarımı

| Alan | Değer |
|---|---|
| **Durum** | Kabul edildi |
| **Karar tarihi** | 2026-05-28 (commit `2ffeeb4`) |
| **Dokümante edilme** | 2026-10-10 |
| **İlgili** | [ADR-001](ADR-001_DONANIM_PLATFORMU_ESP32_S3_SUPER_MINI.md), SRS FG-BAT-*, KIS-07, SDD §4.5, [GUIDE-011](../guide/GUIDE-011_BATARYA_OLCUMU_AYARI.md), KS-10 |

## Bağlam

Cihaz tek hücre LiPo ile taşınabilir çalışır. Kullanıcı kalan şarjı ve şarj olup olmadığını görmek istiyor. Super Mini kartında batarya gerilimine bağlı bir ADC hattı veya şarj entegresinin durum (CHRG/STDBY) pinine erişim standart değildir. LiPo gerilimi (≤ 4,2 V) ADC'nin doğrudan ölçebileceği aralığın üzerindedir.

## Karar

**Ölçüm**
- 100 kΩ / 100 kΩ gerilim bölücü → GPIO5 (**ADC1_CH4** — WiFi açıkken ADC2 kullanılamadığı için ADC1 zorunlu).
- `analogReadResolution(12)`, `analogSetAttenuation(ADC_11db)`, `analogReadMilliVolts()` (çekirdeğin eFuse kalibrasyonlu mV dönüşümü).
- Her örneklemede 8 okuma ortalaması; 2000 ms'de bir örnekleme.
- `V_bat = V_adc × (R1+R2)/R2`.
- IIR filtre: `f ← 0,85·f + 0,15·V_bat` (τ ≈ 12 s).
- Yüzde: 3,0 V → %0, 4,2 V → %100, doğrusal ve sınırlı.

**Şarj çıkarımı** (pin olmadan, gerilim eğiliminden)
- *Kısa vade:* 30 s'lik pencere tabanına göre ≥ 10 mV artış ve < 4,15 V.
- *Uzun vade:* ≥ 120 s boyunca adım başına ≤ 6 mV değişimle kararlı kaldıktan sonra tabana göre ≥ 10 mV artış ve < 4,15 V.
- Gerilim pencere tabanının 20 mV altına inerse taban hemen yenilenir (deşarjda yanlış pozitifi azaltmak için).
- UI metni bilinçli olarak "Voltaj yükseliyor"dur, "Şarj oluyor" değil.

**Raporlama**
- ≥ 10 mV değişim, yüzde değişimi, şarj durumu değişimi veya son rapordan ≥ 5 s geçmesi.

## Değerlendirilen Alternatifler

| Seçenek | Neden seçilmedi |
|---|---|
| Yakıt göstergesi entegresi (MAX17048 vb.) | Ek donanım, I²C; doğruluk artar — **ileride değerlendirilebilir** |
| Şarj entegresinin CHRG pinini okumak | Super Mini kartlarında erişilebilir değil / karttan karta değişir |
| Ham ADC değeri (`analogRead`) + elle ölçek | eFuse kalibrasyonu kullanılmaz, kartlar arası sapma |
| LiPo deşarj eğrisi tablosu | Daha doğru yüzde; ilk sürüm için doğrusal yaklaşım yeterli görüldü |
| Daha düşük dirençli bölücü (10k/10k) | Sürekli 210 µA kaçak; 100k/100k ≈ 21 µA |

## Sonuçlar

**Olumlu**
- İki direnç ile çalışır; batarya bağlı değilse sistem yine çalışır (değerler anlamsız olur).
- Gürültüye dayanıklı, gereksiz WS trafiği üretmeyen raporlama.

**Olumsuz / Riskler**
- Yüzde doğrusal olduğundan LiPo'nun düz bölgesinde (3,7–3,9 V) gerçek kapasiteyle uyuşmaz.
- "Şarj" yalnızca bir tahmindir; tam şarjda (> 4,15 V) veya yük değişimlerinde yanlış sonuç verebilir.
- 100 kΩ kaynak empedansı ADC örnekleme hatasına yol açabilir; GPIO5–GND arası ~100 nF önerilir.
- Kullanılmayan `BATTERY_SAMPLE_MS` ve şarj/boşta için aynı periyot; yorumlarla değerler uyumsuz (KS-10).

## Uygulama Notları

- Sabitler: [main.cpp:14-30](../../src/main.cpp#L14-L30)
- Algoritma: [main.cpp:345-420](../../src/main.cpp#L345-L420)
