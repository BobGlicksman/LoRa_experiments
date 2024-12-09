/*
    tpp_LoRa.h - routines for communication with the LoRa module
    created by Bob Glicksman and Jim Schrempp 2024
    as part of Team Practical Projects (tpp)

*/

//xxx #include "Particle.h"
#include "tpp_LoRa.h"

#define TPP_LORA_DEBUG 0  // Do NOT enable this for ATmega328

bool mg_LoRaBusy = false;

String tempString; // xxx


void tpp_LoRa::debugPrint(const String& message) {
    #if TPP_LORA_DEBUG 
        tempString = F("tpp_LoRa: ");
        tempString += message;
        Serial.print(tempString);
    #endif
}
void tpp_LoRa::debugPrintNoHeader(const String& message) {
    #if TPP_LORA_DEBUG
        Serial.print(message);
    #endif
}
void tpp_LoRa::debugPrintln(const String& message) {
    #if TPP_LORA_DEBUG
        tempString = F("tpp_LoRa: ");
        tempString += message;
        Serial.println(tempString);
    #endif
}

void tpp_LoRa::clearClassVariabels() {
    LoRaStringBuffer = "";
    receivedData = "";
    loraStatus = "";
    deviceNum = "";
    payload = "";
    RSSI = "";
    SNR = "";
    receivedMessageState = 0;
}   

// Do some class initialization stuff
// and make sure LoRa will respond
int tpp_LoRa::begin() {
    LoRaStringBuffer.reserve(200);  // reserve some space for the LoRa string buffer so it is not constantly reallocating
    UID.reserve(5);
    parameters.reserve(50);
    receivedData.reserve(200);
    loraStatus.reserve(50);
    deviceNum.reserve(5);
    payload.reserve(100);
    RSSI.reserve(5);
    SNR.reserve(5);
    clearClassVariabels();
    tempString.reserve(50); // xxx

    LORA_SERIAL.begin(38400);

    bool error = false;
    // check that LoRa is ready
    LoRaStringBuffer = F("AT");
    if(sendCommand(LoRaStringBuffer) != 0) {
        debugPrintln(F("LoRa reply bad, trying again"));
        delay(1000);

        if(sendCommand(LoRaStringBuffer) != 0) { // try again for photon 1
            debugPrintln(F("LoRa is not ready"));
            error = true;
        } 
    }

    return error;

}


// Configure the LoRa module with settings 
// rtn True if successful

bool tpp_LoRa::configDevice(int deviceAddress) {

    debugPrintln(F("LoRa is ready"));
    bool error = false;

    LoRaStringBuffer = F("AT+NETWORKID=");
    LoRaStringBuffer += String(LoRaNETWORK_NUM);
    // Set the network number
    if(sendCommand(LoRaStringBuffer) != 0) {
            debugPrintln(F("Network ID not set"));
            error = true;
    } else {
        LoRaStringBuffer = F("AT+ADDRESS=");
        LoRaStringBuffer += String(deviceAddress);
        if(sendCommand(LoRaStringBuffer) != 0) {
            debugPrintln(F("Device number not set"));
            error = true;
        } else {
            LoRaStringBuffer = F("AT+PARAMETER=");
            LoRaStringBuffer += String(LoRaSPREADING_FACTOR);
            LoRaStringBuffer += F(",");
            LoRaStringBuffer += String(LoRaBANDWIDTH);
            LoRaStringBuffer += F(",");
            LoRaStringBuffer += String(LoRaCODING_RATE);
            LoRaStringBuffer += F(",");
            LoRaStringBuffer += String(LoRaPREAMBLE);
            if(sendCommand(LoRaStringBuffer) != 0) {
                debugPrintln(F("Parameters not set"));
                error = true;
            } else  {
                LoRaStringBuffer = F("AT+MODE=0");
                if (sendCommand(LoRaStringBuffer) != 0) {
                    debugPrintln(F("Tranciever mode not set"));
                    error = true;
                } else {
                    LoRaStringBuffer = F("AT+BAND=915000000");
                        if (sendCommand(LoRaStringBuffer) != 0) {
                        debugPrintln(F("Band not set"));
                        error = true;
                    } else { 
                        LoRaStringBuffer = F("AT+CRFOP=22");
                        if (sendCommand(LoRaStringBuffer) != 0) {
                            debugPrintln(F("Power not set"));
                            error = true;
                        } else {
                            debugPrintln(F("LoRo module is initialized"));
                        }
                    }
                }
            }
        }
    }
    
    thisDeviceNetworkID = deviceAddress; 

    return error;

}

// Read current settings and print them to the serial monitor
//  If error then the D7 will blink twice
bool tpp_LoRa::readSettings() {
    // READ LoRa Settings
    LoRaStringBuffer = F("\r\n\r\n-----------------\r\nReading back the settings");
    debugPrintln(LoRaStringBuffer);

    bool error = false;

    if(sendCommand(F("AT+UID?")) != 0) {
        debugPrintln(F("error reading UID"));
        error = true;
    } else {
        UID = receivedData.substring(5, receivedData.length());
        UID.trim();
    }
    
    if(sendCommand(F("AT+CRFOP?")) != 0) {
        debugPrintln(F("error reading radio power"));
        error = true;
    } else { 
        if (sendCommand(F("AT+NETWORKID?")) != 0) {
            debugPrintln(F("error reading network id"));
            error = true;
        } else  { 
            if(sendCommand(F("AT+ADDRESS?")) != 0) {
                debugPrintln(F("error reading device address"));
                error = true;
            } else {  
                if(sendCommand(F("AT+PARAMETER?")) != 0) {
                    debugPrintln(F("error reading parameters"));
                    error = true;
                } else {
                    // replace commas with colons in the parameters string
                    tempString = F("[");
                    parameters.replace(F(","), F(":"));
                    parameters.trim();  // xxx
                    tempString += parameters;
                    tempString += F("]");
                    parameters = tempString;
                }
            }
        }
    }

    return error;
}

// function to send AT commands to the LoRa module
// returns 0 if successful, 1 if error, -1 if no response
// prints message and result to the serial monitor
int tpp_LoRa::sendCommand(const String& command) {

    if (mg_LoRaBusy) {
        debugPrintln(F("LoRa is busy"));
        return 1;
    }   
    mg_LoRaBusy = true;

    int retcode = 0;
    unsigned int timeoutMS = 1000; // xxx
    receivedData = "";

    tempString = F("\n\rcmd: ");
    tempString += command;
    debugPrintln(tempString);
    LORA_SERIAL.println(command);
    
    // wait for data available, which should be +OK or +ERR
    unsigned int starttimeMS = millis();  // xxx
    int dataAvailable = 0;
    debugPrint(F("waiting "));
    do {
        dataAvailable = LORA_SERIAL.available();
        delay(10);
        debugPrintNoHeader(F("."));
    } while ((dataAvailable == 0) && (millis() - starttimeMS < timeoutMS)) ;
    debugPrintNoHeader(F("\n"));

    delay(100); // wait for the full response

    // Get the response if there is one
    if(dataAvailable > 0) {

        receivedData = LORA_SERIAL.readString();
        // received data has a newline at the end
        receivedData.trim();
        tempString = F("received data = ");
        tempString += receivedData;
        debugPrintln(tempString);
        if(receivedData.indexOf(F("+ERR")) >= 0) {
            debugPrintln(F("LoRa error"));
            retcode = 1;
        } else {
            debugPrintln(F("command worked"));
            retcode = 0;
        }
    } else {
        debugPrintln(F("No response from LoRa"));
        retcode =  -1;
    }
    mg_LoRaBusy = false;
    return retcode;
};

// function to transmit a message to another LoRa device
// returns 0 if successful, 1 if error, -1 if no response
// prints message and result to the serial monitor
int tpp_LoRa::transmitMessage(const String& devAddress, const String& message){

    LoRaStringBuffer = F("AT+SEND=");
    LoRaStringBuffer += devAddress;
    LoRaStringBuffer += F(",");
    LoRaStringBuffer += String(message.length());
    LoRaStringBuffer += F(",");
    LoRaStringBuffer += message;

    return sendCommand(LoRaStringBuffer);

}


// If there is data on Serial1 then read it and parse it into the class variables. 
// Set receivedMessageState to 1 if successful, 0 if no message, -1 if error
// If there is no data on Serial1 then clear the class variables.
void tpp_LoRa::checkForReceivedMessage() {

    if (mg_LoRaBusy) {
        debugPrintln(F("LoRa is busy"));
        receivedMessageState = 0;
        return;
    }   
    mg_LoRaBusy = true;

    clearClassVariabels();

    if(LORA_SERIAL.available()) { // data is in the Serial1 buffer

        debugPrintln(F("\n\r--------------------"));
        delay(100); // wait a bit for the complete message to have been received
        receivedData = LORA_SERIAL.readString();
        // received data has a newline at the end
        receivedData.trim();
        tempString = F("received data = ");
        tempString += receivedData;
        debugPrintln(tempString);

        if ((receivedData.indexOf(F("+OK")) == 0) && receivedData.length() == 3) {

            // this is the normal OK from LoRa that the previous command succeeded
            debugPrintln(F("received data is +OK"));
            receivedMessageState = 1;

        } else {

            if (receivedData.indexOf(F("+RCV")) < 0) {
                // We are expecting a +RCV message
                debugPrintln(F("received data is not +RCV"));
                receivedMessageState = -1;
            } else {
                // find the commas in received data
                unsigned int commas[5];
                bool commaCountError = false;   

                // find first comma
                for(unsigned int i = 0; i < receivedData.length(); i++) {
                    if(receivedData.charAt(i) == ',') {   
                        commas[0] = i;
                        break;
                    }
                }

                // find other commas from the end to the front
                int commaCount = 5;
                for(unsigned int i = receivedData.length()-1; i >= commas[0]; i--) {
                    if(receivedData.charAt(i) == ',') {
                        commaCount--;
                        if (commaCount < 1) {
                            // should never happen
                            debugPrintln(F("ERROR: received data from sensor has weird comma count"));
                            break;
                            commaCountError = true;
                        }   
                        commas[commaCount] = i;
                    }
                }
                
                if (commaCountError) {

                    // error in the received data
                    debugPrintln(F("ERROR: received data from sensor has odd comma count"));

                    receivedMessageState = -1;

                } else {
                    
                    // create substrings from received data
                    deviceNum = receivedData.substring(5, commas[0]);  // skip the "+RCV="
                    //charCount = receivedData.substring(commas[1] + 1, commas[2]);
                    payload = receivedData.substring(commas[2] + 1, commas[3]);
                    RSSI = receivedData.substring(commas[3] + 1, commas[4]);
                    SNR = receivedData.substring(commas[4] + 1, receivedData.length()); // -1 to remove the newline

                    receivedMessageState = 1;

                } // end of if (commaCount != 4) 
            } // end of if(receivedData.indexOf("+RCV") < 0)
        } // end of if ((receivedData.indexOf("+OK") == 0) && receivedData.length() == 5)

    } else {

        // no data in the Serial1 buffer
        clearClassVariabels();
    }

    mg_LoRaBusy = false;

    return;
}




