# Proje Dokümantasyonu

ESP32-S3 Super Mini — Dijital Kumpas Veri Okuma projesinin teknik dokümantasyonu. Hızlı başlangıç ve genel tanıtım için kök [README.md](../README.md) dosyasına bakın.

**Son güncelleme:** 2026-10-10 · **Kapsadığı kod:** commit `2e2619b`

---

## Nereden Başlamalıyım?

| Ben… | Okuyun |
|---|---|
| Cihazı ilk kez kuracağım | [GUIDE-001](guide/GUIDE-001_GELISTIRME_ORTAMI_KURULUMU.md) → [GUIDE-002](guide/GUIDE-002_DONANIM_BAGLANTISI.md) → [GUIDE-004](guide/GUIDE-004_WIFI_YAPILANDIRMASI.md) → [GUIDE-003](guide/GUIDE-003_DERLEME_YUKLEME_VE_SERI_MONITOR.md) |
| Ölçüm alacağım | [GUIDE-005](guide/GUIDE-005_WEB_ARAYUZU_KULLANIMI.md), [GUIDE-007](guide/GUIDE-007_PYTHON_OTOMASYON_SCRIPTI.md) |
| Bir şey çalışmıyor | [GUIDE-012](guide/GUIDE-012_SORUN_GIDERME.md), [Bilinen Sorunlar](BILINEN_SORUNLAR.md) |
| Kodu değiştireceğim | [SDD](SDD_YAZILIM_TASARIM_DOKUMANI.md) (§3, §8, §12) → [GUIDE-014](guide/GUIDE-014_GELISTIRME_VE_KATKI_KURALLARI.md) |
| Farklı bir kumpas bağlayacağım | [GUIDE-010](guide/GUIDE-010_YENI_KUMPAS_MODELINE_UYARLAMA.md) |
| Kendi yazılımımdan ölçüm okuyacağım | [GUIDE-006](guide/GUIDE-006_HTTP_VE_WEBSOCKET_API_REFERANSI.md) |
| Sistemin ne yapması gerektiğini görmek istiyorum | [SRS](SRS_YAZILIM_GEREKSINIM_DOKUMANI.md) |
| "Bu neden böyle yapılmış?" | [ADR listesi](#mimari-karar-kayıtları-adr) |

---

## Ana Dokümanlar

| Doküman | İçerik |
|---|---|
| [SRS — Yazılım Gereksinim Dokümanı](SRS_YAZILIM_GEREKSINIM_DOKUMANI.md) | Kapsam, kullanıcılar, 84 fonksiyonel, 29 fonksiyonel olmayan, 17 arayüz gereksinimi ve 7 kısıt, karşılanma durumu, izlenebilirlik matrisi |
| [SDD — Yazılım Tasarım Dokümanı](SDD_YAZILIM_TASARIM_DOKUMANI.md) | Bağlam, katmanlar, 9 bileşenin ayrıntılı tasarımı, sıra diyagramları, veri/JSON şemaları, bölüm tablosu, zamanlama analizi, güvenlik, tasarım değişmezleri |
| [Bilinen Sorunlar ve Teknik Borç](BILINEN_SORUNLAR.md) | KS-01…KS-27: kod incelemesinde bulunan hatalar, tutarsızlıklar ve riskler; önerilen düzeltmeler |

## Mimari Karar Kayıtları (ADR)

| No | Başlık | Durum |
|---|---|---|
| [ADR-001](adr/ADR-001_DONANIM_PLATFORMU_ESP32_S3_SUPER_MINI.md) | Donanım platformu olarak ESP32-S3 Super Mini | Kabul edildi |
| [ADR-002](adr/ADR-002_PLATFORMIO_VE_ARDUINO_CATISI.md) | PlatformIO derleme sistemi ve Arduino-ESP32 çatısı | Kabul edildi |
| [ADR-003](adr/ADR-003_KESME_TABANLI_KUMPAS_OKUMA.md) | Kumpas verisinin kesme (ISR) tabanlı okunması | Kabul edildi |
| [ADR-004](adr/ADR-004_ZAMAN_BOSLUGU_ILE_CERCEVE_AYIRMA.md) | Zaman boşluğu ile çerçeve ayırma ve gürültü eşiği | Kabul edildi |
| [ADR-005](adr/ADR-005_ISR_ANA_DONGU_VERI_PAYLASIMI.md) | ISR–ana döngü veri paylaşımı: tek slotlu posta kutusu ve portMUX | Kabul edildi |
| [ADR-006](adr/ADR-006_KUMPAS_BIT_DUZENI_VE_ISARET_BITI.md) | Kumpas bit düzeni: 20-bit büyüklük, bit 21 işaret, bit 23 birim | Kabul edildi |
| [ADR-007](adr/ADR-007_TEKRAR_TABANLI_STABILIZASYON_FILTRESI.md) | Tekrar tabanlı stabilizasyon filtresi | Kabul edildi |
| [ADR-008](adr/ADR-008_SUREKLILIK_ILE_BIRIM_CIKARIMI.md) | Süreklilik ile mm/inç birim çıkarımı | Kabul edildi — revizyon önerilir |
| [ADR-009](adr/ADR-009_TEK_SUPER_DONGU_MIMARISI.md) | Tek süper döngü mimarisi | Kabul edildi |
| [ADR-010](adr/ADR-010_MODUL_AYRIMI_VE_WEB_UI_ARAYUZU.md) | Ölçüm ve ağ katmanlarının ayrılması — `web_ui.h` | Kabul edildi |
| [ADR-011](adr/ADR-011_HTTP_POLLING_VE_WEBSOCKET_PUSH.md) | Tarayıcı için HTTP sorgulama, otomasyon için WebSocket push | Kabul edildi |
| [ADR-012](adr/ADR-012_GOMULU_HTML_PROGMEM.md) | Web arayüzünün firmware içine gömülmesi | Kabul edildi |
| [ADR-013](adr/ADR-013_JSON_OLCUM_SOZLESMESI.md) | Ölçüm JSON sözleşmesi | Kabul edildi |
| [ADR-014](adr/ADR-014_KALIBRASYON_OFSETI_NVS_PREFERENCES.md) | Kalibrasyonun tek ofset olarak NVS'de saklanması | Kabul edildi |
| [ADR-015](adr/ADR-015_TARAYICI_UZERINDEN_OTA_VE_CIFT_SLOT.md) | Tarayıcı üzerinden OTA ve 4 MB çift slot bölüm tablosu | Kabul edildi |
| [ADR-016](adr/ADR-016_WIFI_STA_MDNS_VE_GIZLI_BILGI_YONETIMI.md) | WiFi STA, mDNS ve derleme zamanı gizli bilgi yönetimi | Kabul edildi |
| [ADR-017](adr/ADR-017_BATARYA_OLCUMU_VE_SARJ_CIKARIMI.md) | Gerilim bölücü ile batarya ölçümü ve şarj çıkarımı | Kabul edildi |
| [ADR-018](adr/ADR-018_BUTON_GIRISI_VE_DEBOUNCE.md) | Fiziksel buton girişi ve debounce | Kabul edildi — düzeltme gerekli |
| [ADR-019](adr/ADR-019_RAM_TABANLI_HALKA_LOG_TAMPONU.md) | RAM tabanlı halka log tamponu | Kabul edildi |
| [ADR-020](adr/ADR-020_ISTEMCI_TARAFI_BUTON_LOGU_VE_CSV.md) | Buton logunun tarayıcıda tutulması ve CSV | Kabul edildi |
| [ADR-021](adr/ADR-021_PYTHON_ILE_KLAVYE_EMULASYONU.md) | Python + pyautogui ile klavye emülasyonu | Kabul edildi |
| [ADR-022](adr/ADR-022_KIMLIK_DOGRULAMASIZ_YEREL_AG_GUVENLIK_MODELI.md) | Güvenilir yerel ağ varsayımı — kimlik doğrulamasız servisler | Kabul edildi — yeniden değerlendirilmeli |
| [ADR-023](adr/ADR-023_DOKUMANTASYON_YAPISI_ADR_VE_GUIDE.md) | Dokümantasyon yapısı: SRS, SDD, ADR, GUIDE | Kabul edildi |

## Rehberler (GUIDE)

| No | Başlık | Hedef kitle |
|---|---|---|
| [GUIDE-001](guide/GUIDE-001_GELISTIRME_ORTAMI_KURULUMU.md) | Geliştirme ortamı kurulumu | Herkes |
| [GUIDE-002](guide/GUIDE-002_DONANIM_BAGLANTISI.md) | Donanım bağlantısı | Kurulumcu |
| [GUIDE-003](guide/GUIDE-003_DERLEME_YUKLEME_VE_SERI_MONITOR.md) | Derleme, yükleme ve seri monitör | Kurulumcu, geliştirici |
| [GUIDE-004](guide/GUIDE-004_WIFI_YAPILANDIRMASI.md) | WiFi yapılandırması ve cihaza erişim | Kurulumcu |
| [GUIDE-005](guide/GUIDE-005_WEB_ARAYUZU_KULLANIMI.md) | Web arayüzü kullanımı | Operatör |
| [GUIDE-006](guide/GUIDE-006_HTTP_VE_WEBSOCKET_API_REFERANSI.md) | HTTP ve WebSocket API referansı | Entegratör |
| [GUIDE-007](guide/GUIDE-007_PYTHON_OTOMASYON_SCRIPTI.md) | Python otomasyon scripti | Operatör, kurulumcu |
| [GUIDE-008](guide/GUIDE-008_KALIBRASYON.md) | Kalibrasyon | Kurulumcu, kalite |
| [GUIDE-009](guide/GUIDE-009_OTA_GUNCELLEME.md) | OTA (kablosuz) firmware güncelleme | Kurulumcu, geliştirici |
| [GUIDE-010](guide/GUIDE-010_YENI_KUMPAS_MODELINE_UYARLAMA.md) | Yeni bir kumpas modeline uyarlama | Geliştirici |
| [GUIDE-011](guide/GUIDE-011_BATARYA_OLCUMU_AYARI.md) | Batarya ölçümünün ayarlanması | Kurulumcu, geliştirici |
| [GUIDE-012](guide/GUIDE-012_SORUN_GIDERME.md) | Sorun giderme | Herkes |
| [GUIDE-013](guide/GUIDE-013_PARAMETRE_REFERANSI.md) | Parametre referansı | Geliştirici |
| [GUIDE-014](guide/GUIDE-014_GELISTIRME_VE_KATKI_KURALLARI.md) | Geliştirme ve katkı kuralları | Geliştirici |
| [GUIDE-015](guide/GUIDE-015_TEST_VE_DOGRULAMA.md) | Test ve doğrulama | Geliştirici, test |

---

## Klasör Yapısı ve Adlandırma

```
doc/
├── README.md                              ← bu dosya (dizin + şablonlar)
├── SRS_YAZILIM_GEREKSINIM_DOKUMANI.md
├── SDD_YAZILIM_TASARIM_DOKUMANI.md
├── BILINEN_SORUNLAR.md
├── adr/
│   └── ADR-NNN_BUYUK_HARF_BASLIK.md
└── guide/
    └── GUIDE-NNN_BUYUK_HARF_BASLIK.md
```

- `NNN`: sıfır dolgulu üç haneli numara (`001`, `002`, …). Numaralar **yeniden kullanılmaz**.
- Başlık kısmı büyük harf, kelimeler alt çizgiyle ayrılır, **Türkçe karakter kullanılmaz** (Ş→S, Ğ→G, İ/I→I, Ü→U, Ö→O, Ç→C).
- İçerik Türkçe karakterlerle yazılır.
- Gerekçesi: [ADR-023](adr/ADR-023_DOKUMANTASYON_YAPISI_ADR_VE_GUIDE.md).

## Kimlik Kodları

| Önek | Anlam | Tanımlandığı yer |
|---|---|---|
| `FG-…`, `FOG-…`, `AG-…`, `KIS-…` | Gereksinimler | SRS |
| `C1…C9` | Bileşenler | SDD §3.2 |
| `KS-nn` | Bilinen sorun | BILINEN_SORUNLAR.md |
| `TC-nn` | Test senaryosu | GUIDE-015 |
| `ADR-NNN`, `GUIDE-NNN` | Karar / rehber | Bu dizin |

---

## Şablonlar

### ADR Şablonu

```markdown
# ADR-NNN: <Kararın Kısa Adı>

| Alan | Değer |
|---|---|
| **Durum** | Önerildi / Kabul edildi / Reddedildi / Kullanımdan kaldırıldı / Yerine geçildi: ADR-XXX |
| **Karar tarihi** | YYYY-MM-DD (commit `xxxxxxx`) |
| **Dokümante edilme** | YYYY-MM-DD |
| **İlgili** | ADR-…, SRS …, SDD §…, KS-… |

## Bağlam
Hangi sorun/ihtiyaç bu kararı gerektirdi? Hangi kısıtlar vardı?

## Karar
Ne yapılacak? Somut değerler, kurallar.

## Değerlendirilen Alternatifler
| Seçenek | Artı | Eksi / Neden seçilmedi |
|---|---|---|

## Sonuçlar
**Olumlu**
- …

**Olumsuz / Riskler**
- …

## Uygulama Notları
Kod referansları (`../../src/…#Lnn`).

## Doğrulama
Kararın doğru uygulandığı nasıl gösterilir? (TC-nn)
```

### GUIDE Şablonu

```markdown
# GUIDE-NNN: <Görevin Adı>

**Hedef kitle:** …
**Ön koşul:** …
**İlgili:** ADR-…, KS-…

---

## 1. Ne Zaman / Neden
## 2. Gerekenler
## 3. Adımlar
1. …
## 4. Doğrulama (kontrol listesi)
- [ ] …
## 5. Sorun Giderme
| Belirti | Neden | Çözüm |
|---|---|---|
```

### Bilinen Sorun Kaydı Şablonu

```markdown
### KS-nn — <Başlık> 🔴/🟠/🟡

- **Yer:** dosya:satır bağlantısı
- **Gözlem:** ne oluyor
- **Etki:** kullanıcıya/sisteme etkisi
- **Öneri:** önerilen düzeltme
- **İlgili:** SRS gereksinimi, ADR
```

---

## Bakım Kuralları

1. Davranışı değiştiren her PR ilgili SRS/SDD bölümünü ve gerekirse ADR/GUIDE'ı **aynı PR'da** günceller ([GUIDE-014 §5](guide/GUIDE-014_GELISTIRME_VE_KATKI_KURALLARI.md)).
2. ADR gövdeleri karar kabul edildikten sonra değiştirilmez; yeni ADR yazılır ve eskisinin durumu güncellenir.
3. Kod satır referansları (`#L123`) kod değiştikçe kayabilir; büyük refaktörlerden sonra SDD ve ADR'lerdeki bağlantıları gözden geçirin.
4. Doküman başlığındaki "Kapsadığı kod" commit'ini güncel tutun.
