//#################################### Duco Serial Helpers ##################################
//
//  https://github.com/arnemauer/Ducobox-ESPEasy-Plugin
//  http://arnemauer.nl/ducobox-gateway/
//#######################################################################################################

#include "DucoSerialHelpers.h"
#include <ESP8266WiFi.h>
#include "../../src/ESPEasy_common.h"
#include <stdint.h>
#include <Wire.h>
#include "../../src/src/ESPEasyCore/ESPEasy_Log.h"
#include "../../src/src/Helpers/StringConverter.h"

uint8_t duco_serial_buf[DUCO_SERIAL_BUFFER_SIZE];
unsigned int duco_serial_bytes_read = 0;
unsigned int duco_serial_rowCounter = 0; 

uint8_t serialSendCommandCurrentChar = 0;
bool serialSendCommandInProgress = false;
/* To support non-blocking code, each plugins checks if the serial port is in use by another task before 
claiming the serial port. After a task is done using the serial port the variable 'serialPortInUseByTask'
is set to a default value '255'. 

If espeasy calling 'PLUGIN_READ' from a task, it is possible that the serual port is in use.
If that is the case the task will set a flag for itself. In 'PLUGIN_ONCE_A_SECOND' the task will check if
there is a flag and if so, check if the serial port is in use. When the serial port isnt used by a 
plugin (serialPortInUseByTask=255) than it will call 'PLUGIN_READ' to start receiving data.
*/

uint8_t serialPortInUseByTask = 255; 
unsigned long ducoSerialStartReading; // filled with millis 
char serialSendCommand[30]; // sensorinfo, network, nodeparaget xx xx

void DucoSerialStartSendCommand(const char *command){
    safe_strncpy(serialSendCommand, command, sizeof(serialSendCommand));
	serialSendCommandCurrentChar = 0;
	serialSendCommandInProgress = true; //start sending the serial command (in 50)
    return;
}

// non blocking serial send function
// calling this function from 50persec. call this functions every 20ms :) 
void DucoSerialSendCommand(String logPrefix) {
    if(serialSendCommandCurrentChar == 0){
        serialSendCommandInProgress = true;
    }

    if(serialSendCommandCurrentChar <= strlen(serialSendCommand)-1){
        int bytesSend = Serial.write(serialSendCommand[serialSendCommandCurrentChar]);
        if(bytesSend != 1){
			if(loglevelActiveFor(LOG_LEVEL_ERROR)){
				String log = logPrefix;
				log += F("Error, failed sending command. Clear command and buffer");
				addLog(LOG_LEVEL_ERROR, log);
			}
            // press "enter" to clear send bytes.
            Serial.write(0x0d);
            delay(25);
            Serial.write(0x0a);

            serialSendCommandInProgress = false;
            DucoTaskStopSerial(logPrefix);
        }else{
            if(serialSendCommandCurrentChar >= (strlen(serialSendCommand)-1) ){
                serialSendCommandInProgress = false;
				if(loglevelActiveFor(LOG_LEVEL_DEBUG)){
					String log = logPrefix;
					log += F("Send command successfull!");
					addLog(LOG_LEVEL_ERROR, log);
				}
            }else{
                serialSendCommandCurrentChar++;
            }
        }

    }else{
        // counter is higher than number of chars
        serialSendCommandInProgress = false;
    }
} // end of DucoSerialSendCommand


void DucoSerialFlush()
{
   // empty buffer while serial.available > 0 and we are done within 20ms.
    long start = millis();
    while( (Serial.available() > 0) && ((millis() - start) < 20) ){
        Serial.read();
    }
}  

uint8_t DucoSerialInterrupt(){
    while (Serial.available() > 0) {
        delay(0);
        // check if there is space left in the buffer (duco_serial_buf)
        if(duco_serial_bytes_read < DUCO_SERIAL_BUFFER_SIZE){ 
            duco_serial_buf[duco_serial_bytes_read] = Serial.read();

            if (duco_serial_buf[duco_serial_bytes_read] == 0x0d){
                duco_serial_rowCounter++;
                return DUCO_MESSAGE_ROW_END;
            }    

            if(duco_serial_bytes_read == 1){
           		if( (duco_serial_buf[0] == 0x3e) && (duco_serial_buf[1] == 0x20) ){  // 0x0d 0x20 0x20 0x0d 0x3e 0x20
                    return DUCO_MESSAGE_END;
                }
            }

            duco_serial_bytes_read++;

        }else{
            // buffer is full, do we need an extra status for this? TODO!
            return DUCO_MESSAGE_END;
        }
    }
    return DUCO_MESSAGE_FIFO_EMPTY;
}


void DucoTaskStopSerial(String logPrefix){
	serialPortInUseByTask = 255;
	if(loglevelActiveFor(LOG_LEVEL_DEBUG)){
		String log = logPrefix;
		log += F("Stop reading from serial");
		addLog(LOG_LEVEL_DEBUG, log);
	}
}

void DucoThrowErrorMessage(String logPrefix, uint8_t messageStatus){
    switch(messageStatus){
        case DUCO_MESSAGE_END:{
			if(loglevelActiveFor(LOG_LEVEL_DEBUG)){
				String log = logPrefix;
				log += F("End of message received.");
				addLog(LOG_LEVEL_DEBUG, log);
			}
	        break;
        }
        case DUCO_MESSAGE_TIMEOUT:{
			if(loglevelActiveFor(LOG_LEVEL_DEBUG)){
				String log = logPrefix;
				log += F("Serial message timeout.");
				addLog(LOG_LEVEL_DEBUG, log);
			}
    	    break;
        }
        case DUCO_MESSAGE_ARRAY_OVERFLOW:{
			if(loglevelActiveFor(LOG_LEVEL_DEBUG)){
				String log = logPrefix;
				log += F("Serial message array overflow.");
				addLog(LOG_LEVEL_DEBUG, log);
			}
        	break;
        }
    }

    return;
}


bool DucoSerialCheckCommandInResponse(String logPrefix, const char* command){
    // Ducobox returns the command, lets check if the command matches 
    if (strlen(command) <= duco_serial_bytes_read &&
        strncmp(command, (char*) duco_serial_buf, strlen(command)) == 0) {
        return true;
    }else{
        return false;
    }
}


void DucoSerialLogArray(String logPrefix, uint8_t array[], unsigned int len, int fromByte) {
	unsigned int counter = 1;

	if(len > 0){
		String logstring = logPrefix + F("Pakket ontvangen: ");
		char lossebyte[6];

		for (unsigned int i = fromByte; i <= len - 1; i++){
			sprintf_P(lossebyte, PSTR("%02X"), array[i]); // hex output = %02X / ascii = %c
			//sprintf_P(lossebyte, PSTR("%c"), array[i]); // hex output = %02X / ascii = %c

			logstring += lossebyte;
			if (((counter * 50) + fromByte) == i){
				counter++;
				logstring += F(">");
				addLog(LOG_LEVEL_INFO, logstring);
				delay(50);
				logstring = F("");
			}
		}
		logstring += F(" END");
		addLog(LOG_LEVEL_INFO, logstring);   
	}
}