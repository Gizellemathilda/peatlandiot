
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ==========================================
// KONFIGURASI OLED SSD1306 128x32
// ==========================================
#define I2C_SDA 32
#define I2C_SCL 33

Adafruit_SSD1306 display(128, 32, &Wire, -1);

// ==========================================
// KONFIGURASI SENSOR ULTRASONIK HC-SR04
// ==========================================
#define PIN_TRIGGER 26
#define PIN_ECHO    27

#define KECEPATAN_SUARA 0.0343f

long durasi;
float jarak_cm;
bool sensor_valid;

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200);

  // Inisialisasi pin sensor ultrasonik
  pinMode(PIN_TRIGGER, OUTPUT);
  pinMode(PIN_ECHO, INPUT);

  digitalWrite(PIN_TRIGGER, LOW);

  // Inisialisasi I2C OLED
  Wire.begin(I2C_SDA, I2C_SCL);

  // Inisialisasi OLED, menggunakan konfigurasi program kamu
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED gagal init");
    while (true) {
      delay(1000);
    }
  }

  // Tampilan awal
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("ULTRASONIC SENSOR");

  display.setCursor(0, 16);
  display.println("Memulai sensor...");

  display.display();

  Serial.println("Sistem ultrasonik dan OLED siap");
  delay(1000);
}

// ==========================================
// LOOP
// ==========================================
void loop() {
  // Kirim pulsa trigger
  digitalWrite(PIN_TRIGGER, LOW);
  delayMicroseconds(2);

  digitalWrite(PIN_TRIGGER, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIGGER, LOW);

  // Membaca durasi pantulan dengan timeout 30 ms
  durasi = pulseIn(PIN_ECHO, HIGH, 30000UL);

  // Perhitungan jarak
  sensor_valid = (durasi > 0);

  if (sensor_valid) {
    jarak_cm = (durasi * KECEPATAN_SUARA) / 2.0f;

    // Tampilkan pada Serial Monitor
    Serial.print("Jarak: ");
    Serial.print(jarak_cm, 1);
    Serial.println(" cm");
  } else {
    Serial.println("Jarak tidak terbaca");
  }

  // ========================================
  // TAMPILAN OLED
  // ========================================
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Judul
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("ULTRASONIC SENSOR");

  // Garis pemisah
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  // Hasil pengukuran
  if (sensor_valid) {
    display.setTextSize(2);
    display.setCursor(0, 15);
    display.print(jarak_cm, 1);

    display.setTextSize(1);
    display.print(" cm");
  } else {
    display.setTextSize(1);
    display.setCursor(0, 18);
    display.print("Tidak terbaca");
  }

  // Perbarui layar OLED
  display.display();

  delay(500);
}
