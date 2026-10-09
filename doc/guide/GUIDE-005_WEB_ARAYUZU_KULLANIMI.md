# GUIDE-005: Web Arayüzü Kullanımı

**Hedef kitle:** Operatör  
**Ön koşul:** Cihaz ağa bağlı ([GUIDE-004](GUIDE-004_WIFI_YAPILANDIRMASI.md))  
**İlgili:** [ADR-011](../adr/ADR-011_HTTP_POLLING_VE_WEBSOCKET_PUSH.md), [ADR-020](../adr/ADR-020_ISTEMCI_TARAFI_BUTON_LOGU_VE_CSV.md)

---

## 1. Sayfalar

| Adres | Sayfa | Amaç |
|---|---|---|
| `http://kumpas.local/` | Ana sayfa | Canlı ölçüm, batarya, buton logu, CSV |
| `http://kumpas.local/log` | Log | Cihaz olay kayıtları (WiFi, OTA, kalibrasyon) |
| `http://kumpas.local/calibrate` | Kalibrasyon | Ofset girme/sıfırlama ([GUIDE-008](GUIDE-008_KALIBRASYON.md)) |
| `http://kumpas.local/update` | Yazılım Güncelle | OTA ([GUIDE-009](GUIDE-009_OTA_GUNCELLEME.md)) |

Arayüz internet gerektirmez; telefon, tablet ve bilgisayarda çalışır, sistem temasına göre açık/koyu görünür.

## 2. Ana Sayfa

### 2.1 Ölçüm Kartı

- **Milimetre:** 2 ondalık, kalibrasyon ofseti uygulanmış.
- **İnç:** 4 ondalık, **ofset uygulanmamış** (KS-12).
- Değerler yalnızca kumpas değeri **durduğunda** güncellenir. Çene hareket ederken son kararlı değer kalır — bu, filtrenin bilinçli davranışıdır ([ADR-007](../adr/ADR-007_TEKRAR_TABANLI_STABILIZASYON_FILTRESI.md)).

### 2.2 Durum Rozetleri

| Rozet | Örnek | Anlamı | Ne zaman endişelenmeli |
|---|---|---|---|
| Bağlantı | `online` / `offline` | Son `/api/measure` isteği başarılı mı | `offline` sürekli ise ağ/cihaz sorunu |
| Yaş | `120 ms` | Cihaz durumunun son güncellenmesinden bu yana geçen süre | Ölçüm yaşı **değildir**: batarya raporları her 5 s'de sıfırlar (KS-06) |
| Ham | `raw=0x0009A6` | Son kararlı çerçevenin alt 22 biti (hex) | Kumpas uyarlarken kullanılır |
| Birim | `unit=mm` | Cihazın çıkardığı kumpas birimi | Kumpas mm modundayken `inch` görünüyorsa → [GUIDE-012](GUIDE-012_SORUN_GIDERME.md) |
| İşaret | `sign=+` | Kumpas değeri negatif mi (ofset öncesi) | |
| Buton | `btn=released` | Butonun anlık durumu | Boşta `pressed` görünüyorsa → KS-05 |
| Kalibrasyon | `cal=0.000 mm` | Aktif ofset | 0'dan farklıysa bilinçli olduğundan emin olun |

### 2.3 Pil Kartı

- **Yüzde** ve **gerilim** (filtrelenmiş).
- **Dolum çubuğu:** %20 ve altında kırmızı/turuncu.
- **"Voltaj yükseliyor"** rozeti: gerilim artış eğilimi algılandığında yanar (şarj tahmini; kesin şarj göstergesi değildir).
- Batarya bağlı değilse anlamsız değerler görünür.

### 2.4 Buton Logu

1. (İsteğe bağlı) **Not** alanına parça no/konum yazın.
2. Kumpası ölçüm noktasında sabit tutun, değerin oturmasını bekleyin.
3. Fiziksel butona basın → tabloya en üste yeni satır eklenir: tarayıcı saati, mm, inç, not. Not alanı temizlenir.
4. Bir satırın notunu düzeltmek için ✎'ye basın, yazın, alanın dışına tıklayın.

| Özellik | Değer |
|---|---|
| Azami satır | 200 (eskiler silinir) |
| Saklama yeri | Yalnızca bu tarayıcı sekmesi |
| Sayfa yenilenirse | **Tüm kayıtlar silinir** — önce CSV indirin |
| Kısa basışlar | 200 ms'den kısa basışlar kaçırılabilir (KS-07) — butona tam basıp yarım saniye kadar tutun |

**Temizle** yalnızca bu sekmedeki tabloyu temizler.

### 2.5 CSV İndirme

**CSV İndir** → `buton-logu-YYYYMMDD-HHMMSS.csv`. Biçim:

```csv
"Zaman","mm","in","Not"
"14:23:01.234","12.35","0.4862","Parça A-1"
```

- Sıra eskiden yeniye.
- UTF-8 BOM içerir; Excel Türkçe karakterleri doğru gösterir.
- Ondalık ayırıcı **nokta**dır.

**Excel (Türkçe bölgesel ayar) ile açma:** Çift tıklamada sütunlar ayrılmaz veya sayılar metin olarak gelirse: Excel → **Veri → Metinden/CSV'den** → Sınırlayıcı: *Virgül* → **Veriyi Dönüştür** → mm/in sütunlarında *Türü Değiştir → Yerel Ayarı Kullanarak → Ondalık Sayı, İngilizce (ABD)*.

## 3. Log Sayfası

- Cihaz açıldığından beri tutulan son 100 olay; 250 ms'de bir yeni satırlar eklenir.
- Örnek satırlar: `Boot: main setup`, `WiFi: connected 192.168.1.42`, `Cal: offset=-0.015`, `OTA: update ok, rebooting`.
- **Temizle** görünümü temizler ama satırlar bir sonraki yenilemede geri gelir (KS-08).
- Cihaz yeniden başlatılınca log sıfırlanır.

## 4. Birden Fazla Kullanıcı/Sekme

Birden fazla tarayıcı aynı anda bağlanabilir; her biri kendi buton logunu tutar. Sunucu senkron çalıştığı için çok sayıda sekme (≥ 5) güncelleme aralığını uzatabilir.

## 5. Sık Sorulan Sorular

**Değer kumpas ekranından farklı.** Ofseti (`cal=`) kontrol edin. Fark sabit bir katsayıysa (×2, ×0,5, ×1,27) → [GUIDE-010](GUIDE-010_YENI_KUMPAS_MODELINE_UYARLAMA.md) / [GUIDE-012](GUIDE-012_SORUN_GIDERME.md).

**Değer hiç değişmiyor (`--`).** Cihaz hiç kararlı çerçeve almamış → donanım bağlantısı ([GUIDE-012](GUIDE-012_SORUN_GIDERME.md) §2).

**Butona basıyorum, satır eklenmiyor.** `btn=` rozetinin değiştiğini kontrol edin. Değişmiyorsa → KS-05 ve [GUIDE-002 §5](GUIDE-002_DONANIM_BAGLANTISI.md).
