# GUIDE-013: Parametre Referansı

**Hedef kitle:** Geliştirici, kurulumcu  
**Amaç:** Projedeki tüm ayarlanabilir değerlerin tek tabloda; ne işe yaradıkları, güvenli aralıkları ve değiştirildiğinde neyi etkiledikleri.

Tüm değerler derleme zamanında sabittir; değişiklik sonrası yeniden derleyip yükleyin. Çalışma zamanında değiştirilebilen tek değer kalibrasyon ofsetidir ([GUIDE-008](GUIDE-008_KALIBRASYON.md)).

---

## 1. `src/main.cpp`

### 1.1 Pinler — [main.cpp:9-12](../../src/main.cpp#L9-L12)

| Sabit | Varsayılan | Kısıt |
|---|---|---|
| `CALIPER_DATA_PIN` | 8 | Strapping pinleri (0, 3, 45, 46) ve USB pinleri (19, 20) kullanılmamalı |
| `CALIPER_CLK_PIN` | 9 | Kesme destekli herhangi bir GPIO |
| `BUTTON_PIN` | 10 | Dahili pull-up/pull-down destekli GPIO |
| `BATTERY_ADC_PIN` | 5 | **ADC1** (GPIO1–10) olmalı |

### 1.2 Protokol ve Ölçek — [main.cpp:32-63](../../src/main.cpp#L32-L63)

| Sabit | Varsayılan | Açıklama | Güvenli aralık / not |
|---|---|---|---|
| `COUNTS_PER_MM` | 200,0 | mm başına count | 100 / 200 / 400 (modele göre) |
| `COUNTS_PER_INCH` | 4000,0 | inç başına count | Genellikle 4000 |
| `FRAME_GAP_US` | 1500 | Bu süreden uzun sessizlik = yeni çerçeve | `Tclk < değer < sessizlik` |
| `MIN_EDGE_US` | 5 | Bu süreden kısa kenar arası = gürültü | `değer < Tclk` |
| `MAX_ABS_MM` | 500,0 | Bu değerin üstü yayınlanmaz | Kumpas aralığı + pay |
| `REJECT_IF_TOP_BITS_SET` | `false` | Bit 22–23'ten biri 1 ise çerçeveyi at | Bit 23 kullanılıyorsa `false` kalmalı |
| `CALIPER_VALUE_BITS` | 20 | Büyüklük bit sayısı | 20 (tipik) |
| `CALIPER_SIGN_BIT` | 21 | İşaret biti konumu | 20 / 21 / 23 |
| `CALIPER_UNIT_BIT` | 23 | Birim biti konumu | İşaret bitinden **farklı** olmalı |

### 1.3 Çalışma Zamanı Yerleşim Değişkenleri — [main.cpp:86-91](../../src/main.cpp#L86-L91)

| Değişken | Varsayılan | Açıklama |
|---|---|---|
| `g_signBit` | `CALIPER_SIGN_BIT` | Kullanılan işaret biti |
| `g_unitBit` | `CALIPER_UNIT_BIT` | Kullanılan birim biti |
| `g_unitBitMeansInchWhen1` | `true` | `false` → bit 0 iken inç |
| `g_signBitMeansNegativeWhen1` | `true` | `false` → bit 0 iken negatif |
| `g_autoUnitFix` | `true` | Süreklilik ile birim çıkarımı ([ADR-008](../adr/ADR-008_SUREKLILIK_ILE_BIRIM_CIKARIMI.md)) |

### 1.4 Birim Çıkarımı — [main.cpp:99-101](../../src/main.cpp#L99-L101)

| Sabit | Varsayılan | Etki |
|---|---|---|
| `UNIT_SWITCH_CONFIRM` | 3 | Birim değişimi için gereken ardışık güçlü ipucu |
| `UNIT_SWITCH_MIN_BAD_MM` | 4,0 | Mevcut yorumun "çok yanlış" sayılma eşiği; düşürmek algılama sınırını (~15–19 mm) aşağı çeker ama yanlış pozitifi artırır |
| `UNIT_SWITCH_MAX_GOOD_MM` | 0,40 | Alternatif yorumun "doğru" sayılma toleransı |

### 1.5 Stabilizasyon Filtresi — [main.cpp:104-106](../../src/main.cpp#L104-L106)

| Sabit | Varsayılan | Etki |
|---|---|---|
| `STABLE_REPEATS_NORMAL` | 2 | **≥ 2 olmalı** (1 → tek bozuk çerçeve yayınlanabilir) |
| `STABLE_REPEATS_ON_JUMP` | 4 | Büyük sıçramada gereken tekrar |
| `JUMP_MM_THRESHOLD` | 0,50 | "Büyük sıçrama" eşiği (mm) |

### 1.6 Batarya — [main.cpp:14-30](../../src/main.cpp#L14-L30)

| Sabit | Varsayılan | Açıklama |
|---|---|---|
| `BAT_R1_OHMS` | 100 000 | Üst direnç |
| `BAT_R2_OHMS` | 100 000 | Alt direnç |
| `BATTERY_MIN_V` | 3,0 | %0 gerilimi |
| `BATTERY_MAX_V` | 4,2 | %100 gerilimi |
| `BATTERY_SAMPLE_MS` | 500 | **Kullanılmıyor** (KS-10) |
| `BATTERY_SAMPLES` | 8 | Örnek başına ADC okuma |
| `BATTERY_SAMPLE_MS_CHARGING` | 2000 | Şarj eğilimi varken örnekleme periyodu |
| `BATTERY_SAMPLE_MS_IDLE` | 2000 | Boşta örnekleme periyodu |
| `BATTERY_REPORT_MAX_INTERVAL_MS` | 5000 | Değişim olmasa da en geç bu sürede rapor |
| `CHARGE_WINDOW_MS` | 30 000 | Kısa vadeli eğilim penceresi |
| `CHARGE_MIN_DELTA_V` | 0,010 | Kısa vadede "yükseliyor" eşiği |
| `LONG_CHARGE_MIN_DELTA_V` | 0,010 | Uzun vadede "yükseliyor" eşiği |
| `CHARGE_NEAR_FULL_V` | 4,15 | Bu gerilimin üstünde "yükseliyor" gösterilmez |
| `STABLE_PERIOD_MS` | 120 000 | Uzun vadeli kararlılık süresi |
| `STABLE_DELTA_V` | 0,006 | Kararlı sayılan azami adım |

### 1.7 Kod İçine Gömülü Değerler (sabit olarak tanımlı değil)

| Değer | Yer | Açıklama |
|---|---|---|
| 115200 | `Serial.begin` | Seri hız (platformio.ini `monitor_speed` ile aynı olmalı) |
| 300 ms | `setup` | USB-CDC için açılış beklemesi |
| 12 bit, `ADC_11db` | `setup` | ADC çözünürlük ve zayıflatma |
| 20 ms | [main.cpp:337](../../src/main.cpp#L337) | Buton debounce süresi |
| 0,85 / 0,15 | [main.cpp:369](../../src/main.cpp#L369) | Batarya IIR filtre katsayıları |
| 0,02 V | [main.cpp:389](../../src/main.cpp#L389) | Şarj penceresi tabanını düşüşte sıfırlama eşiği |
| 0,01 V | [main.cpp:407](../../src/main.cpp#L407) | Batarya raporu için anlamlı değişim |
| 2 ms | [main.cpp:472](../../src/main.cpp#L472) | `loop()` sonu bekleme |
| 24 | [main.cpp:206](../../src/main.cpp#L206) | Çerçeve bit sayısı |

## 2. `src/web_ui.cpp`

| Sabit / değer | Varsayılan | Açıklama |
|---|---|---|
| HTTP portu | 80 | `WebServer server(80)` |
| WS portu | 81 | `WebSocketsServer wsServer(81)` |
| `MDNS_HOSTNAME` | `"kumpas"` | → `kumpas.local` |
| `UPDATE_PATH` | `"/update"` | OTA yolu |
| `CAL_PATH` | `"/calibrate"` | Kalibrasyon yolu |
| `UPDATE_MAX_SIZE` | 3 MB | Etkisiz; gerçek sınır 1,25 MB (KS-01) |
| `CAL_NAMESPACE` | `"kumpas"` | NVS ad alanı |
| `CAL_KEY_OFFSET` | `"offset_mm"` | NVS anahtarı |
| `LOG_CAP` | 100 | Log satır kapasitesi |
| `LOG_LINE_MAX` | 96 | Satır başına bayt (95 karakter + `\0`) |
| WiFi zaman aşımı | 15 000 ms | `connectWiFi(15000)` |
| OTA sonrası bekleme | 400 ms | Yanıt gönderildikten sonra yeniden başlatma |
| JSON `reserve` | 160 / 512 | Ölçüm / log JSON'u için ön ayırma |
| `rawKey` maskesi | `0x3FFFFF` | 22 bit (KS-11) |

### Gömülü JavaScript

| Değer | Yer | Açıklama |
|---|---|---|
| 200 ms | INDEX_HTML `setInterval(tick, 200)` | Ölçüm yoklama aralığı |
| 250 ms | LOG_HTML `setInterval(tick, 250)` | Log yoklama aralığı |
| 200 | `maxRows` | Buton logu azami satır |
| 80 | `maxlength` | Not uzunluğu |
| 20 % | `pct <= 20` | Düşük pil renk eşiği |
| 2 / 4 | `toFixed` | mm / inç gösterim ondalığı |

## 3. `scripts/measure_ws.py`

| Sabit | Varsayılan |
|---|---|
| `WS_URL` | `ws://kumpas.local:81/` |
| `ENTER_DELAY_SEC` | 0,1 |
| `MIN_PRESS_INTERVAL_SEC` | 0,2 |
| `DECIMALS` | 2 |
| `DECIMAL_COMMA` | `True` |
| ping aralığı / zaman aşımı | 20 s / 5 s |
| yeniden bağlanma beklemesi | 2 s |

## 4. `platformio.ini` ve Bölüm Tablosu

| Ayar | Değer |
|---|---|
| `board` | `esp32-s3-devkitc-1` |
| `board_build.flash_size` / `board_upload.flash_size` | `4MB` |
| `board_build.partitions` | `partitions_ota_4mb.csv` |
| `build_flags` | `-D ARDUINO_USB_CDC_ON_BOOT=1 -D ARDUINO_USB_MODE=1` |
| `monitor_speed` | 115200 |
| `lib_deps` | `links2004/WebSockets` |
| App slot boyutu | 0x140000 (1 310 720 B) × 2 |
| NVS | 0x5000 (20 KB) |
| SPIFFS | 0x170000 (kullanılmıyor) |

## 5. `include/wifi_secrets.h`

| Sabit | Açıklama |
|---|---|
| `WIFI_SSID` | Ağ adı (2,4 GHz) |
| `WIFI_PASSWORD` | Ağ şifresi |
