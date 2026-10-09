# GUIDE-008: Kalibrasyon

**Hedef kitle:** Kurulumcu, kalite sorumlusu  
**İlgili:** [ADR-014](../adr/ADR-014_KALIBRASYON_OFSETI_NVS_PREFERENCES.md), [ADR-006](../adr/ADR-006_KUMPAS_BIT_DUZENI_VE_ISARET_BITI.md), [KS-12](../BILINEN_SORUNLAR.md#ks-12--kalibrasyon-ofseti-yalnızca-mmye-uygulanıyor-), [KS-13](../BILINEN_SORUNLAR.md#ks-13--kalibrasyon-girişi-doğrulanmıyor-)

---

## 1. Hangi Hatayı Hangi Yöntemle Düzeltirim?

| Gözlem | Hata türü | Çözüm |
|---|---|---|
| Okunan değer her ölçümde sabit miktar kadar fazla/eksik (ör. hep +0,02 mm) | Sıfır kayması (ofset) | **Bu rehber** — `/calibrate` |
| Okunan değer kumpas ekranından farklı ama oransal (×2, ×0,5) | Ölçek hatası | `COUNTS_PER_MM` — [GUIDE-010 §6](GUIDE-010_YENI_KUMPAS_MODELINE_UYARLAMA.md) |
| ×1,27 veya ×0,787 fark | Birim hatası | [KS-24](../BILINEN_SORUNLAR.md#ks-24--birim-çıkarımının-algılama-sınırı-ve-yanlış-pozitif-riski-), kumpası mm moduna alıp cihazı yeniden başlatın |
| Kumpas ekranı da yanlış | Kumpasın kendisi | Kumpasın **ZERO** tuşu; çeneleri temizleyin |

> Cihaz, kumpasın ekranında gösterdiği sayıyı aynen aktarır. Kumpas ekranı doğruysa ve web arayüzü ekranla aynıysa kalibrasyona gerek yoktur.

## 2. Ofset Belirleme Prosedürü

**Gerekenler:** Bilinen boyutta bir referans (mastar bloğu, kalibreli pim). Ölçüm ortamı oda sıcaklığında.

1. `/calibrate` sayfasından mevcut ofseti **sıfırlayın** (Offset Sıfırla).
2. Kumpasın çenelerini temizleyin, kapatın, kumpasın ZERO tuşuna basın.
3. Referansı 5 kez ölçün; her seferinde değer oturduktan sonra web arayüzündeki **mm** değerini not edin.
4. Ortalamayı hesaplayın: `ort = (m1+…+m5)/5`.
5. `ofset = referans − ort`.

   Örnek: Referans 25,000 mm; okunanlar 25,02 / 25,01 / 25,02 / 25,02 / 25,01 → ort = 25,016 → **ofset = −0,016 mm**.

6. Ofsetin büyüklüğü kumpas çözünürlüğünün (0,01 mm) birkaç katından küçükse ve tekrarlanabilirlik bundan kötüyse ofset uygulamak anlamlı değildir.

## 3. Ofseti Girme

1. `http://kumpas.local/calibrate`
2. Değeri girin (ör. `-0.015`). Ondalık ayırıcı olarak **nokta** kullanın; adım 0,001 mm.
3. **Kaydet** → sayfa yeniden yüklenir.
4. Ana sayfada `cal=-0.015 mm` rozetini doğrulayın.
5. Kumpası hafifçe oynatıp yeni bir değerde durdurun — **ofset yalnızca yeni gelen ölçümlere uygulanır**, ekrandaki eski değer kendiliğinden değişmez.
6. Referansı tekrar ölçerek doğrulayın.

Komut satırından:

```bash
curl -X POST -d "offset=-0.015" http://kumpas.local/calibrate
```

## 4. Sıfırlama

`/calibrate` → **Offset Sıfırla**. Alternatif olarak `pio run -t erase` (tüm NVS'yi siler).

## 5. Kalıcılık

- Ofset NVS'de `kumpas/offset_mm` olarak saklanır; güç kesintisi ve OTA güncellemesinden etkilenmez.
- Yalnızca flash silme (`erase`) ile kaybolur.

## 6. Bilinen Kısıtlamalar

| Kısıt | Etki | Kayıt |
|---|---|---|
| Ofset yalnızca `mm` alanına eklenir | `inch` ve `isNegative` ofsetsiz | KS-12 |
| Giriş doğrulanmıyor | "abc" girerseniz ofset sessizce 0 olur; aşırı değerler kabul edilir | KS-13 |
| Yetkilendirme yok | Ağdaki herkes ofseti değiştirebilir | KS-03 |
| Tek nokta (yalnızca ofset) | Ölçek hatası düzeltilmez | ADR-014 |

## 7. Kalibrasyon Kaydı (Önerilen Şablon)

| Tarih | Kişi | Referans | Ölçülen ort. | Uygulanan ofset | Doğrulama | Firmware commit |
|---|---|---|---|---|---|---|
| 2026-10-10 | | 25,000 mm | | | | |
