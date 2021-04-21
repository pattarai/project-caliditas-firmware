#include <Adafruit_MLX90614.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <MFRC522.h>
#include <SPI.h>
#include <WiFiClient.h>
#include <Wire.h>

// Enter Device Credentials here
int dev_id = 1;
char *dev_pass = "licet";

// Enter Wifi Credentials here
const char *ssid = "Phantom";
const char *wifiPassword = "8754462663";

// Your device API url to POST data
// const char *serverName =
// "https://xstack-caliditas.herokuapp.com/api/device.php";
const char *serverName = "https://89fe95c88cc6.ngrok.io/api/nodecheck.php";

// Initialize temperature module
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

/*===========================
 RFID init starts here
===========================*/

constexpr uint8_t RST_PIN = 0; // Configurable, see typical pin layout above
constexpr uint8_t SS_PIN = 2;  // Configurable, see typical pin layout above

MFRC522 rfid(SS_PIN, RST_PIN); // Instance of the class

MFRC522::MIFARE_Key key;

byte nuidPICC[4]; // Init array that will store new NUID

/*===========================
 RFID init ends here
===========================*/

void setup() {
  // Set baud rate
  Serial.begin(9600);

  // Start connection
  WiFi.begin(ssid, wifiPassword);

  // Start Temperature sensor
  mlx.begin();

  Serial.println("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());

  /*===========================
    RFID setup starts here
  ===========================*/

  SPI.begin();     // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522

  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  Serial.println(F("This code scan the MIFARE Classsic NUID."));
  Serial.print(F("Using the following key:"));
  printHex(key.keyByte, MFRC522::MF_KEY_SIZE);

  /* ===========================
    RFID setup ends here
  =========================== */
}

void loop() {
  // Look for new cards
  if (!rfid.PICC_IsNewCardPresent()) {
    return;
  }

  // Verify if the NUID has been readed
  if (!rfid.PICC_ReadCardSerial()) {
    return;
  }

  // Get temperature from MLX sensor
  float temperature = mlx.readObjectTempF();
  temperature = temperature + 5; // Offset

  // Get registerNo from RFiD sensor
  int register_no = getRegisterNo();

  bool status = xStackHubPost(dev_id, dev_pass, register_no, temperature);

  // Post data to the API
  if (status) {
    Serial.print("SUCCESS");
    Serial.println();
  } else {
    Serial.print("FAIL");
    Serial.println();
  }
}

bool xStackHubPost(int devId, char *devPass, int registerNo, float temp) {
  // For storing the params in the string
  char buffer[100];
  sprintf(buffer, "dev_id=%d&dev_pass=%s&register_no=%d&temperature=%f", devId,
          devPass, registerNo, temp);
  // sprintf(buffer, "id=%d", registerNo);

  Serial.println(buffer);

  // Check WiFi connection status
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    // Your Domain name with URL path or IP address with path
    http.begin(serverName);

    // Specify content-type header
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    // Data to send with HTTP POST
    int httpResponseCode = http.POST(buffer);
    Serial.println();

    Serial.println("HTTP Response code: ");
    Serial.print(httpResponseCode);

    // Free resources
    http.end();

    if (httpResponseCode == -1) {
      // POST REQUEST FAILED
      return false;
    } else {
      // POST REQUEST IS SENT
      return true;
    }
  } else {
    Serial.println("WiFi Disconnected");
    return false;
  }
}

int getRegisterNo() {
  int regNo;
  // Using RFID sensor
  // Serial.print(F("PICC type: "));
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  // Serial.println(rfid.PICC_GetTypeName(piccType));

  // Check is the PICC of Classic MIFARE type
  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&
      piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
      piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
    Serial.println(F("Your tag is not of type MIFARE Classic."));
    return 0;
  }

  if (rfid.uid.uidByte[0] != nuidPICC[0] ||
      rfid.uid.uidByte[1] != nuidPICC[1] ||
      rfid.uid.uidByte[2] != nuidPICC[2] ||
      rfid.uid.uidByte[3] != nuidPICC[3]) {
    Serial.println(F("A new card has been detected."));

    // Store NUID into nuidPICC array
    for (byte i = 0; i < 4; i++) {
      nuidPICC[i] = rfid.uid.uidByte[i];
    }

    // Serial.println(F("The NUID tag is:"));
    // Serial.print(F("In dec: "));
    regNo = printDec(rfid.uid.uidByte, rfid.uid.size);
    Serial.println();
  } else
    Serial.println(F("Card read previously."));

  // Halt PICC
  rfid.PICC_HaltA();

  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();

  return regNo;
}

/*
 Helper routine to dump a byte array as hex values to Serial.
*/
void printHex(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

/*
 Helper routine to dump a byte array as dec values to Serial.
*/
int printDec(byte *buffer, byte bufferSize) {
  int reg = 0;
  int digit = 1;
  String bigBuffer = "";
  for (byte i = 0; i < bufferSize; i++) {
    // Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    // Serial.print(buffer[i], DEC);
    String temp = String(buffer[i], DEC);
    reg = reg + (temp.toInt()) * digit;
    bigBuffer = bigBuffer + temp;
    digit = digit * 10;
  }

  return bigBuffer.toInt();
  // return reg;
}