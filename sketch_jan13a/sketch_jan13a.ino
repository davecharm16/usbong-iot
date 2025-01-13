#include <ModbusMaster.h>

#define RXD2 16
#define TXD2 17
#define RE_PIN 4 // RE (Receiver Enable)
#define DE_PIN 5

ModbusMaster node;

void preTransmission() {
  digitalWrite(RE_PIN, HIGH);
  digitalWrite(DE_PIN, HIGH);
  delay(5); // Stabilization delay
}

void postTransmission() {
  digitalWrite(DE_PIN, LOW);
  digitalWrite(RE_PIN, LOW);
  delay(5); // Stabilization delay
}

void setup() {
  // Pin configurations for RS485 control
  pinMode(RE_PIN, OUTPUT);
  pinMode(DE_PIN, OUTPUT);
  digitalWrite(RE_PIN, LOW); // Default to Receiver mode
  digitalWrite(DE_PIN, LOW);

  // Initialize Serial for debugging
  Serial.begin(115200);

  // Initialize Serial2 for RS485 communication
  Serial2.begin(9200, SERIAL_8N1, RXD2, TXD2);

  // Initialize Modbus communication
  node.begin(1, Serial2); // Sensor ID = 1
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);

  Serial.println("NPK Sensor Initialized...");
}

void loop() {
  static unsigned long lastModbusPoll = 0;
  unsigned long currentTime = millis();

  static int lastNitrogen = -1;
  static int lastPhosphorus = -1;
  static int lastPotassium = -1;

  // Variables for current read
  int nitrogen = lastNitrogen;
  int phosphorus = lastPhosphorus;
  int potassium = lastPotassium;

  if (currentTime - lastModbusPoll > 2000) { // Poll every 2 seconds
    lastModbusPoll = currentTime;

    Serial.println("Requesting NPK data...");

    // Read NPK data from sensor
    uint8_t result = node.readHoldingRegisters(0x001E, 3); // Read 3 registers starting at 0x001E

    if (result == node.ku8MBSuccess) {
      nitrogen = node.getResponseBuffer(0);  // Nitrogen
      phosphorus = node.getResponseBuffer(1); // Phosphorus
      potassium = node.getResponseBuffer(2);  // Potassium

      // Update last known good values
      lastNitrogen = nitrogen;
      lastPhosphorus = phosphorus;
      lastPotassium = potassium;

      // Print data
      Serial.printf("NPK Data -> Nitrogen: %d mg/kg, Phosphorus: %d mg/kg, Potassium: %d mg/kg\n",
                    nitrogen, phosphorus, potassium);
    } else {
      Serial.print("Modbus Error: ");
      Serial.println(result);

      if (result == 226) {
        Serial.println("Timeout: Sensor did not respond.");
      } else if (result == 227) {
        Serial.println("CRC Error: Data corruption.");
      } else {
        Serial.println("Unknown error.");
      }

      // Use last known good data
      Serial.println("Using last known good data:");
      Serial.printf("Nitrogen: %d mg/kg, Phosphorus: %d mg/kg, Potassium: %d mg/kg\n",
                    lastNitrogen, lastPhosphorus, lastPotassium);
    }
  }
}
