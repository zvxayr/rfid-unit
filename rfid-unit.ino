#include <SPI.h>
#include <MFRC522.h>

constexpr uint8_t RST_PIN = 9;
constexpr uint8_t SS_PIN = 10;

MFRC522 rfid(SS_PIN, RST_PIN); // Instance of the class

MFRC522::MIFARE_Key key;

// Init array that will store new NUID 
byte nuidPICC[4];

// possible states
const int NONE   = 0; // wait for new card
const int ALL    = 1; // card not recognized
const int GREEN  = 2; // card accepted
const int YELLOW = 3; // wait for response
const int RED    = 4; // card rejected

int state = 0;

// constants
const int forgetDuration = 30 * 1000;
const int flickrDuration = 50;

// variables
unsigned long flicker  = 0;
unsigned long previous = 0;

void setup() {
  Serial.begin(9600);
  SPI.begin(); // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522 

  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }
}

void loop() {
  unsigned long current = millis();

  // Turn lights off is time exceeds forgetDuration
  if (state != NONE && current - previous >= forgetDuration)
    lightState(NONE, current);
  else
    lightState(state, current);
  
  // Look for new cards
  if ( ! rfid.PICC_IsNewCardPresent()) return;

  // Verify if the NUID has been readed
  if ( ! rfid.PICC_ReadCardSerial()) return;

  // set last interaction to previous
  previous = current;

  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  
  // Check is the PICC of Classic MIFARE type
  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&  
    piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
    piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
    state = ALL;
    return;
  }

  if (state == NONE ||
    rfid.uid.uidByte[0] != nuidPICC[0] || 
    rfid.uid.uidByte[1] != nuidPICC[1] || 
    rfid.uid.uidByte[2] != nuidPICC[2] || 
    rfid.uid.uidByte[3] != nuidPICC[3]) {

    // Store NUID into nuidPICC array
    for (byte i = 0; i < 4; i++) {
      nuidPICC[i] = rfid.uid.uidByte[i];
    }
    
    lightState(YELLOW, current);
    printHex(rfid.uid.uidByte, rfid.uid.size);
    Serial.println();

    while( ! Serial.available() );
    
    String action = Serial.readStringUntil('\n');

    if (action.equals("unknown")) {
      state = ALL;
    } else if (action.equals("accept")) {
      state = GREEN;
    } else {
      state = RED;
    }
  }
  else {
    flicker = current;
  }
  
  // Halt PICC
  rfid.PICC_HaltA();

  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();
}

void lightState(int newState, unsigned long current) {
  state = newState;

  if (current - flicker <= flickrDuration || state == NONE) {
    for (int pin = 2; pin < 5; pin++) analogWrite(pin, 0);
    return;
  }

  if (state == ALL) {
    for (int pin = 2; pin < 5; pin++) analogWrite(pin, 128);
    return;
  }
  
  for (int pin = 2; pin < 5; pin++) analogWrite(pin, 0);
  if (state >= 2 && state <= 4) {
    analogWrite(state, 128);
    return;
  }
}

/**
 * Helper routine to dump a byte array as hex values to Serial. 
 */
void printHex(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? "0" : "");
    Serial.print(buffer[i], HEX);
  }
}
