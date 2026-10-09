# ADR-023: Dokümantasyon Yapısı — `doc/` Altında SRS, SDD, ADR ve GUIDE

| Alan | Değer |
|---|---|
| **Durum** | Kabul edildi |
| **Karar tarihi** | 2026-10-10 |
| **İlgili** | [doc/README.md](../README.md), SRS FOG-BAK-03, [GUIDE-014](../guide/GUIDE-014_GELISTIRME_VE_KATKI_KURALLARI.md) |

## Bağlam

Proje bilgisinin tamamı tek bir uzun `README.md` dosyasındaydı (~850 satır): kurulum, kullanım, protokol, mimari, sorun giderme ve "tasarım kararları" tablosu iç içeydi. Kararların **neden** alındığı, hangi alternatiflerin reddedildiği ve hangi risklerin kabul edildiği kayıt altında değildi. Kod incelemesinde README ile kod arasında tutarsızlıklar bulundu (KS-18).

## Karar

Dokümantasyon `doc/` klasöründe dört türe ayrılır:

| Tür | Dosya adı biçimi | Amaç | Değişim sıklığı |
|---|---|---|---|
| SRS | `SRS_YAZILIM_GEREKSINIM_DOKUMANI.md` | Sistem **ne** yapmalı | Özellik eklendikçe |
| SDD | `SDD_YAZILIM_TASARIM_DOKUMANI.md` | Sistem **nasıl** yapıyor | Mimari değiştikçe |
| ADR | `adr/ADR-NNN_BUYUK_HARF_BASLIK.md` | **Neden** böyle karar verildi | Karar başına bir kez; sonra yalnızca durum değişir |
| GUIDE | `guide/GUIDE-NNN_BUYUK_HARF_BASLIK.md` | Bir işi **adım adım** nasıl yaparım | Prosedür değiştikçe |
| Bilinen sorunlar | `BILINEN_SORUNLAR.md` | Açık hatalar ve teknik borç | Sürekli |

Kurallar:

- Numaralar sıfır dolgulu üç hanelidir ve **asla yeniden kullanılmaz**.
- Dosya adlarında Türkçe karakter kullanılmaz (ı→I, ş→S, ğ→G, ü→U, ö→O, ç→C) — farklı işletim sistemleri ve araçlarla uyumluluk için. Doküman içeriği Türkçe karakterlerle yazılır.
- ADR'ler **değiştirilmez, yerine geçilir**: kararı değiştiren yeni ADR yazılır, eskisinin durumu "Yerine geçildi: ADR-NNN" yapılır. Yazım düzeltmesi ve "İlgili" bağlantıları istisnadır.
- ADR durumları: `Önerildi`, `Kabul edildi`, `Reddedildi`, `Kullanımdan kaldırıldı`, `Yerine geçildi`.
- Kök `README.md` hızlı başlangıç ve genel bakış olarak kalır; ayrıntılar `doc/`'a bağlanır.
- Kod referansları depo köküne göre göreli Markdown bağlantılarıyla verilir (`../../src/main.cpp#L186`).
- Bir PR davranış değiştiriyorsa ilgili SRS/SDD bölümü ve gerekiyorsa yeni ADR aynı PR'da güncellenir.

Şablonlar: [doc/README.md](../README.md#şablonlar).

## Değerlendirilen Alternatifler

| Seçenek | Neden seçilmedi |
|---|---|
| Her şeyi README'de tutmak | Büyüdükçe gezinilemez; karar gerekçesi kaybolur |
| Wiki (GitHub Wiki, Notion) | Kodla aynı commit'te sürümlenmez |
| Doxygen | API belgesi için iyi; karar/rehber için uygun değil — gelecekte tamamlayıcı olabilir |
| MADR şablonunu birebir kullanmak | Temel alındı; Türkçe başlıklar ve "Uygulama Notları/Doğrulama" bölümleri eklendi |

## Sonuçlar

**Olumlu**
- Yeni katkıcı (veya AI asistanı) için net giriş noktaları.
- Kararlar ve kabul edilen riskler izlenebilir.

**Olumsuz**
- Doküman–kod senkronizasyonu disiplin gerektirir; PR kontrol listesi [GUIDE-014](../guide/GUIDE-014_GELISTIRME_VE_KATKI_KURALLARI.md)'te.
