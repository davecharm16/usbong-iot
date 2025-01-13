#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#include <addons/TokenHelper.h>

// WiFi credentials
#define WIFI_SSID "HUAWEI-W8wQ"
#define WIFI_PASSWORD "wonderful"

// Firebase project credentials
#define API_KEY "AIzaSyDCOQlxEa1OgcXq737wvPa_GejvMNdOSfs"
#define FIREBASE_PROJECT_ID "usbong-45c3e"
#define USER_EMAIL "test@gmail.com"
#define USER_PASSWORD "test1234"

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Function to connect to WiFi
void connectToWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

// Function to configure and initialize Firebase
void initializeFirebase() {
  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true);

  Serial.println("Firebase initialized.");
}

// Define relay pin
#define RELAY_PIN 23

int autoOnHour = 15;              // Default: 3 PM
int autoOnMinute = 30;            // Default: 30 minutes past the hour
int moisture = 0;                 // Current moisture value (from Firebase)
int moistureThreshold = 500;      // Moisture threshold (from Firebase)

// Time client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 28800, 60000); // Adjust timezone as needed

bool pumpState = false;
unsigned long pumpStartTime = 0;

void setup() {
  Serial.begin(115200);

  // Set up relay
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Ensure pump is OFF initially

  // Connect to WiFi
  connectToWiFi();

  // Initialize Firebase
  initializeFirebase();

  // Initialize NTP client
  timeClient.begin();
}

void loop() {
  // Fetch current time from NTP
  timeClient.update();
  int currentHour = timeClient.getHours();
  int currentMinute = timeClient.getMinutes();

  // Fetch pump control data from Firestore
  readPumpControlData("pumpControl/settings");

  // Auto ON based on time
  if (currentHour == autoOnHour && currentMinute == autoOnMinute && !pumpState) {
    turnPumpOn();
    delay(60000); // Prevent multiple activations in the same minute
  }

  // Auto ON based on moisture threshold
  if (moisture < moistureThreshold && !pumpState) {
    turnPumpOn();
  }

  // Turn OFF pump after 5 seconds
  if (pumpState && millis() - pumpStartTime >= 5000) {
    turnPumpOff();
  }

  delay(100); // Small delay to avoid rapid Firestore calls
}

void turnPumpOn() {
  digitalWrite(RELAY_PIN, HIGH); // Turn pump ON
  pumpStartTime = millis();
  pumpState = true;
  Serial.println("Pump turned ON.");

  // Update Firestore
  FirebaseJson json;
  json.set("fields.state.stringValue", "ON");
  if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "(default)", "pumpControl/settings", json.raw(), "")) {
    Serial.println("Firestore updated: Pump ON.");
  } else {
    Serial.println("Error updating Firestore: " + fbdo.errorReason());
  }
}

void turnPumpOff() {
  digitalWrite(RELAY_PIN, LOW); // Turn pump OFF
  pumpState = false;
  Serial.println("Pump turned OFF.");

  // Update Firestore
  FirebaseJson json;
  json.set("fields.state.stringValue", "OFF");
  if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "(default)", "pumpControl/settings", json.raw(), "")) {
    Serial.println("Firestore updated: Pump OFF.");
  } else {
    Serial.println("Error updating Firestore: " + fbdo.errorReason());
  }
}

void readPumpControlData(String path) {
  Serial.println("Getting pump control document...");
  if (Firebase.Firestore.getDocument(&fbdo, FIREBASE_PROJECT_ID, "(default)", path.c_str())) {
    Serial.println("Document retrieved successfully!");
    Serial.println("Payload:");
    Serial.println(fbdo.payload());

    // Parse the JSON payload
    FirebaseJson json;
    FirebaseJsonData jsonData;
    json.setJsonData(fbdo.payload());

    if (json.get(jsonData, "fields.autoOnHour.integerValue")) {
      autoOnHour = jsonData.to<int>();
      Serial.printf("Auto On Hour: %d\n", autoOnHour);
    } else {
      Serial.println("Failed to get Auto On Hour.");
    }

    if (json.get(jsonData, "fields.autoOnMinute.integerValue")) {
      autoOnMinute = jsonData.to<int>();
      Serial.printf("Auto On Minute: %d\n", autoOnMinute);
    } else {
      Serial.println("Failed to get Auto On Minute.");
    }

    if (json.get(jsonData, "fields.moisture.integerValue")) {
      moisture = jsonData.to<int>();
      Serial.printf("Moisture: %d\n", moisture);
    } else {
      Serial.println("Failed to get Moisture.");
    }

    if (json.get(jsonData, "fields.moistureThreshold.integerValue")) {
      moistureThreshold = jsonData.to<int>();
      Serial.printf("Moisture Threshold: %d\n", moistureThreshold);
    } else {
      Serial.println("Failed to get Moisture Threshold.");
    }

    String pumpCommand;
    if (json.get(jsonData, "fields.state.stringValue")) {
      pumpCommand = jsonData.stringValue;
      Serial.printf("Pump Command: %s\n", pumpCommand.c_str());
      if (pumpCommand == "ON" && !pumpState) {
        turnPumpOn();
      }
    } else {
      Serial.println("Failed to get Pump Command.");
    }
  } else {
    Serial.print("Error: ");
    Serial.println(fbdo.errorReason());
  }
}
