#include <WiFi.h>
#include <Firebase_ESP_Client.h>

// Provide the token generation process info.
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

// Function to write NPK data to Firestore
void writeNPKData(String collection, String document, int nitrogen, int phosphorus, int potassium) {
  FirebaseJson content;
  content.set("fields/n/integerValue", String(nitrogen));
  content.set("fields/p/integerValue", String(phosphorus));
  content.set("fields/k/integerValue", String(potassium));

  String path = collection + "/" + document;
  Serial.println("Writing NPK document...");
  if (Firebase.Firestore.createDocument(&fbdo, FIREBASE_PROJECT_ID, "", path.c_str(), content.raw())) {
    Serial.println("NPK Document written successfully!");
    Serial.println(fbdo.payload());
  } else {
    Serial.print("Error: ");
    Serial.println(fbdo.errorReason());
  }
}

// Function to read data from Firestore and parse NPK data
void readNPKData(String path) {
  Serial.println("Getting NPK document...");
  if (Firebase.Firestore.getDocument(&fbdo, FIREBASE_PROJECT_ID, "", path.c_str())) {
    Serial.println("Document retrieved successfully!");
    Serial.println("Payload:");
    Serial.println(fbdo.payload());

    // Parse the JSON payload
    FirebaseJson json;
    FirebaseJsonData jsonData;
    json.setJsonData(fbdo.payload());

    int nitrogen, phosphorus, potassium;

    if (json.get(jsonData, "fields/n/integerValue")) {
      nitrogen = jsonData.to<int>();
      Serial.printf("Nitrogen: %d%%\n", nitrogen);
    } else {
      Serial.println("Failed to get Nitrogen.");
    }

    if (json.get(jsonData, "fields/p/integerValue")) {
      phosphorus = jsonData.to<int>();
      Serial.printf("Phosphorus: %d%%\n", phosphorus);
    } else {
      Serial.println("Failed to get Phosphorus.");
    }

    if (json.get(jsonData, "fields/k/integerValue")) {
      potassium = jsonData.to<int>();
      Serial.printf("Potassium: %d%%\n", potassium);
    } else {
      Serial.println("Failed to get Potassium.");
    }
  } else {
    Serial.print("Error: ");
    Serial.println(fbdo.errorReason());
  }
}

void setup() {
  Serial.begin(115200);

  // Connect to WiFi
  connectToWiFi();

  // Initialize Firebase
  initializeFirebase();

  // Example: Writing NPK data to Firestore
  writeNPKData("npk", "npkdata", 100, 33, 20);

  // Read and parse NPK data
  readNPKData("npk/npkdata");
}

void loop() {
  // Poll Firestore periodically to check for updates
  static unsigned long lastPollTime = 0;
  unsigned long currentTime = millis();

  if (currentTime - lastPollTime > 10000) { // Poll every 10 seconds
    lastPollTime = currentTime;
    Serial.println("Polling Firestore for updates...");
    readNPKData("npk/npkdata");
  }
}
