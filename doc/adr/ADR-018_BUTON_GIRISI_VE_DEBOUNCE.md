# ADR-018: Fiziksel Buton Girişi ve Yazılımsal Debounce

| Alan | Değer |
|---|---|
| **Durum** | Kabul edildi — **düzeltme gerekli** (KS-05) |
| **Karar tarihi** | 2026-04-28 (commit `fdddf76`) |
| **Dokümante edilme** | 2026-10-10 |
| **İlgili** | [ADR-020](ADR-020_ISTEMCI_TARAFI_BUTON_LOGU_VE_CSV.md), [ADR-021](ADR-021_PYTHON_ILE_KLAVYE_EMULASYONU.md), SRS FG-BTN-*, SDD §4.6, [GUIDE-002](../guide/GUIDE-002_DONANIM_BAGLANTISI.md), [KS-05](../BILINEN_SORUNLAR.md#ks-05--buton-bağlantısı-ile-kod-mantığı-uyumsuz-) |

## Bağlam

Operatör ölçüm aldığında bir tuşa basarak değeri kaydetmek ister ("ayak pedalı" veya kumpas üzerindeki bir buton). Kayıt mantığı istemcide (tarayıcı veya Python) yapılır; cihazın görevi yalnızca butonun durumunu güvenilir şekilde bildirmektir.

## Karar

- GPIO10, `INPUT_PULLUP`.
- Her `loop()` turunda okunur; seviye 20 ms boyunca sabit kalırsa değişim kabul edilir (zaman tabanlı debounce).
- Kabul edilen her değişim `webUpdateButton(pressed)` ile web katmanına itilir; WS istemcileri anında, HTTP istemcileri sonraki sorguda görür.
- **Mevcut kodda** "basılı" = pin HIGH kabul edilir (`pressed = (digitalRead != LOW)`).
- Kenar algılama (basıldı anı) cihazda değil, istemcide yapılır.

## Sorun

`INPUT_PULLUP` ile "HIGH = basılı" birbirine zıttır:

| Bağlantı | Boşta | Basılı | Sonuç |
|---|---|---|---|
| Buton → 3,3 V (README'deki) | HIGH | HIGH | **Basış hiç algılanmaz** |
| Buton → GND | HIGH (= "basılı") | LOW (= "bırakıldı") | **Ters**: kayıt bırakmada olur, boşta `btn=pressed` |
| Aktif-yüksek modül (ör. TTP223) | LOW | HIGH | Doğru |

## Önerilen Düzeltme (bu ADR'nin revizyonu)

**Seçenek A (önerilen):** Buton → GND, dahili pull-up korunur, mantık ters çevrilir:

```cpp
const bool buttonReading = (digitalRead(BUTTON_PIN) == LOW);   // basılı = LOW
static bool lastButtonReading = false;
static bool lastButtonStable  = false;
```

Gerekçe: GND'ye çeken buton en yaygın ve gürültüye en dayanıklı bağlantıdır; dahili pull-up ek direnç gerektirmez.

**Seçenek B:** Buton → 3,3 V, `pinMode(BUTTON_PIN, INPUT_PULLDOWN)`; mevcut mantık korunur.

Hangisi seçilirse README §5, [GUIDE-002](../guide/GUIDE-002_DONANIM_BAGLANTISI.md) ve bu ADR güncellenmeli; KS-05 kapatılmalıdır.

## Değerlendirilen Alternatifler

| Seçenek | Neden seçilmedi |
|---|---|
| Buton için GPIO kesmesi | 20 ms debounce ile loop yoklaması yeterli; kesmede debounce karmaşık |
| Cihazda basış sayacı / olay kuyruğu | İstemci tarafı kenar algılama daha basitti; ama HTTP istemcilerinde kısa basışlar kaçabiliyor (KS-07) → **gelecekte `pressCount` alanı önerilir** |
| Donanım RC debounce | Yazılım çözümü yeterli |

## Uygulama Notları

- [main.cpp:11](../../src/main.cpp#L11), [main.cpp:294](../../src/main.cpp#L294), [main.cpp:315-317](../../src/main.cpp#L315-L317), [main.cpp:332-343](../../src/main.cpp#L332-L343)
