#define BLYNK_TEMPLATE_ID "TMPL6YnRXF8lA"
#define BLYNK_TEMPLATE_NAME "Smart Plant"
#define BLYNK_AUTH_TOKEN "obHCGHln6K5j9_0wgWgH28VPpqbkRxEI"

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>

// --- PIN DEFINITIONS ---
#define DHTPIN          4 
#define SOIL_PIN        32 
#define PIR_PIN         27 
#define RELAY_PIN_1     26 
#define PUSH_BUTTON_1   25
#define VPIN_BUTTON_1   V12 

LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHTPIN, DHT11);
BlynkTimer timer;

// --- CREDENTIALS ---
char auth[] = "obHCGHln6K5j9_0wgWgH28VPpqbkRxEI";
char ssid[] = "Jag"; 
char pass[] = "12345678"; 

int relay1State = 0;      // 0 = OFF, 1 = ON
int pushButton1State = 1; // 1 = Not pressed (Pull-up)

void setup() {
  Serial.begin(115200);
  
  // 1. RELAY SAFETY: Set OFF immediately (1 = OFF for Active-Low)
  pinMode(RELAY_PIN_1, OUTPUT);
  digitalWrite(RELAY_PIN_1, 1); 
  
  // 2. LCD START
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Smart Garden");
  lcd.setCursor(0, 1);
  lcd.print("Init Hardware...");

  // 3. PINS & SENSORS
  pinMode(PIR_PIN, INPUT);
  pinMode(PUSH_BUTTON_1, INPUT_PULLUP);
  dht.begin();

  // 4. NON-BLOCKING WIFI CONFIG
  WiFi.begin(ssid, pass);
  Blynk.config(auth); // Allows the loop to run even without WiFi
  
  // 5. TIMERS
  timer.setInterval(2000L, checkSensors);      
  timer.setInterval(100L,  checkPhysicalButton); 

  lcd.clear();
  Serial.println("System Ready!");
}

void checkSensors() {
  // --- DHT11 ---
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  
  lcd.setCursor(0, 0);
  if (isnan(h) || isnan(t)) {
    lcd.print("DHT Error       ");
  } else {
    lcd.print("T:"); lcd.print((int)t); lcd.print("C ");
    lcd.setCursor(8, 0);
    lcd.print("H:"); lcd.print((int)h); lcd.print("%   ");
    
    if (Blynk.connected()) {
      Blynk.virtualWrite(V0, t);
      Blynk.virtualWrite(V1, h);
    }
  }

  // --- SOIL MOISTURE ---
  int soilRaw = analogRead(SOIL_PIN);
  int soilPer = map(soilRaw, 4095, 0, 0, 100);
  if(soilPer > 100) soilPer = 100;
  if(soilPer < 0) soilPer = 0;

  lcd.setCursor(0, 1);
  lcd.print("S:"); lcd.print(soilPer); lcd.print("% ");
  
  if (Blynk.connected()) {
    Blynk.virtualWrite(V3, soilPer);
  }

  // --- PIR MOTION ---
  int motion = digitalRead(PIR_PIN);
  lcd.setCursor(7, 1);
  if (motion == 1) { 
    lcd.print("MOV!");
    if (Blynk.connected()) {
      Blynk.logEvent("pirmotion", "WARNING: Motion Detected!");
    }
  } else {
    lcd.print("--- ");
  }
}

void checkPhysicalButton() {
  if (digitalRead(PUSH_BUTTON_1) == 0) { // 0 = Pressed
    if (pushButton1State == 1) { 
      relay1State = !relay1State;
      updateRelay();
      if (Blynk.connected()) {
        Blynk.virtualWrite(VPIN_BUTTON_1, relay1State);
      }
      pushButton1State = 0;
    }
  } else {
    pushButton1State = 1;
  }
}

BLYNK_WRITE(VPIN_BUTTON_1) {
  relay1State = param.asInt();
  updateRelay();
}

void updateRelay() {
  if (relay1State == 1) {
    digitalWrite(RELAY_PIN_1, 0); // 0 = Relay ON
    Serial.println("Pump: ON");
  } else {
    digitalWrite(RELAY_PIN_1, 1); // 1 = Relay OFF
    Serial.println("Pump: OFF");
  }
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    Blynk.run();
  }
  timer.run();

  // LCD Pump Status
  lcd.setCursor(12, 1); 
  lcd.print(relay1State == 1 ? "P:ON" : "P:OF");
}