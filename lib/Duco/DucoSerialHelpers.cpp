#include "DucoSerialHelpers.h"

//#################################### Duco Serial Helpers ##################################
//
//  https://github.com/arnemauer/Ducobox-ESPEasy-Plugin
//  http://arnemauer.nl/ducobox-gateway/
//#######################################################################################################


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
            addLog(LOG_LEVEL_ERROR, logPrefix + "Error, failed sending command. Clear command and buffer");

            // press "enter" to clear send bytes.
            Serial.write(0x0d);
            delay(25);
            Serial.write(0x0a);

            serialSendCommandInProgress = false;
            DucoTaskStopSerial(logPrefix);
        }else{
            if(serialSendCommandCurrentChar >= (strlen(serialSendCommand)-1) ){
                serialSendCommandInProgress = false;
                addLog(LOG_LEVEL_DEBUG, logPrefix + "Send command successfull!");
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
	addLog(LOG_LEVEL_DEBUG, logPrefix +  F("Stop reading from serial"));
}

void DucoThrowErrorMessage(String logPrefix, uint8_t messageStatus){
    switch(messageStatus){
        case DUCO_MESSAGE_END:
            addLog(LOG_LEVEL_DEBUG, logPrefix + "End of message received.");
        break;
        case DUCO_MESSAGE_TIMEOUT:
            addLog(LOG_LEVEL_DEBUG, logPrefix + "Serial message timeout.");
        break;
        case DUCO_MESSAGE_ARRAY_OVERFLOW:
            addLog(LOG_LEVEL_DEBUG, logPrefix + "Serial message array overflow.");
        break;
    }
    return;
}


bool DucoSerialCheckCommandInResponse(String logPrefix, const char* command)
{
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