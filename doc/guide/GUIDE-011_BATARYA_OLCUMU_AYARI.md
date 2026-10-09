# GUIDE-011: Batarya Ölçümünün Ayarlanması

**Hedef kitle:** Kurulumcu, geliştirici  
**İlgili:** [ADR-017](../adr/ADR-017_BATARYA_OLCUMU_VE_SARJ_CIKARIMI.md), [SDD §4.5](../SDD_YAZILIM_TASARIM_DOKUMANI.md#45-c5--batarya-izleyici), [KS-10](../BILINEN_SORUNLAR.md#ks-10--batarya-sabitlerinde-kullanılmayanyanıltıcı-değerler-)

---

## 1. Donanım

```
LiPo(+) ── R1 ──┬── R2 ── GND
                ├── GPIO5 (ADC1)
                └── 100 nF ── GND
```

| Parametre | Varsayılan | Sabit |
|---|---|---|
| R1 (üst) | 100 kΩ | `BAT_R1_OHMS` |
| R2 (alt) | 100 kΩ | `BAT_R2_OHMS` |
| Oran | (R1+R2)/R2 = 2,0 | — |

**Direnç seçimi:** ADC (11 dB) yaklaşık 0,1–3,1 V aralığında doğrusal ölçer. Bataryanın azami gerilimi × R2/(R1+R2) bu aralıkta kalmalıdır. 4,2 V × 0,5 = 2,1 V ✓. %1 toleranslı dirençler önerilir.

## 2. Bölücü Oranını Kalibre Etme

Direnç toleransı ve ADC sapması nedeniyle gösterilen gerilim birkaç on mV hatalı olabilir.

1. Bataryanın gerçek gerilimini multimetreyle ölçün: `Vgerçek`.
2. Web arayüzünde `batAdcMv`'yi okuyun (`/api/measure`).
3. Gerçek oran: `oran = Vgerçek / (batAdcMv / 1000)`.
4. `BAT_R1_OHMS`'u oran tutacak şekilde ayarlayın: `R1 = R2 × (oran − 1)`.

   Örnek: Vgerçek = 4,05 V, batAdcMv = 1995 → oran = 2,030 → R1 = 100 000 × 1,030 = **103 000**.

5. Derleyip yükleyin, birkaç farklı gerilimde doğrulayın.

## 3. Yüzde Eğrisi

Varsayılan doğrusal: 3,0 V → %0, 4,2 V → %100 (`BATTERY_MIN_V`, `BATTERY_MAX_V`). LiPo'nun gerilim–kapasite eğrisi doğrusal olmadığından, daha gerçekçi gösterim için [main.cpp:79-83](../../src/main.cpp#L79-L83)'teki `batteryPercentFromVolts`'u tablo ile değiştirebilirsiniz:

```cpp
static uint8_t batteryPercentFromVolts(float v) {
  // Tipik 1S LiPo, düşük yükte (yaklaşık değerler)
  static const float V[]   = {3.30f, 3.60f, 3.70f, 3.75f, 3.80f, 3.85f, 3.95f, 4.05f, 4.20f};
  static const uint8_t P[] = {   0,    10,    20,    30,    45,    55,    70,    85,   100};
  if (v <= V[0]) return 0;
  for (int i = 1; i < 9; i++) {
    if (v < V[i]) return (uint8_t)(P[i-1] + (P[i] - P[i-1]) * (v - V[i-1]) / (V[i] - V[i-1]) + 0.5f);
  }
  return 100;
}
```

Bu değişiklik bir davranış değişikliğidir; ADR-017'ye not ekleyin.

## 4. Şarj Göstergesini Ayarlama

| Sabit | Varsayılan | Artırırsanız | Azaltırsanız |
|---|---|---|---|
| `CHARGE_WINDOW_MS` | 30 000 | Daha yavaş ama kararlı algılama | Daha hızlı, gürültüye duyarlı |
| `CHARGE_MIN_DELTA_V` | 0,010 V | Daha az yanlış pozitif | Daha erken algılama |
| `CHARGE_NEAR_FULL_V` | 4,15 V | Tam doluya yakın da "yükseliyor" görünür | Daha erken kapanır |
| `STABLE_PERIOD_MS` | 120 000 | Uzun vade algısı gecikir | — |
| `STABLE_DELTA_V` | 0,006 V | Daha kolay "kararlı" sayılır | — |
| `LONG_CHARGE_MIN_DELTA_V` | 0,010 V | — | — |

Örnekleme ve raporlama:

| Sabit | Varsayılan | Not |
|---|---|---|
| `BATTERY_SAMPLE_MS_CHARGING` | 2000 | Yorum "daha sık" der; değer boşta ile aynı (KS-10) |
| `BATTERY_SAMPLE_MS_IDLE` | 2000 | |
| `BATTERY_SAMPLES` | 8 | Örnek başına ADC okuma sayısı |
| `BATTERY_REPORT_MAX_INTERVAL_MS` | 5000 | Yorum "dakikada bir" der (KS-10) |
| `BATTERY_SAMPLE_MS` | 500 | **Kullanılmıyor** |

## 5. Batarya Kullanmıyorsanız

GPIO5'i boş bırakın. Arayüzde rastgele/sıfır yakını değerler görünür; işlevsel sorun yoktur. Gürültülü "Voltaj yükseliyor" rozetini önlemek için GPIO5'i GND'ye bağlayabilirsiniz (%0 görünür).

## 6. Doğrulama

- [ ] Multimetre ile web arayüzü arasındaki fark < 30 mV
- [ ] USB'den şarj başladığında ≤ ~60 s içinde "Voltaj yükseliyor" yanıyor
- [ ] USB çıkarıldığında rozet sönüyor (≤ ~30–60 s)
- [ ] %20 altında çubuk kırmızı
