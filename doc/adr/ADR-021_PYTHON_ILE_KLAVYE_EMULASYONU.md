# ADR-021: Hedef Uygulamaya Veri Girişi için Python + pyautogui (BLE Klavye Yerine)

| Alan | Değer |
|---|---|
| **Durum** | Kabul edildi |
| **Karar tarihi** | 2026-04-28 (WS istemcisi, `fdddf76`), 2026-05-28 (pyautogui, `2ffeeb4`) |
| **Dokümante edilme** | 2026-10-10 |
| **İlgili** | [ADR-011](ADR-011_HTTP_POLLING_VE_WEBSOCKET_PUSH.md), [ADR-018](ADR-018_BUTON_GIRISI_VE_DEBOUNCE.md), SRS FG-PY-*, SDD §4.9, [GUIDE-007](../guide/GUIDE-007_PYTHON_OTOMASYON_SCRIPTI.md), KS-15, KS-16 |

## Bağlam

En sık istenen iş akışı: "Kumpasla ölç, butona bas, değer Excel'deki seçili hücreye yazılsın ve bir alt hücreye geçilsin." Bunun için ölçümün, bilgisayarda **o an odakta olan uygulamaya** klavye girişi olarak ulaşması gerekir. Bu sayede Excel, Google Sheets, CAD, ERP formu gibi her uygulama entegrasyon gerektirmeden çalışır.

## Karar

- Bilgisayarda çalışan bir **Python scripti** ([scripts/measure_ws.py](../../scripts/measure_ws.py)) WS ile cihaza bağlanır.
- `buttonPressed` `false → true` geçişinde `pyautogui.typewrite()` ile mm değeri yazılır, 0,1 s sonra `Enter` basılır.
- Biçim: `DECIMALS = 2`, `DECIMAL_COMMA = True` (Türkçe yerel ayar için `12,35`).
- 0,2 s'den sık tetiklemeler yok sayılır.
- Bağlantı koparsa 2 s sonra yeniden bağlanılır; WS ping 20 s.
- Depoda `lib/ESP32_BLE_Keyboard/` iskeleti bulunur (`library.json`, kaynak yok); **kullanılmaz**.

## Değerlendirilen Alternatifler

| Seçenek | Artı | Eksi | Durum |
|---|---|---|---|
| **Python + pyautogui** | Her uygulamaya yazar; firmware değişmez; ayarları kolay | Bilgisayarda Python gerekir; hedef pencere odakta olmalı | **Seçildi** |
| ESP32 BLE HID klavye | Bilgisayarda yazılım gerekmez; kablosuz | BLE + WiFi aynı anda bellek/RF yükü; eşleştirme sorunları; klavye düzeni (TR-Q virgül/nokta) sorunları; 4 MB slot sınırına yaklaşır | İskelet eklendi, uygulanmadı |
| ESP32-S3 USB HID klavye | Sürücüsüz, gecikmesiz | Kablo gerekir — kablosuz hedefiyle çelişir | Reddedildi |
| Excel eklentisi / VBA WS istemcisi | Doğrudan hücreye yazar | Excel'e özel, kurulumu zor | Reddedildi |
| Tarayıcı CSV (ADR-020) | Kurulum yok | Anlık hücre girişi değil | Tamamlayıcı olarak mevcut |

## Sonuçlar

**Olumlu**
- Firmware tarafında ek kod yok; WS yayını zaten var.
- WS push sayesinde kısa basışlar kaçmaz.

**Olumsuz / Riskler**
- Odak başka penceredeyse değer yanlış yere yazılır (kullanıcı eğitimi gerekir).
- `pyautogui.typewrite` klavye düzenine duyarlıdır; bazı düzenlerde virgül/nokta farklı tuşlara eşlenir.
- Linux Wayland'de pyautogui çalışmayabilir.
- Mevcut buton mantığıyla (KS-05) yazım bırakma anında olabilir.
- Eksik alanlarda konsol yazdırma istisnası (KS-16).
- Kullanılmayan BLE iskeleti kafa karıştırır (KS-15).

## Gelecek

Python gerektirmeyen çözüm istenirse BLE HID yeniden değerlendirilmeli; o durumda bu ADR "Yerine geçildi" olarak işaretlenip yeni ADR yazılmalıdır.
