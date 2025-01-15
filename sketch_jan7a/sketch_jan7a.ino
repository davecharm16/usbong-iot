  #include <WiFi.h>
  #include <Firebase_ESP_Client.h>
  #include <ModbusMaster.h>


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
  #define RXD2 16
  #define TXD2 17
  #define RE_PIN 4 // RE (Receiver Enable)
  #define DE_PIN 5

  // RS485 Control Pins for NPK Sensor
  #define RX3 21
  #define TX3 22
  #define RE_PIN2 19
  #define DE_PIN2 18

  ModbusMaster node;
  ModbusMaster node2;

  void preTransmission() {
      digitalWrite(RE_PIN, HIGH);
      digitalWrite(DE_PIN, HIGH);
      delay(5); // Delay to stabilize
  }

  void postTransmission() {
      digitalWrite(DE_PIN, LOW);
      digitalWrite(RE_PIN, LOW);
      delay(5); // Delay to stabilize
  }


  void preTransmission2() {
    digitalWrite(RE_PIN2, HIGH);
    digitalWrite(DE_PIN2, HIGH);
    delay(5);
  }

  void postTransmission2() {
    digitalWrite(DE_PIN2, LOW);
    digitalWrite(RE_PIN2, LOW);
    delay(5);
  }


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

  // Placeholder function to get random NPK data
  void getFakeNPKData(int &nitrogen, int &phosphorus, int &potassium) {
    nitrogen = random(0, 101);  // Random value between 0 and 100
    phosphorus = random(0, 101);
    potassium = random(0, 101);
    Serial.printf("Generated NPK Data -> N: %d, P: %d, K: %d\n", nitrogen, phosphorus, potassium);
  }

  // Placeholder function to get random MPST data
  void getMPSTData(int &moisture, float &temperature, float &salinity, float &pH) {
    moisture = random(0, 101);  // Random value between 0 and 100
    temperature = random(200, 350) / 10.0;  // Random value between 20.0 and 35.0
    salinity = random(0, 100) / 10.0;  // Random value between 0.0 and 10.0
    pH = random(50, 90) / 10.0;  // Random value between 5.0 and 9.0
    Serial.printf("Generated MPST Data -> Moisture: %d, Temperature: %.1f, Salinity: %.1f, pH: %.1f\n", moisture, temperature, salinity, pH);
  }

void getNPKData(int &nitrogen, int &phosphorus, int &potassium, int &lastNitrogen, int &lastPhosphorus, int &lastPotassium) {
  Serial.println("Requesting NPK data...");
  uint8_t result = node2.readHoldingRegisters(0x001E, 3); // Read 3 registers starting at 0x001E

  if (result == node2.ku8MBSuccess) {
    nitrogen = node2.getResponseBuffer(0);
    phosphorus = node2.getResponseBuffer(1);
    potassium = node2.getResponseBuffer(2);

    // Validate and update last known values
    if (nitrogen >= 0 && phosphorus >= 0 && potassium >= 0) {
      lastNitrogen = nitrogen;
      lastPhosphorus = phosphorus;
      lastPotassium = potassium;
    }

    Serial.printf("NPK Data -> Nitrogen: %d mg/kg, Phosphorus: %d mg/kg, Potassium: %d mg/kg\n",
                  nitrogen, phosphorus, potassium);
  } else {
    Serial.println("Modbus error, using last known good values...");
    nitrogen = lastNitrogen;
    phosphorus = lastPhosphorus;
    potassium = lastPotassium;
  }
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

  // Function to write MPST data to Firestore
  void writeMPSTData(String collection, String document, int moisture, float temperature, float salinity, float pH) {
    FirebaseJson content;
    content.set("fields/moisture/integerValue", String(moisture));
    content.set("fields/temperature/doubleValue", String(temperature));
    content.set("fields/salinity/doubleValue", String(salinity));
    content.set("fields/pH/doubleValue", String(pH));

    String path = collection + "/" + document;
    Serial.println("Writing MPST document...");
    if (Firebase.Firestore.createDocument(&fbdo, FIREBASE_PROJECT_ID, "", path.c_str(), content.raw())) {
      Serial.println("MPST Document written successfully!");
      Serial.println(fbdo.payload());
    } else {
      Serial.print("Error: ");
      Serial.println(fbdo.errorReason());
    }
  }

  // Function to read data from Firestore and parse MPST data
  void readMPSTData(String path) {
    Serial.println("Getting MPST document...");
    if (Firebase.Firestore.getDocument(&fbdo, FIREBASE_PROJECT_ID, "", path.c_str())) {
      Serial.println("Document retrieved successfully!");
      Serial.println("Payload:");
      Serial.println(fbdo.payload());

      // Parse the JSON payload
      FirebaseJson json;
      FirebaseJsonData jsonData;
      json.setJsonData(fbdo.payload());

      int moisture;
      float temperature, salinity, pH;

      if (json.get(jsonData, "fields/moisture/integerValue")) {
        moisture = jsonData.to<int>();
        Serial.printf("Moisture: %d%%\n", moisture);
      } else {
        Serial.println("Failed to get Moisture.");
      }

      if (json.get(jsonData, "fields/temperature/doubleValue")) {
        temperature = jsonData.to<float>();
        Serial.printf("Temperature: %.2f°C\n", temperature);
      } else {
        Serial.println("Failed to get Temperature.");
      }

      if (json.get(jsonData, "fields/salinity/doubleValue")) {
        salinity = jsonData.to<float>();
        Serial.printf("Salinity: %.2f‰\n", salinity);
      } else {
        Serial.println("Failed to get Salinity.");
      }

      if (json.get(jsonData, "fields/pH/doubleValue")) {
        pH = jsonData.to<float>();
        Serial.printf("pH: %.2f\n", pH);
      } else {
        Serial.println("Failed to get pH.");
      }
    } else {
      Serial.print("Error: ");
      Serial.println(fbdo.errorReason());
    }
  }

  void updateNPKData(String collection, String document, int nitrogen, int phosphorus, int potassium) {
    FirebaseJson content;
    content.set("fields/n/integerValue", String(nitrogen));
    content.set("fields/p/integerValue", String(phosphorus));
    content.set("fields/k/integerValue", String(potassium));

    String path = collection + "/" + document;
    Serial.println("Updating NPK document...");
    if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", path.c_str(), content.raw(), "")) {
      Serial.println("NPK Document updated successfully!");
      Serial.println(fbdo.payload());
    } else {
      Serial.print("Error: ");
      Serial.println(fbdo.errorReason());
    }
  }

  void updateMPSTData(String collection, String document, int moisture, float temperature, float salinity, float pH) {
    FirebaseJson content;
    content.set("fields/moisture/integerValue", String(moisture));
    content.set("fields/temperature/doubleValue", String(temperature));
    content.set("fields/salinity/doubleValue", String(salinity));
    content.set("fields/pH/doubleValue", String(pH));

    String path = collection + "/" + document;
    Serial.println("Updating MPST document...");
    if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", path.c_str(), content.raw(), "")) {
      Serial.println("MPST Document updated successfully!");
      Serial.println(fbdo.payload());
    } else {
      Serial.print("Error: ");
      Serial.println(fbdo.errorReason());
    }
  }

  void updateMoistueOnPump(String collection, String document, int moisture){
    FirebaseJson content;
    content.set("fields/moisture/integerValue", String(moisture));
    String path = collection + "/" + document;
    Serial.println("Updating Moisture on Settings document...");
    if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", path.c_str(), content.raw(), "")) {
      Serial.println("MPST Document updated successfully!");
      Serial.println(fbdo.payload());
    } else {
      Serial.print("Error: ");
      Serial.println(fbdo.errorReason());
    }
  }


  void setup() {
    pinMode(RE_PIN, OUTPUT);
    pinMode(DE_PIN, OUTPUT);
    digitalWrite(RE_PIN, LOW); // Default to Receiver mode
    digitalWrite(DE_PIN, LOW); 

    pinMode(RE_PIN2, OUTPUT);
    pinMode(DE_PIN2, OUTPUT);
    digitalWrite(RE_PIN2, LOW);
    digitalWrite(DE_PIN2, LOW);


    Serial.begin(115200);
    Serial2.begin(4800, SERIAL_8N1, RXD2, TXD2);

    node.begin(1, Serial2); // Sensor ID = 1
    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);

    Serial1.begin(9200, SERIAL_8N1, RX3, TX3);
    node2.begin(1, Serial1);
    node2.preTransmission(preTransmission2);
    node2.postTransmission(postTransmission2);


    // Connect to WiFi
    connectToWiFi();

    // Initialize Firebase
    initializeFirebase();

    // Example: Writing NPK data to Firestore
    writeNPKData("npk", "npkdata", 100, 33, 20);

    // Example: Writing MPST data to Firestore
    writeMPSTData("mpst", "mpstdata", 75, 22.5, 3.2, 7.1);

    // Update MPST data
    updateMPSTData("mpst", "mpstdata", 80, 24.0, 3.5, 7.2);

    // Read and parse NPK data
    readNPKData("npk/npkdata");
  }

  void loop() {
    static unsigned long lastPollTime = 0;
    unsigned long currentTime = millis();
    static unsigned long lastModbusPoll = 0;

    // Variables for storing the latest valid data
    static int lastMoisture = 0;
    static float lastTemperature = 0.0;
    static float lastSalinity = 0.0;
    static float lastPH = 0.0;
    static int lastNitrogen = -1;
    static int lastPhosphorus = -1;
    static int lastPotassium = -1;

    // Temporary variables for current Modbus read
    int moisture = lastMoisture;
    float temperature = lastTemperature;
    float salinity = lastSalinity;
    float pH = lastPH;
  
    int nitrogen, phosphorus, potassium;

    // Check if it's time to poll Modbus
    if (currentTime - lastModbusPoll > 2000) { // Modbus every 2 seconds
      lastModbusPoll = currentTime;

      // Read data from Modbus
      uint8_t result = node.readInputRegisters(0x0000, 4); // Read 4 registers starting at 0x0000

      if (result == node.ku8MBSuccess) {
        // Assign values to temporary variables
        moisture = node.getResponseBuffer(0) / 10;        // Moisture
        temperature = node.getResponseBuffer(1) / 10.0;  // Temperature
        salinity = node.getResponseBuffer(2);            // Conductivity
        pH = node.getResponseBuffer(3) / 10.0;           // pH

        // Update last valid data
        lastMoisture = moisture;
        lastTemperature = temperature;
        lastSalinity = salinity;
        lastPH = pH;

        // Print updated data to Serial Monitor
        Serial.printf("Modbus Data -> Moisture: %d%%, Temperature: %.1f°C, Salinity: %.1f‰, pH: %.1f\n",
                      moisture, temperature, salinity, pH);
      } else {
        // Modbus error - reuse last valid data
        Serial.print("Modbus Error Code: ");
        Serial.println(result);
        Serial.println("Using last valid data for Firestore update...");
      }

      getNPKData(nitrogen, phosphorus, potassium, lastNitrogen, lastPhosphorus, lastPotassium );
    }

    // Check if it's time to update Firestore
    if (currentTime - lastPollTime > 10000) { // Poll Firestore every 10 seconds
      lastPollTime = currentTime;

      Serial.println("Updating Firestore with latest data...");

      // Update Firestore with the latest data (either from Modbus or last valid values)
      updateMPSTData("mpst", "mpstdata", moisture, temperature, salinity, pH);

      // Update Firebase
      updateNPKData("npk", "npkdata", lastNitrogen, lastPhosphorus, lastPotassium);

      readNPKData("npk/npkdata");
      readMPSTData("mpst/mpstdata");
    }
  }

