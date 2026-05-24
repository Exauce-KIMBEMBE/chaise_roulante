#include <IRremote.hpp>

#define IR_PIN 25

void setup() {
  Serial.begin(115200);

  IrReceiver.begin(IR_PIN, ENABLE_LED_FEEDBACK);

  Serial.println("Test recepteur IR VS1838B");
}

void loop() {

  if (IrReceiver.decode()) {

    Serial.println("=== Signal recu ===");

    // Code hexadecimal
    Serial.print("HEX : 0x");
    Serial.println(IrReceiver.decodedIRData.decodedRawData, HEX);

    // Protocole
    Serial.print("Protocole : ");
    Serial.println(getProtocolString(IrReceiver.decodedIRData.protocol));

    // Commande
    Serial.print("Commande : ");
    Serial.println(IrReceiver.decodedIRData.command, HEX);

    // Adresse
    Serial.print("Adresse : ");
    Serial.println(IrReceiver.decodedIRData.address, HEX);

    Serial.println();

    // Pret pour prochaine reception
    IrReceiver.resume();
  }
}


