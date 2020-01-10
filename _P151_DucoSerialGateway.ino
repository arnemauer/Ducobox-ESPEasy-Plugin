//#################################### Plugin 151: DUCO Serial Gateway ##################################
//
//  DUCO Serial Gateway to read Ducobox data
//  https://github.com/arnemauer/Ducobox-ESPEasy-Plugin
//  http://arnemauer.nl/ducobox-gateway/
//#######################################################################################################

#define PLUGIN_151
#define PLUGIN_ID_151 151
#define PLUGIN_NAME_151 "DUCO Serial Gateway"
#define PLUGIN_VALUENAME1_151 "DUCO_STATUS" // networkcommand
#define PLUGIN_VALUENAME2_151 "FLOW_PERCENTAGE"     // networkcommand
#define PLUGIN_READ_TIMEOUT_151 40
#define PLUGIN_LOG_PREFIX_151 String("[P151] DUCO SER GW: ")
#define DUCOBOX_NODE_NUMBER 1

boolean Plugin_151_init = false;

const char *cmdReadNetwork = "Network\r\n";
const char *answerReadNetwork = "network\r";

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

#define DUCO_STATUS_MODUS_COUNT 13
const char DucoStatusModes[DUCO_STATUS_MODUS_COUNT][5] =
    {"AUTO", "MAN1", "MAN2", "MAN3", "EMPT", "ALRM", "CNT1", "CNT2", "CNT3", "AUT0", "AUT1", "AUT2", "AUT3"};
const int DucoStatusModesNumbers[DUCO_STATUS_MODUS_COUNT] =
    {0, 1, 2, 3, 4, 5, 11, 12, 13, 20, 21, 22, 23};

typedef enum {
	P151_CONFIG_VALUE_TYPE = 0,
	P151_CONFIG_LOG_SERIAL = 1,
} P151PluginConfigs; // max 8 values


typedef enum {
	P151_VALUE_VENTMODE = 0,
	P151_VALUE_VENT_PERCENTAGE = 1,
	P151_VALUE_CURRENT_FAN = 2,
	P151_VALUE_COUNTDOWN = 3,
	P151_VALUE_NONE = 4,
} P151Values;


//global variables
int P151_DATA_VALUE[4]; // store here data of P151_VALUE_VENTMODE, P151_VALUE_VENT_PERCENTAGE, P151_VALUE_CURRENT_FAN and P151_VALUE_COUNTDOWN
bool mainPluginActivatedInTask = false;

#define P151_NR_OUTPUT_VALUES 4 // how many values do we need to read from networklist.

#define P151_NR_OUTPUT_OPTIONS 5
String Plugin_151_valuename(byte value_nr, bool displayString) {
	switch (value_nr) {
		case P151_VALUE_VENTMODE:  return displayString ? F("Ventilationmode") : F("stat");
		case P151_VALUE_VENT_PERCENTAGE:  return displayString ? F("Ventilation_Percentage") : F("%dbt");
		case P151_VALUE_CURRENT_FAN:  return displayString ? F("Current_Fan_Value") : F("cval");
		case P151_VALUE_COUNTDOWN:  return displayString ? F("Countdown") : F("cntdwn");
		case P151_VALUE_NONE: return displayString ? F("None") : F("");
		default:
      break;
  	}
  	return "";
}


boolean Plugin_151(byte function, struct EventStruct *event, String &string)
{
	boolean success = false;

	switch (function){

	case PLUGIN_DEVICE_ADD:{
		Device[++deviceCount].Number = PLUGIN_ID_151;
		Device[deviceCount].Type = DEVICE_TYPE_SINGLE;
		Device[deviceCount].VType = SENSOR_TYPE_SINGLE;
		Device[deviceCount].Ports = 0;
		Device[deviceCount].PullUpOption = false;
		Device[deviceCount].InverseLogicOption = false;
		Device[deviceCount].FormulaOption = false;
		Device[deviceCount].DecimalsOnly = true;
		Device[deviceCount].ValueCount = 1;
		Device[deviceCount].SendDataOption = true;
		Device[deviceCount].TimerOption = true;
		Device[deviceCount].GlobalSyncOption = true;
		break;
  	}

	case PLUGIN_GET_DEVICENAME: {
   	string = F(PLUGIN_NAME_151);
    	break;
	}

	case PLUGIN_GET_DEVICEVALUENAMES: {
		safe_strncpy(ExtraTaskSettings.TaskDeviceValueNames[0], Plugin_151_valuename(PCONFIG(P151_CONFIG_VALUE_TYPE), true), sizeof(ExtraTaskSettings.TaskDeviceValueNames[0]));
		break;
  	}

	case PLUGIN_GET_DEVICEGPIONAMES:{
    	event->String1 = formatGpioName_output(F("Status LED"));
    	break;
  	}

	case PLUGIN_EXIT: {
		if(PCONFIG(P151_CONFIG_VALUE_TYPE) == P151_VALUE_VENTMODE){
			mainPluginActivatedInTask = false;
		}

		addLog(LOG_LEVEL_INFO, PLUGIN_LOG_PREFIX_151 + "EXIT PLUGIN_151");
      clearPluginTaskData(event->TaskIndex); // clear plugin taskdata
		//ClearCustomTaskSettings(event->TaskIndex); // clear networkID settings
		
		success = true;
		break;
	}

	case PLUGIN_WEBFORM_LOAD: {

		addRowLabel(F("Output Value Type"));
		addSelector_Head(PCONFIG_LABEL(P151_CONFIG_VALUE_TYPE), false);

		for (byte x = 0; x < P151_NR_OUTPUT_OPTIONS; x++){
			String name     = Plugin_151_valuename(x, true);
			bool   disabled = false;

			if(x == 0){
				name = F("Ventilationmode (main plugin)");
				if(mainPluginActivatedInTask && (PCONFIG(P151_CONFIG_VALUE_TYPE) != P151_VALUE_VENTMODE) ){
					disabled = true;
				}
			}

			addSelector_Item(name, x, PCONFIG(P151_CONFIG_VALUE_TYPE) == x, disabled, "");
		}
		addSelector_Foot();

		if(PCONFIG(P151_CONFIG_VALUE_TYPE) == P151_VALUE_VENTMODE){
			addFormCheckBox(F("Log serial messages to syslog"), F("Plugin151_log_serial"), PCONFIG(P151_CONFIG_LOG_SERIAL));
		}

		success = true;
		break;
  	}

  case PLUGIN_SET_DEFAULTS:{
    	PCONFIG(P151_CONFIG_VALUE_TYPE) = P151_VALUE_NONE;
    	success = true;
    	break;
  	}

	case PLUGIN_WEBFORM_SAVE:{
		// Save output selector parameters.
		sensorTypeHelper_saveOutputSelector(event, P151_CONFIG_VALUE_TYPE, 0, Plugin_151_valuename(PCONFIG(P151_CONFIG_VALUE_TYPE), true));

		if(PCONFIG(P151_CONFIG_VALUE_TYPE) == P151_VALUE_VENTMODE){
			mainPluginActivatedInTask = true;
		}else{
			mainPluginActivatedInTask = false;
		}

		if(PCONFIG(P151_CONFIG_VALUE_TYPE) == P151_VALUE_VENTMODE){
	   	PCONFIG(P151_CONFIG_LOG_SERIAL) = isFormItemChecked(F("Plugin151_log_serial")); // no errors if this checkbox does not exist?
		}

	 	success = true;
    	break;
  }

	case PLUGIN_INIT: {
		//LoadTaskSettings(event->TaskIndex);

    	Serial.begin(115200, SERIAL_8N1);
		Plugin_151_init = true;

		if(PCONFIG(P151_CONFIG_VALUE_TYPE) == P151_VALUE_VENTMODE){
			mainPluginActivatedInTask = true;
		}

		if (CONFIG_PIN1 != -1){
			pinMode(CONFIG_PIN1, OUTPUT);
			digitalWrite(CONFIG_PIN1, HIGH);
		}

		success = true;
		break;
  }

	case PLUGIN_ONCE_A_SECOND:{
	   if(UserVar[event->BaseVarIndex] != P151_DATA_VALUE[PCONFIG(P151_CONFIG_VALUE_TYPE)] ){
      	UserVar[event->BaseVarIndex]  = P151_DATA_VALUE[PCONFIG(P151_CONFIG_VALUE_TYPE)];
			sendData(event); // force to send data to controller
			addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_151 + "New value for " + Plugin_151_valuename( PCONFIG(P151_CONFIG_VALUE_TYPE), true));
		}

		success = true;
		break;
	}

	case PLUGIN_READ:{
   	if (Plugin_151_init && PCONFIG(P151_CONFIG_VALUE_TYPE) < P151_VALUE_NONE ){
      	if (CONFIG_PIN1 != -1){
        		digitalWrite(CONFIG_PIN1, LOW);
      	}
			
			if(PCONFIG(P151_CONFIG_VALUE_TYPE) == P151_VALUE_VENTMODE){
  				addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_151 + "Start readNetworkList");
      		Plugin_151_readNetworkList(event, PCONFIG(P151_CONFIG_LOG_SERIAL) );
			}
    	} // end off   if (Plugin_151_init)

    	success = true;
    	break;
  	}

	case PLUGIN_TEN_PER_SECOND: {
   	// set statusled off
    	if (CONFIG_PIN1 != -1){
      	digitalWrite(CONFIG_PIN1, HIGH);
    	}

	   success = true;
    	break;
  	}



  } // switch() end


  return success;
}


// ventilation % and DUCO status
void Plugin_151_readNetworkList(struct EventStruct *event, bool serialLoggingEnabled ){

	uint8_t result = 0; // stores the result of DucoSerialReceiveRow

	// SEND COMMAND
	bool commandSendResult = DucoSerialSendCommand(PLUGIN_LOG_PREFIX_151, cmdReadNetwork);

	// if succesfully send command then receive response
	if (commandSendResult) {
   	// get the first row to check for command, skip row 2 & 3, get row 4 (columnnames)
   	for (int j = 0; j <= 3; j++){
      	result = DucoSerialReceiveRow(PLUGIN_LOG_PREFIX_151, PLUGIN_READ_TIMEOUT_151, PCONFIG(P151_CONFIG_LOG_SERIAL));
			if (result != DUCO_MESSAGE_ROW_END) {
				DucoThrowErrorMessage(PLUGIN_LOG_PREFIX_151, result);
				delay(10);
				return;
			}

      	// check first row for correct command
      	if (j == 0) {
        		if (DucoSerialCheckCommandInResponse(PLUGIN_LOG_PREFIX_151, answerReadNetwork, PCONFIG(P151_CONFIG_LOG_SERIAL)) ) {
          		addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_151 + "Received correct response on network");
        		} else {
          		addLog(LOG_LEVEL_INFO, PLUGIN_LOG_PREFIX_151 + "Received invalid response on network");
          		return;
        		}
      	}
    	}

	// split row with colomnnames
	uint8_t varColumnNumbers[P151_NR_OUTPUT_VALUES];
	Plugin_151_getColumnNumbersByColumnName(event, varColumnNumbers);

   // receive next row while result is DUCO_MESSAGE_ROW_END
   while ((result = DucoSerialReceiveRow(PLUGIN_LOG_PREFIX_151, PLUGIN_READ_TIMEOUT_151, PCONFIG(P151_CONFIG_LOG_SERIAL)  )) == DUCO_MESSAGE_ROW_END){
      if ( PCONFIG(P151_CONFIG_LOG_SERIAL) ){
      	DucoSerialLogArray(PLUGIN_LOG_PREFIX_151, duco_serial_buf, duco_serial_bytes_read - 1, 0);
      }

		duco_serial_buf[duco_serial_bytes_read] = '\0'; // null terminate string
      int splitCounter = 0;
      char *buf = (char *)duco_serial_buf;
      char *token = strtok(buf, "|");

      if (token != NULL){
      	long nodeAddress = strtol(token, NULL, 0);
      	if (nodeAddress == DUCOBOX_NODE_NUMBER) {
         	while (token != NULL) {

         	// check if there is a match with
         	for (int i = 0; i < P151_NR_OUTPUT_VALUES; i++){
            	if (splitCounter == varColumnNumbers[i]) {
						int newValue = -1;

						if(i == P151_VALUE_VENTMODE){  // ventilationmode / "stat"
							newValue = Plugin_151_parseVentilationStatus(token);			
						}else if(i < P151_NR_OUTPUT_VALUES){ // parse numeric value:"cntdwn", "%dbt", "trgt", "cval", "snsr", "ovrl"
							newValue = Plugin_151_parseNumericValue(token);			
						}

						if(newValue != -1){
							P151_DATA_VALUE[i] = newValue;
							char logBuf[20];
    						snprintf(logBuf, sizeof(logBuf), " value: %d", newValue);
    						addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_151 + "New " + Plugin_151_valuename(i,true) + logBuf);
						}
              	}
            } // for end

            splitCounter++;
            token = strtok(NULL, "|");

          } //while (token != NULL)
        }
      }
    } // end while

    if (result != DUCO_MESSAGE_ROW_END)
    {
      DucoThrowErrorMessage(PLUGIN_LOG_PREFIX_151, result);
    }
  }

  return;
} // end of readNetworkList()

void Plugin_151_getColumnNumbersByColumnName(struct EventStruct *event, uint8_t varColumnNumbers[]) {

	for (int i = 0; i < P151_NR_OUTPUT_VALUES; i++) {
		varColumnNumbers[i] = 255; // set a default value in case we cant find a column
	}

	duco_serial_buf[duco_serial_bytes_read] = '\0';
  	int splitCounter = 0;
  	char *buf = (char *)duco_serial_buf;
  	char *token = strtok(buf, "|");

	while (token != NULL) {
		// check if there is a match with
    	for (int i = 0; i < P151_NR_OUTPUT_VALUES; i++) {
			if (strstr(token, Plugin_151_valuename(i, false).c_str() ) != NULL){ // if not null, save position
        		varColumnNumbers[i] = splitCounter;
      	}
    		
		} // for

   	splitCounter++;
   	token = strtok(NULL, "|");

	} //while (token != NULL) {

  return;
}

int Plugin_151_parseVentilationStatus(char *token){
  	for (int i = 0; i < DUCO_STATUS_MODUS_COUNT; i++) {
    	// strstr is case sensitive!
    	if (strstr(token, DucoStatusModes[i]) != NULL) { // if not null, status found :)
			return DucoStatusModesNumbers[i];
    	}
  	}

	// if value not found return -1
   addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_151 + "Status value not found.");
	return -1;
}

int Plugin_151_parseNumericValue(char *token) {
	int raw_value;
  	if (sscanf(token, "%d", &raw_value) == 1){
   	return raw_value; /* No conversion required */
  }else{
    	addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_151 + "Numeric value not found.");
  		return -1;
	}

}
