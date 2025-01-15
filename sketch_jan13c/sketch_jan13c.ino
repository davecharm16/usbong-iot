#include <WiFi.h>
#include <Firebase_ESP_Client.h>

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
int currentHour = -2;
int currentMinute = -2;
int lastTriggeredMinute = -1;     // Tracks the last minute the pump was triggered

#define TIMEZONE_OFFSET 8 * 3600
#define DAYLIGHT_OFFSET 0

void updateTimeFromNTP() {
    configTime(TIMEZONE_OFFSET, DAYLIGHT_OFFSET, "ph.pool.ntp.org", "time.nist.gov");
    Serial.println("Waiting for time");
    while (!time(nullptr)) {
        Serial.print(".");
        delay(1000);
    }
}

void getCurrentTimeFormatted(int &hour, int &minute) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        hour = -1;   // Set to -1 as an error indicator
        minute = -1; // Set to -1 as an error indicator
        return;
    }
    hour = timeinfo.tm_hour;   // Extract the hour
    minute = timeinfo.tm_min;
}

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

bool readMoistureData(String documentPath) {
    Serial.println("Getting moisture data document...");
    if (Firebase.Firestore.getDocument(&fbdo, FIREBASE_PROJECT_ID, "(default)", documentPath.c_str())) {
        Serial.println("Document retrieved successfully!");
        Serial.println("Payload:");
        Serial.println(fbdo.payload());

        FirebaseJson json;
        FirebaseJsonData jsonData;

        json.setJsonData(fbdo.payload());
        
        // Extract Moisture
        if (json.get(jsonData, "fields/moisture/integerValue")) {
            moisture = String(jsonData.stringValue).toInt(); // Convert string to int
            Serial.printf("Moisture: %d\n", moisture);
            return true;
        } else {
            Serial.println("Failed to get Moisture.");
            return false;
        }
    } else {
        Serial.print("Error: ");
        Serial.println(fbdo.errorReason());
        return false;
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

void setup() {
    Serial.begin(115200);

    // Set up relay
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, HIGH); // Ensure pump is OFF initially

    // Connect to WiFi
    connectToWiFi();

    // Update time from NTP
    updateTimeFromNTP();

    // Initialize Firebase
    initializeFirebase();
}

void loop() {
    getCurrentTimeFormatted(currentHour, currentMinute);

    // Log target Auto On Time
    if (autoOnHour >= 0 && autoOnMinute >= 0) {
        Serial.printf("Auto On Time: %02d:%02d\n", autoOnHour, autoOnMinute);
        Serial.printf("Current Time: %02d:%02d\n", currentHour, currentMinute);
    } else {
        Serial.println("Auto On Time is not set.");
    }

    // Fetch moisture data from the specified Firestore document
    if (readMoistureData("mpst/mpstdata")) {
        Serial.println("Moisture data fetched successfully.");
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

        // Auto ON based on time, ensuring no re-trigger within the same minute
        if (autoOnHour >= 0 && autoOnMinute >= 0 &&
            currentHour == autoOnHour && currentMinute == autoOnMinute &&
            lastTriggeredMinute != currentMinute) {
            Serial.println("Triggering Pump ON: Time Match.");
            turnPumpOn();
            lastTriggeredMinute = currentMinute; // Update last triggered minute
        }

        // Auto ON based on moisture threshold
        if (moisture >= 0 && moistureThreshold >= 0 &&
            moisture < moistureThreshold && !pumpState) {
            Serial.printf("Triggering Pump ON: Moisture (%d) below Threshold (%d).\n", moisture, moistureThreshold);
            turnPumpOn();
        }
    }

    // Turn OFF pump after 10 seconds unless real-time command overrides
    if (pumpState && millis() - pumpStartTime >= 10000) {
        Serial.println("Pump turned OFF: 10-second Timer Expired.");
        turnPumpOff();
    }

    // Reset lastTriggeredMinute if the minute has passed
    if (lastTriggeredMinute != -1 && currentMinute != lastTriggeredMinute) {
        lastTriggeredMinute = -1;
    }

    delay(1500); // Adjust as necessary to balance responsiveness and Firestore limits
}
