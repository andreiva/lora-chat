/*
   RadioLib SX127x Transmit Example

   This example transmits packets using SX1278 LoRa radio module.
   Each packet contains up to 256 bytes of data, in the form of:
    - Arduino String
    - null-terminated char array (C-string)
    - arbitrary binary data (byte array)

   Other modules from SX127x/RFM9x family can also be used.

   For default module settings, see the wiki page
   https://github.com/jgromes/RadioLib/wiki/Default-configuration#sx127xrfm9x---lora-modem

   For full API reference, see the GitHub Pages
   https://jgromes.github.io/RadioLib/
*/

// include the library
#include <RadioLib.h>

#define CS      8
#define IRQ     7
#define RST     4

// MHz
float FREQUENCY = 433.0;
// Hz
float F_OFFSET = 1250 / 1e6;

// 2 - 20dBm
int8_t POWER = 10;

// 6 - 12
// higher is slower
uint8_t SPREADING_FACTOR = 8;

// 10.4, 15.6, 20.8, 31.25, 41.7, 62.5, 125, 250 and 500 kHz.
float BANDWIDTH = 20.8;

// 5 - 8
// high data rate / low range -> low data rate / high range
uint8_t CODING_RATE = 7;

// Module(RADIOLIB_PIN_TYPE cs, RADIOLIB_PIN_TYPE irq, RADIOLIB_PIN_TYPE rst);
SX1278 radio = new Module(CS, IRQ, RST);




// your name
String myname = "Morpheus: ";

// flag to indicate that a packet was received
volatile bool receivedFlag = false;

// disable interrupt when it's not needed
volatile bool enableInterrupt = true;

void setup() {
  Serial.begin(9600);
  Serial.print(F("[SX1278] Initializing ... "));

  int state = radio.begin();

  if (state == ERR_NONE) {
    radio.setFrequency(FREQUENCY + F_OFFSET);
    radio.setOutputPower(POWER);
    radio.setBandwidth(BANDWIDTH);
    radio.setSpreadingFactor(SPREADING_FACTOR);
    radio.setCodingRate(CODING_RATE);
    radio.setCRC(true);

    // set the function that will be called
    // when new packet is received
    radio.setDio0Action(setFlag);
    radio.startReceive();

    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true);
  }
}



// this function is called when a complete packet
// is received by the module
// IMPORTANT: this function MUST be 'void' type
// and MUST NOT have any arguments!
void setFlag(void) {
  // check if the interrupt is enabled
  if (!enableInterrupt) {
    return;
  }
  receivedFlag = true;
}

bool parseATcommand(String s) {

  //   AT+SETFR=433
  //   AT+SETSF=12
  //   AT+SETCR=5
  //   AT+SETBW=125
  //   AT+SETPW=10
  //   AT+SETRS      reset SX1278

  if (!s.startsWith(String("AT+SET"))) {
    return false;
  } else {
    Serial.println(s);
    radio.standby();

    String param = s.substring(6, 8);
    // remove prefixes
    s.remove(0, 9);

    if (param.equals("FR")) {
      float freq = s.toFloat();
      radio.setFrequency(freq);
      Serial.print("FREQUENCY "); Serial.println(freq);

    } else if (param.equals("SF")) {
      int sf = constrain(s.toInt(), 6, 12);
      radio.setSpreadingFactor(sf);
      Serial.print("SPREAD_FACTOR "); Serial.println(sf);

    } else if (param.equals("CR")) {
      int cr = constrain(s.toInt(), 5, 8);
      radio.setCodingRate(cr);
      Serial.print("CODING_RATE "); Serial.println(cr);

    } else if (param.equals("BW")) {
      float bw = s.toFloat();
      radio.setBandwidth(bw);
      Serial.print("BANDWIDTH "); Serial.println(bw);

    } else if (param.equals("PW")) {
      int power = constrain(s.toInt(), 2, 20);
      radio.setOutputPower(power);
      Serial.print("POWER "); Serial.println(power);
    } else if (param.equals("RS")) {
      radio.reset();
      radio.begin();
      Serial.print("RESET ");
    }
  }
  radio.startReceive();
  return true;
}


void loop() {

  if (receivedFlag)
    receive_package();


  // TX loop
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');

    if (input.length() > 0) {

      // if AT command then jump out
      if (parseATcommand(input)) {
        return;
      }

      // stop reception
      radio.standby();
      enableInterrupt = false;


      // TX
      String msg = myname + input;
      int state = radio.transmit(msg);

      if (state == ERR_NONE) {
        // print measured data rate
        Serial.print(msg);
        Serial.print(" ");
        Serial.print(radio.getDataRate());
        Serial.println(F(" bps"));
        input = "";

      } else if (state == ERR_PACKET_TOO_LONG) {
        // the supplied packet was longer than 256 bytes
        Serial.println(F("Tx too long!"));

      } else if (state == ERR_TX_TIMEOUT) {
        // timeout occurred while transmitting packet
        Serial.println(F("Tx timeout!"));

      } else {
        // some other error occurred
        Serial.print(F("Tx failed "));
        Serial.println(state);
      }
    }
    enableInterrupt = true;
    radio.startReceive();
  }
}

void receive_package() {
  // check if the flag is set
  if (receivedFlag) {
    // disable the interrupt service routine while
    // processing the data
    enableInterrupt = false;

    // reset flag
    receivedFlag = false;

    // you can read received data as an Arduino String
    String str = "";
    int state = radio.readData(str);

    // you can also read received data as byte array
    /*
      byte byteArr[8];
      int state = radio.readData(byteArr, 8);
    */

    if (state == ERR_NONE) {
      // print data of the packet
      Serial.print(str);

      // print RSSI (Received Signal Strength Indicator)
      Serial.print(F("\t| RSSI: "));
      Serial.print(radio.getRSSI());
      Serial.print(F("dBm "));

      // print SNR (Signal-to-Noise Ratio)
      Serial.print(F("SNR: "));
      Serial.print(radio.getSNR());
      Serial.print(F("dB "));

      // print frequency error
      Serial.print(F("Frq err: "));
      Serial.print(radio.getFrequencyError());
      Serial.println(F("Hz |"));

    } else if (state == ERR_CRC_MISMATCH) {
      // packet was received, but is malformed
      Serial.println(F("Rx CRC error!"));

    } else {
      // some other error occurred
      Serial.print(F("Rx failed "));
      Serial.println(state);

    }

    // put module back to listen mode
    radio.startReceive();

    // we're ready to receive more packets,
    // enable interrupt service routine
    enableInterrupt = true;
  }
}
