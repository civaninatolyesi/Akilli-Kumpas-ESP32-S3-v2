# ADR-016: WiFi STA Modu, mDNS ve Derleme Zamanı Gizli Bilgi Yönetimi

| Alan | Değer |
|---|---|
| **Durum** | Kabul edildi |
| **Karar tarihi** | 2026-04-27 (STA + `wifi_secrets.h`, `1c9ff0d`), 2026-04-28 (mDNS, `fdddf76`) |
| **Dokümante edilme** | 2026-10-10 |
| **İlgili** | [ADR-022](ADR-022_KIMLIK_DOGRULAMASIZ_YEREL_AG_GUVENLIK_MODELI.md), SRS FG-NET-*, FOG-SEC-01/02, KIS-05, SDD §4.7.1, [GUIDE-004](../guide/GUIDE-004_WIFI_YAPILANDIRMASI.md), KS-04, KS-09 |

## Bağlam

Cihaz, kullanıcının mevcut atölye/ev ağına katılmalı ki aynı ağdaki bilgisayar ve telefonlar ona erişebilsin. IP adresi DHCP ile değişebilir; kullanıcıdan IP takip etmesi beklenmemeli. WiFi şifresi kaynak deposuna girmemeli.

## Karar

1. **STA (istemci) modu**, `WiFi.setSleep(false)` (modem uyku kapalı → düşük ve tutarlı gecikme).
2. Açılışta **15 s** bağlantı denemesi; başarısızsa cihaz ağsız devam eder (ölçüm sürer, HTTP/WS sunucuları yine başlatılır).
3. Bağlantı başarılıysa **mDNS**: ana makine adı `kumpas` → `http://kumpas.local`, servis `_http._tcp:80`.
4. Kimlik bilgileri `include/wifi_secrets.h` içinde `WIFI_SSID` / `WIFI_PASSWORD` sabitleri olarak tutulur; dosya `.gitignore`'dadır.

## Değerlendirilen Alternatifler

| Seçenek | Neden seçilmedi |
|---|---|
| AP (erişim noktası) modu | Bilgisayar internetten kopar; Python ve Excel aynı anda çalışırken ağ değiştirmek pratik değil |
| AP+STA ve yapılandırma portalı (WiFiManager vb.) | Daha kullanıcı dostu; ek bağımlılık ve karmaşıklık. **Gelecek için önerilir** (KS-04'ü de çözer) |
| Statik IP | Ağdan ağa taşınamaz |
| Kimlik bilgilerini `platformio.ini` `build_flags`'e koymak | `platformio.ini` depoya girer |
| Sabit IP yerine yalnızca IP ile erişim | Kullanıcı deneyimi kötü |

## Sonuçlar

**Olumlu**
- Basit, tek dosyada yapılandırma; depo temiz kalır.
- `kumpas.local` çoğu macOS/Linux/iOS/Android ve güncel Windows istemcisinde çalışır.
- WiFi yokken bile cihaz seri port üzerinden teşhis edilebilir ve ölçüm yapmaya devam eder.

**Olumsuz / Riskler**
- Ağ değiştirmek yeniden derleme ve USB/OTA yükleme gerektirir.
- Şifre `firmware.bin` içinde düz metindir (KS-04).
- WiFi açılıştan sonra bağlanırsa mDNS başlamaz; açık yeniden bağlanma işleyicisi yoktur (KS-09).
- Aynı ağda iki cihaz `kumpas` adını kullanırsa mDNS çakışır; ad sabittir.
- Bazı Windows ortamlarında `.local` çözülmez → IP kullanılmalı.
- 5 GHz ağlar desteklenmez (ESP32-S3 yalnızca 2,4 GHz).
- Klonlanan projede `wifi_secrets.h` olmadığı için derleme hatası alınır — bilinçli bir hatırlatıcıdır ([GUIDE-004](../guide/GUIDE-004_WIFI_YAPILANDIRMASI.md)).

## Uygulama Notları

- [web_ui.cpp:11](../../src/web_ui.cpp#L11), [web_ui.cpp:17](../../src/web_ui.cpp#L17), [web_ui.cpp:767-801](../../src/web_ui.cpp#L767-L801)
- [.gitignore](../../.gitignore)
