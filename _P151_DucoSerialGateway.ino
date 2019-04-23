//#################################### Plugin 151: DUCO Serial Gateway ##################################
//
//  DUCO Serial Gateway to read Ducobox data
//  https://github.com/arnemauer/Ducobox-ESPEasy-Plugin
//  http://arnemauer.nl/ducobox-gateway/
//#######################################################################################################


#define PLUGIN_151
#define PLUGIN_ID_151           151
#define PLUGIN_NAME_151         "DUCO serial Gateway"
#define PLUGIN_VALUENAME1_151   "FLOW_PERCENTAGE" // networkcommand
#define PLUGIN_VALUENAME2_151   "DUCO_STATUS" // networkcommand
#define PLUGIN_VALUENAME3_151   "CO2_PPM" // nodeparaget 2 74
#define PLUGIN_VALUENAME4_151   "CO2_TEMP" // // networkcommand
#define PLUGIN_READ_TIMEOUT_151   1200 // DUCOBOX askes "live" CO2 sensor info, so answer takes sometimes a second.

#define DUCOBOX_NODE_NUMBER     1
#define CO2_NODE_NUMBER         2

boolean Plugin_151_init = false;

            String loggy;


#define ANSWER_READ_NODE_SIZE 16
uint8_t answerReadNode[ANSWER_READ_NODE_SIZE] = {0x6e, 0x6f , 0x64 , 0x65 , 0x70 , 0x61 , 0x72 , 0x61 , 0x67 , 0x65 , 0x74 , 0x20 , 0x32 , 0x20 , 0x37 , 0x34};

// 9 bytes
#define CMD_READ_NETWORK_SIZE 9
uint8_t cmdReadNetwork[CMD_READ_NETWORK_SIZE] = {0x4e, 0x65, 0x74, 0x77, 0x6f, 0x72, 0x6b, 0x0d, 0x0a};
#define ANSWER_READ_NETWORK_SIZE 8
uint8_t answerReadNetwork[ANSWER_READ_NETWORK_SIZE] = {0x6e, 0x65, 0x74, 0x77, 0x6f, 0x72, 0x6b, 0x0d};

uint8_t ventilationColumnName[] = {0x25, 0x64, 0x62, 0x74}; // %dbt
uint8_t ducoboxStatusColumnName[] =  {0x73, 0x74, 0x61, 0x74}; // stat = 0x73, 0x74, 0x61, 0x74
uint8_t tempColumnName[] = {0x74, 0x65, 0x6d, 0x70};  //temp = 0x74, 0x65, 0x6d, 0x70

const uint8_t DucoStatusModes [13][5] = 
{
   {0, 0x41, 0x55, 0x54, 0x4f},    // 0 -> "auto" = AutomaticMode;
   {1, 0x4d, 0x41, 0x4e, 0x31},    // 1 -> "man1" = ManualMode1;
   {2, 0x4d, 0x41, 0x4e, 0x32},    // 2 -> "man2" = ManualMode2;
   {3, 0x4d, 0x41, 0x4e, 0x33},    // 3 -> "man3" = ManualMode3;
   {4, 0x45, 0x4d, 0x50, 0x54},    // 4 -> "empt" = EmptyHouse;
   {5, 0x41, 0x4c, 0x52, 0x4d},    // 5 -> "alrm" = AlarmMode;
   {11, 0x43, 0x4e, 0x54, 0x31},   // 11 -> "cnt1" = PermanentManualMode1;
   {12, 0x43, 0x4e, 0x54, 0x32},   // 12 -> "cnt2" = PermanentManualMode2;
   {13, 0x43, 0x4e, 0x54, 0x33},   // 13 -> "cnt3"= PermanentManualMode3;
   {20, 0x41, 0x55, 0x54, 0x30},   // 20 -> "aut0" = AutomaticMode;
   {21, 0x41, 0x55, 0x54, 0x31},   // 21 -> "aut1" = Boost10min;
   {22, 0x41, 0x55, 0x54, 0x32},   // 22 -> "aut2" = Boost20min;
   {23, 0x41, 0x55, 0x54, 0x33},   // 23 -> "aut3" = Boost30min;
   };

// define buffers, large, indeed. The entire datagram checksum will be checked at once
#define SERIAL_BUFFER_SIZE 750
//#define NETBUF_SIZE 600
uint8_t serial_buf[SERIAL_BUFFER_SIZE];
unsigned int P151_bytes_read = 0;
uint8_t task_index = 0;

typedef enum {
    DUCO_DATA_FLOW = 0,
    DUCO_DATA_STATUS = 1,
    DUCO_DATA_CO2_PPM = 2,
    DUCO_DATA_CO2_TEMP = 3,
    DUCO_DATA_LAST = 4,
} DucoDataTypes;
float duco_data[DUCO_DATA_LAST];

typedef enum {
    PLUGIN_CONFIG_IDX_FLOW = 0,
    PLUGIN_CONFIG_IDX_STATUS = 1,
    PLUGIN_CONFIG_IDX_CO2_PPM = 2,
    PLUGIN_CONFIG_IDX_CO2_TEMP = 3,
    PLUGIN_CONFIG_DUCO_BOX_NODE = 4,
    PLUGIN_CONFIG_CO2_NODE = 5,
    PLUGIN_CONFIG_LOG_SERIAL = 6,
} PluginConfigs;

int stopwordCounter = 0; // 0x0d 0x20 0x20 0x0d 0x3e 0x20
unsigned int rows[15]; // byte number where a new row starts
unsigned int rowCounter = 0;


boolean Plugin_151(byte function, struct EventStruct *event, String& string)
{
  boolean success = false;

  switch (function)
  {

    case PLUGIN_DEVICE_ADD:
      {
        Device[++deviceCount].Number = PLUGIN_ID_151;
        Device[deviceCount].Type = DEVICE_TYPE_DUMMY;
        Device[deviceCount].VType = SENSOR_TYPE_QUAD;
        Device[deviceCount].Ports = 0;
        Device[deviceCount].PullUpOption = false;
        Device[deviceCount].InverseLogicOption = false;
        Device[deviceCount].FormulaOption = false;
        Device[deviceCount].DecimalsOnly = true;
        Device[deviceCount].ValueCount = 4;
        Device[deviceCount].SendDataOption = true;
        Device[deviceCount].TimerOption = true;
        Device[deviceCount].GlobalSyncOption = true;
        break;
      }

    case PLUGIN_GET_DEVICENAME:
      {
        string = F(PLUGIN_NAME_151);
        break;
      }

    case PLUGIN_GET_DEVICEVALUENAMES:
      {
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_151));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[1], PSTR(PLUGIN_VALUENAME2_151));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[2], PSTR(PLUGIN_VALUENAME3_151));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[3], PSTR(PLUGIN_VALUENAME4_151));
        break;
      }

    case PLUGIN_WEBFORM_LOAD:
      {

        addFormNote(F("For use with domoticz you can define an idx per value. If you use this you can ignore 'Send to Controller' below."));
        addFormNumericBox(F("IDX Flow percentage"), F("Plugin_151_IDX1"), PCONFIG(PLUGIN_CONFIG_IDX_FLOW), 0,5000);
        addFormNumericBox(F("IDX DUCOBOX status"), F("Plugin_151_IDX2"), PCONFIG(PLUGIN_CONFIG_IDX_STATUS), 0,5000);
        addFormNumericBox(F("IDX CO2-sensor PPM"), F("Plugin_151_IDX3"), PCONFIG(PLUGIN_CONFIG_IDX_CO2_PPM), 0,5000);
        addFormNumericBox(F("IDX CO2-sensor temperature"), F("Plugin_151_IDX4"), PCONFIG(PLUGIN_CONFIG_IDX_CO2_TEMP), 0, 5000);
        addFormNumericBox(F("Ducobox nodenumber (default: 1)"), F("Plugin_151_ducobox_nodenumber"), PCONFIG(PLUGIN_CONFIG_DUCO_BOX_NODE), 0,1000);
        addFormNumericBox(F("CO2 controller nodenumber"), F("Plugin_151_co2controller_nodenumber"), PCONFIG(PLUGIN_CONFIG_CO2_NODE), 0,1000);
        addFormCheckBox(F("Log serial messages to syslog"), F("Plugin151_log_serial"), PCONFIG(PLUGIN_CONFIG_LOG_SERIAL));

        success = true;
        break;
      }

    case PLUGIN_WEBFORM_SAVE:
      {

        PCONFIG(PLUGIN_CONFIG_IDX_FLOW) = getFormItemInt(F("Plugin_151_IDX1"));
        PCONFIG(PLUGIN_CONFIG_IDX_STATUS) = getFormItemInt(F("Plugin_151_IDX2"));
        PCONFIG(PLUGIN_CONFIG_IDX_CO2_PPM) = getFormItemInt(F("Plugin_151_IDX3"));
        PCONFIG(PLUGIN_CONFIG_IDX_CO2_TEMP) = getFormItemInt(F("Plugin_151_IDX4"));
        PCONFIG(PLUGIN_CONFIG_DUCO_BOX_NODE) = getFormItemInt(F("Plugin_151_ducobox_nodenumber"));
        PCONFIG(PLUGIN_CONFIG_CO2_NODE) = getFormItemInt(F("Plugin_151_co2controller_nodenumber"));
        PCONFIG(PLUGIN_CONFIG_LOG_SERIAL) = isFormItemChecked(F("Plugin151_log_serial"));
        success = true;
        break;
      }
      
    case PLUGIN_INIT:{
            LoadTaskSettings(event->TaskIndex);

            Serial.begin(115200, SERIAL_8N1);
            Plugin_151_init = true;

            duco_data[DUCO_DATA_FLOW] = 0; // flow %
            duco_data[DUCO_DATA_STATUS] = 0; // DUCO_STATUS
            duco_data[DUCO_DATA_CO2_PPM] = 0; // CO2 ppm
            duco_data[DUCO_DATA_CO2_TEMP] = 0.0; // co2 temp

            // temp woraround, ESP Easy framework does not currently prepare this...
            for (byte y = 0; y < TASKS_MAX; y++){
                if (Settings.TaskDeviceNumber[y] == PLUGIN_ID_151){
                task_index = y;
                break;
                }
            }

            success = true;
            break;
        }

    case PLUGIN_READ:
      {
       
        if (Plugin_151_init){
                addLog(LOG_LEVEL_DEBUG, "[P151] DUCO SER GW: read networkList");
                readNetworkList();

                addLog(LOG_LEVEL_DEBUG, "[P151] DUCO SER GW: read CO2 PPM");
                readCO2PPM();                

                if(UserVar[event->BaseVarIndex] != duco_data[DUCO_DATA_FLOW]){
                    UserVar[event->BaseVarIndex]   = duco_data[DUCO_DATA_FLOW]; // flow percentage
                    Plugin_151_Controller_Update(task_index, event->BaseVarIndex, 0, PCONFIG(PLUGIN_CONFIG_IDX_FLOW), 0);
                }

                if(UserVar[event->BaseVarIndex+1] != duco_data[DUCO_DATA_STATUS]){
                    UserVar[event->BaseVarIndex+1]   = duco_data[DUCO_DATA_STATUS]; // flow percentage
                    Plugin_151_Controller_Update(task_index, event->BaseVarIndex, 1, PCONFIG(PLUGIN_CONFIG_IDX_STATUS), 0);
                }

                if(UserVar[event->BaseVarIndex+2] != duco_data[DUCO_DATA_CO2_PPM]){
                    UserVar[event->BaseVarIndex+2]   = duco_data[DUCO_DATA_CO2_PPM]; // flow percentage
                    Plugin_151_Controller_Update(task_index, event->BaseVarIndex, 2, PCONFIG(PLUGIN_CONFIG_IDX_CO2_PPM), 0);
                }

                if(UserVar[event->BaseVarIndex+3] != duco_data[DUCO_DATA_CO2_TEMP]){
                    UserVar[event->BaseVarIndex+3]   = duco_data[DUCO_DATA_CO2_TEMP]; // CO2 temperature
                    Plugin_151_Controller_Update(task_index, event->BaseVarIndex, 3, PCONFIG(PLUGIN_CONFIG_IDX_CO2_TEMP), 1);
                }
                //sendData(event);
                
            } // end off   if (Plugin_151_init)


        success = true;
        break;
      }
  }
  return success;
}


//    // row 3 = "  node|addr|type|ptcl|cerr|prnt|asso|stts|stat|cntdwn|%dbt|trgt|cval|snsr|ovrl|capin |capout|tree|temp|info" (header)

unsigned int getColumnNumberByColumnName(uint8_t columnName[]){
    int currentRow = 3; // row with columnnames
    int separatorCounter = 0; 
    int columnNumber = 0;
    //bool startColumnNameCheck = true;

    int charCount = 0;

    // count characters of the columnName whe are looking for
    charCount = sizeof(columnName)/sizeof(uint8_t);

           // String loggy = F("[P151] DUCO SER GW: chars: " );
            char lossebyte[6];
         
           loggy = F("[P151] loggy: " );


    // loop through all characters of row 3 (row with columnnames)
    for (int j = rows[currentRow]; j <= rows[currentRow+1]; j++) {
        if(serial_buf[j] == 0x7C){
            separatorCounter++;

            // check if our current position + the columnname isn't exceeding the total characters of the row 
            if(j+charCount <= rows[currentRow+1]){
                // after each separator start checking if the column name matches
                for(int charNr = 0; charNr < charCount; charNr++ ){ // loop through every character of the columnname till all characters are matched
                    if(serial_buf[j+1+charNr] == columnName[charNr]){

                        sprintf_P(lossebyte, PSTR("%c "), serial_buf[j+1+charNr]);
                        loggy += lossebyte;
                        loggy += F("-");
                        sprintf_P(lossebyte, PSTR("%c "), columnName[charNr]);
                        loggy += lossebyte;
                        loggy += F(" | ");

                        loggy += F(" >");
                        sprintf_P(lossebyte, PSTR("%d "), charNr);
                        loggy += lossebyte;
                        loggy += F("=");
                        sprintf_P(lossebyte, PSTR("%d "), charCount);
                        loggy += lossebyte;
                        loggy += F("< ");

                        // we matched a character!
                        if(charNr == (charCount-1)){ // check if we matched ALL characters of "columnName"
                            return separatorCounter; // return the columnNumber of "columnName"
                        }
                    }else{
                        break; // otherwise break out of this loop and wait till next separator
                    }
                }
            }
        } // end of if(serial_buf[j] == 0x7C){
    }

return 9999;
}


unsigned int getValueByNodeNumberAndColumn(uint8_t ducoNodeNumber,uint8_t ducoColumnNumber ){

    unsigned int start_byte_number = 0;
    unsigned int end_byte_number = 0;
    unsigned int row_number;
    for (int currentRow = 4; currentRow <= rowCounter; currentRow++) {
        unsigned int number_size = 0;
        unsigned int row_node_number = 0;

        for (int j = 0; j <= 6; j++) {
            if( serial_buf[ rows[currentRow]+j ] != 0x20){
                if(serial_buf[ rows[currentRow]+j ] >= '0' && serial_buf[ rows[currentRow]+j ] <= '9'){
                    if(number_size > 0){
                        row_node_number = row_node_number + (( serial_buf[ rows[currentRow]+j ] - '0' ) * (10 * number_size)) ;
                    }else{
                        row_node_number = row_node_number + ( serial_buf[ rows[currentRow]+j ] - '0' );
                        number_size++;
                    }
                }
            }
        }


        if(row_node_number == ducoNodeNumber){
            start_byte_number = rows[currentRow]; //rows[k] == startbyte of row with device id
            end_byte_number   = rows[currentRow+1];
            //row_number        = k;
            break;
        }
    }

    if(start_byte_number > 0){
        unsigned int column_counter = 0;
        for (int i = start_byte_number; i <= end_byte_number; i++) { // smaller then P151_bytes_read OR... the next row start?
            if(serial_buf[i] == 0x7c){
                column_counter++;
                if(column_counter >= ducoColumnNumber){
                    return i+1;
                    break;
                }
            }
        }
    }
    return 0;
}

int sendSerialCommand(byte command[], int size){
    bool error=false;

    // DUCO throws random debug info on the serial, lets flush input buffer.
    serialFlush();

    for (int m = 0; m <= size-1; m++) {
       delay(10); // 20ms = buffer overrun while reading serial data. // ducosoftware uses +-15ms.
       int bytesSend = Serial.write(command[m]);
       if(bytesSend != 1){
           error=true;
           break;
        }
    } // end of forloop

    // if error: log, clear command and flush buffer!
    if(error){
        addLog(LOG_LEVEL_ERROR, "[P151] DUCO SER GW: Error, failed sending command. Clear command and buffer");

        // press "enter" to clear send bytes.
        Serial.write(0x0d);
        delay(20);
        Serial.write(0x0a);

       serialFlush();
        return false;

    }else{
        addLog(LOG_LEVEL_DEBUG, "[P151] DUCO SER GW: Send command successfull!");
        return true;
    }
} // end of sendSerialCommand


void serialFlush(){
   // empty buffer
        long start = millis();
        while ((millis() - start) < 200) {
            while(Serial.available() > 0){
                Serial.read();
            }
        }
}  


bool receiveSerialData(){
    long start = millis();
    stopwordCounter = 0;
    P151_bytes_read = 0; // reset bytes read counter
    rows[0] = 0; // row zero = command in answer from duco
    rowCounter = 1; 

    // Handle RESPONSE, read serial till we received the stopword or a timeout occured.
    while (((millis() - start) < PLUGIN_READ_TIMEOUT_151) && (stopwordCounter < 6)) {
        if (Serial.available() > 0) {
            serial_buf[P151_bytes_read] = Serial.read();

            // check if there is a new row, 0x0d = new row.
            if(serial_buf[P151_bytes_read] == 0x0d){
                rows[rowCounter] = P151_bytes_read;
                rowCounter++;
            }

            if ((serial_buf[P151_bytes_read] == 0x0d) || (serial_buf[P151_bytes_read] == 0x20) || (serial_buf[P151_bytes_read] == 0x3e)) {
                stopwordCounter++; // character of stopwoord found 
            }else{
                stopwordCounter = 0;
            }            
            P151_bytes_read++;
        }

    }// end while

    if(stopwordCounter == 6){
        logSerial(LOG_LEVEL_INFO, "[P151] DUCO SER GW: Package received with stopword.");
        return true;
    }else{
        // timout occurred, stopword not found!
        logSerial(LOG_LEVEL_INFO, "[P151] DUCO SER GW: Package receive timeout.");
        serialFlush();
        return false;
    }

}


bool checkCommandInResponse(uint8_t command[]){
    // Ducobox returns the command, lets check if the command matches 
    bool commandReceived = false;

    for (int j = 0; j <= (sizeof(command) / sizeof(uint8_t)  ); j++) {
            
        if(serial_buf[j] == command[j]){
            commandReceived = true;
        }else{
            commandReceived = false;
            break;
        }
    }

    if(commandReceived == true){
        logSerial(LOG_LEVEL_DEBUG, "[P151] DUCO SER GW: Exptected command received.");
    }else{
        logSerial(LOG_LEVEL_DEBUG, "[P151] DUCO SER GW: Unexpected command received.");
    }
    return commandReceived;
}


    // DUCOBOX prints a device list; lets find the startbyte of each row.
    // row 0 = ".network" (command of response
    // row 1 =  "Network:"
    // row 2 = "  --- start list ---"
    // row 3 = "  node|addr|type|ptcl|cerr|prnt|asso|stts|stat|cntdwn|%dbt|trgt|cval|snsr|ovrl|capin |capout|tree|temp|info" (header)


      /////////// READ VENTILATION % ///////////
            // row 4 = duco box
            // 10th column is %dbt = ventilation%.
float parseVentilationPercentage(){
    unsigned int temp_ventilation_value = 0;
    unsigned int ventilationPercentageColumnNumber = getColumnNumberByColumnName(ventilationColumnName); // %dbt = 0x25 0x64 0x62 0x74
    if(ventilationPercentageColumnNumber == 9999){
        return 0;
    }
    
    //addLog(LOG_LEVEL_DEBUG, loggy);
    // delay(20);
    /*
       String logstring = F("[P151] DUCO SER GW: columnnumber  " );
            char lossebyte[6];
            sprintf_P(lossebyte, PSTR("%d "), ventilationPercentageColumnNumber);
            logstring += lossebyte;
            addLog(LOG_LEVEL_DEBUG, logstring);
            delay(20);
    */

    unsigned int start_ventilation_byte = getValueByNodeNumberAndColumn( Settings.TaskDevicePluginConfig[task_index][PLUGIN_CONFIG_DUCO_BOX_NODE], ventilationPercentageColumnNumber);
    /*
    String logstring5 = F("start_ventilation_byte: ");
    char lossebyte9[25];
    sprintf_P(lossebyte9, PSTR("%u"), start_ventilation_byte);
    logstring5 += lossebyte9;
    addLog(LOG_LEVEL_DEBUG, logstring5);
    delay(20);
    */
    if(start_ventilation_byte > 0){
        unsigned int number_size = 0;
        unsigned int ventilation_value_bytes[3];
        for (int j = 0; j <= 3; j++) {
            if( (serial_buf[start_ventilation_byte+j] == 0x20) || (serial_buf[start_ventilation_byte+j] == 0x7C) ){ // 0x20 is spatie achter het getal, 0x7c is einde value, negeren!
                number_size = j-1;
                break;
            }else if (serial_buf[start_ventilation_byte+j] >= '0' && serial_buf[start_ventilation_byte+j] <= '9'){
                ventilation_value_bytes[j] = serial_buf[start_ventilation_byte+j] - '0';
            }else{
                ventilation_value_bytes[j] = 0;
            }
        }

        if (serialLoggingEnabled()) {
            String logstring4 = F("Ventilation valuebytes: ");
            char lossebyte4[25];
            sprintf_P(lossebyte4, PSTR("%02X %02X %02X sizenr %u"), serial_buf[start_ventilation_byte],serial_buf[start_ventilation_byte+1],serial_buf[start_ventilation_byte+2],number_size);
            logstring4 += lossebyte4;
            addLog(LOG_LEVEL_DEBUG, logstring4);
            delay(20);
        }

        if(number_size == 0){ // one byte number
            temp_ventilation_value  = (ventilation_value_bytes[0]);
        }else if(number_size == 1){ // two byte number
            temp_ventilation_value  = ((ventilation_value_bytes[0] * 10) + (ventilation_value_bytes[1]));
        }else if(number_size == 2){ // three byte number
            temp_ventilation_value  = ((ventilation_value_bytes[0] * 100) + (ventilation_value_bytes[1] * 10) + (ventilation_value_bytes[2]));
        }

        if( (temp_ventilation_value >= 0) && (temp_ventilation_value <= 100)){
            return (float)temp_ventilation_value;
        }else{
            addLog(LOG_LEVEL_DEBUG, "[P151] DUCO SER GW: Ventilation value not between 0 and 100%. Ignore value.");
            return 0;
        }
    }

    addLog(LOG_LEVEL_DEBUG, "[P151] DUCO SER GW: No ventilation value found.");
    return 0;
}





    /////////// READ DUCOBOX status ///////////
    // row 4 = duco box
    // for loop, na 8 x 7c. = STAT
    // domoticz value -> "duco value" = duco modename;
    // 0 -> "auto" = AutomaticMode;

    // 1 -> "man1" = ManualMode1;
    // 2 -> "man2" = ManualMode2;
    // 3 -> "man3" = ManualMode3;

    // 4 -> "empt" = EmptyHouse;
    // 5 -> "alrm" = AlarmMode;

    // 11 -> "cnt1" = PermanentManualMode1;
    // 12 -> "cnt2" = PermanentManualMode2;
    // 13 -> "cnt3"= PermanentManualMode3;

    // 20 -> "aut0" = AutomaticMode;
    // 21 -> "aut1" = Boost10min;
    // 22 -> "aut2" = Boost20min;
    // 23 -> "aut3" = Boost30min;
int parseVentilationStatus(){
     unsigned int ducoboxStatusColumnNumber = getColumnNumberByColumnName(ducoboxStatusColumnName); // stat = 0x73, 0x74, 0x61, 0x74
    if(ducoboxStatusColumnNumber == 9999){
        return 0;
    }

    unsigned int start_ducoboxstatus_byte = getValueByNodeNumberAndColumn( Settings.TaskDevicePluginConfig[task_index][PLUGIN_CONFIG_DUCO_BOX_NODE], ducoboxStatusColumnNumber);

    if(start_ducoboxstatus_byte > 0){
        uint8_t ventilationStatus = 255;

        for (int j = 0; j <= 13; j++) {
            for (int byteNumber = 0; byteNumber <= 3; byteNumber++) {
                if(serial_buf[ (start_ducoboxstatus_byte + byteNumber )] == DucoStatusModes[j][ (byteNumber + 1) ]){
                    if(byteNumber == 3){
                        ventilationStatus = DucoStatusModes[j][0];
                    }
                }else{
                    break;
                }
            }
            if(ventilationStatus != 255) break; // exit loop if there is a match
        }

        if(ventilationStatus == 255){
            addLog(LOG_LEVEL_DEBUG, "[P151] DUCO SER GW: Status not available.");
        }else{
            String logstring = F("[P151] DUCO SER GW: Status mode: " );
            //char lossebyte[6];
            //sprintf_P(lossebyte, PSTR("%d "), (int)duco_data[DUCO_DATA_STATUS]);
            logstring += ventilationStatus;
            addLog(LOG_LEVEL_DEBUG, logstring);
            return ventilationStatus;
        }
    }

    return 0;
}

/////////// READ CO2 temperture % ///////////
// column 18 = temp
float parseNodeTemperature(uint8_t nodeNumber){
    unsigned int tempColumnNumber = getColumnNumberByColumnName(tempColumnName); // temp = 0x74, 0x65, 0x6d, 0x70
    if(tempColumnNumber == 9999){
        return 0;
    }

    unsigned int start_CO2temp_byte = getValueByNodeNumberAndColumn( Settings.TaskDevicePluginConfig[task_index][PLUGIN_CONFIG_CO2_NODE], tempColumnNumber);
    uint8_t CO2temp_value_bytes[3];

    if (serialLoggingEnabled()) {
        String logstring5 = F("[P151] DUCO SER GW: Start_CO2temp_byte: ");
        char lossebyte9[25];
        sprintf_P(lossebyte9, PSTR("%u"), start_CO2temp_byte);
        logstring5 += lossebyte9;
        addLog(LOG_LEVEL_DEBUG, logstring5);
        delay(20);
    }


    if(start_CO2temp_byte > 0){
        for (int j = 0; j <= 3-1; j++) {
            if(serial_buf[start_CO2temp_byte+j] == 0x20){ // 0d is punt achter het getal, negeren!
                CO2temp_value_bytes[j] = 0;
            }else if (serial_buf[start_CO2temp_byte+j] >= '0' && serial_buf[start_CO2temp_byte+j] <= '9'){
                CO2temp_value_bytes[j] = serial_buf[start_CO2temp_byte+j] - '0';
            }else{
                CO2temp_value_bytes[j] = 0;
            }
        }

        if (serialLoggingEnabled()) {
            String logstring4 = F("[P151] DUCO SER GW: co2 temp valuebytes: ");
            char lossebyte4[25];
            sprintf_P(lossebyte4, PSTR("raw = %02X %02X %02X "), serial_buf[start_CO2temp_byte],serial_buf[start_CO2temp_byte+1],serial_buf[start_CO2temp_byte+2]);
            logstring4 += lossebyte4;
            sprintf_P(lossebyte4, PSTR("%02X %02X %02X "), CO2temp_value_bytes[0],CO2temp_value_bytes[1],CO2temp_value_bytes[2]);
            logstring4 += lossebyte4;
            /*
            for (int vv = 0; vv <= 20; vv++) {
                sprintf_P(lossebyte4, PSTR("%02X "), CO2temp_value_bytes[start_CO2temp_byte+vv]);
                logstring4 += lossebyte4;
            }*/
            addLog(LOG_LEVEL_DEBUG, logstring4);
            delay(20);
        }

        float temp_CO2_temp_value = (float)(CO2temp_value_bytes[0] *10) + (float)CO2temp_value_bytes[1] + (float)(CO2temp_value_bytes[2]/10.0);
        if (temp_CO2_temp_value >= 0 && temp_CO2_temp_value <= 50){ // between
            return temp_CO2_temp_value;
        }else{
            addLog(LOG_LEVEL_DEBUG, "[P151] DUCO SER GW: CO2 temp value not between 0 and 50 degrees celsius. Ignoring value.");
            return 0;
        }
    }
    return 0;
}


// read CO2 temp, ventilation %, DUCO status
void readNetworkList(){
    addLog(LOG_LEVEL_DEBUG, "[P151] DUCO SER GW: start readNetworkList");

    // SEND COMMAND
    bool commandSendResult = sendSerialCommand(cmdReadNetwork, CMD_READ_NETWORK_SIZE);

    // if succesfully send command then receive response
    if(commandSendResult){
        if(receiveSerialData()){
            logArray(serial_buf, P151_bytes_read-1, 0);

            if(checkCommandInResponse(answerReadNetwork)){
                duco_data[DUCO_DATA_FLOW] = parseVentilationPercentage(); // parse ventilation percentage from data
                duco_data[DUCO_DATA_STATUS] = (float)parseVentilationStatus(); // parse ventilationstatus from data
                duco_data[DUCO_DATA_CO2_TEMP] = parseNodeTemperature(CO2_NODE_NUMBER);
/*
               String logstring3 = F("Ducobox CO temp: ");
               char lossebyte3[10];
               sprintf_P(lossebyte3, PSTR("%d%d.%d"), CO2temp_value_bytes[0], CO2temp_value_bytes[1], CO2temp_value_bytes[2]);
               logstring3 += lossebyte3;
               addLog(LOG_LEVEL_DEBUG, logstring3);
*/
            }
        }
    }
} // end of void readNetworkList(){



void readCO2PPM(){

    addLog(LOG_LEVEL_DEBUG, "[P151] DUCO SER GW: Start readCO2PPM");

    // SEND COMMAND: nodeparaget <Node> 74
    char command[20] = ""; /* 17 bytes + max 2 byte nodenumber + \r\n */
    snprintf_P(command, sizeof(command), "nodeparaget %d 74\r\n", Settings.TaskDevicePluginConfig[task_index][PLUGIN_CONFIG_CO2_NODE]);
    int commandSendResult = sendSerialCommand((byte*) command, strlen(command));
   
    // if succesfully send command then receive response
    if(commandSendResult){
        if(receiveSerialData()){
            if(checkCommandInResponse(answerReadNode)){
                unsigned int temp_CO2PPM = parseCO2PPM();
                duco_data[DUCO_DATA_CO2_PPM] = temp_CO2PPM;

                String logString = F("[P151] DUCO SER GW: CO2 PPM value = ");
                char logData[10];
                sprintf_P(logData, PSTR("%u"), temp_CO2PPM);
                logString += logData;
                addLog(LOG_LEVEL_DEBUG, logString);
             }
        }
    }
}


/* command: NodeParaGet <Node> 74
answer:
.nodeparaget <Node> 74
  Get PARA 74 of NODE <Node>
  --> 566
  Done
  */
unsigned int parseCO2PPM(){
    unsigned int CO2_start_byte = 0;

/*
   String logstring3 = F("SER BYTES: ");
        char lossebyte3[10];

            sprintf_P(lossebyte3, PSTR("%u "), P151_bytes_read);
            logstring3 += lossebyte3;

            addLog(LOG_LEVEL_INFO, logstring3);


            logstring3 = F("Received serial data: ");

        for (int jj = 0; jj <= P151_bytes_read; jj++) {
            sprintf_P(lossebyte3, PSTR("%X "), serial_buf[jj]);
            logstring3 += lossebyte3;

            /*if(debugCounter == 30 || j == P151_bytes_read){
                debugCounter = 0;
                addLog(LOG_LEVEL_INFO, logstring3);
                delay(10);
                logstring3 = F("");
            } *
            //debugCounter++;
        }
                        addLog(LOG_LEVEL_INFO, logstring3);
*/

    // CO2 value is after this 4 bytes: 2d 2d 3e 20 = "--> ", loop through data to find this bytes
    for (int i = 0; i <= P151_bytes_read-1; i++) {
        if(serial_buf[i] == 0x2d){
            if(serial_buf[i+1] == 0x2d && serial_buf[i+2] == 0x3e && serial_buf[i+3] == 0x20){ 
                // if ducobox failes to receive current co2 ppm than it says failed (66 61 69 6c 65 64).
                if (serial_buf[i+4] >= '0' && serial_buf[i+4] <= '9'){
                // the first byte is a valid number...
                    CO2_start_byte = i+4;
                }
            }
        }
    }

    if (serialLoggingEnabled()) {
        String logstring5 = F("[P151] DUCO SER GW: start_CO2_PPM_byte: ");
        char lossebyte9[25];
        sprintf_P(lossebyte9, PSTR("%u"), CO2_start_byte);
        logstring5 += lossebyte9;
        addLog(LOG_LEVEL_DEBUG, logstring5);
        delay(20);
 
        String logstring4 = F("[P151] DUCO SER GW: co2 ppm valuebytes: ");
        char lossebyte4[25];
        sprintf_P(lossebyte4, PSTR("raw = %02X %02X %02X "), serial_buf[CO2_start_byte],serial_buf[CO2_start_byte+1],serial_buf[CO2_start_byte+2]);
        logstring4 += lossebyte4;
        
        addLog(LOG_LEVEL_DEBUG, logstring4);
        delay(20);
    }

    if(CO2_start_byte > 0){
        uint8_t value[4];
        unsigned int number_size = 0;
        for (int j = 0; j <= 4; j++) {
            if(serial_buf[CO2_start_byte+j] == 0x0d){ // 0x0d = Carriage Return (next line), determine size of numbers
                number_size = j-1;
                break;
            }else if (serial_buf[CO2_start_byte+j] >= '0' && serial_buf[CO2_start_byte+j] <= '9'){ // check if number is valid ascii character
                value[j] = serial_buf[CO2_start_byte+j] - '0'; // convert ascii to int
            }else{
                value[j] = 0; // invalid ascii character, ignore byte
            }
        }

        if(number_size == 0){ // 0-9
            return value[0];
        }else if(number_size == 1){ // 0 - 99
            return ((value[0] * 10) + (value[1]));
        }else if(number_size == 2){ // 0 - 999
            return ((value[0] * 100) + (value[1] * 10) + (value[2]));
        }else if(number_size == 3){ // 0 - 9999
            return ((value[0] * 1000) + (value[1] * 100) + (value[2] * 10) + (value[3]));
        }

    }

    addLog(LOG_LEVEL_DEBUG, "[P151] DUCO SER GW: No CO2 PPM value found in package.");
    return 0;
}

bool serialLoggingEnabled() {
    return Settings.TaskDevicePluginConfig[task_index][PLUGIN_CONFIG_LOG_SERIAL] == 1;
}

void logSerial(int level, const char *message) {
    if (serialLoggingEnabled()) {
        addLog(level, message);
    }
}

int logArray(uint8_t array[], int len, int fromByte) {
    if (!serialLoggingEnabled()) {
        return -1;
    }

    unsigned int counter= 1;

    String logstring = F("[P151] DUCO SER GW: Pakket ontvangen: ");
    char lossebyte[6];

    for (int i = fromByte; i <= len-1; i++) {

        sprintf_P(lossebyte, PSTR("%02X"), array[i]);
        logstring += lossebyte;
        if( (counter * 60) == i){
            counter++;
            logstring += F("END");
            addLog(LOG_LEVEL_INFO,logstring);
            delay(50);
            logstring = F("");
        }
    }
    logstring += F("END");
    addLog(LOG_LEVEL_INFO,logstring);
    return -1;
}


// https://www.letscontrolit.com/forum/viewtopic.php?f=4&t=2104&p=9908&hilit=no+svalue#p9908

void Plugin_151_Controller_Update(byte TaskIndex, byte BaseVarIndex, byte relIndex, int IDX, int valueDecimals)
{

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

            addLog(LOG_LEVEL_DEBUG, String("[P151] DUCO SER GW: Variable #") + (int) relIndex + " updated for controller " + (int)ControllerIndex);
        }
    }
}

