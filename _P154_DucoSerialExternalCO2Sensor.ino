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
#define PLUGIN_READ_TIMEOUT_154   1000 // DUCOBOX askes "live" CO2 sensor info, so answer takes sometimes a second.
#define PLUGIN_LOG_PREFIX_154   String("[P154] DUCO Ext. CO2 Sensor: ")


/* if there are multiple external sensors we want to read them with the same plugin,*/
//boolean Plugin_154_init[TASKS_MAX];

boolean Plugin_154_init = false;
/*
boolean Plugin_154_toggle_sensor = false; // switches between reading temperature and reading CO2/RH
uint8_t Plugin_154_task_index = 0;
*/

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

boolean Plugin_154(byte function, struct EventStruct *event, String& string)
{
  boolean success = false;

  switch (function)
  {

    case PLUGIN_DEVICE_ADD:
      {
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

    case PLUGIN_GET_DEVICENAME:
      {
        string = F(PLUGIN_NAME_154);
        break;
      }

    case PLUGIN_GET_DEVICEVALUENAMES:
      {
			if(PCONFIG(P154_CONFIG_DEVICE) == P154_DUCO_DEVICE_CO2){
       		safe_strncpy(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR("CO2_PPM"), sizeof(ExtraTaskSettings.TaskDeviceValueNames[0]));
			}else if(PCONFIG(P154_CONFIG_DEVICE) == P154_DUCO_DEVICE_CO2_TEMP){
       		safe_strncpy(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR("Temperature"), sizeof(ExtraTaskSettings.TaskDeviceValueNames[0]));
			}
        break;
      }

    case PLUGIN_WEBFORM_LOAD:
      {
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

    case PLUGIN_WEBFORM_SAVE:
      {
      	PCONFIG(P154_CONFIG_DEVICE) = getFormItemInt(F("Plugin_154_DEVICE_TYPE"));

      	if(PCONFIG(P154_CONFIG_DEVICE) != P154_DUCO_DEVICE_NA){
         	PCONFIG(P154_CONFIG_NODE_ADDRESS) = getFormItemInt(F("Plugin_154_NODE_ADDRESS"));
            ZERO_FILL(ExtraTaskSettings.TaskDeviceValueNames[0]);
      	}

        	PCONFIG(P154_CONFIG_LOG_SERIAL) = isFormItemChecked(F("Plugin154_log_serial"));

        	success = true;
        	break;
      }
      
		case PLUGIN_EXIT:
	{
		addLog(LOG_LEVEL_INFO, PLUGIN_LOG_PREFIX_150 + "EXIT PLUGIN_154");
      clearPluginTaskData(event->TaskIndex); // clear plugin taskdata
		success = true;
		break;
	}


		case PLUGIN_INIT:{
         if(!Plugin_154_init){
            Serial.begin(115200, SERIAL_8N1);
            addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_154 + "Init plugin done.");
            Plugin_154_init = true;
         }

			success = true;
			break;
        }

    case PLUGIN_READ:
      {
        	if (Plugin_154_init && (PCONFIG(P154_CONFIG_NODE_ADDRESS) != 0) ){
	
         	addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_154 + "Read external CO2 sensor.");
            if(PCONFIG(P154_CONFIG_DEVICE) == P154_DUCO_DEVICE_CO2){
               readExternalSensors(PLUGIN_LOG_PREFIX_154, PLUGIN_READ_TIMEOUT_154, event->BaseVarIndex + 0, DUCO_DATA_EXT_SENSOR_CO2_PPM, PCONFIG(P154_CONFIG_NODE_ADDRESS), PCONFIG(P154_CONFIG_LOG_SERIAL));
				}else if(PCONFIG(P154_CONFIG_DEVICE) == P154_DUCO_DEVICE_CO2_TEMP){
               readExternalSensors(PLUGIN_LOG_PREFIX_154, PLUGIN_READ_TIMEOUT_154, event->BaseVarIndex + 0, DUCO_DATA_EXT_SENSOR_TEMP, PCONFIG(P154_CONFIG_NODE_ADDRESS), PCONFIG(P154_CONFIG_LOG_SERIAL));
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

void readExternalSensors(String logPrefix, long readTimeout, uint8_t userVarIndex, uint8_t dataType, int nodeAddress, bool serialLoggingEnabled){
   char logBuf[34];
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
   
    /* Reset value to NAN; only update values we can correctly read */
    //p154_duco_data[dataType] = NAN;

   snprintf_P(command, sizeof(command), "nodeparaget %d %d\r\n", nodeAddress, parameter);
   int commandSendResult = DucoSerialSendCommand(logPrefix, command);

   if (commandSendResult) {
      uint8_t result = 0; // stores the result of DucoSerialReceiveRow  
      result = DucoSerialReceiveRow(logPrefix, readTimeout, serialLoggingEnabled);

	   if (result != DUCO_MESSAGE_ROW_END){
   	   DucoThrowErrorMessage(PLUGIN_LOG_PREFIX_152, result);
      	return;
      }else{
         /* Expected response is command minus the \n */
         command[strlen(command) - 1] = '\0';
         if (DucoSerialCheckCommandInResponse(logPrefix, command, false)) {
      	   addLog(LOG_LEVEL_DEBUG, logPrefix + "Command confirmed by ducobox");
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

                /* Loop over all lines; make sure string is nul-terminated */
                // receive next row while result is P151_MESSAGE_ROW_END
            while ((result = DucoSerialReceiveRow(logPrefix, readTimeout, serialLoggingEnabled)) == DUCO_MESSAGE_ROW_END)
            {
               if(serialLoggingEnabled){
                  DucoSerialLogArray(logPrefix, duco_serial_buf, duco_serial_bytes_read-1, 0);
               }

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
						return;
               }
				} // while

				// if value not found
				addLog(LOG_LEVEL_INFO, logPrefix + "Value not found in serial message.");


         } else { 
            addLog(LOG_LEVEL_INFO, logPrefix + "Received invalid response (command not confirmed by ducobox)");
         } //DucoSerialCheckCommandInResponse

      } //       if (result != DUCO_MESSAGE_ROW_END){


   }else{ //    if (commandSendResult) {
      // if commandsend failed throw error
   	addLog(LOG_LEVEL_INFO, logPrefix + "Failed to send commando to Ducobox");
   }



} // end of readExternalSensors()

