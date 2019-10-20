//#################################### Duco Serial Helpers ##################################
//
//  https://github.com/arnemauer/Ducobox-ESPEasy-Plugin
//  http://arnemauer.nl/ducobox-gateway/
//#######################################################################################################

// define buffers, large, indeed. The entire datagram checksum will be checked at once
#define DUCO_SERIAL_BUFFER_SIZE 750
//#define NETBUF_SIZE 600
uint8_t duco_serial_buf[DUCO_SERIAL_BUFFER_SIZE];
unsigned int duco_serial_bytes_read = 0;

int duco_serial_stopwordCounter = 0; // 0x0d 0x20 0x20 0x0d 0x3e 0x20

unsigned int duco_serial_rows[15]; // byte number where a new row starts
unsigned int duco_serial_rowCounter = 0;

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

    for (int m = 0; m < strlen(command); m++) {
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

bool DucoSerialReceiveData(String logPrefix, long timeout, bool verbose)
{
    long start = millis();
    duco_serial_stopwordCounter = 0;
    duco_serial_bytes_read = 0; // reset bytes read counter
    duco_serial_rows[0] = 0; // row zero = command in answer from duco
    duco_serial_rowCounter = 1; 

    // Handle RESPONSE, read serial till we received the stopword or a timeout occured.
    while (((millis() - start) < timeout) && (duco_serial_stopwordCounter < 6)) {
        if (Serial.available() > 0) {
            duco_serial_buf[duco_serial_bytes_read] = Serial.read();

            // check if there is a new row, 0x0d = new row.
            if(duco_serial_buf[duco_serial_bytes_read] == 0x0d){
                duco_serial_rows[duco_serial_rowCounter] = duco_serial_bytes_read;
                duco_serial_rowCounter++;
            }

            if ((duco_serial_buf[duco_serial_bytes_read] == 0x0d) || (duco_serial_buf[duco_serial_bytes_read] == 0x20) || (duco_serial_buf[duco_serial_bytes_read] == 0x3e)) {
                duco_serial_stopwordCounter++; // character of stopwoord found 
            }else{
                duco_serial_stopwordCounter = 0;
            }            
            duco_serial_bytes_read++;
        }

    }// end while

    if(duco_serial_stopwordCounter == 6){
        if (verbose) {
            addLog(LOG_LEVEL_INFO, logPrefix + "Package received with stopword.");
        }
        return true;
    }else{
        // timout occurred, stopword not found!
        if (verbose) {
            addLog(LOG_LEVEL_INFO, logPrefix + "Package receive timeout.");
        }
        DucoSerialFlush();
        return false;
    }

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
