# GUIDE-004: WiFi Yapılandırması ve Cihaza Erişim

**Hedef kitle:** Kurulumcu  
**İlgili:** [ADR-016](../adr/ADR-016_WIFI_STA_MDNS_VE_GIZLI_BILGI_YONETIMI.md), [ADR-022](../adr/ADR-022_KIMLIK_DOGRULAMASIZ_YEREL_AG_GUVENLIK_MODELI.md), [KS-04](../BILINEN_SORUNLAR.md#ks-04--wifi-şifresi-firmwarebin-içinde-düz-metin-), [KS-09](../BILINEN_SORUNLAR.md#ks-09--wifi-sonradan-bağlanırsa-mdns-başlamıyor-)

---

## 1. Ağ Gereksinimleri

| Gereksinim | Açıklama |
|---|---|
| Frekans | **2,4 GHz** (5 GHz desteklenmez). Çift bantlı router'da 2,4 GHz açık olmalı |
| Güvenlik | Açık, WPA veya WPA2-Personal. WPA2/WPA3 karma mod genellikle çalışır; yalnız-WPA3 ve WPA2-Enterprise (kurumsal, kullanıcı adlı) desteklenmez |
| IP | DHCP |
| İstemci izolasyonu | **Kapalı** olmalı — misafir ağlarında cihazlar birbirini görmez |
| Multicast | mDNS (`kumpas.local`) için engellenmemeli |
| Güven | Yalnızca güvendiğiniz ağlar (cihazda kimlik doğrulama yok — ADR-022) |

Telefon hotspot'u da kullanılabilir (bilgisayar da aynı hotspot'a bağlı olmalı).

## 2. `wifi_secrets.h` Oluşturma

`include/wifi_secrets.h`:

```cpp
#pragma once

static constexpr const char *WIFI_SSID     = "AgAdiniz";
static constexpr const char *WIFI_PASSWORD = "Sifreniz";
```

- SSID ve şifre **büyük/küçük harfe duyarlıdır**.
- Şifredeki `"` ve `\` karakterlerini `\"` ve `\\` olarak yazın.
- Dosya `.gitignore`'dadır; `git status` çıktısında görünmemelidir. Görünüyorsa `.gitignore`'u kontrol edin ve **commit etmeyin**.

## 3. Cihazın Adresini Bulma

Tercih sırasıyla:

1. **mDNS:** `http://kumpas.local/`
2. **Seri monitör:** açılışta `WiFi baglandi: … IP: 192.168.x.y` satırı ([GUIDE-003 §4](GUIDE-003_DERLEME_YUKLEME_VE_SERI_MONITOR.md))
3. **Router arayüzü:** DHCP istemci listesinde `esp32s3-XXXXXX` benzeri bir ad
4. **Ağ tarama:** `arp -a` veya bir ağ tarayıcı uygulaması

### Windows'ta `.local` Çalışmıyorsa

- Windows 10 (1803+) ve 11 mDNS'i çoğunlukla çözer; çözmezse IP kullanın veya Bonjour (Apple) kurun.
- VPN'ler ve bazı güvenlik duvarları multicast'i engeller.
- `ping kumpas.local` ile test edin.

### Sabit Adres İstiyorsanız

Router'da cihazın MAC adresine **DHCP rezervasyonu** yapın. Firmware'de statik IP tanımlamak önerilmez (ağ değişince erişim kaybolur).

## 4. Ağsız Çalışma

WiFi 15 s içinde bağlanamazsa cihaz:

- Kumpas okumaya, buton ve bataryayı izlemeye devam eder,
- HTTP/WS sunucularını yine başlatır (WiFi sonradan bağlanırsa IP ile erişilebilir),
- **mDNS'i başlatmaz** (KS-09) → `kumpas.local` çalışmaz; Python scriptinde `WS_URL`'yi IP ile değiştirin.

Router'dan sonra açılması gereken durumlarda cihazı router hazır olduktan sonra yeniden başlatın (RESET).

## 5. Ağ Değiştirme

1. `wifi_secrets.h`'yi yeni bilgilerle güncelleyin.
2. Derleyin ve yükleyin:
   - **USB ile** (önerilen): `pio run -t upload`
   - **OTA ile:** cihaz hâlâ eski ağdayken `/update`'ten yükleyin. ⚠️ Yeni bilgiler yanlışsa cihaz yeni ağa bağlanamaz ve OTA'ya da erişilemez → USB ile kurtarma gerekir ([GUIDE-009 §8](GUIDE-009_OTA_GUNCELLEME.md)).

## 6. Aynı Ağda Birden Fazla Cihaz

mDNS adı sabittir (`kumpas`). İkinci cihazı derlemeden önce [web_ui.cpp:17](../../src/web_ui.cpp#L17)'yi değiştirin:

```cpp
static const char MDNS_HOSTNAME[] = "kumpas2";   // → http://kumpas2.local
```

Python scriptinde `WS_URL = "ws://kumpas2.local:81/"`.

## 7. Güvenlik Notları

- Şifre derlenmiş `firmware.bin` içinde **düz metin** olarak bulunur. `firmware.bin`'i kimseyle paylaşmayın, depoya eklemeyin (KS-04).
- Cihazı internete açmayın (port yönlendirme yapmayın).

## 8. Kontrol Listesi

- [ ] Seri monitörde `WiFi baglandi` ve IP görünüyor
- [ ] `ping kumpas.local` yanıt veriyor (veya IP'ye ping)
- [ ] Tarayıcıda `http://kumpas.local/` açılıyor, durum `online`
- [ ] `git status` çıktısında `wifi_secrets.h` yok
