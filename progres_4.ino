#define BLYNK_TEMPLATE_ID "TMPL6CnjwdBoQ"         // Ganti dengan Template ID Anda
#define BLYNK_TEMPLATE_NAME "Indikator Infus"      // Nama project di Blynk Cloud
#define BLYNK_AUTH_TOKEN "T-KxP6RJmCmslspEltz58uP8ckwin9PU"  // Token dari Blynk Cloud

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <WiFiManager.h>
#include <Wire.h>
#include <Adafruit_MPR121.h>

// Pin Configuration
#define SDA_PIN D2
#define SCL_PIN D1
#define LED_PIN D5
#define BUZZER_PIN D6
#define BUTTON_PIN D7

// Sensor Thresholds
const int THRESHOLD_BAWAH = 135;
const int THRESHOLD_TENGAH = 135;
const int THRESHOLD_ATAS = 100;

Adafruit_MPR121 cap = Adafruit_MPR121();

// Blynk Virtual Pins
#define BLYNK_LEVEL_PIN V1    // Untuk mengirim status level infus
#define BLYNK_BUZZER_PIN V2   // Untuk kontrol buzzer dari Blynk

// Variabel Sistem
bool buzzerEnabled = true;
bool sudahBunyi20 = false;
unsigned long lastBuzzerToggleTime = 0;

void setup() {
  Serial.begin(115200);
  delay(1000); // Stabilisasi serial

  // Inisialisasi Hardware
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  digitalWrite(BUZZER_PIN, LOW);

  // Inisialisasi I2C dengan speed rendah
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(50000); // 50 kHz untuk stabilitas

  // Cek Sensor MPR121
  if (!cap.begin(0x5A)) {
    Serial.println("MPR121 tidak terdeteksi! Cek koneksi.");
    while (1) {
      digitalWrite(LED_PIN, HIGH);
      delay(200);
      digitalWrite(LED_PIN, LOW);
      delay(200);
    }
  }

  // Koneksi WiFi + Blynk
  WiFiManager wm;
  wm.setConnectTimeout(30); // Timeout 30 detik
  if (!wm.autoConnect("Infus_Config")) {
    Serial.println("Gagal koneksi WiFi! Restart...");
    delay(1000);
    ESP.restart();
  }

  Blynk.config(BLYNK_AUTH_TOKEN);
  if (!Blynk.connect(3000)) {
    Serial.println("Gagal koneksi Blynk! Operasi offline.");
  }
}

BLYNK_WRITE(BLYNK_BUZZER_PIN) { // Kontrol buzzer dari Blynk
  buzzerEnabled = !param.asInt();
  if (!buzzerEnabled) noTone(BUZZER_PIN);
}

void handleButton() {
  static int lastButtonState = HIGH;
  static unsigned long lastDebounceTime = 0;
  
  int reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > 50) {
    if (reading == LOW) {
      buzzerEnabled = false;
      noTone(BUZZER_PIN);
      Blynk.virtualWrite(BLYNK_BUZZER_PIN, 1); // Update Blynk
      Serial.println("Buzzer: NONAKTIF (Tombol Fisik)");
    }
  }
  lastButtonState = reading;
}

void handleSensor() {
  static unsigned long lastRead = 0;
  if (millis() - lastRead < 200) return; // Batasi pembacaan
  lastRead = millis();

  // Baca sensor dengan error handling
  Wire.beginTransmission(0x5A);
  byte error = Wire.endTransmission();
  if (error != 0) {
    Serial.println("Error I2C! Coba reset...");
    Wire.begin(SDA_PIN, SCL_PIN);
    return;
  }

  int bawah = cap.filteredData(0);
  int tengah = cap.filteredData(1);
  int atas = cap.filteredData(2);

  // Debug nilai sensor
  Serial.printf("Sensor: B=%d T=%d A=%d\n", bawah, tengah, atas);

  // Logika Level Infus
  int statusGambar = 3; // Default: Habis (3)
  
  if (bawah < THRESHOLD_BAWAH && tengah < THRESHOLD_TENGAH && atas < THRESHOLD_ATAS) {
    statusGambar = 0; // Penuh (0)
    digitalWrite(LED_PIN, HIGH);
    noTone(BUZZER_PIN);
    sudahBunyi20 = false;
  } 
  else if (bawah < THRESHOLD_BAWAH && tengah >= THRESHOLD_TENGAH && atas >= THRESHOLD_ATAS) {
    statusGambar = 2; // 20% (2)
    digitalWrite(LED_PIN, LOW);
    if (buzzerEnabled && !sudahBunyi20) {
      tone(BUZZER_PIN, 1000, 2000);
      sudahBunyi20 = true;
    }
  }
  else if (bawah >= THRESHOLD_BAWAH && tengah >= THRESHOLD_TENGAH && atas >= THRESHOLD_ATAS) {
    statusGambar = 3; // Habis (3)
    digitalWrite(LED_PIN, LOW);
    if (buzzerEnabled) tone(BUZZER_PIN, 2000);
  }
  else {
    statusGambar = 1; // Tengah/80% (1)
    digitalWrite(LED_PIN, LOW);
    noTone(BUZZER_PIN);
  }

  // Kirim status ke Blynk
  Blynk.virtualWrite(BLYNK_LEVEL_PIN, statusGambar);
}

void loop() {
  Blynk.run();
  yield(); // Penting untuk stabilitas WiFi
  
  handleButton();
  handleSensor();
  
  // Auto-aktifkan buzzer setelah 30 menit
  if (!buzzerEnabled && millis() - lastBuzzerToggleTime > 30 * 60 * 1000) {
    buzzerEnabled = true;
    Blynk.virtualWrite(BLYNK_BUZZER_PIN, 0);
  }}
