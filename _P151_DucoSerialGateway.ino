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
#define PLUGIN_READ_TIMEOUT_151 1000
#define PLUGIN_LOG_PREFIX_151 String("[P151] DUCO SER GW: ")
#define DUCOBOX_NODE_NUMBER 1

boolean Plugin_151_init = false;

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
bool P151_mainPluginActivatedInTask = false;

// when calling 'PLUGIN_READ', if serial port is in use set this flag and check in PLUGIN_ONCE_A_SECOND if serial port is free.
bool P151_waitingForSerialPort = false;

#define P151_NR_OUTPUT_VALUES 4 // how many values do we need to read from networklist.
uint8_t varColumnNumbers[P151_NR_OUTPUT_VALUES]; // stores the columnNumbers



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
			P151_mainPluginActivatedInTask = false;
		}

		addLog(LOG_LEVEL_INFO, PLUGIN_LOG_PREFIX_151 + F("EXIT PLUGIN_151"));
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
				if(P151_mainPluginActivatedInTask && (PCONFIG(P151_CONFIG_VALUE_TYPE) != P151_VALUE_VENTMODE) ){
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
			P151_mainPluginActivatedInTask = true;
		}else{
			P151_mainPluginActivatedInTask = false;
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
			P151_mainPluginActivatedInTask = true;
		}

		if (CONFIG_PIN1 != -1){
			pinMode(CONFIG_PIN1, OUTPUT);
			digitalWrite(CONFIG_PIN1, HIGH);
		}

		success = true;
		break;
  }

	case PLUGIN_ONCE_A_SECOND:{

		if (Plugin_151_init && PCONFIG(P151_CONFIG_VALUE_TYPE) == P151_VALUE_VENTMODE ){

			if(P151_waitingForSerialPort){
				if(serialPortInUseByTask == 255){
					Plugin_151(PLUGIN_READ, event, string);
					P151_waitingForSerialPort = false;
				}
			}

			if(serialPortInUseByTask == event->TaskIndex){
				if( (millis() - ducoSerialStartReading) > PLUGIN_READ_TIMEOUT_151){
					addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_151 + F("Serial reading timeout"));
					DucoTaskStopSerial(PLUGIN_LOG_PREFIX_151);
					serialPortInUseByTask = 255;
				}
			}
		}

	   if(UserVar[event->BaseVarIndex] != P151_DATA_VALUE[PCONFIG(P151_CONFIG_VALUE_TYPE)] ){
      	UserVar[event->BaseVarIndex]  = P151_DATA_VALUE[PCONFIG(P151_CONFIG_VALUE_TYPE)];
			sendData(event); // force to send data to controller
			addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_151 + "New value for " + Plugin_151_valuename( PCONFIG(P151_CONFIG_VALUE_TYPE), true));
		}

		success = true;
		break;
	}

	case PLUGIN_SERIAL_IN: {

		// if we unexpectedly receive data we need to flush and return success=true so espeasy won't interpret it as an serial command.
		if(serialPortInUseByTask == 255){
			DucoSerialFlush();
			success = true;
		}

		if(serialPortInUseByTask == event->TaskIndex){
			uint8_t result =0;
			bool stop = false;
			
			while( (result = DucoSerialInterrupt()) != DUCO_MESSAGE_FIFO_EMPTY && stop == false){

				switch(result){
					case DUCO_MESSAGE_ROW_END: {
						Plugin_151_processRow(event,  PCONFIG(P151_CONFIG_LOG_SERIAL));
						duco_serial_bytes_read = 0; // reset bytes read counter
						break;
					}
					case DUCO_MESSAGE_END: {
				      DucoThrowErrorMessage(PLUGIN_LOG_PREFIX_151, result);
						DucoTaskStopSerial(PLUGIN_LOG_PREFIX_151);
						stop = true;
						break;
					}
				}
			}
			success = true;
		}

		break;
	}

	case PLUGIN_READ:{

		if(Plugin_151_init && PCONFIG(P151_CONFIG_VALUE_TYPE) == P151_VALUE_VENTMODE){
			// check if serial port is in use by another task, otherwise set flag.
			if(serialPortInUseByTask == 255){
				serialPortInUseByTask = event->TaskIndex;
				
				if (CONFIG_PIN1 != -1){
					digitalWrite(CONFIG_PIN1, LOW);
				}
				Plugin_151_startReadNetworkList();
			}else{
				addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_151 + F("Serial port in use, set flag to read data later."));
				P151_waitingForSerialPort = true;
			}
		}
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


void Plugin_151_startReadNetworkList(){
	addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_151 + F("Start readNetworkList"));
	
	// set this variables before sending command
	ducoSerialStartReading = millis();
	duco_serial_bytes_read = 0; // reset bytes read counter
	duco_serial_rowCounter = 0; // reset row counter

	// SEND COMMAND
	bool commandSendResult = DucoSerialSendCommand(PLUGIN_LOG_PREFIX_151, "Network\r\n");

	// if succesfully send command then receive response
	if (!commandSendResult) {
      addLog(LOG_LEVEL_ERROR, PLUGIN_LOG_PREFIX_151 + F("Failed to send commando to Ducobox"));
      DucoTaskStopSerial(PLUGIN_LOG_PREFIX_151);
   }
}







void Plugin_151_processRow(struct EventStruct *event, bool serialLoggingEnabled ){
    
	if ( PCONFIG(P151_CONFIG_LOG_SERIAL) && duco_serial_rowCounter < 4){
		addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_151 + F("ROW:") + (duco_serial_rowCounter) + F(" bytes read:") + duco_serial_bytes_read);
		DucoSerialLogArray(PLUGIN_LOG_PREFIX_151, duco_serial_buf, duco_serial_bytes_read, 0);
	}


	if( (duco_serial_rowCounter) == 1){
		if (DucoSerialCheckCommandInResponse(PLUGIN_LOG_PREFIX_151, "network") ) {
     		addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_151 + F("Received correct response on network"));
      } else {
         addLog(LOG_LEVEL_ERROR, PLUGIN_LOG_PREFIX_151 + F("Received invalid response on network"));
			DucoTaskStopSerial(PLUGIN_LOG_PREFIX_151);
         return;
      }
	}else if( duco_serial_rowCounter == 4){
		// ROW 4: columnnames => get columnnumbers by columnname
		Plugin_151_getColumnNumbersByColumnName(event, varColumnNumbers);

	
	}else if( duco_serial_rowCounter > 4){



		duco_serial_buf[duco_serial_bytes_read] = '\0'; // null terminate string
      int splitCounter = 0;
      char *buf = (char *)duco_serial_buf;
      char *token = strtok(buf, "|");

      if (token != NULL){
      	long nodeAddress = strtol(token, NULL, 0);
      	if (nodeAddress == DUCOBOX_NODE_NUMBER) {
				addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_151 + F("Ducobox row found!"));

         	while (token != NULL) {
					// check if there is a match for varColumnNumbers 
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
	}

  return;
} // end of Plugin_151_processRow()

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
   addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_151 + F("Status value not found."));
	return -1;
}

int Plugin_151_parseNumericValue(char *token) {
	int raw_value;
  	if (sscanf(token, "%d", &raw_value) == 1){
   	return raw_value; /* No conversion required */
  }else{
    	addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_151 + F("Numeric value not found."));
  		return -1;
	}

}
