//#################################### Plugin 154: DUCO Serial Box Sensors ##################################
//
//  DUCO Serial Gateway to read out the exernal installed Box Sensors
//
//  https://github.com/arnemauer/Ducobox-ESPEasy-Plugin
//  http://arnemauer.nl/ducobox-gateway/
//#######################################################################################################

// TODO: add a device parser for device droplist.

#define PLUGIN_154
#define PLUGIN_ID_154           154
#define PLUGIN_NAME_154         "DUCO Serial GW - External CO2 Sensor (CO2 & Temperature)"
#define PLUGIN_READ_TIMEOUT_154   1500 // DUCOBOX askes "live" CO2 sensor info, so answer takes sometimes a second.
#define PLUGIN_LOG_PREFIX_154   String("[P154] DUCO Ext. CO2 Sensor: ")

boolean Plugin_154_init = false;
// when calling 'PLUGIN_READ', if serial port is in use set this flag and check in PLUGIN_ONCE_A_SECOND if serial port is free.
bool P154_waitingForSerialPort[TASKS_MAX];



// a duco temp/humidity sensor can report the two values to the same IDX (domoticz)
// a duco CO2 sensor reports CO2 PPM and temperature. each needs an own IDX (domoticz)

typedef enum {
   DUCO_DATA_EXT_SENSOR_TEMP = 0,
   DUCO_DATA_EXT_SENSOR_RH = 1,
   DUCO_DATA_EXT_SENSOR_CO2_PPM = 2,
} DucoExternalSensorDataTypes;

typedef enum {
    P154_CONFIG_DEVICE = 0,
    P154_CONFIG_NODE_ADDRESS = 1,
    P154_CONFIG_LOG_SERIAL = 2,
} P154PluginConfigs;

typedef enum {
    P154_DUCO_DEVICE_NA = 0,
    P154_DUCO_DEVICE_CO2 = 1,
    P154_DUCO_DEVICE_CO2_TEMP = 2,
    P154_DUCO_DEVICE_RH = 3,
} P154DucoSensorDeviceTypes;

typedef enum {
    P154_DUCO_PARAMETER_TEMP = 73,
    P154_DUCO_PARAMETER_CO2 = 74,
    P154_DUCO_PARAMETER_RH = 75,
} P154DucoParameters;

boolean Plugin_154(byte function, struct EventStruct *event, String& string){
   boolean success = false;

   switch (function){

      case PLUGIN_DEVICE_ADD:{
         Device[++deviceCount].Number = PLUGIN_ID_154;
         Device[deviceCount].Type = DEVICE_TYPE_DUMMY;
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

      case PLUGIN_GET_DEVICENAME:{
         string = F(PLUGIN_NAME_154);
         break;
      }

      case PLUGIN_GET_DEVICEVALUENAMES:{
			if(PCONFIG(P154_CONFIG_DEVICE) == P154_DUCO_DEVICE_CO2){
       		safe_strncpy(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR("CO2_PPM"), sizeof(ExtraTaskSettings.TaskDeviceValueNames[0]));
			}else if(PCONFIG(P154_CONFIG_DEVICE) == P154_DUCO_DEVICE_CO2_TEMP){
       		safe_strncpy(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR("Temperature"), sizeof(ExtraTaskSettings.TaskDeviceValueNames[0]));
			}
         break;
      }

      case PLUGIN_WEBFORM_LOAD:{
         String options[3];
         options[P154_DUCO_DEVICE_NA] = "";
         options[P154_DUCO_DEVICE_CO2] = F("CO2 Sensor (CO2 PPM)");
         options[P154_DUCO_DEVICE_CO2_TEMP] = F("CO2 Sensor (Temperature)");

         addHtml(F("<TR><TD>Sensor type:<TD>"));
         byte choice = PCONFIG(P154_CONFIG_DEVICE);
         addSelector(String(F("Plugin_154_DEVICE_TYPE")), 3, options, NULL, NULL, choice, true);
         addFormNumericBox(F("Sensor Node address"), F("Plugin_154_NODE_ADDRESS"), PCONFIG(P154_CONFIG_NODE_ADDRESS), 0, 5000);
         addFormCheckBox(F("Log serial messages to syslog"), F("Plugin154_log_serial"), PCONFIG(P154_CONFIG_LOG_SERIAL));

         success = true;
         break;
      }

      case PLUGIN_WEBFORM_SAVE:{
      	PCONFIG(P154_CONFIG_DEVICE) = getFormItemInt(F("Plugin_154_DEVICE_TYPE"));

      	if(PCONFIG(P154_CONFIG_DEVICE) != P154_DUCO_DEVICE_NA){
         	PCONFIG(P154_CONFIG_NODE_ADDRESS) = getFormItemInt(F("Plugin_154_NODE_ADDRESS"));
            ZERO_FILL(ExtraTaskSettings.TaskDeviceValueNames[0]);
      	}

        	PCONFIG(P154_CONFIG_LOG_SERIAL) = isFormItemChecked(F("Plugin154_log_serial"));
        	success = true;
        	break;
      }
      
	   case PLUGIN_EXIT:{
         addLog(LOG_LEVEL_INFO, PLUGIN_LOG_PREFIX_154 + F("EXIT PLUGIN_154"));
         clearPluginTaskData(event->TaskIndex); // clear plugin taskdata
         success = true;
         break;
	   }


		case PLUGIN_INIT:{
            if(!Plugin_154_init && !ventilation_gateway_disable_serial){
               Serial.begin(115200, SERIAL_8N1);
            }
         addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_154 + F("Init plugin done."));
         Plugin_154_init = true;
         P154_waitingForSerialPort[event->TaskIndex] = false;
			success = true;
			break;
        }

      case PLUGIN_READ:{
        	if (Plugin_154_init && (PCONFIG(P154_CONFIG_NODE_ADDRESS) != 0) && !ventilation_gateway_disable_serial){

            addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_154 + F("start read, eventid:") +event->TaskIndex);


	         // check if serial port is in use by another task, otherwise set flag.
			   if(serialPortInUseByTask == 255){
				   serialPortInUseByTask = event->TaskIndex;

               addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_154 + F("Read external CO2 sensor."));
               if(PCONFIG(P154_CONFIG_DEVICE) == P154_DUCO_DEVICE_CO2){
                  startReadExternalSensors(PLUGIN_LOG_PREFIX_154, DUCO_DATA_EXT_SENSOR_CO2_PPM, PCONFIG(P154_CONFIG_NODE_ADDRESS));
               }else if(PCONFIG(P154_CONFIG_DEVICE) == P154_DUCO_DEVICE_CO2_TEMP){
                  startReadExternalSensors(PLUGIN_LOG_PREFIX_154, DUCO_DATA_EXT_SENSOR_TEMP, PCONFIG(P154_CONFIG_NODE_ADDRESS));
               }else{
                  serialPortInUseByTask = 255; // no device type, stop reading.
               }

            }else{
               char serialPortInUse[40];
               snprintf(serialPortInUse, sizeof(serialPortInUse)," %u, set flag to read data later.", serialPortInUseByTask);
	            addLog(LOG_LEVEL_DEBUG,PLUGIN_LOG_PREFIX_154 + F("Serial port in use by taskid") + serialPortInUse );
               P154_waitingForSerialPort[event->TaskIndex] = true;

			   }
         }

         success = true;
         break;
      }

      case PLUGIN_ONCE_A_SECOND:{
         if(!ventilation_gateway_disable_serial){

            if(P154_waitingForSerialPort[event->TaskIndex]){
               if(serialPortInUseByTask == 255){
                  Plugin_154(PLUGIN_READ, event, string);
                  P154_waitingForSerialPort[event->TaskIndex] = false;

               }
            }

            if(serialPortInUseByTask == event->TaskIndex){
               if( (millis() - ducoSerialStartReading) > PLUGIN_READ_TIMEOUT_154){
                  addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_154 + F("Serial reading timeout"));
                  DucoTaskStopSerial(PLUGIN_LOG_PREFIX_154);
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
                     if(PCONFIG(P154_CONFIG_DEVICE) == P154_DUCO_DEVICE_CO2){
                        readExternalSensorsProcessRow(PLUGIN_LOG_PREFIX_154, event->BaseVarIndex, DUCO_DATA_EXT_SENSOR_CO2_PPM, PCONFIG(P154_CONFIG_NODE_ADDRESS), PCONFIG(P154_CONFIG_LOG_SERIAL));
                     }else if(PCONFIG(P154_CONFIG_DEVICE) == P154_DUCO_DEVICE_CO2_TEMP){
                        readExternalSensorsProcessRow(PLUGIN_LOG_PREFIX_154, event->BaseVarIndex, DUCO_DATA_EXT_SENSOR_TEMP, PCONFIG(P154_CONFIG_NODE_ADDRESS), PCONFIG(P154_CONFIG_LOG_SERIAL));
                     }
                     duco_serial_bytes_read = 0; // reset bytes read counter
                     break;
                  }
                  case DUCO_MESSAGE_END: {
                     DucoThrowErrorMessage(PLUGIN_LOG_PREFIX_154, result);
                     DucoTaskStopSerial(PLUGIN_LOG_PREFIX_154);
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
            DucoSerialSendCommand(PLUGIN_LOG_PREFIX_154);
			}
		}

	   success = true;
    	break;
  	}


  }
  return success;
}



   /*
    * Read external sensor information; this could contain CO2, Temperature and
    * Relative Humidity values, depending on the installed external sensors.
    */
        // SEND COMMAND: nodeparaget <Node> 74
void startReadExternalSensors(String logPrefix, uint8_t dataType, int nodeAddress){
   char logBuf[65];
	char command[20] = ""; /* 17 bytes + max 2 byte nodenumber + \r\n */
   uint8_t parameter;
	char dataTypeName[5] = "";

	if(dataType == DUCO_DATA_EXT_SENSOR_TEMP){ 
		safe_strncpy(dataTypeName, "temp", sizeof(dataTypeName));
		parameter = P154_DUCO_PARAMETER_TEMP;
	}else if(dataType == DUCO_DATA_EXT_SENSOR_RH){ 
		safe_strncpy(dataTypeName, "CO2", sizeof(dataTypeName));
		parameter = P154_DUCO_PARAMETER_RH;
	}else	if(dataType == DUCO_DATA_EXT_SENSOR_CO2_PPM){ 
		safe_strncpy(dataTypeName, "RH", sizeof(dataTypeName));
		parameter = P154_DUCO_PARAMETER_CO2;
	}else{ 
		return;
	}

   snprintf(logBuf, sizeof(logBuf), "Start read external sensor. NodeAddress: %u. Type: %s", nodeAddress, dataTypeName);
	addLog(LOG_LEVEL_INFO, logPrefix + logBuf );

   // set this variables before sending command
	ducoSerialStartReading = millis();
	duco_serial_bytes_read = 0; // reset bytes read counter
	duco_serial_rowCounter = 0; // reset row counter

   // SEND COMMAND
   snprintf_P(command, sizeof(command), "nodeparaget %d %d\r\n", nodeAddress, parameter);
	DucoSerialStartSendCommand(command);

/*
   snprintf_P(command, sizeof(command), "nodeparaget %d %d\r\n", nodeAddress, parameter);
   bool commandSendResult = DucoSerialSendCommand(logPrefix, command);

	// if succesfully send command then receive response
	if (!commandSendResult) {
      addLog(LOG_LEVEL_ERROR, logPrefix + F("Failed to send commando to Ducobox"));
      DucoTaskStopSerial(logPrefix);
   }
   */
}

                /* Example output:
                   > nodeparaget 2 73
                    Get PARA 73 of NODE 2
                    --> 216
                    Done
                    
                  > nodeparaget 2 74
                    Get PARA 74 of NODE 2
                    --> 492
                    Done
                 */
void readExternalSensorsProcessRow(String logPrefix, uint8_t userVarIndex, uint8_t dataType, int nodeAddress, bool serialLoggingEnabled){
    // get the first row to check for command, skip row 2 & 3, get row 4 (columnnames)


      if(serialLoggingEnabled){	     
         addLog(LOG_LEVEL_DEBUG, logPrefix + F("ROW:") + (duco_serial_rowCounter) + F(" bytes:") + duco_serial_bytes_read);
         DucoSerialLogArray(logPrefix, duco_serial_buf, duco_serial_bytes_read, 0);
      }

	if(duco_serial_rowCounter == 1){
      uint8_t parameter;
	   char command[20] = ""; /* 17 bytes + max 2 byte nodenumber + \r\n */

      switch(dataType){
         case DUCO_DATA_EXT_SENSOR_TEMP:{ parameter = P154_DUCO_PARAMETER_TEMP; break; }
         case DUCO_DATA_EXT_SENSOR_RH:{ parameter = P154_DUCO_PARAMETER_RH; break; }
         case DUCO_DATA_EXT_SENSOR_CO2_PPM:{ parameter = P154_DUCO_PARAMETER_CO2; break; }
         default: { return; break; }
      }
         
      snprintf_P(command, sizeof(command), "nodeparaget %d %d", nodeAddress, parameter);

		if (DucoSerialCheckCommandInResponse(logPrefix, command) ) {
     		addLog(LOG_LEVEL_DEBUG, logPrefix + F("Command confirmed by ducobox"));
      } else {
         addLog(LOG_LEVEL_ERROR, logPrefix + F("Received invalid response"));
			DucoTaskStopSerial(logPrefix);
         return;
      }
	}else if(duco_serial_rowCounter > 2){



      duco_serial_buf[duco_serial_bytes_read] = '\0';
      unsigned int raw_value;
      char logBuf[30];

      if (sscanf((const char*)duco_serial_buf, "  --> %u", &raw_value) == 1) {

         if(dataType == DUCO_DATA_EXT_SENSOR_CO2_PPM){
            unsigned int co2_ppm = raw_value; /* No conversion required */
            UserVar[userVarIndex] = co2_ppm;
            snprintf(logBuf, sizeof(logBuf), "CO2 PPM: %u = %u PPM", raw_value, co2_ppm);

         }else if(dataType == DUCO_DATA_EXT_SENSOR_TEMP){
            float temp = (float) raw_value / 10.;
            UserVar[userVarIndex] = temp;
            snprintf(logBuf, sizeof(logBuf), "TEMP: %u = %.1f°C", raw_value, temp);

         }else if(dataType == DUCO_DATA_EXT_SENSOR_RH){
            float rh = (float) raw_value / 100.;
            UserVar[userVarIndex] = rh;
            snprintf(logBuf, sizeof(logBuf), "RH: %u = %.2f%%", raw_value, rh);
         }
         addLog(LOG_LEVEL_INFO, logPrefix + logBuf);
      }
   } 
} // end of readExternalSensors()

