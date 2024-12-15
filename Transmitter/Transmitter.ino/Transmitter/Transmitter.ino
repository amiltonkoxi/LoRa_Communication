#include "Arduino.h"
#include "LoRa_E220.h"

// Pin definitions
#define PIN_M0 19
#define PIN_M1 21
#define PIN_TX 17
#define PIN_RX 16
#define PIN_AUX 18
#define PIN_AX -1 // Not connected

// Create an object to work with the E220 module
LoRa_E220 e220ttl(&Serial2, PIN_AX, PIN_M0, PIN_M1, UART_BPS_RATE_9600);

bool sending = false; // Flag to control message sending
int packetCount = 0;  // Packet counter

bool configureModule(); // Function prototype for module configuration
void changeAirRate(int rate); // Function prototype for changing data transfer rate

void setup() {
  Serial.begin(9600); // Initialize the serial port
  delay(500);

  Serial2.begin(9600, SERIAL_8N1, PIN_RX, PIN_TX); // Initialize Serial2 for communication with the module
  Serial.println("Serial2 started");

  e220ttl.begin(); // Initialize the E220 module
  Serial.println("LoRa E220 initialized");

  configureModule(); // Configure the module
  Serial.println("LoRa E220 Transmitter ready!");
}

void loop() {
  // Check if there are messages available to receive
  if (e220ttl.available() > 1) {
    ResponseContainer rc = e220ttl.receiveMessage(); // Receive a message
    if (rc.status.code == E220_SUCCESS) {
      String command = String(rc.data);
      command.trim();
      Serial.print("Received command: ");
      Serial.println(command);

      // Process received commands
      if (command.startsWith("start")) {
        int rate = command.substring(command.indexOf('=') + 1).toInt();
        changeAirRate(rate); // Change the data transfer rate
        sending = true;
        packetCount = 0;
        Serial.println("Beginning transmission.");
      } else if (command.equalsIgnoreCase("stop")) {
        sending = false;
        Serial.println("Ending transmission.");
        sendTotalPackets();
        delay(1000);
      }
    } else {
      Serial.print("Error receiving command: ");
      Serial.println(rc.status.getResponseDescription());
    }
  }

  // Send packets if transmission is active
  if (sending) {
    int force = random(0, 501); // Generate random data for force
    int temperature = random(-40, 81); // Generate random data for temperature
    int time = random(1, 3601); // Generate random data for time

    String message = "F=" + String(force) + "N, T=" + String(temperature) + "C, t=" + String(time) + "s";

    Serial.print("Sending packet: ");
    Serial.println(packetCount);
    ResponseStatus rs = e220ttl.sendMessage(message); // Send the message
    Serial.println(rs.getResponseDescription());
    if (rs.code != E220_SUCCESS) {
      Serial.print("Failed to send packet: ");
      Serial.println(rs.getResponseDescription());
    } else {
      Serial.println("Packet sent successfully.");
    }

    packetCount++;
    delay(5000); // Delay between packet transmissions

    if (packetCount >= 500) {
      sending = false;
      Serial.println("500 packets sent successfully.");
      sendTotalPackets();
      delay(500);
    }
  }
}

void sendTotalPackets() {
  String message = "Total packets sent: " + String(packetCount);
  Serial.println("Sending total packets message: " + message);
  ResponseStatus rs = e220ttl.sendMessage(message);
  if (rs.code != E220_SUCCESS) {
    Serial.print("Failed to send total packets message: ");
    Serial.println(rs.getResponseDescription());
  }
}

bool configureModule() {
  ResponseStructContainer c;
  c = e220ttl.getConfiguration(); // Retrieve the current configuration
  if (c.status.code == E220_SUCCESS) {
    Configuration configuration = *(Configuration*) c.data;
    Serial.println(c.status.getResponseDescription());

    configuration.ADDL = 0x00;
    configuration.ADDH = 0x00;
    configuration.CHAN = 23;
    configuration.SPED.airDataRate = AIR_DATA_RATE_010_24;
    configuration.SPED.uartBaudRate = UART_BPS_RATE_9600;
    configuration.SPED.uartParity = MODE_00_8N1;
    configuration.OPTION.transmissionPower = POWER_22;
    configuration.TRANSMISSION_MODE.enableRSSI = RSSI_ENABLED;

    ResponseStatus rs = e220ttl.setConfiguration(configuration, WRITE_CFG_PWR_DWN_SAVE); // Apply the new configuration
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

void changeAirRate(int rate) {
  ResponseStructContainer c;
  c = e220ttl.getConfiguration(); // Retrieve the current configuration to change the data transfer rate
  if (c.status.code == E220_SUCCESS) {
    Configuration configuration = *(Configuration*) c.data;

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
        return;
    }

    ResponseStatus rs = e220ttl.setConfiguration(configuration, WRITE_CFG_PWR_DWN_SAVE); // Apply the new data transfer rate
    if (rs.code != E220_SUCCESS) {
      Serial.print("Error setting new air rate: ");
      Serial.println(rs.getResponseDescription());
    } else {
      Serial.print("Air rate set to: ");
      Serial.println(rate);
    }
    c.close();
  } else {
    Serial.print("Error retrieving configuration for air rate change: ");
    Serial.println(c.status.getResponseDescription());
  }
}
