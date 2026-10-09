# GUIDE-007: Python Otomasyon Scripti (`measure_ws.py`)

**Hedef kitle:** Ölçümleri doğrudan Excel/CAD/form alanlarına yazdırmak isteyen operatör ve kurulumcular  
**İlgili:** [ADR-021](../adr/ADR-021_PYTHON_ILE_KLAVYE_EMULASYONU.md), [GUIDE-006](GUIDE-006_HTTP_VE_WEBSOCKET_API_REFERANSI.md), [KS-16](../BILINEN_SORUNLAR.md#ks-16--python-scripti-eksik-alanlarda-çöküyor-)

---

## 1. Ne Yapar?

Script cihaza WebSocket ile bağlanır. Fiziksel butona her basıldığında güncel **mm** değerini, bilgisayarda o an **odakta olan pencereye** klavyeden yazılmış gibi yazar ve **Enter**'a basar. Excel'de bu, değeri hücreye yazıp bir alt hücreye geçmek demektir.

```
Buton ─► ESP32 ─WS─► measure_ws.py ─► pyautogui ─► "12,35" + Enter ─► Excel
```

## 2. Kurulum

```bash
pip install websocket-client pyautogui
```

> **Dikkat:** `websocket` (tire yok) adlı farklı bir paket kuruluysa çakışır. Script bunu algılayıp uyarır:
> ```bash
> pip uninstall websocket
> pip install websocket-client
> ```

| Platform | Not |
|---|---|
| Windows | Doğrudan çalışır |
| macOS | Sistem Ayarları → Gizlilik → **Erişilebilirlik**'te Terminal/Python'a izin verin |
| Linux | X11'de çalışır; Wayland'de pyautogui klavye girişi gönderemeyebilir |

## 3. Çalıştırma

```bash
python scripts/measure_ws.py
```

Beklenen konsol çıktısı:

```
ws connected
mm=12.35 inch=0.4862 unit=mm sign=+ age=0 raw=0x0009A6
...
```

Durdurmak için `Ctrl+C`.

## 4. Kullanım Akışı (Excel)

1. Excel'i açın, ilk ölçümün yazılacağı hücreyi seçin.
2. Scripti başlatın (konsol penceresi arka planda kalabilir).
3. **Excel penceresine tıklayın** — odak Excel'de olmalı.
4. Ölçün, değer oturunca butona basın → hücreye değer yazılır, alt hücreye geçilir.

## 5. Ayarlar

[scripts/measure_ws.py](../../scripts/measure_ws.py) başındaki sabitler:

| Sabit | Varsayılan | Açıklama |
|---|---|---|
| `WS_URL` | `ws://kumpas.local:81/` | mDNS çalışmıyorsa `ws://192.168.x.y:81/` |
| `DECIMALS` | `2` | Yazılacak ondalık basamak |
| `DECIMAL_COMMA` | `True` | `True` → `12,35` (Türkçe Excel), `False` → `12.35` |
| `MIN_PRESS_INTERVAL_SEC` | `0.2` | Bu süreden sık tetiklemeler yok sayılır |
| `ENTER_DELAY_SEC` | `0.1` | Değer ile Enter arasındaki bekleme |

**Enter yerine Tab** (sağdaki hücreye geçmek) için: `pyautogui.press("enter")` → `pyautogui.press("tab")`.

## 6. Davranış Ayrıntıları

- İlk alınan mesaj yalnızca buton durumunu öğrenmek için kullanılır; tetikleme yapmaz.
- Tetikleme koşulu: `buttonPressed` önceki mesajda `false`, şimdi `true`.
- Yazılan değer **ofsetli mm** değeridir; kumpas inç modunda olsa bile mm yazılır.
- Bağlantı koparsa 2 s sonra otomatik yeniden bağlanır. Ping aralığı 20 s, zaman aşımı 5 s.
- Mevcut firmware'de buton mantığı ters olabilir (KS-05): bu durumda yazma, butonu **bıraktığınızda** gerçekleşir.

## 7. Özelleştirme Örnekleri

### Değeri CSV Dosyasına da Yaz

`on_message` içinde `pyautogui.press("enter")` satırından sonra:

```python
import csv, datetime
with open("olcumler.csv", "a", newline="", encoding="utf-8") as f:
    csv.writer(f).writerow([datetime.datetime.now().isoformat(timespec="milliseconds"), f"{mm:.3f}"])
```

### İnç Değerini Yaz

```python
value = f"{inch:.4f}"
```

### Yalnızca Konsola Yaz (Klavye Emülasyonu Olmadan)

`pyautogui.typewrite(...)` ve `pyautogui.press(...)` satırlarını `print("KAYIT:", value)` ile değiştirin.

## 8. Windows'ta Otomatik Başlatma

1. `Win+R` → `shell:startup`.
2. Bu klasöre bir kısayol oluşturun: Hedef `pythonw.exe "C:\…\scripts\measure_ws.py"` (`pythonw` konsol penceresi açmaz; çıktıyı görmek istiyorsanız `python.exe` kullanın).

## 9. Sorun Giderme

| Belirti | Çözüm |
|---|---|
| `ws error: … getaddrinfo failed` | mDNS çözülmüyor → `WS_URL`'de IP kullanın |
| Bağlanıyor ama butonda hiçbir şey olmuyor | Web arayüzünde `btn=` rozeti değişiyor mu? Değişmiyorsa donanım/KS-05 |
| Değer yanlış pencereye yazılıyor | Odak hedef pencerede olmalı |
| Ondalık ayırıcı yanlış / Excel tarih yapıyor | `DECIMAL_COMMA`'yı bölgesel ayarınıza göre değiştirin |
| Karakterler yanlış (ör. `ş` yerine `;`) | Klavye düzeni farkı; yalnızca rakam ve ayırıcı yazıldığı için genelde sorun olmaz. Ayırıcıyı kontrol edin |
| `TypeError: unsupported format string passed to NoneType` | Eksik alan (KS-16) — firmware güncel mi? |
| Çift yazım | `MIN_PRESS_INTERVAL_SEC`'i 0,5'e çıkarın |
| pyautogui fareyi köşeye götürünce durdu | pyautogui'nin güvenlik kilidi (FAILSAFE); fareyi köşeden uzaklaştırın |
