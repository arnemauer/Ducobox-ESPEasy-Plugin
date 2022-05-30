//#################################### Plugin 155: DUCO Serial Box Sensors ##################################
//
//  DUCO Serial Gateway to read out the exernal installed Box Sensors
//
//  https://github.com/arnemauer/Ducobox-ESPEasy-Plugin
//  http://arnemauer.nl/ducobox-gateway/
//#######################################################################################################

#include "_Plugin_Helper.h"

#define PLUGIN_155
#define PLUGIN_ID_155           155
#define PLUGIN_NAME_155         "DUCO Serial GW - External Humidity Sensor"
#define PLUGIN_READ_TIMEOUT_155   1500 // DUCOBOX askes "live" CO2 sensor info, so answer takes sometimes a second.
#define PLUGIN_LOG_PREFIX_155   String("[P155] DUCO Ext. Hum. Sensor: ")

#define PLUGIN_VALUENAME1_155 "Temperature"
#define PLUGIN_VALUENAME2_155 "Relative_humidity"

boolean Plugin_155_init = false;
// when calling 'PLUGIN_READ', if serial port is in use set this flag and check in PLUGIN_ONCE_A_SECOND if serial port is free.
bool P155_waitingForSerialPort[TASKS_MAX];
bool P155_readTemperature[TASKS_MAX]; // true = read temperature, false = read humidity

typedef enum {
    P155_CONFIG_NODE_ADDRESS = 0,
    P155_CONFIG_LOG_SERIAL = 1,
} P155PluginConfigs;

typedef enum {
    P155_DUCO_PARAMETER_TEMP = 73,
    P155_DUCO_PARAMETER_CO2 = 74,
    P155_DUCO_PARAMETER_RH = 75,
} P155DucoParameters;

boolean Plugin_155(byte function, struct EventStruct *event, String& string){
	boolean success = false;
  	switch (function){
   		case PLUGIN_DEVICE_ADD:{
        	Device[++deviceCount].Number = PLUGIN_ID_155;
        	Device[deviceCount].Type = DEVICE_TYPE_DUMMY;
			Device[deviceCount].VType = Sensor_VType::SENSOR_TYPE_TEMP_HUM;
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

		case PLUGIN_GET_DEVICENAME: {
			string = F(PLUGIN_NAME_155);
			break;
		}

		case PLUGIN_GET_DEVICEVALUENAMES:{
			strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_155));
			strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[1], PSTR(PLUGIN_VALUENAME2_155));
			break;
		}

		case PLUGIN_WEBFORM_LOAD:{
			addFormNumericBox(F("Sensor Node address"), F("Plugin_155_NODE_ADDRESS"), PCONFIG(P155_CONFIG_NODE_ADDRESS), 0, 5000);
			addFormCheckBox(F("Log serial messages to syslog"), F("Plugin155_log_serial"), PCONFIG(P155_CONFIG_LOG_SERIAL));
			success = true;
			break;
		}

		case PLUGIN_WEBFORM_SAVE:{
			PCONFIG(P155_CONFIG_NODE_ADDRESS) = getFormItemInt(F("Plugin_155_NODE_ADDRESS"));
			PCONFIG(P155_CONFIG_LOG_SERIAL) = isFormItemChecked(F("Plugin155_log_serial"));
			success = true;
			break;
		}
		
		case PLUGIN_EXIT: {
			if(serialPortInUseByTask == event->TaskIndex){
        		serialPortInUseByTask = 255;
      		}

			String log = PLUGIN_LOG_PREFIX_155;
			log += F("EXIT PLUGIN_155");
			addLogMove(LOG_LEVEL_INFO, log);

			clearPluginTaskData(event->TaskIndex); // clear plugin taskdata
			success = true;
			break;
		}

		case PLUGIN_INIT:{
         	if(!Plugin_155_init && !ventilation_gateway_disable_serial){
            	Serial.begin(115200, SERIAL_8N1);
         	}
   			String log = PLUGIN_LOG_PREFIX_155;
         	log += F("Init plugin done.");
         	addLogMove(LOG_LEVEL_INFO, log);

		 	P155_waitingForSerialPort[event->TaskIndex] = false;
			P155_readTemperature[event->TaskIndex] = true; // read temperature first,
         	Plugin_155_init = true;
			success = true;
			break;
        }

    	case PLUGIN_READ:{
        	if (Plugin_155_init && (PCONFIG(P155_CONFIG_NODE_ADDRESS) != 0) && !ventilation_gateway_disable_serial){
				if(loglevelActiveFor(LOG_LEVEL_DEBUG)){
					String log = PLUGIN_LOG_PREFIX_155;
					log += F("start read, eventid:");
					log += event->TaskIndex;
					addLogMove(LOG_LEVEL_DEBUG, log);
				}
				// check if serial port is in use by another task, if serial is in use set flag P155_waitingForSerialPort.
				if(serialPortInUseByTask == 255){
					serialPortInUseByTask = event->TaskIndex;
					
					// toggle P155_readTemperature for this task to read temperature/humidity; 
					P155_readTemperature[event->TaskIndex] = !P155_readTemperature[event->TaskIndex];

					if(P155_readTemperature[event->TaskIndex]){ // true = read temperature
						startReadExternalSensors(PLUGIN_LOG_PREFIX_155, DUCO_DATA_EXT_SENSOR_TEMP, PCONFIG(P155_CONFIG_NODE_ADDRESS));
					}else{ // false = read humidity
						startReadExternalSensors(PLUGIN_LOG_PREFIX_155, DUCO_DATA_EXT_SENSOR_RH, PCONFIG(P155_CONFIG_NODE_ADDRESS));
					}

            	}else{
					if(loglevelActiveFor(LOG_LEVEL_DEBUG)){
						char serialPortInUse[40];
						snprintf(serialPortInUse, sizeof(serialPortInUse)," %u, set flag to read data later.", serialPortInUseByTask);
						log = PLUGIN_LOG_PREFIX_155;
						log += F("Serial port in use by taskid");
						log += serialPortInUse;
						addLogMove(LOG_LEVEL_DEBUG, log);
					}
					P155_waitingForSerialPort[event->TaskIndex] = true;
			   	}
			}
        	success = true;
        	break;
      	}
		
      	case PLUGIN_ONCE_A_SECOND:{
			if(!ventilation_gateway_disable_serial){
				// if this task is waiting for the serial port and the port is not used, start plugin_read.
				if(P155_waitingForSerialPort[event->TaskIndex]){
					if(serialPortInUseByTask == 255){ 
						Plugin_155(PLUGIN_READ, event, string);
						P155_waitingForSerialPort[event->TaskIndex] = false;
					}
				}
				if(serialPortInUseByTask == event->TaskIndex){
					if( (millis() - ducoSerialStartReading) > PLUGIN_READ_TIMEOUT_155){
						if(loglevelActiveFor(LOG_LEVEL_DEBUG)){
							String log = PLUGIN_LOG_PREFIX_155;
							log += F("Serial reading timeout");
							addLogMove(LOG_LEVEL_DEBUG, log);
						}
				  		DucoTaskStopSerial(PLUGIN_LOG_PREFIX_155);
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
            
            	while( (result = DucoSerialInterrupt()) != DUCO_MESSAGE_FIFO_EMPTY && stop == false){
               		switch(result){
                  		case DUCO_MESSAGE_ROW_END: {
							if(P155_readTemperature[event->TaskIndex]){ // true = read temperature
								readExternalSensorsProcessRow(PLUGIN_LOG_PREFIX_155, event->BaseVarIndex, DUCO_DATA_EXT_SENSOR_TEMP, PCONFIG(P155_CONFIG_NODE_ADDRESS), PCONFIG(P155_CONFIG_LOG_SERIAL));
							}else{ // false = read humidity
								readExternalSensorsProcessRow(PLUGIN_LOG_PREFIX_155, (event->BaseVarIndex + 1), DUCO_DATA_EXT_SENSOR_RH, PCONFIG(P155_CONFIG_NODE_ADDRESS), PCONFIG(P155_CONFIG_LOG_SERIAL));
							}
                     		duco_serial_bytes_read = 0; // reset bytes read counter
							break;
                  		}
                  		case DUCO_MESSAGE_END: {
                    		DucoThrowErrorMessage(PLUGIN_LOG_PREFIX_155, result);
                     		DucoTaskStopSerial(PLUGIN_LOG_PREFIX_155);
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
					DucoSerialSendCommand(PLUGIN_LOG_PREFIX_155);
				}
			}
	   		success = true;
    		break;
  		}
  	}
	return success;
}
