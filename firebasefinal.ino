#include <WiFi.h>
#include <FirebaseESP32.h>
#include "time.h"
#include <deque>
#include <Arduino.h>
#include <EMGFilters.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------- OLED ----------
#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---------- WiFi ----------
#define WIFI_SSID "Redmi 13"
#define WIFI_PASSWORD "03540313"

// ---------- Firebase ----------
#define API_KEY "AIzaSyDRa4h7e_YmFE_nhCtE8DQun8NJ5b1nDkM"
#define DATABASE_URL "https://myapp-b8e6f-default-rtdb.asia-southeast1.firebasedatabase.app"
#define USER_EMAIL "user@project.com"
#define USER_PASSWORD "12345678"

// ---------- Firebase Object ----------
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// ---------- NTP Time (Asia/Jakarta) ----------
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7 * 3600;
const int daylightOffset_sec = 0;

// ---------- EMG ----------
#define SensorInputPin 35
EMGFilters myFilter;
SAMPLE_FREQUENCY sampleRate = SAMPLE_FREQ_500HZ;
NOTCH_FREQUENCY humFreq = NOTCH_FREQ_50HZ;
static int Threshold = 0;

String basePath = "/monitoring_tubuh/line1/history";
std::deque<String> dataKeys; 
void setup() {
  Serial.begin(115200);

  // Inisialisasi OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("❌ OLED tidak ditemukan"));
    while (true); // Stop jika gagal
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Step 1: Carpalizer Ready
  display.setCursor(10, 30);
  display.println("Carpalizer Ready");
  display.display();
  delay(5000);
  display.clearDisplay();
  display.display();
  Serial.printf("Connecting WiFi...");
  // Step 2: Connecting WiFi animasi titik  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  display.clearDisplay();
  display.setCursor(10, 30);
  display.println("Connecting WiFi...");
  display.display();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  Serial.printf("WiFi Connected");
  // Step 3: WiFi Connected
  display.clearDisplay();
  display.setCursor(10, 30);
  display.println("WiFi Connected");
  display.display();
  delay(2000);
  display.clearDisplay();
  display.display();

  // Konfigurasi waktu & Firebase
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  myFilter.init(sampleRate, humFreq, true, true, true);
  pinMode(SensorInputPin, INPUT);
}

void checkWiFi() {
  static bool wasConnected = true;

  if (WiFi.status() != WL_CONNECTED) {
    if (wasConnected) {
      Serial.println("❌ WiFi Terputus!");
      display.clearDisplay();
      display.setCursor(10, 30);
      display.println("WiFi Disconnected");
      display.display();
      delay(2000);
    }
    Serial.printf("Reconnecting");
    display.clearDisplay();
    display.setCursor(10, 30);
    display.print("Reconnecting");
    display.display();

    int dotCount = 0;
    while (WiFi.status() != WL_CONNECTED) {
      display.clearDisplay();
      display.setCursor(10, 30);
      display.print("Reconnecting");
      for (int i = 0; i < dotCount; i++) {
        display.print(".");
      }
      display.display();
      dotCount = (dotCount + 1) % 4;
      delay(500);
    }

    Serial.println("✅ WiFi Reconnected");
    display.clearDisplay();
    display.setCursor(10, 30);
    display.println("WiFi Reconnected");
    display.display();
    delay(2000);
    display.clearDisplay();
  }
}

void loop() {
  checkWiFi();

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("❌ Gagal mendapatkan waktu");
    delay(3000);
    return;
  }

  // Baca nilai EMG dan proses filter
  int rawValue = analogRead(SensorInputPin);
  int filtered = myFilter.update(rawValue);
  int envelope = sq(filtered);
  envelope = (envelope > Threshold) ? envelope : 0;

  // Format waktu
  char waktuStr[30];
  strftime(waktuStr, sizeof(waktuStr), "%d-%m-%Y %H:%M:%S", &timeinfo);
  String waktuKey = String(waktuStr);
  waktuKey.replace(" ", "_");
  waktuKey.replace(":", "-");

  // Serial log
  Serial.printf("✅ %s | EMG: %d\n", waktuStr, envelope);

  // Tampilkan di OLED
  display.clearDisplay();
  display.setCursor(0, 10);
  display.println("EMG Monitoring");
  display.setCursor(0, 30);
  display.println(waktuStr);
  display.setCursor(0, 50);
  display.print("EMG: ");
  display.print(envelope);
  display.display();

  // Kirim ke Firebase
  FirebaseJson json;
  json.set("waktu", waktuStr);
  json.set("emg", envelope);
  String path = basePath + "/" + waktuKey;

  if (Firebase.setJSON(fbdo, path.c_str(), json)) {
    dataKeys.push_back(waktuKey);
    if (dataKeys.size() > 50) {
      String oldestKey = dataKeys.front();
      dataKeys.pop_front();
      String deletePath = basePath + "/" + oldestKey;
      if (Firebase.deleteNode(fbdo, deletePath.c_str())) {
        Serial.println("🗑️ Hapus lama: " + oldestKey);
      } else {
        Serial.println("❌ Gagal hapus lama: " + fbdo.errorReason());
      }
    }
  } else {
    Serial.println("❌ Gagal kirim: " + fbdo.errorReason());
  }

  delay(2000); // Sampling rate bisa dioptimalkan
}
