#define BLYNK_TEMPLATE_ID   "TMPL6YnRXF8lA"
#define BLYNK_TEMPLATE_NAME "Smart Plant"
#define BLYNK_AUTH_TOKEN    "obHCGHln6K5j9_0wgWgH28VPpqbkRxEI"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// ---------------- WiFi ----------------
char ssid[] = "Sabin";
char pass[] = "1234567890";

// ---------------- Pins ----------------
#define DHTPIN 4
#define DHTTYPE DHT11

#define SOIL_AO_PIN 34      // <-- Connect Soil AO to GPIO34 (recommended)
#define PIR_PIN 27
#define RELAY_PIN 26

// ---------------- LCD ----------------
LiquidCrystal_I2C lcd(0x27, 16, 2);  // if no text, try 0x3F

// ---------------- Objects ----------------
DHT dht(DHTPIN, DHTTYPE);
BlynkTimer timer;

// ---------------- Relay settings ----------------
// You said relay works best with VCC=3V3 in your setup.
// Most 1-channel relay modules are LOW-trigger (LOW=ON, HIGH=OFF).
const bool RELAY_LOW_TRIGGER = true;

// ---------------- Motion Email/Event ----------------
// You must create an Event in Blynk Console with this exact code:
const char* MOTION_EVENT = "motion_detected";

// prevent spam: send at most once every 30s
const unsigned long MOTION_COOLDOWN_MS = 30000;

// ---------------- Soil calibration ----------------
// These are example values. You MUST calibrate using Serial Monitor.
// Dry in air usually HIGH raw, wet soil/water usually LOWER raw.
int SOIL_DRY = 3500;   // raw value when soil is DRY (probe in air/dry soil)
int SOIL_WET = 1500;   // raw value when soil is WET (probe in wet soil/water)

// ---------------- State ----------------
bool pumpOn = false;
int lastPir = 0;
unsigned long lastEventMs = 0;

// Keep last sensor values for email message
float lastT = NAN;
float lastH = NAN;
int lastSoilPct = -1;

// ---------------- Helpers ----------------
void setPump(bool on) {
  pumpOn = on;
  if (RELAY_LOW_TRIGGER) {
    digitalWrite(RELAY_PIN, on ? LOW : HIGH);   // LOW=ON, HIGH=OFF
  } else {
    digitalWrite(RELAY_PIN, on ? HIGH : LOW);   // HIGH=ON, LOW=OFF
  }
}

void forcePumpOff() {
  if (RELAY_LOW_TRIGGER) digitalWrite(RELAY_PIN, HIGH);
  else digitalWrite(RELAY_PIN, LOW);
  pumpOn = false;
}

int readSoilPercent() {
  int raw = analogRead(SOIL_AO_PIN);

  // Convert raw -> 0..100 (0=dry, 100=wet)
  int pct = map(raw, SOIL_DRY, SOIL_WET, 0, 100);
  pct = constrain(pct, 0, 100);

  // Keep for Serial debug
  lastSoilPct = pct;

  return pct;
}

String buildStatusMessage() {
  String msg = "Motion detected! ";

  msg += "T=";
  if (isnan(lastT)) msg += "NA";
  else msg += String(lastT, 1);
  msg += "C, H=";

  if (isnan(lastH)) msg += "NA";
  else msg += String(lastH, 0);
  msg += "%, Soil=";

  if (lastSoilPct < 0) msg += "NA";
  else msg += String(lastSoilPct);
  msg += "%";

  return msg;
}

// ---------------- Blynk ----------------
BLYNK_CONNECTED() {
  // Safety: OFF at boot/connect
  forcePumpOff();

  // Sync and set UI to OFF
  Blynk.virtualWrite(V12, 0);
  Blynk.syncVirtual(V12);

  Serial.println("Blynk connected. Pump forced OFF.");
}

BLYNK_WRITE(V12) {
  int v = param.asInt();   // 0/1 from Switch widget
  setPump(v == 1);
  Serial.print("V12 -> Pump ");
  Serial.println(pumpOn ? "ON" : "OFF");
}

// ---------------- Main update loop ----------------
void sendData() {
  // Read sensors
  lastT = dht.readTemperature();
  lastH = dht.readHumidity();

  int pir = digitalRead(PIR_PIN);
  int soilPct = readSoilPercent();

  // Send to Blynk (your UI pins)
  if (!isnan(lastT)) Blynk.virtualWrite(V0, lastT);
  if (!isnan(lastH)) Blynk.virtualWrite(V1, lastH);

  Blynk.virtualWrite(V3, soilPct);  // Soil gauge now 0..100
  Blynk.virtualWrite(V6, pir);
  Blynk.virtualWrite(V5, pir);

  // LCD display
  lcd.setCursor(0, 0);
  lcd.print("T:");
  if (isnan(lastT)) lcd.print("--");
  else lcd.print(lastT, 1);
  lcd.print(" H:");
  if (isnan(lastH)) lcd.print("--");
  else lcd.print(lastH, 0);
  lcd.print("  ");

  lcd.setCursor(0, 1);
  lcd.print("Soil:");
  lcd.print(soilPct);
  lcd.print("% ");
  lcd.print("P:");
  lcd.print(pir ? "1" : "0");
  lcd.print(pumpOn ? " ON" : " OF");

  // Serial output (includes raw)
  int raw = analogRead(SOIL_AO_PIN);
  Serial.print("Temp: ");
  Serial.print(isnan(lastT) ? 0 : lastT, 2);
  Serial.print(" C | Hum: ");
  Serial.print(isnan(lastH) ? 0 : lastH, 2);
  Serial.print(" % | SoilRaw: ");
  Serial.print(raw);
  Serial.print(" | Soil%: ");
  Serial.print(soilPct);
  Serial.print(" | PIR: ");
  Serial.print(pir);
  Serial.print(" | Pump: ");
  Serial.println(pumpOn ? "ON" : "OFF");

  // Motion event (rising edge) with cooldown
  if (pir == 1 && lastPir == 0) {
    unsigned long now = millis();
    if (now - lastEventMs > MOTION_COOLDOWN_MS) {
      lastEventMs = now;

      // Send Blynk Event with full info (this can trigger email automation)
      Blynk.logEvent(MOTION_EVENT, buildStatusMessage());

      Serial.println("Event sent: motion_detected");
    } else {
      Serial.println("Motion detected (cooldown) - no event sent.");
    }
  }

  lastPir = pir;
}

void setup() {
  Serial.begin(115200);
  delay(200);

  // Pins
  pinMode(PIR_PIN, INPUT);
  pinMode(SOIL_AO_PIN, INPUT);

  pinMode(RELAY_PIN, OUTPUT);
  forcePumpOff();

  // Sensors
  dht.begin();

  // LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting...");
  lcd.setCursor(0, 1);
  lcd.print("WiFi+Blynk");

  // Blynk connect
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Ready!");
  delay(800);
  lcd.clear();

  // Update every 2 seconds
  timer.setInterval(2000L, sendData);

  Serial.println("Setup complete.");
  Serial.println("TIP: Calibrate SOIL_DRY & SOIL_WET using Serial Monitor.");
}

void loop() {
  Blynk.run();
  timer.run();
}
