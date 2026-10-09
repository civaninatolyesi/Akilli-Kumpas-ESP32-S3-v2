# ADR-022: Güvenilir Yerel Ağ Varsayımı — Kimlik Doğrulamasız ve Şifresiz Servisler

| Alan | Değer |
|---|---|
| **Durum** | Kabul edildi (bilinçli risk kabulü) — **yeniden değerlendirilmeli** |
| **Karar tarihi** | 2026-04-27 / 2026-04-28 (web ve OTA eklenirken örtük olarak) |
| **Dokümante edilme** | 2026-10-10 |
| **İlgili** | [ADR-015](ADR-015_TARAYICI_UZERINDEN_OTA_VE_CIFT_SLOT.md), [ADR-016](ADR-016_WIFI_STA_MDNS_VE_GIZLI_BILGI_YONETIMI.md), SRS FOG-SEC-*, SDD §10, KS-03, KS-04 |

## Bağlam

Cihaz, kişisel bir atölye/ev ağında tek kullanıcı tarafından kullanılmak üzere tasarlandı. Hedef, sıfır kurulumlu kullanım: tarayıcıda adresi açıp hemen ölçüm görmek. ESP32 üzerinde HTTPS/WSS mümkündür ancak sertifika yönetimi, bellek ve kullanıcı deneyimi maliyeti yüksektir.

## Karar

- HTTP (80) ve WS (81) **şifresiz** çalışır.
- Hiçbir uç noktada kimlik doğrulama yoktur — `/update` (OTA) ve `POST /calibrate` dahil.
- Güvenlik sınırı **ağın kendisidir**: cihaz yalnızca güvenilir WPA2 ağlarına bağlanmalıdır.
- Gizli bilgi olarak yalnızca WiFi kimlik bilgileri vardır; depoya girmez (ADR-016).
- UI tarafında XSS'e karşı `textContent` kullanılır; JSON çıktısı kaçışlanır.

## Risk Değerlendirmesi

| Risk | Olasılık (güvenilir LAN) | Etki | Değerlendirme |
|---|---|---|---|
| Yetkisiz OTA ile kötü amaçlı firmware | Düşük | Yüksek (cihaz ele geçirilir, WiFi şifresi okunur) | **Kabul edildi, en öncelikli iyileştirme** |
| Yetkisiz kalibrasyon değişikliği | Düşük | Orta (fark edilmeyen ölçüm hatası) | Kabul edildi; `cal=` göstergesi ile görünür |
| Ölçüm verisinin dinlenmesi | Düşük | Düşük | Kabul edildi |
| `firmware.bin`'den WiFi şifresinin okunması | Orta (dosya paylaşılırsa) | Orta | Dokümantasyonla azaltıldı (KS-04) |
| Hizmet engelleme (istek seli) | Düşük | Düşük | Kabul edildi |

## Kullanım Kısıtları (bu kararın geçerli olması için)

1. Cihaz **misafir/açık/halka açık** ağlara bağlanmamalıdır.
2. Cihaz portları internete yönlendirilmemelidir (port forwarding yok).
3. `firmware.bin` dosyası paylaşılmamalıdır.
4. Paylaşılan/kurumsal ağlarda kullanılacaksa aşağıdaki iyileştirmeler önce uygulanmalıdır.

## Önerilen İyileştirmeler (öncelik sırasıyla)

1. `/update` ve `POST /calibrate` için HTTP Basic Auth (`server.authenticate()` / `requestAuthentication()`); kullanıcı/şifre `wifi_secrets.h`'de.
2. OTA'da beklenen boyut + MD5 kontrolü (`Update.setMD5()`).
3. Kimlik bilgilerini NVS'ye taşıyıp yapılandırma portalı ile almak (KS-04).
4. Kurumsal kullanımda IoT VLAN/istemci izolasyonu.

## Değerlendirilen Alternatifler

| Seçenek | Neden şimdilik seçilmedi |
|---|---|
| HTTPS/WSS | Sertifika dağıtımı, tarayıcı uyarıları, bellek |
| Uygulama içi oturum/token | UI ve Python istemcisinde ek karmaşıklık |
| Yalnızca Basic Auth | Düşük maliyetli ve **önerilen ilk adım**; henüz uygulanmadı |
