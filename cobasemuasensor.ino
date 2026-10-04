#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <max6675.h>
#include <math.h>

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

const float KECEPATAN_SUARA = 0.0343f;

unsigned long durasi = 0;
float jarak_cm = 0.0f;
bool sensor_jarak_valid = false;

// ==========================================
// CAPACITIVE SOIL MOISTURE SENSOR V2
// =================b ,.';m  '.,=========================
#define SOIL_PIN 34

// Kalibrasi awal, sesuaikan dengan hasil pengukuran
const int ADC_KERING = 3000;
const int ADC_BASAH  = 1300;

int nilai_adc_tanah = 0;
float kelembapan_tanah = 0.0f;
bool sensor_tanah_valid = false;

// ==========================================
// THERMOCOUPLE TYPE-K MAX6675
// Pin dipilih agar tidak bentrok dengan sensor lain
// ==========================================
#define MAX6675_SCK 23
#define MAX6675_CS  21
#define MAX6675_SO  22

MAX6675 thermocouple(
  MAX6675_SCK,
  MAX6675_CS,
  MAX6675_SO
);

float suhu_celsius = -999.0f;
bool sensor_suhu_valid = false;

// ==========================================
// BACA SENSOR ULTRASONIK
// ==========================================
void bacaUltrasonik() {
  digitalWrite(PIN_TRIGGER, LOW);
  delayMicroseconds(3);

  digitalWrite(PIN_TRIGGER, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIGGER, LOW);

  durasi = pulseIn(PIN_ECHO, HIGH, 30000UL);

  if (durasi == 0) {
    sensor_jarak_valid = false;
    return;
  }

  // Jarak = waktu tempuh x kecepatan suara / 2
  float hasil_jarak =
    ((float)durasi * KECEPATAN_SUARA) / 2.0f;

  // Batas pengukuran umum HC-SR04
  if (hasil_jarak < 2.0f || hasil_jarak > 400.0f) {
    sensor_jarak_valid = false;
    return;
  }

  jarak_cm = hasil_jarak;
  sensor_jarak_valid = true;
}

// ==========================================
// BACA CAPACITIVE SOIL MOISTURE
// ==========================================
void bacaKelembapanTanah() {
  const int JUMLAH_SAMPEL = 20;
  uint32_t total_adc = 0;

  // Buang pembacaan pertama untuk membantu stabilisasi ADC
  analogRead(SOIL_PIN);
  delay(5);

  for (int i = 0; i < JUMLAH_SAMPEL; i++) {
    total_adc += analogRead(SOIL_PIN);
    delay(5);
  }

  nilai_adc_tanah =
    (int)round((float)total_adc / JUMLAH_SAMPEL);

  if (ADC_KERING == ADC_BASAH) {
    sensor_tanah_valid = false;
    kelembapan_tanah = 0.0f;
    return;
  }

  // Konversi ADC menjadi kelembapan relatif
  // Rumus tetap berlaku apabila nilai ADC meningkat
  // atau menurun ketika sensor menjadi basah.
  float persen =
    ((float)(nilai_adc_tanah - ADC_KERING) /
     (float)(ADC_BASAH - ADC_KERING)) * 100.0f;

  kelembapan_tanah = constrain(persen, 0.0f, 100.0f);
  sensor_tanah_valid = true;
}

// ==========================================
// BACA THERMOCOUPLE MAX6675
// ==========================================
void bacaThermocouple() {
  // MAX6675 melakukan konversi dan kompensasi
  // sambungan dingin di dalam modul.
  // Resolusi hasil pembacaan adalah 0,25 derajat C.
  float hasil_suhu = thermocouple.readCelsius();

  // MAX6675 hanya dapat mengukur suhu 0 hingga 1024 C.
  // Pembacaan tidak valid ditandai sebagai error.
  if (isnan(hasil_suhu) ||
      isinf(hasil_suhu) ||
      hasil_suhu < 0.0f ||
      hasil_suhu > 1024.0f) {
    sensor_suhu_valid = false;
    suhu_celsius = -999.0f;
    return;
  }

  suhu_celsius = hasil_suhu;
  sensor_suhu_valid = true;
}

// ==========================================
// TAMPILKAN HASIL KE OLED 128x32
// Tiga baris, satu halaman
// ==========================================
void tampilkanOLED() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  // Baris 1: jarak ultrasonik
  display.setCursor(0, 0);
  display.print("Jarak: ");

  if (sensor_jarak_valid) {
    display.print(jarak_cm, 1);
    display.print(" cm");
  } else {
    display.print("Error");
  }

  // Baris 2: kelembapan tanah
  display.setCursor(0, 11);
  display.print("Tanah: ");

  if (sensor_tanah_valid) {
    display.print(kelembapan_tanah, 0);
    display.print("%");
  } else {
    display.print("Error");
  }

  // Baris 3: suhu thermocouple
  display.setCursor(0, 22);
  display.print("Suhu: ");

  if (sensor_suhu_valid) {
    display.print(suhu_celsius, 2);
    display.print(" C");
  } else {
    display.print("Error MAX6675");
  }

  display.display();
}

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  delay(500);

  // ---------- ULTRASONIK ----------
  pinMode(PIN_TRIGGER, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  digitalWrite(PIN_TRIGGER, LOW);

  // ---------- SOIL MOISTURE ----------
  pinMode(SOIL_PIN, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(SOIL_PIN, ADC_11db);

  // ---------- MAX6675 ----------
  pinMode(MAX6675_CS, OUTPUT);
  digitalWrite(MAX6675_CS, HIGH);

  // ---------- OLED ----------
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED gagal init!");

    while (true) {
      delay(1000);
    }
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Monitoring Sensor");
  display.println("Soil + Ultrasonic");
  display.println("Thermocouple K");
  display.display();

  // Tunggu MAX6675 menyelesaikan konversi awal
  delay(500);

  Serial.println();
  Serial.println("==================================");
  Serial.println("MONITORING SENSOR ESP32");
  Serial.println("OLED + HC-SR04 + Soil Moisture");
  Serial.println("Thermocouple Type-K MAX6675");
  Serial.println("==================================");
}

// ==========================================
// LOOP UTAMA
// ==========================================
void loop() {
  bacaUltrasonik();
  bacaKelembapanTanah();
  bacaThermocouple();

  tampilkanOLED();

  // ---------- SERIAL MONITOR ----------
  Serial.println();
  Serial.println("========== HASIL SENSOR ==========");

  Serial.print("Durasi ultrasonic : ");
  Serial.print(durasi);
  Serial.println(" us");

  Serial.print("Jarak ultrasonik  : ");
  if (sensor_jarak_valid) {
    Serial.print(jarak_cm, 2);
    Serial.println(" cm");
  } else {
    Serial.println("Tidak terbaca");
  }

  Serial.print("ADC tanah         : ");
  Serial.println(nilai_adc_tanah);

  Serial.print("Kelembapan tanah  : ");
  if (sensor_tanah_valid) {
    Serial.print(kelembapan_tanah, 1);
    Serial.println(" %");
  } else {
    Serial.println("Error kalibrasi");
  }

  Serial.print("Thermocouple      : ");
  if (sensor_suhu_valid) {
    Serial.print(suhu_celsius, 2);
    Serial.println(" C");
  } else {
    Serial.println("Error - cek thermocouple/wiring");
  }

  Serial.println("==================================");

  delay(500);
}
