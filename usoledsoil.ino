
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ==========================================
// OLED SSD1306 128x32
// ==========================================
#define I2C_SDA 32
#define I2C_SCL 33
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(128, 32, &Wire, -1);

// ==========================================
// SENSOR ULTRASONIK HC-SR04
// ==========================================
#define PIN_TRIGGER 26
#define PIN_ECHO    27

#define KECEPATAN_SUARA 0.0343f

long durasi = 0;
float jarak_cm = 0.0f;
bool sensor_jarak_valid = false;

// ==========================================
// CAPACITIVE SOIL MOISTURE SENSOR V2
// ==========================================
#define SOIL_PIN 34

// NILAI KALIBRASI SEMENTARA
// Ganti dengan nilai ADC hasil pengukuran sensor kamu.
const int ADC_KERING = 3000;
const int ADC_BASAH  = 1300;

int nilai_adc_tanah = 0;
float kelembapan_tanah = 0.0f;


// ==========================================
// BACA SENSOR ULTRASONIK
// ==========================================
void bacaUltrasonik() {
  digitalWrite(PIN_TRIGGER, LOW);
  delayMicroseconds(2);

  digitalWrite(PIN_TRIGGER, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIGGER, LOW);

  durasi = pulseIn(PIN_ECHO, HIGH, 30000UL);

  if (durasi > 0) {
    jarak_cm = (durasi * KECEPATAN_SUARA) / 2.0f;
    sensor_jarak_valid = true;
  } else {
    sensor_jarak_valid = false;
  }
}

// ==========================================
// BACA CAPACITIVE SOIL MOISTURE
// ==========================================
void bacaKelembapanTanah() {
  const int JUMLAH_SAMPEL = 20;
  uint32_t total_adc = 0;

  for (int i = 0; i < JUMLAH_SAMPEL; i++) {
    total_adc += analogRead(SOIL_PIN);
    delay(5);
  }

  nilai_adc_tanah = total_adc / JUMLAH_SAMPEL;

  // Validasi nilai kalibrasi
  if (ADC_KERING == ADC_BASAH) {
    kelembapan_tanah = 0.0f;
    return;
  }

  // Konversi ADC menjadi persentase relatif
  // Rumus ini mendukung ADC naik atau turun ketika basah.
  float persen =
    ((float)(nilai_adc_tanah - ADC_KERING) /
     (float)(ADC_BASAH - ADC_KERING)) * 100.0f;

  // Batasi nilai antara 0 sampai 100%
  kelembapan_tanah = constrain(persen, 0.0f, 100.0f);
}

// ==========================================
// TAMPILKAN HASIL KE OLED
// ==========================================
void tampilkanOLED() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  // Baris 1: judul
  display.setCursor(0, 0);
  display.println("MONITORING SENSOR");

  // Garis pemisah
  display.drawLine(0, 9, 127, 9, SSD1306_WHITE);

  // Baris 2: jarak ultrasonik
  display.setCursor(0, 12);
  display.print("Jarak : ");

  if (sensor_jarak_valid) {
    display.print(jarak_cm, 1);
    display.print(" cm");
  } else {
    display.print("Error");
  }

  // Baris 3: kelembapan tanah
  display.setCursor(0, 23);
  display.print("Tanah : ");
  display.print(kelembapan_tanah, 0);
  display.print("%");

  display.display();
}

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200);

  // Konfigurasi sensor ultrasonik
  pinMode(PIN_TRIGGER, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  digitalWrite(PIN_TRIGGER, LOW);

  // Konfigurasi ADC ESP32
  analogReadResolution(12);
  analogSetPinAttenuation(SOIL_PIN, ADC_11db);

  // Inisialisasi I2C OLED
  Wire.begin(I2C_SDA, I2C_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED gagal init");

    while (true) {
      delay(1000);
    }
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Sistem Monitoring");
  display.setCursor(0, 16);
  display.println("Inisialisasi...");
  display.display();

  delay(1000);

  Serial.println("Sistem monitoring siap");
}

// ==========================================
// LOOP UTAMA
// ==========================================
void loop() {
  bacaUltrasonik();
  bacaKelembapanTanah();

  // Perbarui OLED
  tampilkanOLED();

  // ========================================
  // SERIAL MONITOR
  // ========================================
  Serial.println("========== HASIL SENSOR ==========");

  if (sensor_jarak_valid) {
    Serial.print("Jarak ultrasonik : ");
    Serial.print(jarak_cm, 2);
    Serial.println(" cm");
  } else {
    Serial.println("Jarak ultrasonik : Tidak terbaca");
  }

  Serial.print("ADC tanah        : ");
  Serial.println(nilai_adc_tanah);

  Serial.print("Kelembapan tanah : ");
  Serial.print(kelembapan_tanah, 1);
  Serial.println(" %");

  Serial.println("==================================");

  delay(500);
}
