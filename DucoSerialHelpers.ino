//#################################### Duco Serial Helpers ##################################
//
//  https://github.com/arnemauer/Ducobox-ESPEasy-Plugin
//  http://arnemauer.nl/ducobox-gateway/
//#######################################################################################################

// define buffers, large, indeed. The entire datagram checksum will be checked at once
#define DUCO_SERIAL_BUFFER_SIZE 180 // duco networklist is max 150 chars
uint8_t duco_serial_buf[DUCO_SERIAL_BUFFER_SIZE];
unsigned int duco_serial_bytes_read = 0;

unsigned int duco_serial_stopwordCounter = 0; // 0x0d 0x20 0x20 0x0d 0x3e 0x20

//unsigned int duco_serial_rows[15]; // byte number where a new row starts
//uint8_t duco_serial_rowCounter = 0;

typedef enum {
    DUCO_MESSAGE_END = 1, // status after receiving the end of a message (0x0d 0x20 0x20 )
    //P153_MESSAGE_END_STEP2 = 2,  //step 2 (0x0d 0x3e 0x20)
    DUCO_MESSAGE_ROW_END = 2, // end of a row
    DUCO_MESSAGE_TIMEOUT = 3,
    DUCO_MESSAGE_ARRAY_OVERFLOW = 4,

} DucoSerialMessageStatus;

/**
 * Send an updated value to the various controllers.
 *
 * This works arounds the limitation of not being able to specify separate IDX per value.
 *
 * @param logPrefix         Prefix to use when creating log messages
 * @param TaskIndex         Task Index
 * @param BaseVarIndex      BaseVarIndex for UserVar
 * @param relIndex          Value's index relative from BaseVarIndex
 * @param IDX               IDX of the value for the controller
 * @param valueDecimals     Number of decimals for the value
 *
 * See also:  https://www.letscontrolit.com/forum/viewtopic.php?f=4&t=2104&p=9908&hilit=no+svalue#p9908
 */
void DucoSerialSendUpdate(String logPrefix, byte TaskIndex, byte BaseVarIndex, byte relIndex, int IDX, int valueDecimals)
{
    if (IDX == 0) {
        /*
         * Sending IDX 0 is not supported and most likely means the user has
         * not configured this value; do not send an update.
         */
        return;
    }

    LoadTaskSettings(TaskIndex);   
    ExtraTaskSettings.TaskDeviceValueDecimals[0] = valueDecimals;

// backward compatibility for version 148 and lower...
#if defined(BUILD) && BUILD <= 150
    #define CONTROLLER_MAX 0
#endif

    for (byte x=0; x < CONTROLLER_MAX; x++){
        byte ControllerIndex = x;

        if (Settings.ControllerEnabled[ControllerIndex] && Settings.Protocol[ControllerIndex])
        {
            byte ProtocolIndex = getProtocolIndex(Settings.Protocol[ControllerIndex]);
            struct EventStruct TempEvent;
            TempEvent.TaskIndex = TaskIndex;
            TempEvent.ControllerIndex = ControllerIndex;
            TempEvent.BaseVarIndex = BaseVarIndex + relIndex; // ophogen per verzending.
            TempEvent.idx = IDX;
            TempEvent.ProtocolIndex = ProtocolIndex;
            TempEvent.sensorType = SENSOR_TYPE_SINGLE;

            /*
             * Controllers like domoticz HTTP do not use UserVar[BaseVarIndex + relIndex] but instead
             * use UserVar[BaseVarIndex + 0] for SENSOR_TYPE_SINGLE; temporarily overwrite this value
             * to make sure we send the correct sensor reading.
             */
            float oldValue = UserVar[BaseVarIndex];
            UserVar[BaseVarIndex] = UserVar[BaseVarIndex + relIndex];
            CPluginCall(TempEvent.ProtocolIndex, CPLUGIN_PROTOCOL_SEND, &TempEvent, dummyString);
            UserVar[BaseVarIndex] = oldValue;

            addLog(LOG_LEVEL_DEBUG, logPrefix + "Variable #" + (int) relIndex + " updated for controller " + (int)ControllerIndex);
        }
    }
}

int DucoSerialSendCommand(String logPrefix, const char *command)
{
    bool error=false;

    // DUCO throws random debug info on the serial, lets flush input buffer.
    DucoSerialFlush();

    for (unsigned m = 0; m < strlen(command); m++) {
       delay(10); // 20ms = buffer overrun while reading serial data. // ducosoftware uses +-15ms.
       int bytesSend = Serial.write(command[m]);
       if(bytesSend != 1){
           error=true;
           break;
        }
    } // end of forloop

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
   // empty buffer
        long start = millis();
        while ((millis() - start) < 200) {
            while(Serial.available() > 0){
               Serial.read();
            }
        }
}  

// dont use this function if the serial messages is bigger than 1000 characters!
uint8_t DucoSerialReceiveRow(String logPrefix, long timeout, bool verbose)
{
    long start = millis();
    duco_serial_stopwordCounter = 0;
    duco_serial_bytes_read = 0; // reset bytes read counter
    uint8_t status = 0;

    // Handle RESPONSE, read serial till we received the stopword or a timeout occured.
    while (((millis() - start) < timeout) && (status < 1) && (duco_serial_bytes_read < DUCO_SERIAL_BUFFER_SIZE) ) {
       if (Serial.available() > 0) {
            duco_serial_buf[duco_serial_bytes_read] = Serial.read();

            if (duco_serial_buf[duco_serial_bytes_read] == 0x0d){
                status = DUCO_MESSAGE_ROW_END;
            }    

            if(duco_serial_bytes_read == 1){
                if( (duco_serial_buf[0] == 0x3e) && (duco_serial_buf[1] == 0x20) ){  // 0x0d 0x20 0x20 0x0d 0x3e 0x20
                    status = DUCO_MESSAGE_END;
                }
            }

            duco_serial_bytes_read++;
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


bool DucoSerialCheckCommandInResponse(String logPrefix, const char* command, bool verbose)
{
    // Ducobox returns the command, lets check if the command matches 
    if (strlen(command) <= duco_serial_bytes_read &&
        strncmp(command, (char*) duco_serial_buf, strlen(command)) == 0) {
        if (verbose) {
            addLog(LOG_LEVEL_DEBUG, logPrefix + "Expected command received.");
        }
        return true;
    }else{
        if (verbose) {
            addLog(LOG_LEVEL_DEBUG, logPrefix + "Unexpected command received.");
        }
        return false;
    }
}


int DucoSerialLogArray(String logPrefix, uint8_t array[], int len, int fromByte)
{
   unsigned int counter = 1;

  String logstring = logPrefix + F("Pakket ontvangen: ");
  char lossebyte[6];

  for (unsigned int i = fromByte; i <= len - 1; i++)
  {

    sprintf_P(lossebyte, PSTR("%02X"), array[i]);
    logstring += lossebyte;
    if (((counter * 60) + fromByte) == i)
    {
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