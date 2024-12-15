#include "Arduino.h"
#include "LoRa_E220.h"

// Pin definitions
#define PIN_M0 19
#define PIN_M1 21
#define PIN_TX 17
#define PIN_RX 16
#define PIN_AUX 18

// Create an object for the E220 module
LoRa_E220 e220ttl(&Serial2, -1, PIN_M0, PIN_M1, UART_BPS_RATE_9600);

bool isSending = false;          // Flag for sending messages
int totalPacketsReceived = 0;    // Counter for received packets
int totalRSSI = 0;               // Sum of RSSI values

bool configureModule(int rate = 24); // Function prototype for module configuration

void setup() {
  Serial.begin(9600); // Initialize the serial port
  delay(500);

  Serial2.begin(9600, SERIAL_8N1, PIN_RX, PIN_TX); // Initialize the second serial port
  Serial.println("Serial2 started");

  e220ttl.begin(); // Initialize the E220 module
  Serial.println("LoRa E220 initialized");

  if (!configureModule()) {
    Serial.println("Failed to configure module. Trying again...");
    delay(1000);
    configureModule();
  }

  Serial.println("LoRa E220 Receiver ready!");
}

void loop() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command.equalsIgnoreCase("start")) {
      Serial.println("Which air rate? (24 for 2.4kbps, 48 for 4.8kbps, 96 for 9.6kbps, etc.)");
      while (!Serial.available())
        ;
      int rate = Serial.readStringUntil('\n').toInt();
      configureModule(rate); // Configure the module with the specified data rate
      sendCommand("start=" + String(rate));
      isSending = true;
      totalPacketsReceived = 0;
      totalRSSI = 0;
      Serial.println("Transmission started.");
    } else if (command.equalsIgnoreCase("stop")) {
      sendCommand("stop");
      isSending = false;
      Serial.println("Transmission stopped.");
      delay(100);
      Serial.print("Total packets received: ");
      Serial.println(totalPacketsReceived);

      if (totalPacketsReceived >= 100 && totalPacketsReceived <= 10000) {
        int noiseFloor = totalRSSI / totalPacketsReceived; // Calculate noise floor
        Serial.print("Estimated Noise Floor: ");
        Serial.print(noiseFloor);
        Serial.println(" dBm");
      } else {
        Serial.println("Number of packets received is out of range for noise floor calculation.");
      }
    }
  }

  if (isSending) {
    if (e220ttl.available() > 1) {
      ResponseContainer rc = e220ttl.receiveMessageRSSI(); // Receive a message with RSSI value
      if (rc.status.code == E220_SUCCESS) {
        String message = String(rc.data);
        message.trim();

        int rssiValue = rc.rssi;
        Serial.print("Raw RSSI value: ");
        Serial.println(rssiValue);

        if (rssiValue > 127) {
          rssiValue -= 256; // Correct the RSSI value
        }

        Serial.print("Adjusted RSSI value: ");
        Serial.println(rssiValue);

        totalRSSI += rssiValue; // Sum the RSSI values

        if (message.startsWith("Total packets sent:")) {
          Serial.println(message);
          Serial.print("Total packets received: ");
          Serial.println(totalPacketsReceived);
          isSending = false; // Stop processing messages
        } else {
          totalPacketsReceived++;
          Serial.println("Received message:");
          int forceIndex = message.indexOf("F=") + 2;
          int forceEndIndex = message.indexOf("N,");
          int tempIndex = message.indexOf("T=") + 2;
          int tempEndIndex = message.indexOf("C,");
          int timeIndex = message.indexOf("t=") + 2;
          int timeEndIndex = message.indexOf("s");

          String force = message.substring(forceIndex, forceEndIndex);
          String temperature = message.substring(tempIndex, tempEndIndex);
          String time = message.substring(timeIndex, timeEndIndex);

          Serial.print("F: ");
          Serial.print(force);
          Serial.println(" N");
          Serial.print("T: ");
          Serial.print(temperature);
          Serial.println(" °C");
          Serial.print("t: ");
          Serial.print(time);
          Serial.println(" s");
          Serial.print("RSSI: ");
          Serial.println(rssiValue);
        }
      } else {
        Serial.print("Error receiving message: ");
        Serial.println(rc.status.getResponseDescription());
      }
    }
  }

  delay(100); // Small delay to prevent overloading the serial monitor
}

void sendCommand(String command) {
  Serial.println("Sending command: " + command);
  ResponseStatus rs = e220ttl.sendMessage(command); // Send a command
  Serial.println(rs.getResponseDescription());
}

bool configureModule(int rate) {
  ResponseStructContainer c;
  c = e220ttl.getConfiguration(); // Retrieve current configuration
  if (c.status.code == E220_SUCCESS) {
    Configuration configuration = *(Configuration*)c.data;
    Serial.println(c.status.getResponseDescription());

    // Configure module parameters
    configuration.ADDL = 0x00;
    configuration.ADDH = 0x00;
    configuration.CHAN = 23;
    configuration.SPED.uartBaudRate = UART_BPS_RATE_9600;
    configuration.SPED.uartParity = MODE_00_8N1;
    configuration.OPTION.transmissionPower = POWER_22;
    configuration.OPTION.RSSIAmbientNoise = RSSI_AMBIENT_NOISE_DISABLED;
    configuration.TRANSMISSION_MODE.enableRSSI = RSSI_ENABLED;

    switch (rate) {
      case 24:
        configuration.SPED.airDataRate = AIR_DATA_RATE_010_24;
        break;
      case 48:
        configuration.SPED.airDataRate = AIR_DATA_RATE_011_48;
        break;
      case 96:
        configuration.SPED.airDataRate = AIR_DATA_RATE_100_96;
        break;
      case 192:
        configuration.SPED.airDataRate = AIR_DATA_RATE_101_192;
        break;
      case 384:
        configuration.SPED.airDataRate = AIR_DATA_RATE_110_384;
        break;
      case 625:
        configuration.SPED.airDataRate = AIR_DATA_RATE_111_625;
        break;
      default:
        Serial.println("Invalid air rate value");
        c.close();
        return false;
    }

    ResponseStatus rs = e220ttl.setConfiguration(configuration, WRITE_CFG_PWR_DWN_SAVE); // Apply new configuration
    if (rs.code != E220_SUCCESS) {
      Serial.print("Error setting configuration: ");
      Serial.println(rs.getResponseDescription());
      return false;
    }
    c.close();
    return true;
  } else {
    Serial.print("Error configuring module: ");
    Serial.println(c.status.getResponseDescription());
    return false;
  }
}
