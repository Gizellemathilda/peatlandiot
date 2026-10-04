
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <max6675.h>
#include <math.h>

// ==================================================
// WIFI
// ==================================================
const char* WIFI_SSID = "";//sesuaikan nama wifi hostpot
const char* WIFI_PASSWORD = "";//sesuaikan pass sesuai hotspot

// ==================================================
// EMQX CLOUD MQTT TLS
// ==================================================
const char* MQTT_HOST = "pc110c86.ala.asia-southeast1.emqxsl.com";
const int MQTT_PORT = 8883;

const char* MQTT_USER = "test";
const char* MQTT_PASSWORD = "Testing123";

// Ganti topic sesuai kebutuhan
const char* MQTT_TOPIC = "peatland/sensor";

WiFiClientSecure secureClient;
PubSubClient mqttClient(secureClient);

// ==================================================
// OLED SSD1306 128x32
// ==================================================
#define I2C_SDA 32
#define I2C_SCL 33
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(128, 32, &Wire, -1);
bool oledValid = false;

// ==================================================
// HC-SR04 ULTRASONIC
// ==================================================
#define PIN_TRIGGER 26
#define PIN_ECHO    27

const float KECEPATAN_SUARA = 0.0343f;

unsigned long durasi = 0;
float jarak_cm = 0.0f;
bool sensor_jarak_valid = false;

// ==================================================
// CAPACITIVE SOIL MOISTURE SENSOR V2
// ==================================================
#define SOIL_PIN 34

const int ADC_KERING = 3000;
const int ADC_BASAH  = 1300;

int nilai_adc_tanah = 0;
float kelembapan_tanah = 0.0f;
bool sensor_tanah_valid = false;

// ==================================================
// THERMOCOUPLE TYPE-K MAX6675
// Sesuaikan dengan wiring fisik
// ==================================================
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

// ==================================================
// TIMER
// ==================================================
unsigned long waktuPublishTerakhir = 0;
const unsigned long INTERVAL_PUBLISH = 5000;

// ==================================================
// BACA ULTRASONIK
// ==================================================
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

  float hasilJarak =
    ((float)durasi * KECEPATAN_SUARA) / 2.0f;

  if (hasilJarak < 2.0f || hasilJarak > 400.0f) {
    sensor_jarak_valid = false;
    return;
  }

  jarak_cm = hasilJarak;
  sensor_jarak_valid = true;
}

// ==================================================
// BACA SOIL MOISTURE
// ==================================================
void bacaKelembapanTanah() {
  const int JUMLAH_SAMPEL = 20;
  uint32_t totalADC = 0;

  analogRead(SOIL_PIN);
  delay(5);

  for (int i = 0; i < JUMLAH_SAMPEL; i++) {
    totalADC += analogRead(SOIL_PIN);
    delay(5);
  }

  nilai_adc_tanah =
    (int)round((float)totalADC / JUMLAH_SAMPEL);

  if (ADC_KERING == ADC_BASAH) {
    sensor_tanah_valid = false;
    return;
  }

  float persen =
    ((float)(nilai_adc_tanah - ADC_KERING) /
     (float)(ADC_BASAH - ADC_KERING)) * 100.0f;

  kelembapan_tanah = constrain(persen, 0.0f, 100.0f);
  sensor_tanah_valid = true;
}

// ==================================================
// BACA THERMOCOUPLE MAX6675
// ==================================================
void bacaThermocouple() {
  float hasilSuhu = thermocouple.readCelsius();

  if (isnan(hasilSuhu) ||
      isinf(hasilSuhu) ||
      hasilSuhu < 0.0f ||
      hasilSuhu > 1024.0f) {
    sensor_suhu_valid = false;
    suhu_celsius = -999.0f;
    return;
  }

  suhu_celsius = hasilSuhu;
  sensor_suhu_valid = true;
}

// ==================================================
// OLED
// ==================================================
void tampilkanOLED() {
  if (!oledValid) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print("TMA/Jarak: ");

  if (sensor_jarak_valid) {
    display.print(jarak_cm, 1);
    display.print(" cm");
  } else {
    display.print("Error");
  }

  display.setCursor(0, 11);
  display.print("Moisture: ");

  if (sensor_tanah_valid) {
    display.print(kelembapan_tanah, 0);
    display.print("%");
  } else {
    display.print("Error");
  }

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

// ==================================================
// KONEKSI WIFI
// ==================================================
void koneksiWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.println("Menghubungkan ke WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long mulai = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - mulai < 20000UL) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("WiFi terhubung");
    Serial.print("IP ESP32: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println("WiFi belum terhubung");
  }
}

// ==================================================
// KONEKSI MQTT
// ==================================================
void koneksiMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (mqttClient.connected()) return;

  Serial.println("Menghubungkan ke EMQX MQTT...");

  // ID klien harus unik untuk setiap perangkat
  String clientId = "ESP32-WYIIA-";
  clientId += String((uint32_t)ESP.getEfuseMac(), HEX);

  if (mqttClient.connect(
        clientId.c_str(),
        MQTT_USER,
        MQTT_PASSWORD)) {
    Serial.println("MQTT terhubung!");
  } else {
    Serial.print("MQTT gagal, state = ");
    Serial.println(mqttClient.state());
  }
}

// ==================================================
// BENTUK NILAI JSON
// Jika sensor tidak valid, kirim null
// ==================================================
String nilaiJSON(float nilai, bool valid, int angkaDesimal) {
  if (!valid || isnan(nilai) || isinf(nilai)) {
    return "null";
  }

  return String(nilai, angkaDesimal);
}

// ==================================================
// PUBLISH MQTT
// ==================================================
void kirimMQTT() {
  if (!mqttClient.connected()) return;

  // Saat ini TMA diisi dengan jarak ultrasonik dalam cm.
  String payload = "{";
  payload += "\"tma\":";
  payload += nilaiJSON(jarak_cm, sensor_jarak_valid, 2);

  payload += ",\"moisture\":";
  payload += nilaiJSON(kelembapan_tanah, sensor_tanah_valid, 1);

  payload += ",\"temperature\":";
  payload += nilaiJSON(suhu_celsius, sensor_suhu_valid, 2);

  payload += "}";

  bool berhasil = mqttClient.publish(
    MQTT_TOPIC,
    payload.c_str()
  );

  Serial.print("Payload MQTT: ");
  Serial.println(payload);

  if (berhasil) {
    Serial.println("Publish MQTT berhasil");
  } else {
    Serial.println("Publish MQTT gagal");
  }
}

// ==================================================
// SETUP
// ==================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(PIN_TRIGGER, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  digitalWrite(PIN_TRIGGER, LOW);

  pinMode(SOIL_PIN, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(SOIL_PIN, ADC_11db);

  pinMode(MAX6675_CS, OUTPUT);
  digitalWrite(MAX6675_CS, HIGH);

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);

  oledValid = display.begin(
    SSD1306_SWITCHCAPVCC,
    OLED_ADDR
  );

  if (oledValid) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Monitoring IoT");
    display.println("Connecting WiFi...");
    display.display();
  } else {
    Serial.println("OLED gagal init");
  }

  // MAX6675 membutuhkan waktu konversi awal
  delay(500);

  // Untuk pengujian awal.
  // setInsecure mengenkripsi koneksi TLS, tetapi
  // tidak memverifikasi sertifikat server.
  secureClient.setInsecure();

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setKeepAlive(30);
  mqttClient.setSocketTimeout(5);

  koneksiWiFi();
  koneksiMQTT();

  Serial.println("Monitoring sensor dan MQTT siap");
}

// ==================================================
// LOOP
// ==================================================
void loop() {
  // Periksa koneksi WiFi dan MQTT
  if (WiFi.status() != WL_CONNECTED) {
    koneksiWiFi();
  }

  if (WiFi.status() == WL_CONNECTED &&
      !mqttClient.connected()) {
    koneksiMQTT();
  }

  mqttClient.loop();

  // Baca semua sensor
  bacaUltrasonik();
  bacaKelembapanTanah();
  bacaThermocouple();

  // Tampilkan hasil pada OLED
  tampilkanOLED();

  // Serial Monitor
  Serial.println();
  Serial.println("========== DATA SENSOR ==========");

  Serial.print("TMA/Jarak: ");
  if (sensor_jarak_valid) {
    Serial.print(jarak_cm, 2);
    Serial.println(" cm");
  } else {
    Serial.println("Tidak terbaca");
  }

  Serial.print("ADC tanah: ");
  Serial.println(nilai_adc_tanah);

  Serial.print("Moisture: ");
  if (sensor_tanah_valid) {
    Serial.print(kelembapan_tanah, 1);
    Serial.println(" %");
  } else {
    Serial.println("Tidak valid");
  }

  Serial.print("Temperature: ");
  if (sensor_suhu_valid) {
    Serial.print(suhu_celsius, 2);
    Serial.println(" C");
  } else {
    Serial.println("Tidak valid");
  }

  // Publish setiap 5 detik
  if (millis() - waktuPublishTerakhir >= INTERVAL_PUBLISH) {
    waktuPublishTerakhir = millis();
    kirimMQTT();
  }

  delay(500);
}
