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

// Relay pin
#define RELAY_PIN 23

// Control variables
int autoOnHour = -1;              // Default: Invalid hour
int autoOnMinute = -1;            // Default: Invalid minute
int moisture = -1;                // Current moisture value (from Firebase)
int moistureThreshold = -1;       // Moisture threshold (from Firebase)
bool pumpOnCommand = false;       // Command to turn on pump from Firebase
bool pumpState = false;           // Current pump state
unsigned long pumpStartTime = 0;  // Pump activation time

const int TIMEZONE_OFFSET = 8 * 3600; 

// Time client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);// Adjust timezone as needed

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

void initializeFirebase() {
  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true);

  Serial.println("Firebase initialized.");
}

void setup() {
  Serial.begin(115200);

  // Set up relay
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Ensure pump is OFF initially

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
  unsigned long epochTime = timeClient.getEpochTime() + TIMEZONE_OFFSET;

  // Convert epoch time to human-readable format
  int currentHour = (epochTime % 86400L) / 3600; // Hours since midnight
  int currentMinute = (epochTime % 3600) / 60;  // Minutes past the hour
  int currentSecond = epochTime % 60;          // Seconds past the minute

  // Log the current time
  Serial.printf("Current Time (Manila): %02d:%02d:%02d\n", currentHour, currentMinute, currentSecond);

  // Log target Auto On Time
  if (autoOnHour >= 0 && autoOnMinute >= 0) {
    Serial.printf("Auto On Time: %02d:%02d\n", autoOnHour, autoOnMinute);
  } else {
    Serial.println("Auto On Time is not set.");
  }

  // Fetch pump control data from Firestore
  if (readPumpControlData("pumpControl/settings")) {
    // Check if the pump should be turned off immediately
    if (!pumpOnCommand && pumpState) {
      Serial.println("Pump turned OFF: Command in Firestore set to false.");
      turnPumpOff();
    }

    // Manual ON based on Firebase command
    if (pumpOnCommand && !pumpState) {
      Serial.println("Triggering Pump ON: Manual Command Received.");
      turnPumpOn();
    }

    // Auto ON based on time
    if (autoOnHour >= 0 && autoOnMinute >= 0 &&
        currentHour == autoOnHour && currentMinute == autoOnMinute && !pumpState) {
      Serial.println("Triggering Pump ON: Time Match.");
      turnPumpOn();
    }

    // Auto ON based on moisture threshold
    if (moisture >= 0 && moistureThreshold >= 0 &&
        moisture < moistureThreshold && !pumpState) {
      Serial.printf("Triggering Pump ON: Moisture (%d) below Threshold (%d).\n", moisture, moistureThreshold);
      turnPumpOn();
    }
  }

  // Turn OFF pump after 10 seconds unless real-time command overrides
  if (pumpState && millis() - pumpStartTime >= 10000 && pumpOnCommand) {
    Serial.println("Pump turned OFF: 10-second Timer Expired.");
    turnPumpOff();
  }

  delay(100); // Adjust as necessary to balance responsiveness and Firestore limits
}


void turnPumpOn() {
  digitalWrite(RELAY_PIN, LOW); // Turn pump ON
  pumpStartTime = millis();
  pumpState = true;
  Serial.println("Pump turned ON.");

  // Update Firestore
   FirebaseJson content;
  content.set("fields/pumpOn/booleanValue", true); // Correct nested structure

  Serial.print("Updating Firestore for Pump ON...");
  if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "(default)", "pumpControl/settings", content.raw(), "pumpOn")) {
    Serial.println("Firestore updated: Pump ON.");
  } else {
    Serial.println("Error updating Firestore: " + fbdo.errorReason());
  }
}

void turnPumpOff() {
  digitalWrite(RELAY_PIN, HIGH); // Turn pump OFF
  pumpState = false;
  Serial.println("Pump turned OFF.");

  // Update Firestore
  FirebaseJson content;
  content.set("fields/pumpOn/booleanValue", false); // Correct nested structure

  Serial.print("Updating Firestore for Pump OFF...");
  if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "(default)", "pumpControl/settings", content.raw(), "pumpOn")) {
    Serial.println("Firestore updated: Pump OFF.");
  } else {
    Serial.println("Error updating Firestore: " + fbdo.errorReason());
  }
}

bool readPumpControlData(String path) {
  Serial.println("Getting pump control document...");
  if (Firebase.Firestore.getDocument(&fbdo, FIREBASE_PROJECT_ID, "(default)", path.c_str())) {
    Serial.println("Document retrieved successfully!");
    Serial.println("Payload:");
    Serial.println(fbdo.payload());

    FirebaseJson json;
    FirebaseJsonData jsonData;

    json.setJsonData(fbdo.payload());
    bool validData = true;

    // Extract Auto On Hour
    if (json.get(jsonData, "fields/autoOnHour/integerValue")) {
      autoOnHour = String(jsonData.stringValue).toInt(); // Convert string to int
      Serial.printf("Auto On Hour: %d\n", autoOnHour);
    } else {
      validData = false;
      Serial.println("Failed to get Auto On Hour.");
    }

    // Extract Auto On Minute
    if (json.get(jsonData, "fields/autoOnMinute/integerValue")) {
      autoOnMinute = String(jsonData.stringValue).toInt(); // Convert string to int
      Serial.printf("Auto On Minute: %d\n", autoOnMinute);
    } else {
      validData = false;
      Serial.println("Failed to get Auto On Minute.");
    }

    // Extract Moisture
    if (json.get(jsonData, "fields/moisture/integerValue")) {
      moisture = String(jsonData.stringValue).toInt(); // Convert string to int
      Serial.printf("Moisture: %d\n", moisture);
    } else {
      validData = false;
      Serial.println("Failed to get Moisture.");
    }

    // Extract Moisture Threshold
    if (json.get(jsonData, "fields/moistureThreshold/integerValue")) {
      moistureThreshold = String(jsonData.stringValue).toInt(); // Convert string to int
      Serial.printf("Moisture Threshold: %d\n", moistureThreshold);
    } else {
      validData = false;
      Serial.println("Failed to get Moisture Threshold.");
    }

    // Extract Pump On Command
    if (json.get(jsonData, "fields/pumpOn/booleanValue")) {
      pumpOnCommand = jsonData.boolValue; // Directly assign boolean
      Serial.printf("Pump On Command: %s\n", pumpOnCommand ? "true" : "false");
    } else {
      validData = false;
      Serial.println("Failed to get Pump On Command.");
    }

    return validData;
  } else {
    Serial.print("Error: ");
    Serial.println(fbdo.errorReason());
    return false;
  }
}
