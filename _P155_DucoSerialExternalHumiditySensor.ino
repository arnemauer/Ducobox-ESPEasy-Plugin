//#################################### Plugin 155: DUCO Serial Box Sensors ##################################
//
//  DUCO Serial Gateway to read out the exernal installed Box Sensors
//
//  https://github.com/arnemauer/Ducobox-ESPEasy-Plugin
//  http://arnemauer.nl/ducobox-gateway/
//#######################################################################################################

#define PLUGIN_155
#define PLUGIN_ID_155           155
#define PLUGIN_NAME_155         "DUCO Serial GW - External Humidity Sensor"
#define PLUGIN_READ_TIMEOUT_155   1000 // DUCOBOX askes "live" CO2 sensor info, so answer takes sometimes a second.
#define PLUGIN_LOG_PREFIX_155   String("[P155] DUCO Ext. Hum. Sensor: ")

#define PLUGIN_VALUENAME1_155 "Temperature"
#define PLUGIN_VALUENAME2_155 "Relative_humidity"

boolean Plugin_155_init = false;

            // a duco temp/humidity sensor can report the two values to the same IDX (domoticz)
            // a duco CO2 sensor reports CO2 PPM and temperature. each needs an own IDX (domoticz)

typedef enum {
    P155_CONFIG_NODE_ADDRESS = 0,
    P155_CONFIG_LOG_SERIAL = 1,
} P155PluginConfigs;

typedef enum {
    P155_DUCO_PARAMETER_TEMP = 73,
    P155_DUCO_PARAMETER_CO2 = 74,
    P155_DUCO_PARAMETER_RH = 75,
} P155DucoParameters;

boolean Plugin_155(byte function, struct EventStruct *event, String& string)
{
  boolean success = false;

  switch (function)
  {
    case PLUGIN_DEVICE_ADD:
      {
        Device[++deviceCount].Number = PLUGIN_ID_155;
        Device[deviceCount].Type = DEVICE_TYPE_DUMMY;
        Device[deviceCount].VType = SENSOR_TYPE_TEMP_HUM;
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
        string = F(PLUGIN_NAME_155);
        break;
      }

    case PLUGIN_GET_DEVICEVALUENAMES:
      {
			strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_155));
			strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[1], PSTR(PLUGIN_VALUENAME2_155));
        break;
      }

    case PLUGIN_WEBFORM_LOAD:
      {
         addFormNumericBox(F("Sensor Node address"), F("Plugin_155_NODE_ADDRESS"), PCONFIG(P155_CONFIG_NODE_ADDRESS), 0, 5000);
         addFormCheckBox(F("Log serial messages to syslog"), F("Plugin155_log_serial"), PCONFIG(P155_CONFIG_LOG_SERIAL));

        success = true;
        break;
      }

    case PLUGIN_WEBFORM_SAVE:
      {
         PCONFIG(P155_CONFIG_NODE_ADDRESS) = getFormItemInt(F("Plugin_155_NODE_ADDRESS"));
        	PCONFIG(P155_CONFIG_LOG_SERIAL) = isFormItemChecked(F("Plugin155_log_serial"));

        	success = true;
        	break;
      }
      
	case PLUGIN_EXIT: {
	   addLog(LOG_LEVEL_INFO, PLUGIN_LOG_PREFIX_150 + "EXIT PLUGIN_155");
      clearPluginTaskData(event->TaskIndex); // clear plugin taskdata
		//ClearCustomTaskSettings(event->TaskIndex); // clear networkID settings
		success = true;
		break;
	}


		case PLUGIN_INIT:{
         //LoadTaskSettings(event->TaskIndex);        

         if(!Plugin_155_init){
            Serial.begin(115200, SERIAL_8N1);
            addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_155 + "Init plugin done.");
            Plugin_155_init = true;
         }

			success = true;
			break;
        }

    case PLUGIN_READ:
      {
        	if (Plugin_155_init && (PCONFIG(P155_CONFIG_NODE_ADDRESS) != 0) ){
         	addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_155 + "Read external duco sensors");
            readExternalSensors(PLUGIN_LOG_PREFIX_155, PLUGIN_READ_TIMEOUT_155, event->BaseVarIndex + 0, DUCO_DATA_EXT_SENSOR_TEMP, PCONFIG(P155_CONFIG_NODE_ADDRESS), PCONFIG(P155_CONFIG_LOG_SERIAL));
            readExternalSensors(PLUGIN_LOG_PREFIX_155, PLUGIN_READ_TIMEOUT_155, event->BaseVarIndex + 1, DUCO_DATA_EXT_SENSOR_RH, PCONFIG(P155_CONFIG_NODE_ADDRESS), PCONFIG(P155_CONFIG_LOG_SERIAL));     
         }

        success = true;
        break;
      }
  }
  return success;
}

