

//************************************************************
// normal mode, sends and recs one byte, std messages
//
//************************************************************

#include <MCP23008.h>
#include <MCP2510.h>
#include <LiquidCrystal.h>
#include <Wire.h>
#include <SPI.h>
#include <Canutil.h>


MCP2510  can_dev(9); // defines pb0 (arduino pin8) as the _CS pin for MCP2510
LiquidCrystal lcd(15, 0, 14, 4, 5, 6, 7);  //  4 bits without R/W pin
MCP23008 i2c_io(MCP23008_ADDR);         // Init MCP23008
Canutil  canutil(can_dev);

uint8_t opmode, txstatus;
volatile int isInt;
uint8_t tosend[8];
uint8_t recSize, recData[8];
uint8_t push = 1;
uint16_t msgID = 0x2AB;
const int nb_nodes_max = 15;
int nb_nodes_activees = 0;
int my_node = 9;
uint8_t list_nodes[nb_nodes_max];
int compteur;
bool initialisation;
bool attente;
unsigned long temps1;
unsigned long temps2;

void setup() {
  pinMode(3, INPUT);
  i2c_io.Write(IOCON, 0x04);   // makes I2C interrupt pin open-drain

  Serial.begin(9600);
  attachInterrupt(0, somethingReceived, FALLING);  // int received on pin 2 if JMP16 is in position A

  lcd.begin(16, 2);
  lcd.clear();
  lcd.print("RX-TX ");
  lcd.print(msgID,HEX);
  opmode = canutil.whichOpMode();
  //lcd.setCursor(0,1);
  lcd.print(" mode ");
  lcd.print(opmode, DEC);

  canutil.setOpMode(OPMODE_CONFIG); // sets configuration mode
  canutil.waitOpMode(OPMODE_CONFIG);  // waits configuration mode

  canutil.flashRxbf();  //just for fun!


  //can.write(CANINTE,0x01);  //disables all interrupts but RX0IE (received message in RX buffer 0)
  can_dev.write(CANINTE, 0x03);
  can_dev.write(CANINTF, 0x00);  // Clears all interrupts flags
  canutil.setClkoutMode(CLKOUT_DISABLED, CLKOUT_DIV_1); // disables CLKOUT
  canutil.setTxnrtsPinMode(PIN_MODE_ALL_PURPOSE, PIN_MODE_ALL_PURPOSE, PIN_MODE_ALL_PURPOSE); // all TXnRTS pins as all-purpose digital input


  // Bit timing section
  //  setting the bit timing registers with Fosc = 16MHz -> Tosc = 62,5ns
  // data transfer = 125kHz -> bit time = 8us, we choose arbitrarily 8us = 16 TQ  (8 TQ <= bit time <= 25 TQ)
  // time quanta TQ = 2(BRP + 1) Tosc, so BRP =3
  // sync_seg = 1 TQ, we choose prop_seg = 2 TQ
  // Phase_seg1 = 7TQ yields a sampling point at 10 TQ (60% of bit length, recommended value)
  // phase_seg2 = 6 TQ SJSW <=4 TQ, SJSW = 1 TQ chosen
  can_dev.write(CNF1, 0x03); // SJW = 1, BRP = 3
  can_dev.write(CNF2, 0b10110001); //BLTMODE = 1, SAM = 0, PHSEG = 6, PRSEG = 1
  can_dev.write(CNF3, 0x05);  // WAKFIL = 0, PHSEG2 = 5

  // Settings for buffer RXB0
  //canutil.setRxOperatingMode(3, 1, 0);  // mask off  and rollover
  canutil.setRxOperatingMode(RXMODE_STDONLY, ROLLOVER_ENABLE, RX_BUFFER_0);  // standard ID messages only  and rollover
  canutil.setAcceptanceFilter(0x2AB, 2000, NORMAL_FRAME, RX_ACCEPT_FILTER_0); // 0 <= stdID <= 2047, 0 <= extID <= 262143, 1 = extended, filter# 0
  canutil.setAcceptanceFilter(0x2AC, 2001, NORMAL_FRAME, RX_ACCEPT_FILTER_1); // 0 <= stdID <= 2047, 0 <= extID <= 262143, 1 = extended, filter# 1
  canutil.setAcceptanceMask(0xFFFF, 0x00000000, RX_BUFFER_0); // 0 <= stdID <= 2047, 0 <= extID <= 262143, buffer# 0
  // in this case, only messages with ID equal to 0x2AB or 0x2AC will be accepted since mask is set to 0xFFF
  // for example, if mask is set to 0xFF0, all the message with ID beginning with 0x2A will be accepted


  // Settings for buffer RXB1
  canutil.setRxOperatingMode(RXMODE_STDONLY, ROLLOVER_ENABLE, RX_BUFFER_1);  // std  ID messages  rollover 
  canutil.setAcceptanceFilter(0x2AA, 2002, NORMAL_FRAME, RX_ACCEPT_FILTER_2); // 0 <= stdID <= 2047, 0 <= extID <= 262143, 
  canutil.setAcceptanceFilter(0x2AA, 2003, NORMAL_FRAME, RX_ACCEPT_FILTER_3); // 0 <= stdID <= 2047, 0 <= extID <= 262143,
  canutil.setAcceptanceFilter(0x2AA, 2004, NORMAL_FRAME, RX_ACCEPT_FILTER_4); // 0 <= stdID <= 2047, 0 <= extID <= 262143,
  canutil.setAcceptanceFilter(0x2AA, 2005, NORMAL_FRAME, RX_ACCEPT_FILTER_5);// 0 <= stdID <= 2047, 0 <= extID <= 262143,
  canutil.setAcceptanceMask(0xFFF, 0xFFFFFFFF, RX_BUFFER_1); // 0 <= stdID <= 2047, 0 <= extID <= 262143, 

  canutil.setOpMode(OPMODE_NORMAL); // sets normal mode
  opmode = canutil.whichOpMode();
  lcd.setCursor(15, 0);
  lcd.print(opmode, DEC);

  canutil.setTxBufferID(msgID, 2000, NORMAL_FRAME, TX_BUFFER_0); // TX standard messsages with buffer 0
  canutil.setTxBufferDataLength(SEND_DATA_FRAME, 1, TX_BUFFER_0); // TX normal data, 1 byte long, with buffer 0
  
 

  for (int i = 0; i < 8; i++) {
    tosend[i] = 0;
  }

  lcd.setCursor(0, 1);
  lcd.print("                ");


  isInt = 0;
  can_dev.write(CANINTF, 0x00);  // Clears all interrupts flags
  push = 0;

  initialisation = false;

   delay(1000);

}



void loop() {

  while (initialisation == false){
      if(){ // bouton SW9 appuyé
        Master();
      }
      else if (isInt == 1){
        reponse_Master();
      }
  }

  Serial.println("Normal Mode");


  /*if ( digitalRead(3) == 0 && push == 0) {
    sendMessage();
    push = 1;
  }


  if ( digitalRead(3) == 1) {
  push = 0;
}

 if (isInt==1){
  displayMessage();
 }*/


}






//******************************************************************
//                     other routines
//******************************************************************

void Master(){

  //delay(5000);
  //msgID = 0x100;
  canutil.setTxBufferID(msgID, 2000, NORMAL_FRAME, TX_BUFFER_0);
  canutil.setTxBufferDataLength(SEND_DATA_FRAME, 1, TX_BUFFER_0);
  Serial.println("initialisation trame");
  
  for(compteur = 1; compteur < nb_nodes_max + 1; compteur++){
    if(nb_nodes_activees < 8){
      if (compteur != my_node){
        
        // envoie
        tosend[0] = compteur;
        canutil.setTxBufferDataField(tosend, TX_BUFFER_0);
        canutil.messageTransmitRequest(TX_BUFFER_0, TX_REQUEST, TX_PRIORITY_HIGHEST);
        Serial.println("initialisation envoie");
        do {
          txstatus = canutil.isTxError(TX_BUFFER_0);  // checks tx error
          Serial.print("TX error = ");
          Serial.println(txstatus, DEC);
          txstatus = canutil.isArbitrationLoss(TX_BUFFER_0);  // checks for arbitration loss
          Serial.print("arb. loss = ");
          Serial.println(txstatus, DEC);
          txstatus = canutil.isMessageAborted(TX_BUFFER_0);  // ckecks for message abort
          Serial.print("TX abort = ");
          Serial.println(txstatus, DEC);
          txstatus = canutil.isMessagePending(TX_BUFFER_0);   // checks transmission
          Serial.print("mess. pending = ");
          Serial.println(txstatus, DEC);
          delay(500);
        }
        while (txstatus != 0);
  
        // réponse
        attente = true;
        temps1 = millis();
        temps2 = millis();
        Serial.println("avant reponse");
        while (attente == true && (temps2 - temps1) < 2000){
          if (isInt == 1){
            
            // vérifier contenu réception

            can_dev.write(CANINTF, 0x00);
            Serial.println("Ajout au tableau");
            list_nodes[nb_nodes_activees] = compteur;
            nb_nodes_activees++;
            isInt = 0;
            attente = false;
          }
          temps2 = millis();
        }
        Serial.println("apres reponse");
      }
      else if (compteur == my_node){
        list_nodes[nb_nodes_activees] = compteur;
        nb_nodes_activees++;
        Serial.println("c'est moi");
      }
    }
    Serial.println("");
  }
  initialisation = true;
}



void sendMessage() {
  tosend[0]++;
  canutil.setTxBufferDataField(tosend, TX_BUFFER_0);   // fills TX buffer
  //Serial.println("setTx");
  canutil.messageTransmitRequest(TX_BUFFER_0, TX_REQUEST, TX_PRIORITY_HIGHEST); // requests transmission of buffer 0 with highest priority
  //Serial.println("msgTx");

  do {
    txstatus = canutil.isTxError(TX_BUFFER_0);  // checks tx error
    Serial.print("TX error = ");
    Serial.println(txstatus, DEC);
    txstatus = canutil.isArbitrationLoss(TX_BUFFER_0);  // checks for arbitration loss
    Serial.print("arb. loss = ");
    Serial.println(txstatus, DEC);
    txstatus = canutil.isMessageAborted(TX_BUFFER_0);  // ckecks for message abort
    Serial.print("TX abort = ");
    Serial.println(txstatus, DEC);
    txstatus = canutil.isMessagePending(TX_BUFFER_0);   // checks transmission
    Serial.print("mess. pending = ");
    Serial.println(txstatus, DEC);
    delay(500);
  }
  while (txstatus != 0);

}


void reponse_Master(){
    can_dev.write(CANINTF, 0x00);
    recSize = canutil.whichRxDataLength(RX_BUFFER_0);
    for (int i = 0; i < recSize; i++) {
      recData[i] = canutil.receivedDataValue(RX_BUFFER_0, i);
    }
    if (recSize == 1 && recData[0] == my_node){
      tosend[0] = my_node;
      canutil.setTxBufferDataField(tosend, TX_BUFFER_0);
      canutil.messageTransmitRequest(TX_BUFFER_0, TX_REQUEST, TX_PRIORITY_HIGHEST);
      Serial.println("initialisation reponse");
      do {
        txstatus = canutil.isTxError(TX_BUFFER_0);  // checks tx error
        Serial.print("TX error = ");
        Serial.println(txstatus, DEC);
        txstatus = canutil.isArbitrationLoss(TX_BUFFER_0);  // checks for arbitration loss
        Serial.print("arb. loss = ");
        Serial.println(txstatus, DEC);
        txstatus = canutil.isMessageAborted(TX_BUFFER_0);  // ckecks for message abort
        Serial.print("TX abort = ");
        Serial.println(txstatus, DEC);
        txstatus = canutil.isMessagePending(TX_BUFFER_0);   // checks transmission
        Serial.print("mess. pending = ");
        Serial.println(txstatus, DEC);
        delay(500);
      }
      while (txstatus != 0);
      Serial.println("reponse envoyee");
    }
    else if (recSize != 1){
      for (compteur = 0; compteur < 8; compteur++){
        if (compteur < recSize){
          list_nodes[compteur] = recData[compteur];
        }
        else {
          list_nodes[compteur] = 0;
        }
      }
      initialisation = true;
   }
}


void displayMessage() {
  isInt = 0; // resets interrupt flag

  can_dev.write(CANINTF, 0x00);  // Clears all interrupts flags

  recSize = canutil.whichRxDataLength(RX_BUFFER_0); // checks the number of bytes received in buffer 0 (max = 8)

  for (int i = 0; i < recSize; i++) { // gets the bytes
    recData[i] = canutil.receivedDataValue(RX_BUFFER_0, i);
  }


  lcd.setCursor(0, 1);
  lcd.print("rec data =      ");
  lcd.setCursor(10, 1);
  lcd.print(recData[0], HEX);
  lcd.print(" Hex");
}
//************************************************
// routine attached to INT pin
//************************************************

void somethingReceived()
{
  isInt = 1;
}