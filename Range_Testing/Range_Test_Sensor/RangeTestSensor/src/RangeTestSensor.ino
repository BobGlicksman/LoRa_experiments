/* 
 * Project RangeTestTx
 * Author: Bob Glicksman
 * Date: 4/25/24
 * 
 * Description:  This is code for a tester of LoRa signal range.  The tester is based upon
 *  a Particle Photon that is set up to not need Wifi (any Arduino can be used in its place).
 *  In addition to the Photon, a N.O. pushbutton switch is connected to ground at one end
 *  and Photon pin D0 on the other.  The LoRa module (RYLR998) is connected as follows:
 *  * Vcc to Photon 3.3v
 *  * GND to Photon GND
 *  * Tx to Photon Rx (Serial1)
 *  * Rx to Photon Tx (Serial1)
 *  * Reset is not connected
 * 
 * The testing concept is to produce a companion device - the "hub".  The hub uses its LoRa module
 *  to listen for a message from the tester.  When a message is received, the hub responds with a
 *  message of its own.  If the tester receives the response message, it is still in range of the
 *  hub.
 * 
 * The tester is assigned device number 0 and the hub is assigned device number 1.  The network
 *  number used for testing is 3 and the baud rate to/from the LoRa modem is 115200.  Otherwise,
 *  the default LoRa module values are used.  NOTE:  the tester code does not set up these
 *  values.  The LoRa modules are set up using a PC and an FTDI USB-serial board.
 * 
 * The software senses pressing of the pushbutton and sends a short message to a companion LoRa
 *  module in the hub.  The message is also printed on the Photon's USB serial port for debugging
 *  purposes.  The unit then waits 3 seconds for a response.  If a response is received 
 *  (data received from the hub, beginning with +RCV), the D7 LED is flashed three times.  If no
 *  data is received from the hub (distance too far), the D7 LED is flashed once.
 * 
 * version 1.0; 4/25/24
 */

#include "tpp_LoRaGlobals.h"

#include "tpp_loRa.h" // include the LoRa class

// The following system directives are to disregard WiFi for Particle devices.  Not needed for Arduino.
#if PARTICLEPHOTON
    SYSTEM_MODE(SEMI_AUTOMATIC);
    SYSTEM_THREAD(ENABLED);
    // Show system, cloud connectivity, and application logs over USB
    // View logs with CLI using 'particle serial monitor --follow'
    SerialLogHandler logHandler(LOG_LEVEL_INFO);
#endif

#define VERSION 1.00
#define STATION_NUM 0 // housekeeping; not used ini the code

#define THIS_LORA_SENSOR_ADDRESS 5 // the address of the sensor

//Jim's addresses
//#define THIS_LORA_SENSOR_ADDRESS 12648 // the address of the sensor LoRaSensor
//#define THIS_LORA_SENSOR_ADDRESS 11139 // the address of the sensor  lora3

tpp_LoRa LoRa; // create an instance of the LoRa class

// module global scope for mimimal string memboery allocation
String mglastRSSI;
String mglastSNR;
String mgpayload;
String mgTemp;

// all debug prints through here so it can be disabled when ATmega328 is used
void debugPrintln(const String& message) {
    #if PARTICLEPHOTON
        String tempString = F("tpp_LoRa: ");
        tempString += message;
        DEBUG_SERIAL.println(tempString);
    #endif
}

// blinkLED(): blinks the indicated LED "times" number of times
void blinkLED(int ledpin, int number, int delayTimeMS) {
    for(int i = 0; i < number; i++) {
        digitalWrite(ledpin, HIGH);
        delay(delayTimeMS);
        digitalWrite(ledpin, LOW);
        delay(delayTimeMS);
    }
    return;
} // end of blinkLED()

void setup() {

    if (PARTICLEPHOTON) {
        pinMode(BUTTON_PIN, INPUT_PULLUP);
    } else {
        pinMode(BUTTON_PIN, INPUT);
    }                                 
    pinMode(GRN_LED_PIN, OUTPUT); 
    pinMode(RED_LED_PIN, OUTPUT); 

    digitalWrite(GRN_LED_PIN, HIGH);
    digitalWrite(RED_LED_PIN, HIGH);

    mglastRSSI.reserve(5);
    mglastSNR.reserve(5);
    mgpayload.reserve(50);
    mgTemp.reserve(50);

    #if PARTICLEPHOTON
        DEBUG_SERIAL.begin(9600); // the USB serial port
    #else
        // ATMega328 has only one serial port, so no debug serial port
    #endif

    int err = LoRa.begin();  // initialize the LoRa class
    if (err) {
        if (PARTICLEPHOTON) {
            debugPrintln(F("error initializing LoRa device - Stopping"));
        }
        while(1) {blinkLED(RED_LED_PIN, 500, 20);};
    }


    if (PARTICLEPHOTON) {
        if (LoRa.configDevice(THIS_LORA_SENSOR_ADDRESS) != 0) {  // initialize the LoRa device
                debugPrintln(F("error configuring LoRa device - Stopping"));
                debugPrintln(F("hint: did you set THIS_LORA_SENSOR_ADDRESS?"));
            while(1) {blinkLED(RED_LED_PIN, 500, 50);};
        }; 

        if (LoRa.readSettings() != 0) {  // read the settings from the LoRa device
            debugPrintln(F("error reading LoRa settings - Stopping"));
            while(1) {blinkLED(RED_LED_PIN, 500, 75);};
        }; 
    }
    
    debugPrintln(F("Sensor ready for testing ...\n" ));   
    
    digitalWrite(GRN_LED_PIN, LOW);
    digitalWrite(RED_LED_PIN, LOW);

} // end of setup()


void loop() {

    static bool awaitingResponse = false; // when waiting for a response from the hub
    static unsigned long startTime = 0;
    static int msgNum = 0;

  // test for button to be pressed and no transmission in progress
    if(digitalRead(BUTTON_PIN) == LOW && !awaitingResponse) { // button press detected  // xxx
        digitalWrite(GRN_LED_PIN, HIGH);
        debugPrintln(F("\n\r--------------------")); 
        msgNum++;
        mgpayload = F("");
        switch (msgNum) {
            case 1:
                mgpayload = F("HELLO m: ");
                mgpayload += String(msgNum);
                mgpayload += F(" uid: ");
                mgpayload += LoRa.UID;
                break;
            case 2:
                mgpayload = F("HELLO m: ");
                mgpayload += String(msgNum);
                mgpayload += F(" p: ");
                mgpayload += LoRa.parameters;
                break;
            default:
                mgpayload = F("HELLO m: ");
                mgpayload += String(msgNum);
                mgpayload += F(" rssi: ");
                mgpayload += mglastRSSI;
                mgpayload += F(" snr: ");
                mgpayload += mglastSNR;
                break;
        }
        LoRa.transmitMessage(String(TPP_LORA_HUB_ADDRESS), mgpayload);
        awaitingResponse = true;
        startTime = millis();
        digitalWrite(GRN_LED_PIN, LOW);
    }


    if(awaitingResponse) {

        if (millis() - startTime > 5000 ) { // wait 5 seconds for a response from the hub
            awaitingResponse = false;  // timed out
            blinkLED(RED_LED_PIN, 1, 250);
            debugPrintln(F("timeout waiting for hub response"));
        }
        LoRa.checkForReceivedMessage();
        switch (LoRa.receivedMessageState) {
            case -1: // error
                awaitingResponse = false;  // error
                blinkLED(RED_LED_PIN, 7, 250);
                debugPrintln(F("error while waiting for response"));
                break;
            case 0: // no message
                delay(5); // wait a little while before checking again
                break;
            case 1: // message received
                mgTemp = F("received data = ");
                mgTemp += LoRa.receivedData;
                debugPrintln(mgTemp);
                mglastRSSI = LoRa.RSSI;
                mglastSNR = LoRa.SNR;

                // test for received data from the hub (denoted by "+RCV")
                if(LoRa.receivedData.indexOf(F("+RCV")) >= 0) { // will be -1 of "+RCV" not in the string
                    
                    awaitingResponse = false; // we got a response
                    debugPrintln(F("response received"));

                    if (LoRa.receivedData.indexOf(F("TESTOK")) >= 0) {
                        debugPrintln(F("response is TESTOK"));
                        blinkLED(GRN_LED_PIN, 3, 150);
                    } else if (LoRa.receivedData.indexOf(F("NOPE")) >= 0) {
                        debugPrintln(F("response is NOPE"));
                        blinkLED(GRN_LED_PIN, 4, 250);
                    } else {
                        debugPrintln(F("response is unrecognized"));
                        blinkLED(RED_LED_PIN, 5, 250);
                    }
                } 
                break;

        } // end of if(Serial1.available())

    } // end of if(awaitingResponse)

    //while(digitalRead(D0) == LOW); // wait for button to be released
    //delay(1); // wait a little while before sampling the button again

} // end of loop()

