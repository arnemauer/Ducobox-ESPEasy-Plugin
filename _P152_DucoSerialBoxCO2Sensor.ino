//#################################### Plugin 152: DUCO Serial Box Sensors ##################################
//
//  DUCO Serial Gateway to read out the optionally installed Box Sensors
//
//  https://github.com/arnemauer/Ducobox-ESPEasy-Plugin
//  http://arnemauer.nl/ducobox-gateway/
//#######################################################################################################

#include "_Plugin_Helper.h"

#define PLUGIN_152
#define PLUGIN_ID_152           152
#define PLUGIN_NAME_152         "DUCO Serial GW - Box Sensor - CO2"
#define PLUGIN_READ_TIMEOUT_152   1000 // DUCOBOX askes "live" CO2 sensor info, so answer takes sometimes a second.
#define PLUGIN_LOG_PREFIX_152   String("[P152] DUCO BOX SENSOR: ")

boolean Plugin_152_init = false;
// when calling 'PLUGIN_READ', if serial port is in use set this flag and check in PLUGIN_ONCE_A_SECOND if serial port is free.
bool P152_waitingForSerialPort = false;

typedef enum {
   P152_CONFIG_LOG_SERIAL = 0,
} P152PluginConfigs;

typedef enum {
   P152_DUCO_DEVICE_NA = 0, 
   P152_DUCO_DEVICE_CO2 = 1, // not used here!
   P152_DUCO_DEVICE_CO2_TEMP = 2,
   P152_DUCO_DEVICE_RH = 3,
} P152DucoSensorDeviceTypes;

boolean Plugin_152(byte function, struct EventStruct *event, String& string){
	boolean success = false;

	switch (function){

		case PLUGIN_DEVICE_ADD: {
			Device[++deviceCount].Number = PLUGIN_ID_152;
			Device[deviceCount].Type = DEVICE_TYPE_DUMMY;
			Device[deviceCount].VType = Sensor_VType::SENSOR_TYPE_DUAL;
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
        	string = F(PLUGIN_NAME_152);
        	break;
      	}

		case PLUGIN_GET_DEVICEVALUENAMES: {
   			safe_strncpy(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR("CO2_PPM"), sizeof(ExtraTaskSettings.TaskDeviceValueNames[0]));
			break;
		}

		case PLUGIN_WEBFORM_LOAD: {
			addFormCheckBox(F("Log serial messages to syslog"), F("Plugin152_log_serial"), PCONFIG(P152_CONFIG_LOG_SERIAL));
     	 	success = true;
      		break;
   		}

		case PLUGIN_WEBFORM_SAVE: {
			PCONFIG(P152_CONFIG_LOG_SERIAL) = isFormItemChecked(F("Plugin152_log_serial"));
			success = true;
			break;
		}
      
		case PLUGIN_INIT:{
			if(!ventilation_gateway_disable_serial){
				Serial.begin(115200, SERIAL_8N1);
			}
			Plugin_152_init = true;
			success = true;
			break;
   		}

		case PLUGIN_EXIT: {
      		if(serialPortInUseByTask == event->TaskIndex){
         		serialPortInUseByTask = 255;
      		}
      		success = true;
			break;
   		}


		case PLUGIN_READ: {
   			if (Plugin_152_init && !ventilation_gateway_disable_serial){
				String log;
		   		if(loglevelActiveFor(LOG_LEVEL_DEBUG)){
					log = PLUGIN_LOG_PREFIX_152;
					log += F("start read, eventid:");
					log += event->TaskIndex;
					addLogMove(LOG_LEVEL_DEBUG, log);
				}

         		// check if serial port is in use by another task, otherwise set flag.
				if(serialPortInUseByTask == 255){
					serialPortInUseByTask = event->TaskIndex;
					if (loglevelActiveFor(LOG_LEVEL_DEBUG)) {
						log = PLUGIN_LOG_PREFIX_152;
						log += F("Start readBoxSensors");
						addLogMove(LOG_LEVEL_DEBUG, log);
					}
            		startReadBoxSensors(PLUGIN_LOG_PREFIX_152);
         		}else{
					if (loglevelActiveFor(LOG_LEVEL_DEBUG)) {
						log = PLUGIN_LOG_PREFIX_152;
						log += F("Serial port in use, set flag to read data later.");
						addLogMove(LOG_LEVEL_DEBUG, log);
					}
					P152_waitingForSerialPort = true;
				}
      
      		}

      		success = true;
      		break;
   		}

		case PLUGIN_ONCE_A_SECOND:{
			if(!ventilation_gateway_disable_serial){
				if(P152_waitingForSerialPort){
					if(serialPortInUseByTask == 255){
						Plugin_152(PLUGIN_READ, event, string);
						P152_waitingForSerialPort = false;
					}
				}

				if(serialPortInUseByTask == event->TaskIndex){
					if( (millis() - ducoSerialStartReading) > PLUGIN_READ_TIMEOUT_152){
						if (loglevelActiveFor(LOG_LEVEL_DEBUG)) {
							String log;
							log = PLUGIN_LOG_PREFIX_152;
							log += F("Serial reading timeout");
							addLogMove(LOG_LEVEL_DEBUG, log);
						}

               			DucoTaskStopSerial(PLUGIN_LOG_PREFIX_152);
               			serialPortInUseByTask = 255;
            		}
         		}
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
				bool receivedNewValue = false;
				while( (result = DucoSerialInterrupt()) != DUCO_MESSAGE_FIFO_EMPTY && stop == false){
					switch(result){
						case DUCO_MESSAGE_ROW_END: {
							receivedNewValue = readBoxSensorsProcessRow(PLUGIN_LOG_PREFIX_152, P152_DUCO_DEVICE_CO2, event->BaseVarIndex, PCONFIG(P152_CONFIG_LOG_SERIAL));
							if(receivedNewValue) sendData(event);
							duco_serial_bytes_read = 0; // reset bytes read counter
							break;
						}
						case DUCO_MESSAGE_END: {
							DucoThrowErrorMessage(PLUGIN_LOG_PREFIX_152, result);
							DucoTaskStopSerial(PLUGIN_LOG_PREFIX_152);
							stop = true;
							break;
						}
					}
				}
				success = true;
			}
			break;
		}


		case PLUGIN_FIFTY_PER_SECOND: {
			if(serialPortInUseByTask == event->TaskIndex){
				if(serialSendCommandInProgress){
					DucoSerialSendCommand(PLUGIN_LOG_PREFIX_152);
				}
			}
			success = true;
			break;
		}



  	}
  	return success;
}





// ventilation % and DUCO status
void startReadBoxSensors(String logPrefix){
	/*
	* Read box sensor information; this could contain CO2, Temperature and
	* Relative Humidity values, depending on the installed box sensors.
	*/

	// set this variables before sending command
	ducoSerialStartReading = millis();
	duco_serial_bytes_read = 0; // reset bytes read counter
	duco_serial_rowCounter = 0; // reset row counter

	char command[] = "sensorinfo\r\n";
	DucoSerialStartSendCommand(command);
}


   /* Example output Ducobox silent, silent connect,
sensorinfo   
  [SENSOR] INFO
  CO2 :  627 [ppm] (0)
  RH : 3246 [.01%] (0)
  TEMP :  235 [.1Â°C] (0)

      Example output Ducobox Energy Premium:
sensorinfo
  [SENSOR] INFO
  RH   - SHT21 : 6252 [.01%] (0)
  TEMP - SHT21 :  174 [.1°C] (0)
   */
bool readBoxSensorsProcessRow( String logPrefix, uint8_t sensorDeviceType, uint8_t userVarIndex, bool serialLoggingEnabled){
    String log;
    if(serialLoggingEnabled && loglevelActiveFor(LOG_LEVEL_DEBUG)){	     
		log = logPrefix;
		log += F("ROW: ");
		log += duco_serial_rowCounter;
		log += F(" bytes read:");
		log += duco_serial_bytes_read;
		addLogMove(LOG_LEVEL_DEBUG, log);
		DucoSerialLogArray(logPrefix, duco_serial_buf, duco_serial_bytes_read, 0);
	}

    // get the first row to check for command
	if( duco_serial_rowCounter == 1){
		if (DucoSerialCheckCommandInResponse(logPrefix, "sensorinfo") ) {
         	if(loglevelActiveFor(LOG_LEVEL_DEBUG)){
				log = logPrefix;
				log += F("Received correct response on sensorinfo");
				addLogMove(LOG_LEVEL_DEBUG, log);
         	}
      	} else {
			if(loglevelActiveFor(LOG_LEVEL_DEBUG)){
				log = logPrefix;
				log += F("Received invalid response");
				addLogMove(LOG_LEVEL_DEBUG, log);
			}
			DucoTaskStopSerial(logPrefix);
         	return false;
      	}
	}else if( duco_serial_rowCounter > 2){
		duco_serial_buf[duco_serial_bytes_read] = '\0';
		unsigned int raw_value;
		char logBuf[30];

	   	if(sensorDeviceType == P152_DUCO_DEVICE_CO2){     
         	if (strlen("  CO") <= duco_serial_bytes_read && strncmp("  CO", (char*) duco_serial_buf, strlen("  CO")) == 0) {
            	char* startByteValue = strchr((char*) duco_serial_buf,':');
            	if(startByteValue != NULL ){
               		if (sscanf(startByteValue, ": %u", &raw_value) == 1) {
						unsigned int co2_ppm = raw_value; /* No conversion required */
						UserVar[userVarIndex] = co2_ppm;
						snprintf(logBuf, sizeof(logBuf), "CO2 PPM: %u = %u PPM", raw_value, co2_ppm);
						log = logPrefix;
						log += logBuf;
						addLogMove(LOG_LEVEL_INFO, log);
						return true;
               		}
            	}
        	}
      	}

		if(sensorDeviceType == P152_DUCO_DEVICE_RH){
         	if (strlen("  RH") <= duco_serial_bytes_read && strncmp("  RH", (char*) duco_serial_buf, strlen("  RH")) == 0) {
            	char* startByteValue = strchr((char*) duco_serial_buf,':');
            	if(startByteValue != NULL ){
               		if (sscanf((const char*)startByteValue, ": %u", &raw_value) == 1) {
                 		float rh = (float) raw_value / 100.;
                  		UserVar[userVarIndex + 1] = rh;
						snprintf(logBuf, sizeof(logBuf), "RH: %u = %.2f%%", raw_value, rh);
						log = logPrefix;
						log += logBuf;
						addLogMove(LOG_LEVEL_INFO, log);
						return true;

               		}
            	}
         	}
      	}

		if(sensorDeviceType == P152_DUCO_DEVICE_CO2_TEMP || sensorDeviceType == P152_DUCO_DEVICE_RH){
        	if (strlen("  TEMP") <= duco_serial_bytes_read && strncmp("  TEMP", (char*) duco_serial_buf, strlen("  TEMP")) == 0) {
				log = logPrefix;
				log += F("row starts with TEMP");
				addLogMove(LOG_LEVEL_DEBUG, log);
				char* startByteValue = strchr((char*) duco_serial_buf,':');
            	if(startByteValue != NULL ){
               		if (sscanf((const char*)startByteValue, ": %u", &raw_value) == 1) {
						float temp = (float) raw_value / 10.;
						UserVar[userVarIndex] = temp;
						snprintf(logBuf, sizeof(logBuf), "TEMP: %u = %.1fÂ°C", raw_value, temp);
						log = logPrefix;
						log += logBuf;
						addLogMove(LOG_LEVEL_INFO, log);
						return true;
               		}
            	}
         	}
   		}
   	}
   	return false;
}