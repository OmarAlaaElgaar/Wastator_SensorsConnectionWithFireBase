#include "DHT.h"
#include <WiFi.h>
#include <Firebase_ESP_Client.h>

// Provide the token generation process info.
#include "addons/TokenHelper.h"
// Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// Insert your network credentials
#define WIFI_SSID "OmarElgaar"
#define WIFI_PASSWORD "STEM 6th of OCT El-Doc. S'25"

// Insert Firebase project API Key
#define API_KEY "AIzaSyB-XKMjuCxBSlx63fylpSKu095BqSQllnQ"

// Define the RTDB URL
#define DATABASE_URL "https://wastator-capstone-default-rtdb.firebaseio.com"

FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

// Define the DHT sensor pin and type
#define DPIN1 4  // Pin to connect DHT sensor (GPIO number)
#define DTYPE1 DHT11
#define DPIN2 19
#define DPINBuzz 27
#define DTYPE2 DHT11

DHT dht1(DPIN1, DTYPE1);
DHT dht2(DPIN2, DTYPE2);

// MQ
#include <MQUnifiedsensor.h>
#define placa "Arduino UNO"
#define Voltage_Resolution 5
int pin = 32;  // Digital input of your arduino
String type = "MQ-135";
#define ADC_Bit_Resolution 10  // For Arduino UNO/MEGA/NANO
#define RatioMQ135CleanAir 3.6
MQUnifiedsensor MQ135(placa, Voltage_Resolution, ADC_Bit_Resolution, pin, type);
#define jiash 22
#define jias 33

unsigned long sendDataPrevMillis = 0;  // Global variable declaration

void MQ() {
  MQ135.update();  // Update data, the arduino will read the voltage from the digital pin
  MQ135.setA(110.47);
  MQ135.setB(-2.862);  // Configure the equation to calculate CO2 concentration value
  float CO2 = MQ135.readSensor();
  Serial.print("CO2: ");
  Serial.print(CO2 + 385);
  delay(500);  // Sampling frequency
  if (CO2 > 1000) {
    digitalWrite(jiash, LOW);
    digitalWrite(jias, LOW);
  } else {
    digitalWrite(jiash, HIGH);
    digitalWrite(jias, HIGH);
  }
}

void setup() {
  Serial.begin(115200);  // Corrected baud rate
  dht1.begin();
  dht2.begin();
  pinMode(DPINBuzz, OUTPUT);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Sign up */
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Ok");
  } else {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback;  // see addons/TokenHelper.h

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // MQ
  pinMode(jiash, OUTPUT);  // Initialize jiash pin as output
  pinMode(jias, OUTPUT);   // Initialize jias pin as output
  MQ135.setRegressionMethod(1);
  MQ135.init();
  float calcR0 = 0;
  for (int i = 1; i <= 10; i++) {
    MQ135.update();  // Update data, the arduino will read the voltage from the digital pin
    calcR0 += MQ135.calibrate(RatioMQ135CleanAir);
    Serial.print(".");
  }
  MQ135.setR0(calcR0 / 10);

  if (isinf(calcR0)) {
    Serial.println("Warning: Connection issue, R0 is infinite (Open circuit detected) please check your wiring and supply");
    while (1)
      ;
  }
  if (calcR0 == 0) {
    Serial.println("Warning: Connection issue found, R0 is zero (Digital pin issue) please check your wiring and supply");
    while (1)
      ;
  }
}

void loop() {
  float tempIN = dht1.readTemperature(false);
  float humIN = dht1.readHumidity();
  float tempOUT = dht2.readTemperature(false);
  float humOUT = dht2.readHumidity();
  float CO2 = MQ135.readSensor() + 385;  // Read CO2
  Serial.print(CO2);
  if (CO2 > 400) {
    digitalWrite(DPINBuzz, HIGH);
  } else {
    digitalWrite(DPINBuzz, LOW);
  }

  if (Firebase.ready() && (millis() - sendDataPrevMillis > 1000 || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();

    // Write temperature, humidity, and CO2 to Firebase
    if (Firebase.RTDB.setFloat(&fbdo, "esp32/Indoor Temperature", tempIN)) {
      Serial.println("Indoor Temperature sent to Firebase");
    } else {
      Serial.println("Failed to send Indoor Temperature to Firebase");
      Serial.println("REASON: " + fbdo.errorReason());
    }

    if (Firebase.RTDB.setFloat(&fbdo, "esp32/Outdoor Temperature", tempOUT)) {
      Serial.println("Outdoor Temperature sent to Firebase");
    } else {
      Serial.println("Failed to send Outdoor Temperature to Firebase");
      Serial.println("REASON: " + fbdo.errorReason());
    }

    if (Firebase.RTDB.setFloat(&fbdo, "esp32/HumidityIn", humIN)) {
      Serial.println("Indoor Humidity sent to Firebase");
    } else {
      Serial.println("Failed to send Indoor Humidity to Firebase");
      Serial.println("REASON: " + fbdo.errorReason());
    }

    if (Firebase.RTDB.setFloat(&fbdo, "esp32/HumidityOut", humOUT)) {
      Serial.println("Outdoor Humidity sent to Firebase");
    } else {
      Serial.println("Failed to send Outdoor Humidity to Firebase");
      Serial.println("REASON: " + fbdo.errorReason());
    }

    if (Firebase.RTDB.setFloat(&fbdo, "esp32/CO2", CO2)) {
      Serial.println("CO2 sent to Firebase");
    } else {
      Serial.println("Failed to send CO2 to Firebase");
      Serial.println("REASON: " + fbdo.errorReason());
    }
  }
  // MQ
  MQ();
}
