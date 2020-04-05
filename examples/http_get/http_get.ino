#include "A7ThinkerLib.h"
#define DEBUG 1

byte RX = 3;
byte TX = 2;
long debut = millis();

bool send = false;
bool init_print = false;
A7ThinkerLib *A7board = new A7ThinkerLib(RX, TX);

String apn="wap";
String host="edmi.ucad.sn";
String path="/~moussadiallo/test.php";

void setup() {
  Serial.begin(9600);
  A7board->begin(9600);
  // put your setup code here, to run once:
  A7board->blockUntilReady(9600);

  A7board->connectGPRS(apn);
  String rcvd_data=A7board->get(host,path);
  Serial.print("Response Length:\t");
  Serial.println(A7board->getResponseLength());
  A7board->getResponseData(rcvd_data);
  delay(1000);
}

void loop() {
  // put your main code here, to run repeatedly:
      // Relay things between Serial and the module's SoftSerial.
    while (A7board->A7->available() > 0) {
        Serial.write(A7board->A7->read());
    }
    while (Serial.available() > 0) {
        A7board->A7->write(Serial.read());
    } 

}
