//#################################### Plugin 151: DUCO Serial Gateway ##################################
//
//  DUCO Serial Gateway to read Ducobox data
//  https://github.com/arnemauer/Ducobox-ESPEasy-Plugin
//  http://arnemauer.nl/ducobox-gateway/
//#######################################################################################################


#define PLUGIN_151
#define PLUGIN_ID_151           151
#define PLUGIN_NAME_151         "DUCO Serial Gateway"
#define PLUGIN_VALUENAME1_151   "FLOW_PERCENTAGE" // networkcommand
#define PLUGIN_VALUENAME2_151   "DUCO_STATUS" // networkcommand
#define PLUGIN_READ_TIMEOUT_151   500
#define PLUGIN_LOG_PREFIX_151   String("[P151] DUCO SER GW: ")

#define DUCOBOX_NODE_NUMBER     1
#define CO2_NODE_NUMBER         2
#define INVALID_COLUMN          9999

boolean Plugin_151_init = false;

const char *cmdReadNetwork = "Network\r\n";
const char *answerReadNetwork = "network\r";

const char ventilationColumnName[] = "%dbt";
const char ducoboxStatusColumnName[] = "stat";

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

uint8_t task_index = 0;

typedef enum {
    P151_DATA_FLOW = 0,
    P151_DATA_STATUS = 1,
    P151_DATA_LAST = 2,
} P151DucoDataTypes;
float p151_duco_data[P151_DATA_LAST];

typedef enum {
    P151_CONFIG_IDX_FLOW = 0,
    P151_CONFIG_IDX_STATUS = 1,
    P151_CONFIG_DUCO_BOX_NODE = 2,
    P151_CONFIG_LOG_SERIAL = 3,
} P151PluginConfigs;


boolean Plugin_151(byte function, struct EventStruct *event, String& string)
{
  boolean success = false;

  switch (function)
  {

    case PLUGIN_DEVICE_ADD:
      {
        Device[++deviceCount].Number = PLUGIN_ID_151;
        Device[deviceCount].Type = DEVICE_TYPE_DUMMY;
        Device[deviceCount].VType = SENSOR_TYPE_DUAL;
        Device[deviceCount].Ports = 0;
        Device[deviceCount].PullUpOption = false;
        Device[deviceCount].InverseLogicOption = false;
        Device[deviceCount].FormulaOption = false;
        Device[deviceCount].DecimalsOnly = true;
        Device[deviceCount].ValueCount = 2;
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
        break;
      }

    case PLUGIN_WEBFORM_LOAD:
      {

        addFormNote(F("For use with domoticz you can define an idx per value. If you use this you can ignore 'Send to Controller' below."));
        addFormNumericBox(F("IDX Flow percentage"), F("Plugin_151_IDX1"), PCONFIG(P151_CONFIG_IDX_FLOW), 0,5000);
        addFormNumericBox(F("IDX DUCOBOX status"), F("Plugin_151_IDX2"), PCONFIG(P151_CONFIG_IDX_STATUS), 0,5000);
        addFormNumericBox(F("Ducobox nodenumber (default: 1)"), F("Plugin_151_ducobox_nodenumber"), PCONFIG(P151_CONFIG_DUCO_BOX_NODE), 0,1000);
        addFormCheckBox(F("Log serial messages to syslog"), F("Plugin151_log_serial"), PCONFIG(P151_CONFIG_LOG_SERIAL));

        success = true;
        break;
      }

    case PLUGIN_WEBFORM_SAVE:
      {

        PCONFIG(P151_CONFIG_IDX_FLOW) = getFormItemInt(F("Plugin_151_IDX1"));
        PCONFIG(P151_CONFIG_IDX_STATUS) = getFormItemInt(F("Plugin_151_IDX2"));
        PCONFIG(P151_CONFIG_DUCO_BOX_NODE) = getFormItemInt(F("Plugin_151_ducobox_nodenumber"));
        PCONFIG(P151_CONFIG_LOG_SERIAL) = isFormItemChecked(F("Plugin151_log_serial"));
        success = true;
        break;
      }
      
    case PLUGIN_INIT:{
            LoadTaskSettings(event->TaskIndex);

            Serial.begin(115200, SERIAL_8N1);
            Plugin_151_init = true;

            p151_duco_data[P151_DATA_FLOW] = NAN; // flow %
            p151_duco_data[P151_DATA_STATUS] = NAN; // DUCO_STATUS

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
                addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_151 + "read networkList");
                readNetworkList();
           
                if(UserVar[event->BaseVarIndex] != p151_duco_data[P151_DATA_FLOW]){
                    UserVar[event->BaseVarIndex]   = p151_duco_data[P151_DATA_FLOW]; // flow percentage
                    DucoSerialSendUpdate(PLUGIN_LOG_PREFIX_151, task_index, event->BaseVarIndex, 0, PCONFIG(P151_CONFIG_IDX_FLOW), 0);
                }

                if(UserVar[event->BaseVarIndex+1] != p151_duco_data[P151_DATA_STATUS]){
                    UserVar[event->BaseVarIndex+1]   = p151_duco_data[P151_DATA_STATUS]; // flow percentage
                    DucoSerialSendUpdate(PLUGIN_LOG_PREFIX_151, task_index, event->BaseVarIndex, 1, PCONFIG(P151_CONFIG_IDX_STATUS), 0);
                }

                sendData(event);
                
            } // end off   if (Plugin_151_init)


        success = true;
        break;
      }
  }
  return success;
}


//    // row 3 = "  node|addr|type|ptcl|cerr|prnt|asso|stts|stat|cntdwn|%dbt|trgt|cval|snsr|ovrl|capin |capout|tree|temp|info" (header)

unsigned int getColumnNumberByColumnName(const char *columnName){
    int currentRow = 3; // row with columnnames
    int separatorCounter = 0; 
    int columnNumber = 0;
    //bool startColumnNameCheck = true;

    int charCount = strlen(columnName);

    // loop through all characters of row 3 (row with columnnames)
    for (int j = duco_serial_rows[currentRow]; j <= duco_serial_rows[currentRow+1]; j++) {
        if(duco_serial_buf[j] == 0x7C){
            separatorCounter++;

            // check if our current position + the columnname isn't exceeding the total characters of the row 
            if(j+charCount <= duco_serial_rows[currentRow+1]){
                // after each separator start checking if the column name matches
                for(int charNr = 0; charNr < charCount; charNr++ ){ // loop through every character of the columnname till all characters are matched
                    if(duco_serial_buf[j+1+charNr] == columnName[charNr]){
                        // we matched a character!
                        if(charNr == (charCount-1)){ // check if we matched ALL characters of "columnName"
                            return separatorCounter; // return the columnNumber of "columnName"
                        }
                    }else{
                        break; // otherwise break out of this loop and wait till next separator
                    }
                }
            }
        } // end of if(duco_serial_buf[j] == 0x7C){
    }

    return INVALID_COLUMN;
}


unsigned int getValueByNodeNumberAndColumn(uint8_t ducoNodeNumber,uint8_t ducoColumnNumber ){

    unsigned int start_byte_number = 0;
    unsigned int end_byte_number = 0;
    unsigned int row_number;
    for (int currentRow = 4; currentRow <= duco_serial_rowCounter; currentRow++) {
        unsigned int number_size = 0;
        unsigned int row_node_number = 0;

        for (int j = 0; j <= 6; j++) {
            if( duco_serial_buf[ duco_serial_rows[currentRow]+j ] != 0x20){
                if(duco_serial_buf[ duco_serial_rows[currentRow]+j ] >= '0' && duco_serial_buf[ duco_serial_rows[currentRow]+j ] <= '9'){
                    if(number_size > 0){
                        row_node_number = row_node_number + (( duco_serial_buf[ duco_serial_rows[currentRow]+j ] - '0' ) * (10 * number_size)) ;
                    }else{
                        row_node_number = row_node_number + ( duco_serial_buf[ duco_serial_rows[currentRow]+j ] - '0' );
                        number_size++;
                    }
                }
            }
        }

        if(row_node_number == ducoNodeNumber){
            start_byte_number = duco_serial_rows[currentRow]; //duco_serial_rows[k] == startbyte of row with device id
            end_byte_number   = duco_serial_rows[currentRow+1];
            //row_number        = k;
            break;
        }
    }

    if(start_byte_number > 0){
        unsigned int column_counter = 0;
        for (int i = start_byte_number; i <= end_byte_number; i++) { // smaller then duco_serial_bytes_read OR... the next row start?
            if(duco_serial_buf[i] == 0x7c){
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

    // DUCOBOX prints a device list; lets find the startbyte of each row.
    // row 0 = ".network" (command of response
    // row 1 =  "Network:"
    // row 2 = "  --- start list ---"
    // row 3 = "  node|addr|type|ptcl|cerr|prnt|asso|stts|stat|cntdwn|%dbt|trgt|cval|snsr|ovrl|capin |capout|tree|temp|info" (header)


      /////////// READ VENTILATION % ///////////
            // row 4 = duco box
            // 10th column is %dbt = ventilation%.
bool parseVentilationPercentage(float *value) {
    if (value == NULL) {
        return false;
    }

    unsigned int temp_ventilation_value = 0;
    unsigned int ventilationPercentageColumnNumber = getColumnNumberByColumnName(ventilationColumnName); // %dbt = 0x25 0x64 0x62 0x74
    if(ventilationPercentageColumnNumber == INVALID_COLUMN){
        return false;
    }
    
    unsigned int start_ventilation_byte = getValueByNodeNumberAndColumn( Settings.TaskDevicePluginConfig[task_index][P151_CONFIG_DUCO_BOX_NODE], ventilationPercentageColumnNumber);

    if(start_ventilation_byte > 0){
        unsigned int number_size = 0;
        unsigned int ventilation_value_bytes[3];
        for (int j = 0; j <= 3; j++) {
            if( (duco_serial_buf[start_ventilation_byte+j] == 0x20) || (duco_serial_buf[start_ventilation_byte+j] == 0x7C) ){ // 0x20 is spatie achter het getal, 0x7c is einde value, negeren!
                number_size = j-1;
                break;
            }else if (duco_serial_buf[start_ventilation_byte+j] >= '0' && duco_serial_buf[start_ventilation_byte+j] <= '9'){
                ventilation_value_bytes[j] = duco_serial_buf[start_ventilation_byte+j] - '0';
            }else{
                ventilation_value_bytes[j] = 0;
            }
        }

        if (serialLoggingEnabled()) {
            String logstring4 = F("Ventilation valuebytes: ");
            char lossebyte4[25];
            sprintf_P(lossebyte4, PSTR("%02X %02X %02X sizenr %u"), duco_serial_buf[start_ventilation_byte],duco_serial_buf[start_ventilation_byte+1],duco_serial_buf[start_ventilation_byte+2],number_size);
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
            *value = temp_ventilation_value;
            return true;
        }else{
            addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_151 + "Ventilation value not between 0 and 100%. Ignore value.");
            return false;
        }
    }

    addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_151 + "No ventilation value found.");
    return false;
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
bool parseVentilationStatus(unsigned int *value){
    if (value == NULL) {
        return false;
    }

    unsigned int ducoboxStatusColumnNumber = getColumnNumberByColumnName(ducoboxStatusColumnName); // stat = 0x73, 0x74, 0x61, 0x74
    if(ducoboxStatusColumnNumber == INVALID_COLUMN){
        return false;
    }

    unsigned int start_ducoboxstatus_byte = getValueByNodeNumberAndColumn( Settings.TaskDevicePluginConfig[task_index][P151_CONFIG_DUCO_BOX_NODE], ducoboxStatusColumnNumber);

    if(start_ducoboxstatus_byte > 0){
        uint8_t ventilationStatus = 255;

        for (int j = 0; j <= 13; j++) {
            for (int byteNumber = 0; byteNumber <= 3; byteNumber++) {
                if(duco_serial_buf[ (start_ducoboxstatus_byte + byteNumber )] == DucoStatusModes[j][ (byteNumber + 1) ]){
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
            addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_151 + "Status not available.");
        }else{
            String logstring = PLUGIN_LOG_PREFIX_151 + F("Status mode: " );
            //char lossebyte[6];
            //sprintf_P(lossebyte, PSTR("%d "), (int)p151_duco_data[P151_DATA_STATUS]);
            logstring += ventilationStatus;
            addLog(LOG_LEVEL_DEBUG, logstring);
            *value = ventilationStatus;
            return true;
        }
    }

    return false;
}

// ventilation % and DUCO status
void readNetworkList(){
    addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_151 + "start readNetworkList");

    // SEND COMMAND
    bool commandSendResult = DucoSerialSendCommand(PLUGIN_LOG_PREFIX_151, cmdReadNetwork);

    // if succesfully send command then receive response
    if(commandSendResult){
        if(DucoSerialReceiveData(PLUGIN_LOG_PREFIX_151, PLUGIN_READ_TIMEOUT_151, serialLoggingEnabled())){
            logArray(duco_serial_buf, duco_serial_bytes_read-1, 0);

            if(DucoSerialCheckCommandInResponse(PLUGIN_LOG_PREFIX_151, answerReadNetwork, serialLoggingEnabled())){
                float ventilationPercentage = NAN;
                 // parse ventilation percentage from data
                if (parseVentilationPercentage(&ventilationPercentage)) {
                    p151_duco_data[P151_DATA_FLOW] = ventilationPercentage;
                } else {
                    p151_duco_data[P151_DATA_FLOW] = NAN;
                }

                // parse ventilationstatus from data
                unsigned int ventilationStatus = 0;
                if (parseVentilationStatus(&ventilationStatus)) {
                    p151_duco_data[P151_DATA_STATUS] = (float)ventilationStatus; 
                } else {
                    p151_duco_data[P151_DATA_STATUS] = NAN;
                }

            }
        }
    }
} // end of void readNetworkList(){


/* command: NodeParaGet <Node> 74
answer:
.nodeparaget <Node> 74
  Get PARA 74 of NODE <Node>
  --> 566
  Done
  */
 

bool serialLoggingEnabled() {
    return Settings.TaskDevicePluginConfig[task_index][P151_CONFIG_LOG_SERIAL] == 1;
}

int logArray(uint8_t array[], int len, int fromByte) {
    if (!serialLoggingEnabled()) {
        return -1;
    }

    unsigned int counter= 1;

    String logstring = PLUGIN_LOG_PREFIX_151 + F("Pakket ontvangen: ");
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
