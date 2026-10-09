# GUIDE-006: HTTP ve WebSocket API Referansı

**Hedef kitle:** Cihazdan ölçüm okuyacak yazılım geliştiren entegratörler  
**İlgili:** [ADR-011](../adr/ADR-011_HTTP_POLLING_VE_WEBSOCKET_PUSH.md), [ADR-013](../adr/ADR-013_JSON_OLCUM_SOZLESMESI.md), [SDD §6.3–6.4](../SDD_YAZILIM_TASARIM_DOKUMANI.md#6-veri-tasarımı)

---

## 1. Genel

| Özellik | Değer |
|---|---|
| Temel adres | `http://kumpas.local` (veya `http://<ip>`) |
| HTTP portu | 80 |
| WebSocket | `ws://kumpas.local:81/` |
| Kimlik doğrulama | Yok |
| Şifreleme | Yok (HTTP/WS) |
| İçerik tipi | `application/json` (API), `text/html; charset=utf-8` (sayfalar) |
| CORS başlığı | Gönderilmez — başka bir kaynaktan çalışan tarayıcı JS'i `/api/*`'yi `fetch` ile okuyamaz; WS'te bu kısıt yoktur |
| Eşzamanlılık | HTTP sunucusu senkron; istekler sırayla işlenir |

## 2. HTTP Uç Noktaları

### `GET /api/measure`

Cihazın son durum anlık görüntüsü.

```bash
curl -s http://kumpas.local/api/measure
```

```json
{"rawKey":2470,"mm":12.350,"inch":0.48622,"isInch":false,"isNegative":false,"buttonPressed":false,"offsetMm":0.000,"batV":3.980,"batAdcMv":1990,"batPct":82,"batCharging":false,"ageMs":45}
```

| Alan | Tip | Açıklama |
|---|---|---|
| `rawKey` | int | Son kararlı çerçeve anahtarının alt 22 biti |
| `mm` | float | Ölçüm (mm), **ofset uygulanmış**, 3 ondalık |
| `inch` | float | Ölçüm (in), **ofsetsiz**, 5 ondalık |
| `isInch` | bool | Kumpasın çıkarılmış birimi |
| `isNegative` | bool | Ofset öncesi işaret |
| `buttonPressed` | bool | Buton durumu (bkz. KS-05) |
| `offsetMm` | float | Aktif kalibrasyon ofseti |
| `batV` | float | Filtrelenmiş batarya gerilimi (V) |
| `batAdcMv` | int | Ham ADC ortalaması (mV) |
| `batPct` | int | 0–100 |
| `batCharging` | bool | Gerilim artış eğilimi |
| `ageMs` | int | Son durum güncellemesinden beri ms (ölçüm, buton **veya** batarya — KS-06) |

Ölçüm hiç alınmadıysa `mm`/`inch` 0'dır. "Kumpas veri gönderiyor mu?" sorusunu `ageMs` ile cevaplamayın (KS-06); bunun yerine `rawKey`'in değişip değişmediğini izleyin.

### `GET /api/log?since=N`

`N`'den büyük kimlikli log satırları. `since` verilmezse 0 kabul edilir.

```bash
curl -s "http://kumpas.local/api/log?since=0"
```

```json
{"seq":4,"lines":[{"id":1,"text":"Boot: main setup"},{"id":2,"text":"Boot: web ui start"},{"id":3,"text":"WiFi: connected 192.168.1.42"},{"id":4,"text":"Cal: offset=0.000"}]}
```

Artımlı okuma için bir sonraki istekte `since=<seq>` gönderin. Tampon 100 satırdır; daha eski satırlar sessizce atlanır.

### `POST /calibrate`

| Parametre | Tip | Açıklama |
|---|---|---|
| `offset` | float (form alanı) | mm cinsinden yeni ofset |

```bash
curl -i -X POST -d "offset=-0.015" http://kumpas.local/calibrate
# HTTP/1.1 303 See Other
# Location: /calibrate
```

`offset` yoksa `400 offset missing`. Geçersiz metin 0 olarak kaydedilir (KS-13).

### `POST /update`

`multipart/form-data`, alan adı `firmware`.

```bash
curl -F "firmware=@.pio/build/esp32s3/firmware.bin" http://kumpas.local/update
```

Başarıda 200 + HTML, ardından cihaz yeniden başlar. Hatada 400 + HTML. Ayrıntı: [GUIDE-009](GUIDE-009_OTA_GUNCELLEME.md).

### Sayfalar

`GET /`, `GET /log`, `GET /calibrate`, `GET /update` → HTML. Diğer tüm yollar → `404 Not Found` (düz metin).

## 3. WebSocket

- Adres: `ws://kumpas.local:81/`
- Sunucu → istemci: **metin** mesajları, her biri `/api/measure` ile aynı şemada tam JSON. WS mesajlarında `ageMs` her zaman `0`'dır.
- İstemci → sunucu: mesajlar yok sayılır.
- Mesaj tetikleyicileri: yeni kararlı ölçüm, buton durum değişimi, batarya raporu (≤ 5 s'de bir). Bağlantı anında otomatik "ilk durum" mesajı **gönderilmez**; ilk durum için ayrıca `GET /api/measure` çağırın veya ilk mesajı bekleyin.
- Ping/pong: istemci ping gönderebilir (Python örneği 20 s).

### Örnek — Python

```python
import json, websocket

def on_message(ws, msg):
    d = json.loads(msg)
    print(d["mm"], "mm", "buton" if d["buttonPressed"] else "")

websocket.WebSocketApp("ws://kumpas.local:81/", on_message=on_message).run_forever()
```

### Örnek — Tarayıcı / Node.js (≥ 22)

```js
const ws = new WebSocket("ws://kumpas.local:81/");
let last = null;
ws.onmessage = (e) => {
  const d = JSON.parse(e.data);
  if (last !== null && !last && d.buttonPressed) console.log("Kayıt:", d.mm);
  last = d.buttonPressed;
};
```

### Örnek — PowerShell (yoklama)

```powershell
while ($true) { (Invoke-RestMethod http://kumpas.local/api/measure).mm; Start-Sleep -Milliseconds 500 }
```

## 4. İstemci Yazarken Öneriler

1. **Buton kaydı için WS kullanın.** HTTP yoklama kısa basışları kaçırabilir.
2. **Kenar algılayın:** `buttonPressed`'in `false → true` geçişi; ilk mesajı yalnızca başlangıç durumu olarak kullanın.
3. **Tekrar koruması:** 200 ms içinde ikinci tetiklemeyi yok sayın.
4. **Yeniden bağlanın:** WS koparsa birkaç saniye bekleyip tekrar bağlanın.
5. **Bilinmeyen alanları yok sayın:** ileride yeni alanlar eklenebilir.
6. **Ondalıkları istemcide biçimlendirin:** `mm` 3 ondalık gelir; kumpas çözünürlüğü 0,005 mm'dir (mm modunda), 0,01 mm gösterim yeterlidir.

## 5. Uyumluluk Sözü

Alan adları, tipleri ve anlamları korunur; yalnızca yeni alan eklenebilir ([ADR-013](../adr/ADR-013_JSON_OLCUM_SOZLESMESI.md)). Anlam değişikliği gereken durumlarda (ör. KS-06 düzeltmesi) önce sürüm notunda duyurulmalıdır.
