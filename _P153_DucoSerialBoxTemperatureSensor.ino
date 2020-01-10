//#################################### Plugin 153: DUCO Serial Box Sensors ##################################
//
//  DUCO Serial Gateway to read out the optionally installed Box Sensors
//
//  https://github.com/arnemauer/Ducobox-ESPEasy-Plugin
//  http://arnemauer.nl/ducobox-gateway/
//#######################################################################################################

#define PLUGIN_153
#define PLUGIN_ID_153           153
#define PLUGIN_NAME_153         "DUCO Serial GW - Box Sensor - CO2 (temp) & humidity sensor (temp & hum)"
#define PLUGIN_READ_TIMEOUT_153   1200 // DUCOBOX askes "live" CO2 sensor info, so answer takes sometimes a second.
#define PLUGIN_LOG_PREFIX_153   String("[P153] DUCO BOX SENSOR: ")

boolean Plugin_153_init = false;

typedef enum {
    P153_DATA_BOX_TEMP = 0,
    P153_DATA_BOX_RH = 1,
    P153_DATA_BOX_CO2_PPM = 2,
    P153_DATA_LAST = 3,
} P153DucoDataTypes;
float p153_duco_data[P153_DATA_LAST];

typedef enum {
    P153_CONFIG_DEVICE = 0,
    P153_CONFIG_LOG_SERIAL = 1,
} P153PluginConfigs;



boolean Plugin_153(byte function, struct EventStruct *event, String& string)
{
  boolean success = false;

  switch (function)
  {

    case PLUGIN_DEVICE_ADD:
      {
        Device[++deviceCount].Number = PLUGIN_ID_153;
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
        string = F(PLUGIN_NAME_153);
        break;
      }

	case PLUGIN_GET_DEVICEVALUENAMES: {
		safe_strncpy(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR("Temperature"), sizeof(ExtraTaskSettings.TaskDeviceValueNames[0]));
		safe_strncpy(ExtraTaskSettings.TaskDeviceValueNames[1], PSTR("Relative_humidity"), sizeof(ExtraTaskSettings.TaskDeviceValueNames[1]));
		break;
   }

    case PLUGIN_WEBFORM_LOAD: {

		addRowLabel(F("Sensor type:"));
		addSelector_Head(PCONFIG_LABEL(P153_CONFIG_DEVICE), false);

		addSelector_Item("", P152_DUCO_DEVICE_NA, PCONFIG(P153_CONFIG_DEVICE) == P152_DUCO_DEVICE_NA, false, "");
		addSelector_Item("CO2 Sensor (Temperature)", P152_DUCO_DEVICE_CO2_TEMP, PCONFIG(P153_CONFIG_DEVICE) == P152_DUCO_DEVICE_CO2_TEMP, false, "");
		addSelector_Item("Humidity Sensor (Temperature & Humidity)", P152_DUCO_DEVICE_RH, PCONFIG(P153_CONFIG_DEVICE) == P152_DUCO_DEVICE_RH, false, "");
		addSelector_Foot();

      addFormCheckBox(F("Log serial messages to syslog"), F("Plugin153_log_serial"), PCONFIG(P153_CONFIG_LOG_SERIAL));
      success = true;
      break;
   }

   case PLUGIN_WEBFORM_SAVE: {
      PCONFIG(P153_CONFIG_DEVICE) = getFormItemInt(PCONFIG_LABEL(P153_CONFIG_DEVICE));
      PCONFIG(P153_CONFIG_LOG_SERIAL) = isFormItemChecked(F("Plugin153_log_serial"));
      success = true;
      break;
	}
      
	case PLUGIN_INIT:{
      Serial.begin(115200, SERIAL_8N1);
      Plugin_153_init = true;
      success = true;
      break;
   }

	case PLUGIN_READ:{
		if (Plugin_153_init){
      	addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_153 + "read box sensors");
			readBoxSensors(PLUGIN_LOG_PREFIX_153, PCONFIG(P153_CONFIG_DEVICE), event->BaseVarIndex, PCONFIG(P153_CONFIG_LOG_SERIAL));
      }

      success = true;
      break;
   }



  }
  return success;
}


