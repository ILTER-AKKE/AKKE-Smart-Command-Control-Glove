# Akıllı Eldiven (Smart Glove) Veri Toplama ve TinyML Planı

Bu belge, 5 adet flex sensör, 1 adet MPU6050, 1 adet RGB LED ve 1 adet buton kullanılarak gerçekleştirilecek el hareketi tanıma (TinyML) projesinin uçtan uca mimarisini ve gerçekleştirme adımlarını içerir.

## Gestures
1. Stop 
2. Listen (1)
3. Go go
4. Come 
5. Stick together 
6. Slow down (2) 
7. Fall back- Geri çekil (3) 
8. Take cover- siper al (4) 
9. Move right (sağa yumruk at)  
10. Move left (sola yumruk at) 
11. Check the area (yukarı tabanca) 
12. Spread out – dağıl (9) 
13. Get down – çök (yere doğru yumruk) 
14. the surroundings are clean - Etraf temiz (el işareti Y) 
15. The operation is over. Gather in the vehicle. (thumbs up) 

## User Review Required

> [!IMPORTANT]
> Aşağıdaki donanım konfigürasyonunu ve kalibrasyon akışını inceleyin. C++ ve Python kısmına geçmeden önce bu mantığın onaylanması gereklidir.
> Ayrıca mikrodenetleyici kartınızı (Arduino Uno, ESP32, Arduino Nano 33 BLE vb.) belirtirseniz TinyML derlemesi aşamasında buna göre spesifik kütüphane seçimi yapılabilir. (Makine öğrenmesi için ESP32, Arduino Nano 33 BLE veya RP2040 gibi RAM'i görece yüksek kartlar tercih edilmelidir).

## 1. Donanım ve Sistem Mimarisi

* **Flex Sensörler (x5):** Parmakların bükülme açılarını ölçecek. `A0-A4` analog pinlerine voltaj bölücü (voltage divider) dirençler ile bağlanacak.
* **MPU6050 (x1):** Elin uzaydaki ivmesini ve dönüşünü ölçecek (3 Eksen İvme - $A_x, A_y, A_z$ + 3 Eksen Jiroskop - $G_x, G_y, G_z$). I2C pini (SDA, SCL) üzerinden bağlanacak.
* **Buton (x1):** Durumları (State) değiştirmek ve kayıtları tetiklemek için. Pull-down/Pull-up direnci ile `D2` gibi kesmeye (interrupt) uygun veya polling yapılabilecek bir dijital pin üzerinden okunacak.
* **RGB LED (x1):** Kullanıcıyı görsel yönlendirmek için `PWM` pinlerine (örn. D9, D10, D11) bağlanacak (Ortak Anot/Katot ayarına göre).

## 2. Kalibrasyon ve Veri Toplama Akışı (C++ Arduino Tarafı)

Sistem bir Durum Makinesi (State Machine) olarak çalışacaktır:

1. **IDLE (Sarı Işık):** Sistem boşta, buton 1'e basılmayı bekliyor.
2. **KALİBRASYON (Buton'a 1. Basış)**
   - **Adım A - Açık El (Mavi Işık):** Python ekranına "Eli tam açık konumda tutun" mesajı gönderilir. Kullanıcı elini açık tutar, sensörlerden "minimum/maksimum" açık değerleri alınır. (Örn: 2 saniye boyunca ölçüm ortalaması veya o andaki tek ölçüm).
   - **Adım B - Kapalı El (Turuncu Işık):** "Bütün eli bükün (yumruk)" mesajı. Sensörlerden yumruk değerleri alınır. İki değer arası Range (açıklık) belirlenir ve normalizasyon (0-100 veya 0-1 arası) yapılır.
   - **Adım C - Gyro/Accel Kalib. (Mor Işık):** "Elini sabit tut" mesajı. MPU6050 2 saniye boyunca ardışık okunup 6 eksen sükunet (offset) ortalamaları bulunur.
3. **KAYIT BEKLEME (Yeşil Işık):** Kalibrasyon tamamlandı. Python ekranında "Hazır, Hareket X bekleniyor" ibaresi çıkar.
4. **KAYIT EKRANI (Butona 2. Basış):** 
   - **3 Saniyelik Ölçüm (Kırmızı Işık Yanıp Söner):** 3 saniye boyunca örneğin 50Hz veya 100Hz hızında sürekli okuma yapılır.
   - Her okuma satırında: `ZamanDamgası, Flex1_Norm, Flex2_Norm, Flex3_Norm, Flex4_Norm, Flex5_Norm, Ax_cal, Ay_cal, Az_cal, Gx_cal, Gy_cal, Gz_cal` bulunur.
   - 3 Saniye dolunca tekrar "KAYIT BEKLEME" alanına (Yeşil Işık) dönülür.

## 3. Python Veri Toplama ve Loglama (Dataset Oluşturma)

C++'tan UART (Serial) ile gelen data Python uygulamasına düşecektir.
Python uygulamasının görevleri:
* **Yönlendirme ve GUI/Konsol Logu:** Serial'dan özel etiketleri (örn: `MSG: Elini Aç`, `MSG: Hareket 1 İçin Hazır`) okuyup konsola basacak.
* **Otomatik Etiketleme (Labeling):** Python çalıştırılırken hangi hareketin çalışılacağı girilir veya uygulamanın içinden "Hareket 1, Hareket 2, Hareket 3" şeklinde yönetilir.
* **Loglanacak Veriler (Header):**
  `label, record_id, timestamp_ms, f1, f2, f3, f4, f5, aX, aY, aZ, gX, gY, gZ`
* **CSV Formatı:** Zaman aralığı boyna tutulan seri veriler CSV içine biriktirilecek. Örn. 50 defa Hareket_1 tamamlandığında 50 farklı `record_id` ile kaydedilecek.

## 4. TinyML / Predictive ML Planı

Toplanan dataset, sinyal işleme tabanlı bir akıllı modele eğitilecektir.

### Model Öğrenimi Seçenekleri
Giyilebilir zaman serisi tahmini (Time-Series classification) için **Edge Impulse** (veya klasik TensorFlow Keras ile özel 1D-CNN) kullanılması en doğru yaklaşımdır.
1. **Veri Ön İşleme (Windowing):** 3 Saniyelik veriler Windowing yöntemi ile işlenecektir.
2. **Feature Extraction (Özellik Çıkarımı):**
   - Flex sensörleri için: Min, max, varyans, ortalama (time-domain features).
   - IMU (MPU6050) için: Time-domain özelliklerin yanı sıra fourier dönüşümü (Spectral Analysis/FFT) kullanılarak hareketin frekans özellikleri çıkartılacaktır.
3. **Makine Öğrenimi Modeli:** Geneli 2-5 Katman arası tam bağlı (Dense) ağlardan veya küçük bir Random Forest modelinden oluşturulan ufak bir Classifier tasarlanır. (Eğer Board RAM'ı çok düşükse doğrudan istatistiksel Thresholding + Random Forest; genişse Neural Network).

### Tahmin (Predictive) Entegrasyonu:
* Model eğitildikte sonra C++ header (örn: `model.h`) dosyası olarak olarak indirilir.
* Arduino koduna entegre edildikten sonra sistemin yeni Durumu (State): "Sürekli ölç - Son 3 Saniyeyi buffer'da tut - İnference yap - Hareket X olarak tespit edildiğinde Python'a veya Mavi/Mor Işık olarak dışa yansıt" olur.

## Open Questions

> [!WARNING]
> * Veri toplanacak mikrodenetleyici tam olarak hangi modeldir? (Arduino R3, ESP32, vs. - Zaman algısı ve hafıza için gerekli.)
> * Veri okuma hızı (Sampling Rate) ne olmalı? İnsan hareketleri için genelde 50Hz (20ms gecikme) veya 25Hz fazlasıyla yeterli olmaktadır. (50Hz planlandı).
> * Python kodunun bir Terminal betiği (CLI) olarak mı yoksa Tkinter/PyQt tabanlı basit bir arayüz (GUI) olarak mı yazılmasını istersiniz?

## Verification Plan

### Test ve Doğrulama
- **Arduino:** Sensör verilerinin doğru voltaj/I2C adreslerinden gelip gelmediğinin Serial Plotter'dan incelenmesi. Kalibrasyon sonrasında flex'lerin uç noktalarında tahmini (0-1) arasında tam normalize sonuç verip vermediğinin testi.
- **Python:** Butona basılıp 3 saniyelik sekans bitince `dataset.csv` dosyasında verilerin doğru formatta yazıldığının ve etiket (label) kontrolünün sağlanması.
