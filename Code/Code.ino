#define BLYNK_TEMPLATE_ID   "TMPL6YnRXF8lA"
#define BLYNK_TEMPLATE_NAME "Smart Plant"
#define BLYNK_AUTH_TOKEN    "obHCGHln6K5j9_0wgWgH28VPpqbkRxEI"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

char ssid[] = "Sabin";
char pass[] = "1234567890";

// Pins
#define DHTPIN      4
#define DHTTYPE     DHT11
#define SOIL_DO_PIN 33
#define PIR_PIN     27
#define RELAY_PIN   26

LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHTPIN, DHTTYPE);
BlynkTimer timer;

// Soil DO behavior
const bool SOIL_DRY_IS_1 = true;

// Relay trigger type (MOST modules: LOW = ON)
const bool RELAY_LOW_TRIGGER = true;

// Blynk Event (configure this in Blynk to send Email)
const char* MOTION_EVENT = "motion_detected";
const unsigned long MOTION_COOLDOWN_MS = 30000;

bool pumpOn = false;
int lastPir = 0;
unsigned long lastEventMs = 0;

bool isSoilDry(int soilDO) {
  return SOIL_DRY_IS_1 ? (soilDO == 1) : (soilDO == 0);
}

void setPump(bool on) {
  pumpOn = on;
  if (RELAY_LOW_TRIGGER) {
    digitalWrite(RELAY_PIN, on ? LOW : HIGH);
  } else {
    digitalWrite(RELAY_PIN, on ? HIGH : LOW);
  }
}

void forcePumpOff() {
  if (RELAY_LOW_TRIGGER) digitalWrite(RELAY_PIN, HIGH);
  else digitalWrite(RELAY_PIN, LOW);
  pumpOn = false;
}

BLYNK_CONNECTED() {
  forcePumpOff();
  Blynk.virtualWrite(V12, 0);
  Blynk.syncVirtual(V12);
  Serial.println("Blynk connected. Pump forced OFF.");
}

BLYNK_WRITE(V12) {
  int v = param.asInt(); // 0/1
  setPump(v == 1);
  Serial.print("V12 button -> Pump ");
  Serial.println(pumpOn ? "ON" : "OFF");
}

void sendData() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  int pir = digitalRead(PIR_PIN);
  int soilDO = digitalRead(SOIL_DO_PIN);
  bool dry = isSoilDry(soilDO);

  // Blynk updates
  if (!isnan(t)) Blynk.virtualWrite(V0, t);
  if (!isnan(h)) Blynk.virtualWrite(V1, h);
  Blynk.virtualWrite(V3, dry ? 1 : 0);
  Blynk.virtualWrite(V6, pir);
  Blynk.virtualWrite(V5, pir);

  // LCD
  lcd.setCursor(0, 0);
  lcd.print("T:");
  if (isnan(t)) lcd.print("--");
  else lcd.print(t, 1);
  lcd.print(" H:");
  if (isnan(h)) lcd.print("--");
  else lcd.print(h, 0);
  lcd.print("  ");

  lcd.setCursor(0, 1);
  lcd.print(dry ? "Soil:DRY " : "Soil:WET ");
  lcd.print("P:");
  lcd.print(pir ? "1" : "0");
  lcd.print(pumpOn ? " ON " : " OFF");

  // Serial
  Serial.print("Temp: "); Serial.print(isnan(t) ? 0 : t, 2);
  Serial.print(" C | Hum: "); Serial.print(isnan(h) ? 0 : h, 2);
  Serial.print(" % | SoilDO: "); Serial.print(soilDO);
  Serial.print(dry ? " (DRY)" : " (WET)");
  Serial.print(" | PIR: "); Serial.print(pir);
  Serial.print(" | Pump: "); Serial.println(pumpOn ? "ON" : "OFF");

  // Motion event (rising edge + cooldown)
  if (pir == 1 && lastPir == 0) {
    unsigned long now = millis();
    if (now - lastEventMs > MOTION_COOLDOWN_MS) {
      lastEventMs = now;
      Blynk.logEvent(MOTION_EVENT, "Motion detected by PIR sensor!");
      Serial.println("Event sent: motion_detected");
    }
  }
  lastPir = pir;
}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(PIR_PIN, INPUT);
  pinMode(SOIL_DO_PIN, INPUT);

  pinMode(RELAY_PIN, OUTPUT);
  forcePumpOff();

  dht.begin();

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting...");

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Ready!");
  delay(800);
  lcd.clear();

  timer.setInterval(2000L, sendData);
}

void loop() {
  Blynk.run();
  timer.run();
}
