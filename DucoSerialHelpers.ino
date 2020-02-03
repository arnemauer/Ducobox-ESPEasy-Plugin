//#################################### Duco Serial Helpers ##################################
//
//  https://github.com/arnemauer/Ducobox-ESPEasy-Plugin
//  http://arnemauer.nl/ducobox-gateway/
//#######################################################################################################

// define buffers, large, indeed. The entire datagram checksum will be checked at once
#define DUCO_SERIAL_BUFFER_SIZE 180 // duco networklist is max 150 chars
uint8_t duco_serial_buf[DUCO_SERIAL_BUFFER_SIZE];
unsigned int duco_serial_bytes_read = 0;
unsigned int duco_serial_rowCounter = 0; 

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


typedef enum {
    DUCO_MESSAGE_END = 1, // status after receiving the end of a message (0x0d 0x20 0x20 )
    DUCO_MESSAGE_ROW_END = 2, // end of a row
    DUCO_MESSAGE_TIMEOUT = 3,
    DUCO_MESSAGE_ARRAY_OVERFLOW = 4,
    DUCO_MESSAGE_FIFO_EMPTY = 5,

} DucoSerialMessageStatus;


int DucoSerialSendCommand(String logPrefix, const char *command)
{
    bool error=false;

    // DUCO throws random debug info on the serial, lets flush input buffer.
    //DucoSerialFlush();
    noInterrupts();
    for (unsigned m = 0; m < strlen(command); m++) {
       delay(10); // 20ms = buffer overrun while reading serial data. // ducosoftware uses +-15ms.
       int bytesSend = Serial.write(command[m]);
       if(bytesSend != 1){
           error=true;
           break;
        }
    } // end of forloop
    interrupts();

    // if error: log, clear command and flush buffer!
    if(error){
        addLog(LOG_LEVEL_ERROR, logPrefix + "Error, failed sending command. Clear command and buffer");

        // press "enter" to clear send bytes.
        Serial.write(0x0d);
        delay(20);
        Serial.write(0x0a);

       DucoSerialFlush();
        return false;

    }else{
        addLog(LOG_LEVEL_DEBUG, logPrefix + "Send command successfull!");
        return true;
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

    }
    return DUCO_MESSAGE_FIFO_EMPTY;

}



// dont use this function if the serial messages is bigger than 1000 characters!
uint8_t DucoSerialReceiveRow(String logPrefix, long timeout, bool verbose)
{
    long start = millis();
    uint8_t status = 0;

    // Handle RESPONSE, read serial till we received the stopword or a timeout occured.
    while (((millis() - start) < timeout) && (status < 1) && (duco_serial_bytes_read < DUCO_SERIAL_BUFFER_SIZE) ) {
       if (Serial.available() > 0) {
            duco_serial_buf[duco_serial_bytes_read] = Serial.read();


        }
    }// end while


    if(status == 0){ // if status isnt DUCO_MESSAGE_ROW_END OR DUCO_MESSAGE_END, check buffer size or timeout.
        if(duco_serial_bytes_read >= DUCO_SERIAL_BUFFER_SIZE){
            status = DUCO_MESSAGE_ARRAY_OVERFLOW;
        }else{
            // timout occurred
            if (verbose) {
                addLog(LOG_LEVEL_INFO, logPrefix + "Package receive timeout.");
            }
            status = DUCO_MESSAGE_TIMEOUT;
            DucoSerialFlush();
        }
    }
    
    return status;
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


int DucoSerialLogArray(String logPrefix, uint8_t array[], int len, int fromByte) {
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

        
       		return -1;


	}

}