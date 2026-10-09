# ADR-010: Ölçüm ve Ağ Katmanlarının Ayrılması — `web_ui.h` Sözleşmesi

| Alan | Değer |
|---|---|
| **Durum** | Kabul edildi |
| **Karar tarihi** | 2026-04-27 (commit `1c9ff0d`); genişletmeler `fdddf76`, `2ffeeb4` |
| **Dokümante edilme** | 2026-10-10 |
| **İlgili** | [ADR-009](ADR-009_TEK_SUPER_DONGU_MIMARISI.md), [ADR-013](ADR-013_JSON_OLCUM_SOZLESMESI.md), SRS AG-YAZ-01, FOG-BAK-02, SDD §3.1 ve §8 |

## Bağlam

İlk sürümde tüm kod `main.cpp`'deydi ve ölçümler yalnızca seri porta yazılıyordu. Web arayüzü eklenirken, ağ kodu (WiFi, HTTP, HTML, OTA) ölçüm mantığıyla karışırsa protokol değişikliklerinin ağ kodunu, UI değişikliklerinin ISR kodunu riske atacağı görüldü.

## Karar

- `src/main.cpp`: donanım, protokol, filtre, birim, batarya, buton.
- `src/web_ui.cpp`: WiFi, mDNS, HTTP, WS, HTML, kalibrasyon, OTA, log.
- `include/web_ui.h`: ikisi arasındaki **tek** sözleşme. Veri akışı tek yönlüdür: `main` → `web_ui` (itme/push). `web_ui`, `main.cpp`'den hiçbir sembol bilmez.

```cpp
void webBegin();
void webLoop();
void webUpdateMeasurement(uint32_t rawKey, float mm, float inch, bool isInch, bool isNegative);
void webUpdateButton(bool isPressed);
void webUpdateBattery(float volts, uint16_t adcMv, uint8_t percent, bool isCharging);
void webLogf(const char *fmt, ...);
```

- Kalibrasyon ofseti **web katmanında** uygulanır: ölçüm katmanı her zaman ham (kumpasın gösterdiği) değeri üretir.

## Değerlendirilen Alternatifler

| Seçenek | Neden seçilmedi |
|---|---|
| Tek dosya | Büyüdükçe yönetilemez |
| Web katmanının ölçüm durumunu çekmesi (getter'lar) | Çift yönlü bağımlılık; WS push için yine bildirim gerekir |
| Olay yayıncısı / gözlemci deseni | Bu ölçek için fazla soyutlama |
| C++ sınıfları (`Caliper`, `WebUi`) | Mümkün; mevcut boyutta serbest fonksiyonlar yeterli |

## Sonuçlar

**Olumlu**
- Ağ katmanı tamamen kaldırılsa bile `main.cpp` stub'larla derlenebilir; ölçüm katmanı bağımsız test edilebilir hale getirilebilir.
- Yeni bir alan eklemek mekanik bir adımdır (GUIDE-014).

**Olumsuz / Riskler**
- Her `webUpdate*` tam durum JSON'u yayınlar; güncelleme türü ayırt edilmez (istemci farkı kendisi bulur).
- Ofsetin web katmanında uygulanması, `inch`/`isNegative` alanlarının ofsetsiz kalmasına yol açtı (KS-12).
- `webLogf` ve `webUpdate*` ISR'den çağrılmamalıdır; bu kural yalnızca dokümantasyonla korunur.

## Uygulama Notları

- [include/web_ui.h](../../include/web_ui.h)
- Ön/son koşullar: SDD §8.
