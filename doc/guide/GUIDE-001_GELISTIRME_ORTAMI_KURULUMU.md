# GUIDE-001: Geliştirme Ortamı Kurulumu

**Hedef kitle:** Projeyi ilk kez bilgisayarına kuracak herkes  
**Süre:** 20–40 dakika (indirme hızına bağlı)  
**Sonraki adım:** [GUIDE-002 Donanım Bağlantısı](GUIDE-002_DONANIM_BAGLANTISI.md) → [GUIDE-004 WiFi](GUIDE-004_WIFI_YAPILANDIRMASI.md) → [GUIDE-003 Derleme ve Yükleme](GUIDE-003_DERLEME_YUKLEME_VE_SERI_MONITOR.md)

---

## 1. Gereksinimler

| Bileşen | Sürüm | Not |
|---|---|---|
| İşletim sistemi | Windows 10/11, macOS 12+, Ubuntu 20.04+ | Proje Windows 11'de geliştirildi |
| VS Code | Güncel | |
| PlatformIO IDE eklentisi | Güncel | Kendi Python ortamını (penv) kurar |
| Git | 2.30+ | Depoyu klonlamak için |
| Python | 3.8+ | **Yalnızca** `measure_ws.py` için; firmware derlemek için gerekmez |
| USB-C **veri** kablosu | — | Yalnızca şarj kabloları çalışmaz |

## 2. Adımlar

### 2.1 VS Code ve PlatformIO

1. <https://code.visualstudio.com> adresinden VS Code'u kurun.
2. VS Code → Extensions (`Ctrl+Shift+X`) → `PlatformIO IDE` → **Install**.
3. Kurulum bitince VS Code'u yeniden başlatın. Sol çubukta karınca simgesi görünmelidir.
4. İlk açılışta PlatformIO çekirdeği ve `penv` arka planda kurulur; alt durum çubuğundaki ilerlemeyi bekleyin.

### 2.2 Projeyi Almak

```bash
git clone <depo-adresi> kumpas-veri-okuma-s3-super-mini
cd kumpas-veri-okuma-s3-super-mini
code .
```

VS Code'da `File → Open Folder` ile **`platformio.ini`'nin bulunduğu klasörü** açın (bir üst klasörü değil).

### 2.3 WiFi Gizli Dosyası

`include/wifi_secrets.h` depoda yoktur (bilinçli olarak). Oluşturmadan derleme `wifi_secrets.h: No such file` hatası verir. Ayrıntı: [GUIDE-004](GUIDE-004_WIFI_YAPILANDIRMASI.md).

```cpp
#pragma once

static constexpr const char *WIFI_SSID     = "AgAdiniz";
static constexpr const char *WIFI_PASSWORD = "Sifreniz";
```

### 2.4 İlk Derleme

```bash
pio run
```

İlk derlemede PlatformIO `espressif32` platformunu, Arduino-ESP32 çekirdeğini ve `links2004/WebSockets` kütüphanesini indirir (birkaç yüz MB). Sonunda `SUCCESS` görmelisiniz.

### 2.5 USB Sürücüsü

ESP32-S3 Super Mini **yerleşik USB** kullanır (ayrı CH340/CP2102 köprüsü yoktur). Windows 10/11, macOS ve Linux'ta genellikle sürücü gerekmez. Aygıt Yöneticisi'nde "USB Seri Cihaz (COMx)" veya "USB JTAG/serial debug unit" olarak görünür.

Kart görünmüyorsa: **BOOT** düğmesini basılı tutun, **RESET**'e basıp bırakın, sonra BOOT'u bırakın. Kart indirme (bootloader) moduna geçer ve yeni bir COM portu belirir.

> Kartınızda ayrı bir USB-UART çipi (CH340/CP2102) varsa üreticinin sürücüsünü kurun.

## 3. Python (İsteğe Bağlı — Otomasyon Scripti İçin)

```bash
python -m venv .venv-tools          # proje kökünde DEĞİL, isterseniz başka bir yerde
.venv-tools\Scripts\activate        # Windows
pip install websocket-client pyautogui
```

> **Dikkat:** Proje kökünde `.venv` adında bir Python sanal ortamı oluşturmak PlatformIO ile çakışabilir (bkz. §5).

## 4. Doğrulama Kontrol Listesi

- [ ] `pio --version` bir sürüm yazdırıyor (veya `.\pio.ps1 --version`)
- [ ] `pio run` → `SUCCESS`
- [ ] Kart takılınca yeni bir seri port beliriyor
- [ ] `pio device list` kartı listeliyor

## 5. Windows'ta PlatformIO penv / `.venv` Sorunları

**Belirti:** `ModuleNotFoundError: No module named 'fatfs.wrapper'`, `ImportError: cannot import name 'lfs'` veya `pio` komutu yanlış Python'u kullanıyor.

**Neden:** PATH'te önce başka bir Python/`pio` bulunuyor ya da proje kökündeki `.venv` VS Code tarafından otomatik etkinleştiriliyor.

**Çözüm 1 — Sarmalayıcıyı kullanın:** Depodaki [pio.ps1](../../pio.ps1) her zaman PlatformIO'nun kendi Python'unu kullanır:

```powershell
.\pio.ps1 run
.\pio.ps1 run -t upload
.\pio.ps1 device monitor
```

`penv` bulunamazsa betik açıklayıcı bir hata verir; VS Code'da PlatformIO eklentisini bir kez çalıştırarak penv'i oluşturun.

**Çözüm 2 — Hangi `pio` çalışıyor kontrol edin:**

```powershell
Get-Command pio.exe
where.exe pio.exe
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -d "$(pwd)"
```

**Çözüm 3 — VS Code ayarı (`.vscode/settings.json`, git dışıdır):**

```json
{
  "python.defaultInterpreterPath": "%USERPROFILE%\\.platformio\\penv\\Scripts\\python.exe",
  "terminal.integrated.env.windows": {
    "PATH": "%USERPROFILE%\\.platformio\\penv\\Scripts;${env:PATH}"
  }
}
```

**Çözüm 4 — Proje kökündeki `.venv`'i kaldırın:**

```powershell
Remove-Item -Recurse -Force .venv
```

`.venv` içinde çalışmak zorundaysanız `fatfs` gibi yerel uzantılar için "Microsoft C++ Build Tools" kurulu olmalıdır.

## 6. Önerilen VS Code Eklentileri

| Eklenti | Amaç |
|---|---|
| PlatformIO IDE | Derleme, yükleme, seri monitör |
| C/C++ (Microsoft) | IntelliSense (PlatformIO otomatik yapılandırır) |
| Python (Microsoft) | `measure_ws.py` düzenleme |
| Markdown All in One | `doc/` belgelerini düzenleme |

## İlgili

- [ADR-002 PlatformIO ve Arduino Çatısı](../adr/ADR-002_PLATFORMIO_VE_ARDUINO_CATISI.md)
- [GUIDE-012 Sorun Giderme](GUIDE-012_SORUN_GIDERME.md)
