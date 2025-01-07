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

// Document path in Firestore
String documentPath = "npk/npkdata";

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

// Function to write data to Firestore
void writeDocument(String path, FirebaseJson content) {
  Serial.println("Writing document...");
  if (Firebase.Firestore.createDocument(&fbdo, FIREBASE_PROJECT_ID, "", path.c_str(), content.raw())) {
    Serial.println("Document written successfully!");
    Serial.println(fbdo.payload());
  } else {
    Serial.print("Error: ");
    Serial.println(fbdo.errorReason());
  }
}

// Function to read data from Firestore
void readDocument(String path) {
  Serial.println("Getting document...");
  if (Firebase.Firestore.getDocument(&fbdo, FIREBASE_PROJECT_ID, "", path.c_str())) {
    Serial.println("Document retrieved successfully!");
    Serial.println("Payload:");
    Serial.println(fbdo.payload());
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

  // Example: Writing data to Firestore
  FirebaseJson content;
  content.set("fields/temperature/doubleValue", 25.5);
  content.set("fields/humidity/doubleValue", 60.2);
  writeDocument(documentPath, content);
}

void loop() {
  // Poll Firestore periodically to check for updates
  static unsigned long lastPollTime = 0;
  unsigned long currentTime = millis();

  if (currentTime - lastPollTime > 10000) { // Poll every 10 seconds
    lastPollTime = currentTime;
    Serial.println("Polling Firestore for updates...");
    readDocument(documentPath);
  }
}
