//#################################### Plugin 152: DUCO Serial Box Sensors ##################################
//
//  DUCO Serial Gateway to read out the optionally installed Box Sensors
//
//  https://github.com/arnemauer/Ducobox-ESPEasy-Plugin
//  http://arnemauer.nl/ducobox-gateway/
//#######################################################################################################

#define PLUGIN_152
#define PLUGIN_ID_152           152
#define PLUGIN_NAME_152         "DUCO Serial GW - Box Sensor - CO2"
#define PLUGIN_READ_TIMEOUT_152   1200 // DUCOBOX askes "live" CO2 sensor info, so answer takes sometimes a second.
#define PLUGIN_LOG_PREFIX_152   String("[P152] DUCO BOX SENSOR: ")

boolean Plugin_152_init = false;

typedef enum {
   P152_CONFIG_LOG_SERIAL = 0,
} P152PluginConfigs;

typedef enum {
   P152_DUCO_DEVICE_NA = 0, 
   P152_DUCO_DEVICE_CO2 = 1, // not used here!
   P152_DUCO_DEVICE_CO2_TEMP = 2,
   P152_DUCO_DEVICE_RH = 3,
} P152DucoSensorDeviceTypes;

boolean Plugin_152(byte function, struct EventStruct *event, String& string)
{
  boolean success = false;

  switch (function)
  {

    case PLUGIN_DEVICE_ADD:
      {
        Device[++deviceCount].Number = PLUGIN_ID_152;
        Device[deviceCount].Type = DEVICE_TYPE_DUMMY;
        Device[deviceCount].VType = SENSOR_TYPE_DUAL;
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
   	//LoadTaskSettings(event->TaskIndex);

      Serial.begin(115200, SERIAL_8N1);
      Plugin_152_init = true;
      success = true;
		break;
   }

	case PLUGIN_READ: {
   	if (Plugin_152_init){
      	addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_152 + "read box sensors");
         readBoxSensors(PLUGIN_LOG_PREFIX_152, P152_DUCO_DEVICE_CO2, event->BaseVarIndex, PCONFIG(P152_CONFIG_LOG_SERIAL));
      }

      success = true;
      break;
   }



  }
  return success;
}








void readBoxSensors( String logPrefix, uint8_t sensorDeviceType, uint8_t userVarIndex, bool serialLoggingEnabled){
   addLog(LOG_LEVEL_DEBUG, logPrefix + "Start readBoxSensors");
    
   /*
    * Read box sensor information; this could contain CO2, Temperature and
    * Relative Humidity values, depending on the installed box sensors.
    */
   char command[] = "sensorinfo\r\n";
   int commandSendResult = DucoSerialSendCommand(logPrefix, command);
   if (commandSendResult) {
   	uint8_t result = 0; // stores the result of DucoSerialReceiveRow
   	result = DucoSerialReceiveRow(logPrefix, PLUGIN_READ_TIMEOUT_152, serialLoggingEnabled);

		if (result != DUCO_MESSAGE_ROW_END){
      	DucoThrowErrorMessage(logPrefix, result);
      	return;
      }else{
 
      	if (DucoSerialCheckCommandInResponse(logPrefix, "sensorinfo", false)) {
         	addLog(LOG_LEVEL_DEBUG, logPrefix + "Received correct response on sensorinfo");
            /* Example output:
                [SENSOR] INFO
                CO2 :  627 [ppm] (0)
                RH : 3246 [.01%] (0)
                TEMP :  235 [.1°C] (0)
            */
            
				// receive next row while result is P151_MESSAGE_ROW_END
            while ((result = DucoSerialReceiveRow(logPrefix, PLUGIN_READ_TIMEOUT_152, serialLoggingEnabled)) == DUCO_MESSAGE_ROW_END){
					if(serialLoggingEnabled){
               	DucoSerialLogArray(logPrefix,duco_serial_buf, duco_serial_bytes_read-1, 0);
               }

               duco_serial_buf[duco_serial_bytes_read] = '\0';
               unsigned int raw_value;
               char logBuf[30];


					if(sensorDeviceType == P152_DUCO_DEVICE_CO2){
               	if (sscanf((const char*)duco_serial_buf, "  CO2 : %u", &raw_value) == 1) {
                  	unsigned int co2_ppm = raw_value; /* No conversion required */
                     UserVar[userVarIndex] = co2_ppm;
                     snprintf(logBuf, sizeof(logBuf), "CO2 PPM: %u = %u PPM", raw_value, co2_ppm);
                     addLog(LOG_LEVEL_DEBUG, logPrefix + logBuf);
                  }
					}

					if(sensorDeviceType == P152_DUCO_DEVICE_RH){
	               if (sscanf((const char*)duco_serial_buf, "  RH : %u", &raw_value) == 1) {
                     float rh = (float) raw_value / 100.;
                     UserVar[userVarIndex + 1] = rh;
                     snprintf(logBuf, sizeof(logBuf), "RH: %u = %.2f%%", raw_value, rh);
                     addLog(LOG_LEVEL_DEBUG, logPrefix + logBuf);
                  }
					}

					if(sensorDeviceType == P152_DUCO_DEVICE_CO2_TEMP || sensorDeviceType == P152_DUCO_DEVICE_RH){
	               if (sscanf((const char*)duco_serial_buf, "  TEMP : %u", &raw_value) == 1) {
                     float temp = (float) raw_value / 10.;
                     UserVar[userVarIndex] = temp;

                     snprintf(logBuf, sizeof(logBuf), "TEMP: %u = %.1f°C", raw_value, temp);
                     addLog(LOG_LEVEL_DEBUG, logPrefix + logBuf);
                  }
					}

            } // while

            if (result != DUCO_MESSAGE_ROW_END){
            	DucoThrowErrorMessage(logPrefix, result);
            }

               
         } else { 
            addLog(LOG_LEVEL_INFO, logPrefix + "Received invalid response on sensorinfo");
         }
      }
   }
   return;
}

