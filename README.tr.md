# AKKE — Akıllı Komuta Kontrol Eldiveni

> *Sessiz taktik iletişim için giyilebilir, el hareketinden sesli komuta dönüşen bir sistem.*

🌐 **Diller:** [English](README.md) · **Türkçe**

[![Durum](https://img.shields.io/badge/durum-aktif--geliştirme-blue)]()
[![Platform](https://img.shields.io/badge/platform-ESP32--informational)]()
[![Ders](https://img.shields.io/badge/TEDU-CMPE%20491%2F492-red)]()

---

## Genel Bakış

**AKKE**, konuşmanın mümkün olmadığı, riskli ya da operasyonel olarak istenmediği ortamlar — askerî operasyonlar, arama-kurtarma görevleri ve yüksek gürültülü endüstriyel sahalar — için tasarlanmış giyilebilir bir el hareketi tanıma sistemidir.

Sistem iki adet ESP32 düğümünden oluşur:

1. **Eldiven birimi** — 5 adet flex sensör ve bir MPU6050 IMU ile elin pozisyonunu ve hareketini yakalar, kuantize edilmiş bir TensorFlow Lite Micro modeliyle (Edge Impulse) hareketi cihaz üzerinde sınıflandırır ve tanınan komutu kablosuz olarak iletir.
2. **Alıcı birim** — komutu ESP-NOW üzerinden alır, DFPlayer Mini'den ilgili ses dosyasını oynatır ve 16×2 I²C LCD üzerinde aktif komutu, ses seviyesini ve pil durumunu gösterir.

Sonuç: kullanıcı bir el işareti yapar ve yaklaşık 200 ms içinde ekip, kimse konuşmadan kulaklığından net bir sesli komut duyar.

---

## Ekip

| İsim | Bölüm | Rol |
|---|---|---|
| **Berk Çakmak** | Bilgisayar Mühendisliği | Proje Lideri |
| Abdullah Esin | Bilgisayar Mühendisliği (EEE çift anadal) | Yazılım & ML |
| Ömer Efe Dikici | Bilgisayar Mühendisliği | Yazılım & Sistem Entegrasyonu |
| Şevval Kurtulmuş | Elektrik-Elektronik Mühendisliği (CMPE çift anadal) | Donanım & Gömülü Yazılım |

**Danışmanlar:** Ali Berkol, Hüseyin Uğur Yıldız
**Jüri:** Hakkı Gökhan İlk, Mehmet Evren Coşkun
**Kurum:** TED Üniversitesi, Mühendislik Fakültesi — CMPE 491 / 492 Bitirme Projesi

**Destekleyen:** HAVELSAN SUIT Programı

---

## Desteklenen Hareketler

Mevcut yazılım 10 komutu sınıflandırıp iletir:

| # | Komut | Kullanım amacı |
|---|---|---|
| 1 | Dur (Stop) | Hareketi durdur |
| 2 | Dinle (Listen) | Dikkat ver |
| 3 | İlerle (Go go) | İleri hareket |
| 4 | Gel (Come) | Yaklaş |
| 5 | Birlikte kal (Stick together) | Topla |
| 6 | Yavaşla (Slow down) | Tempoyu düşür |
| 7 | Geri çekil (Fall back) | Geri çekilme |
| 8 | Siper al (Take cover) | Siper bul |
| 9 | Sağa hareket (Move right) | Sağa yanal hareket |
| 10 | Sola hareket (Move left) | Sola yanal hareket |

Yol haritasında bu küme 15'e çıkarılacak (etrafı kontrol et, dağıl, çök, etraf temiz, operasyon bitti).

---

## Sistem Mimarisi

```
┌────────────────── ELDİVEN BİRİMİ ──────────────────┐         ┌──────────── ALICI BİRİM ────────────┐
│                                                    │         │                                       │
│   5× Flex sensör ──┐                               │         │   ESP32 ──── DFPlayer Mini ─── Hoparlör│
│                    ├─► ESP32-S3 ──► TFLite         │ ESP-NOW │     │                                  │
│   MPU6050 (IMU) ───┘    (Edge Impulse modeli)      │ ───────►│     ├─── 16×2 I²C LCD (durum)         │
│                                                    │ 2.4 GHz │     ├─── Potansiyometre (ses)         │
│   Buton ─► durum makinesi                          │         │     └─── Pil voltaj bölücü            │
│   RGB LED ─► görsel geri bildirim                  │         │                                       │
│                                                    │         │                                       │
└────────────────────────────────────────────────────┘         └───────────────────────────────────────┘
```

### Veri akışı

1. Kullanıcı ~3 saniye boyunca bir el pozisyonunu tutar (örnekleme penceresi Edge Impulse impulse ayarlarına göre belirlenir).
2. Eldiven her zaman adımı için 13 özellik örnekler: 5 normalize flex değeri, Kalman filtrelenmiş pitch ve roll, 3 eksen ivmeölçer (kalibre), 3 eksen jiroskop (kalibre).
3. TFLite Micro sınıflandırıcısı cihaz üzerinde çalışır ve güven skoruyla birlikte bir etiket çıkarır.
4. Güven skoru ≥ %75 ise, etiket bir komut ID'sine (1–10) eşlenir ve ESP-NOW üzerinden yayınlanır.
5. Alıcı, komut ID'sini DFPlayer SD kartındaki bir MP3 dosyasıyla eşleştirir ve oynatır.

---

## Donanım

### Eldiven birimi
| Bileşen | Görev | Pin / Bus |
|---|---|---|
| ESP32-S3 DevKit | Ana MCU, cihaz üstü çıkarım | — |
| 5× flex sensör | Parmak bükülmesi (voltaj bölücü) | GPIO 25, 33, 32, 35, 34 |
| MPU6050 | 3 eksen ivme + 3 eksen jiroskop | I²C: SDA=21, SCL=22 |
| Buton | Durum geçişleri | GPIO 13 (INPUT_PULLUP) |
| RGB LED | Görsel geri bildirim | R=15, G=2, B=0 |
| Pil ölçer | Voltaj bölücü (R1=20 k, R2=10 k) | GPIO 39 |
| Li-Po + TP4056 + HT7333 | Güç zinciri | — |

> ⚠️ **GPIO 0 uyarısı:** Mavi kanal, ESP32'nin boot-mode seçim pini olan GPIO 0'ı kullanıyor. Açılış sırasında LOW'a çekilmesi çipi indirme moduna alır — bu yüzden açılışta LED kapalı tutulmalı veya pull-up uygulanmalı.

### Alıcı birim
| Bileşen | Görev | Pin / Bus |
|---|---|---|
| ESP32 | Alıcı MCU | — |
| DFPlayer Mini | microSD'den MP3 oynatma | UART1: RX=16, TX=17 (9600 baud) |
| 16×2 I²C LCD | Durum ekranı (adres 0x27) | I²C: SDA=21, SCL=22 |
| Potansiyometre | Anlık ses seviyesi (0–30) | GPIO 36 |
| Pil ölçer | Voltaj bölücü | GPIO 35 |
| Hoparlör (3 W, 8 Ω) | Ses çıkışı | DFPlayer SPK_1 / SPK_2 |

---

## Durum Makinesi (eldiven)

```
   ┌─────────┐  buton    ┌──────────────┐   oto.   ┌────────┐  buton   ┌──────────┐
   │ BOŞTA   ├──────────►│ KALİBRASYON  ├─────────►│ HAZIR  ├─────────►│ ÇIKARIM  │
   └─────────┘           └──────────────┘          └────┬───┘          └────┬─────┘
   (mavi LED)            (sarı→kırmızı→mor)       (mavi LED)            (sarı LED)
                                                       │                     │
                                                       └────── oto. ◄────────┘
```

### LED renk anlamları
| Durum | Renk | Anlam |
|---|---|---|
| Açılış | Beyaz yanıp söner (3×) | Açılış kontrolü |
| BOŞTA / HAZIR | Sabit mavi | Girdi bekleniyor |
| Kalibrasyon A | Sarı | El **açık** tutulur |
| Kalibrasyon B | Kırmızı | El **kapalı (yumruk)** tutulur |
| Kalibrasyon C | Mor | El **sabit** tutulur (IMU offset) |
| Çıkarım | Sarı | 3 saniyelik pencere kaydediliyor |
| Başarılı | Sabit yeşil (0.5 sn) | Komut kabul edildi ve iletildi |
| Düşük güven / hata | Kırmızı yanıp söner (2×) | %75 altı güven veya gönderim hatası |
| Düşük pil | Sürekli kırmızı yanıp söner | Pil eşik altında |

---

## ML Boru Hattı

- **Araç:** Edge Impulse Studio (impulse tasarımı, eğitim, Arduino kütüphanesi olarak dağıtım)
- **Pencere:** ~3 sn, her zaman adımı için 13 özellik (5 flex + 2 Kalman açısı + 6 IMU)
- **Model:** Dense sinir ağı, int8 kuantize (TFLite Micro)
- **Cihaz üstü çalışma zamanı:** Edge Impulse Arduino kütüphanesi (`ILTER-AKKE_1.3.3_inferencing.h`)
- **Güven eşiği:** %75 altındaki tahminler atılır
- **Kalibrasyon:** Her oturum için flex sensörlerin normalize edilmesi + IMU offset tahmini (kullanıcılar arasında modeli yeniden eğitmeye gerek yok)

> **Özellik tasarımına dair not:** Yazılım, pencerelenmiş zaman serisi özelliklerini doğrudan Edge Impulse'a besler; Edge Impulse kendi DSP bloğunu (IMU kanalları üzerinde spektral analiz) uygular. Bu yaklaşım, daha önceki istatistiksel özellik çıkarımının (88 özellik = 11 kanal × 8 istatistik) yerine geçer ve en uygun özellik çıkarıcının Edge Impulse tarafından seçilmesine imkân tanır.

---

## İletişim: ESP-NOW

İki ESP32 birbirleriyle doğrudan ESP-NOW üzerinden konuşur — Wi-Fi erişim noktası, yönlendirici veya internet yoktur. Eldiven, alıcının MAC adresini derleme zamanında bilir (`inference.ino` içindeki `receiverMAC[]`) ve 1 baytlık paket gönderir:

```c
typedef struct {
  uint8_t fileNumber;  // 1..10
} SendData;
```

Hareketin bitişinden ses oynatımına kadar geçen toplam gecikme tipik olarak 300 ms altındadır.

> 🔧 **Bağlantıyı kurma:** Önce `reciever.ino` yüklenir, seri monitör açılır ve yazılan MAC adresi `inference.ino` içindeki `receiverMAC[]` dizisine kopyalanır; ardından eldiven yazılımı yüklenir.

---

## Depo Yapısı

```
.
├── README.md                  # İngilizce sürüm
├── README.tr.md               # bu dosya (Türkçe)
├── .gitignore
│
├── data_collection/                       # ── ML veri boru hattı ──
│   ├── data_collection.ino                # ham örnekleri seri porta gönderen yazılım sürümü
│   ├── data_collector.py                  # seri portu okur, dataset.csv'ye etiketli satır yazar
│   ├── count_labels.py                    # hızlı kontrol: hareket başına örnek sayısı
│   ├── split_for_edge_impulse.py          # dataset.csv'yi Edge Impulse'a uygun dosyalara böler
│   ├── dataset.csv                        # ana veri kümesi (her satır bir örnek)
│   └── edge_impulse_data/                 # Edge Impulse yüklemesi için hareket başına dosyalar
│
├── ESP32-Libraries/                       # ── projede paketlenmiş Arduino kütüphaneleri ──
│   ├── KalmanFilter-master/               # açılmış Kalman kütüphanesi (TKJ Electronics)
│   ├── Kalman.zip                         # orijinal arşiv
│   └── ei-ilter-akke_1.3.3-arduino-1.0.5-impulse.zip   # dışa aktarılmış Edge Impulse Arduino kütüphanesi
│
├── inference/
│   └── inference.ino                      # eldiven tarafı yazılımı (ESP32-S3)
│
└── reciever/
    └── reciever.ino                       # alıcı tarafı yazılımı (ESP32 + DFPlayer)
```

### Betiklerin görevleri

| Dosya | Görev |
|---|---|
| `data_collection.ino` | Kalibrasyon yapan ve ardından pencerelenmiş örnekleri etiketle birlikte USB-Serial üzerinden gönderen yazılım modu |
| `data_collector.py` | Host tarafında seri okuyucu: hareket etiketi sorar, satırları `dataset.csv`'ye ekler |
| `count_labels.py` | Hareket başına kaç kayıt olduğunu yazdırır — veri kümesini dengeli tutmak için |
| `split_for_edge_impulse.py` | `dataset.csv`'yi Edge Impulse Studio'nun yüklemede beklediği pencere bazlı dosya formatına dönüştürür |

---

## Kurulum & Yükleme

### Ön koşullar
- Arduino IDE 2.x **veya** PlatformIO
- ESP32 kart paketi (ESP-NOW v5 API için Espressif Arduino core ≥ 3.0)
- Arduino kütüphaneleri:
  - `DFRobotDFPlayerMini` (Library Manager üzerinden)
  - `LiquidCrystal_I2C` (Library Manager üzerinden)
  - `Kalman` — `ESP32-Libraries/Kalman.zip` içinde mevcut (Sketch → Include Library → Add .ZIP Library)
  - Edge Impulse modeli — `ESP32-Libraries/ei-ilter-akke_1.3.3-arduino-1.0.5-impulse.zip` içinde mevcut (aynı şekilde: Add .ZIP Library)

> Projenin bağımlı olduğu kritik kütüphanelerin tümü `ESP32-Libraries/` altında paketlenmiştir; böylece doğru sürümü internette aramaya gerek kalmadan derleme tekrar üretilebilir kalır.

### Adımlar
1. **SD kartı hazırla:** DFPlayer'ın microSD'sinde `mp3` klasörü oluştur ve 10 komuta karşılık gelen `0001.mp3` … `0010.mp3` dosyalarını ekle.
2. **Alıcıyı yükle** (`reciever.ino`) — önce alıcı ESP32'ye.
3. **MAC adresini oku:** 115200 baud'da seri monitörden alıcının MAC adresini al ve `inference.ino` içindeki `receiverMAC[]` dizisine yapıştır.
4. **Eldiveni yükle** (`inference.ino`) — ESP32-S3'e.
5. **Her iki birimi de aç**, açılış sekansını bekle, ardından eldiven üzerindeki butona basarak kalibrasyona başla.

---

## Veri Toplama İş Akışı

`inference.ino` içinde paketlenen model, `data_collection/` altındaki betiklerle toplanan verilerle eğitildi. Hareket kümesini genişletmek veya yeni bir kullanıcıya göre yeniden eğitmek için:

1. **Veri toplama yazılımını yükle:** `data_collection/data_collection.ino` dosyasını açıp eldiven ESP32-S3'e yükle.
2. **Host taraflı toplayıcıyı çalıştır:** eldiveni bilgisayara bağla ve şu komutu çalıştır:
   ```bash
   python data_collection/data_collector.py
   ```
   Betik hareket etiketi soracak; ardından kaydedilen her pencereyi bu etiketle birlikte `dataset.csv`'ye yazacak.
3. **Her hareket için tekrarla** — sınıf başına ~50 örnek toplanana kadar (dengeyi kontrol etmek için `count_labels.py` çalıştırılır).
4. **Veri setini Edge Impulse için hazırla:**
   ```bash
   python data_collection/split_for_edge_impulse.py
   ```
   Bu komut pencere bazlı dosyaları `edge_impulse_data/` klasörüne yazar.
5. **Edge Impulse Studio'ya yükle**, impulse'ı tasarla, eğit, ardından **Deployment → Arduino Library** ile dışa aktar.
6. **Paketlenmiş ZIP dosyasını** `ESP32-Libraries/` altında yenisiyle değiştir ve `inference.ino`'yu yeniden yükle.

---

## Kullanım

1. Eldiveni aç → sabit mavi LED (BOŞTA).
2. **Butona bas**, kalibrasyona gir:
   - El tamamen **açık** tutulur, LED sarıdan kırmızıya geçene kadar (~2.5 sn).
   - El tamamen **kapalı** (yumruk) tutulur, LED kırmızıdan mora geçene kadar (~2.5 sn).
   - El **sabit** tutulur, LED maviye dönene kadar (~2.5 sn).
3. Eldiven artık HAZIR (sabit mavi).
4. **Butona bas**, hareketi kaydetmek için (~3 sn, sarı LED).
5. Güven skoru ≥ %75 ise yeşil LED yanıp söner ve alıcıdan ses oynar. Aksi takdirde LED iki kez kırmızı yanıp söner.
6. Sonraki hareket için 4. adımdan tekrarla.

---

## Bilinen Sorunlar / Yapılacaklar

- Alıcı yazılımındaki `Serial.println` çağrılarında UTF-8 olmayan bayt dizileri var (orijinalde emoji); bunlar terminalde bozuk karakter olarak görünüyor. Temizlenmeli veya yeniden kodlanmalı.
- `getBatteryPercentage()` fonksiyonu `updateLCD()` içinde, tanımlanmadan önce kullanılıyor — Arduino'nun otomatik prototip üretimi sayesinde çalışıyor ama taşınabilirlik için açıkça forward-declare edilmeli.
- `BUTTON_PIN = 13` ve `BATTERY_PIN = 39` ESP32-S3'te hassas olabiliyor; pin haritası kullandığın dev board sürümüne göre doğrulanmalı.
- Pil yüzdesi eğrisi %5'lik adımlarda lineer hesaplanıyor; gerçek Li-Po deşarj eğrisine eşlenmiş bir LUT eklenebilir.
- Eldiven tarafındaki `checkBattery()` `loop()` içinde şu anda yorum satırı olarak duruyor.

---

## Yol Haritası

- [ ] Hareket kümesini 10'dan 15'e çıkar
- [ ] ESP-NOW üzerine şifreleme katmanı ekle (AES, `esp_now_set_pmk` ile)
- [ ] Eldiven için tamamen esnek PCB tasarımına geç
- [ ] Komut iletildi onayı için titreşim motoru (haptic feedback) ekle
- [ ] Çoklu alıcı yayını (mangaca / takım modu)

---

## Teşekkürler

- **HAVELSAN SUIT Programı** — mentorluk ve teknik destek için
- **TED Üniversitesi Mühendislik Fakültesi** — bitirme danışmanlığı ve laboratuvar imkânları
- Edge Impulse, Espressif ve açık kaynak gömülü topluluğu

---

## Lisans

Henüz belirlenmedi. `LICENSE` dosyası (beklemede).

---

<sub>AKKE © 2025–2026 — CMPE 491/492 Bitirme Projesi, TED Üniversitesi</sub>
