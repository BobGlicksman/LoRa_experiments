/* 
 * Project RangeTestHub
 * Author: Bob Glicksman
 * Date: 4/25/24
 * 
 * Description:  This code is for the hub, the code 
 *  for the sensor is in a companion folder, range test.  The hub is based upona Particle Photon 
 *  The LoRa module (RYLR998) is connected as follows:
 *  * Vcc to Photon 3.3v
 *  * GND to Photon GND
 *  * Tx to Photon Rx (Serial1)
 *  * Rx to Photon Tx (Serial1)
 *  * Reset is not connected
 * 
 * The hub uses its LoRa module to listen for a message from the sensor.  When a message is 
 *  received the hub responds with a message of its own. If the message contains "HELLO" then
 *  the hub's response is "TESTOK".  If the message does not contain "HELLO" then the hub's
 *  response is "NOPE".  The hub echos its LoRa communications to the USB serial port for debugging 
 * 
 * The sensor is assigned any device number and the hub is assigned device number 57248 (an arbitrary
 *  choice in the range  0 - 65535). See the tpp_LoRa.h file.  The network
 *  number used for testing is 18 and the baud rate to/from the LoRa modem is 38400.  Otherwise,
 *  the default LoRa module values are used.  NOTE: the hub code will set up these
 *  values in the LoRa module when it boots - with the exception of baud rate.  The LoRa modules 
 *  baud rate are set up using a laptop and an FTDI USB-serial board.
 * 
 * 
 * version 1.0; 4/25/24
 * ver 2.0 12/14/2024 
 *      now uses the same tpp_LoRa library as the sensor. 
 *      #define added to disable cloud logging LOG_TO_CLOUD. This is useful for continuous
 *      testing without filling up the cloud log.
 * ver 2.1 12/15/2024 
 *      Now uses the string defined in tpp_LoRa.h for the message from the sensor to the hub
 * ver 3.0  6/7/2025
 *      - added cloud variable for the current version to aid maintainability
 *      - added cloud function to simulate a sensor trip by generating a sensor trip
 *          event publication to the Particle cloud.  This function can be called from the
 *          Particle Console or from an app that uses the Particle REST API.  The app can be
 *          used to generated administrative messages, in addition to the Hub processing the
 *          LoRa sensor messages.
 */

#include "Particle.h"
#include "tpp_LoRa.h"

#define LOG_TO_CLOUD 1 // set to 1 to log to the cloud; 0 to not log to the cloud

// The following system directives are for Particle devices.  Not needed for Arduino.
SYSTEM_THREAD(ENABLED);
//SerialLogHandler logHandler(LOG_LEVEL_TRACE);

String VERSION = "3.0";

const int DEBUG_LED_PIN = D7;
const int LORA_ADDRESS_PIN = D0;   // Ground this pin to set the LoRa module address for a sensor at boot
                                   // Used to configure a LoRa module.

String NODATA = "NODATA";
tpp_LoRa LoRa;


void logToParticle(String message, int deviceNum, String payload, int SNRhub1, int RSSIHub1) {   
    // create a JSON string to send to the cloud
    String data = "message=" + message
        + "|deviceNum=" + String(deviceNum) + "|payload=" + payload 
        + "|SNRhub1=" + String(SNRhub1) + "|RSSIHub1=" + String(RSSIHub1);

    DEBUG_SERIAL.println("cloudLogging:" + data);
    long rtn = Particle.publish("LoRaHubLogging", data, PRIVATE);
    DEBUG_SERIAL.println("cloudLogging return: " + String(rtn));
}

// Cloud function to generate a "simulated sensor" received message event to the Particle cloud
int simulatedSensor(String sensorNum) {
    int _deviceID = sensorNum.toInt();
    String msg = "Simulated_Sensor";

    logToParticle(msg, _deviceID, "dummyPayload", 0, 0);

    return 0;

}   // end of simulatedSensor()

void setup() {

    pinMode(DEBUG_LED_PIN, OUTPUT); // control the onboard LED
    pinMode(LORA_ADDRESS_PIN, INPUT_PULLUP); // used to set LoRa address on boot

    Particle.variable("Version", VERSION);
    Particle.function("SimSensor", simulatedSensor);

    digitalWrite(D7, HIGH);
    DEBUG_SERIAL.begin(9600); // the USB serial port 
    waitFor(DEBUG_SERIAL.isConnected, 15000);

    waitUntil(Particle.connected);  // wait for the cloud to connect
    DEBUG_SERIAL.println("Hub version: " + String(VERSION));

    int addressForLoRa = TPP_LORA_HUB_ADDRESS;
    int setAddressForHub = digitalRead(LORA_ADDRESS_PIN);
    if (setAddressForHub == LOW) {
        addressForLoRa = (rand() % 10) + 1;  // D0 is low, so set the address to 1
    } 

    if (LoRa.begin() != 0) {
        DEBUG_SERIAL.println("Error initializing LoRa device");
        while(1) {blinkTimes(50);};
        return;
    }

    if (LoRa.configDevice(addressForLoRa) != 0) {  // initialize the LoRa device 
        DEBUG_SERIAL.println("Error configuring LoRa device");
        while(1) {blinkTimes(50);};
        return;
    }
    
    if (LoRa.readSettings() != 0) {
        DEBUG_SERIAL.println("Error reading LoRa settings");
        blinkTimes(50);
        while(1) {blinkTimes(50);};
        return;
    }

    DEBUG_SERIAL.println("Hub ready for testing ...");
    DEBUG_SERIAL.print("waiting for data ...\n");

    digitalWrite(DEBUG_LED_PIN, LOW);

} // end of setup()


void loop() {

    
    static String receivedData = "";  // string to hold the received LoRa dat

    // wait for a message from the tester
    LoRa.checkForReceivedMessage();
    switch (LoRa.receivedMessageState) {
        case -1: // error
            DEBUG_SERIAL.println("Error reading data from LoRa module");
            DEBUG_SERIAL.println("Waiting for messages");
            break;
        case 0: // no message
            break;
        case 1: // message received
            String logMessage = "";
            String messageSent = "";
            long int deviceNum = LoRa.ReceivedDeviceAddress;
            digitalWrite(DEBUG_LED_PIN, HIGH);

            String debugMessage = "From device: " + String(deviceNum);
            debugMessage += " payload: " + LoRa.payload;
            DEBUG_SERIAL.println(debugMessage);

            int helloIndex = LoRa.payload.indexOf(TPP_LORA_MSG_GATE_SENSOR);
            if(helloIndex >= 0) { // will be -1 if "HELLO" not in the string

                // HELLO is the message from our sensors
                // send a message back to the sensor
                if (LoRa.transmitMessage(deviceNum, "TESTOK") == 0) {
                    logMessage = "TESTOK";
                    messageSent = "TESTOK";
                } else {
                    DEBUG_SERIAL.println("error sending TESTOK to sensor");
                    logMessage = "Send of TESTOK failed";
                };

            } else {

                DEBUG_SERIAL.println("received data does not start with a known character (see tpp_LoRa.h)");
                LoRa.transmitMessage(deviceNum, "NOPE");
                logMessage = "NOPE";
                messageSent = "NOPE";

            } // end of if(receivedData.indexOf("HELLO") > 0)


            DEBUG_SERIAL.println("sent message: " + messageSent);

            if (LOG_TO_CLOUD){
                // log the data to the cloud
                logToParticle(logMessage, LoRa.ReceivedDeviceAddress, LoRa.payload, LoRa.SNR, LoRa.RSSI);
            }

            digitalWrite(DEBUG_LED_PIN, LOW);
            DEBUG_SERIAL.println("Waiting for messages");

            break;
    } // end of switch

} // end of loop()

void blinkTimes(int number) {
    blinkTimes(number, 250);
}

void blinkTimes(int number, int delayTimeMS) {
    for(int i = 0; i < number; i++) {
        digitalWrite(DEBUG_LED_PIN, HIGH);
        delay(delayTimeMS);
        digitalWrite(DEBUG_LED_PIN, LOW);
        delay(delayTimeMS);
    }
    return;
} // end of blinkTimes()
