//#################################### Plugin 152: DUCO Serial Box Sensors ##################################
//
//  DUCO Serial Gateway to read out the optionally installed Box Sensors
//
//  https://github.com/arnemauer/Ducobox-ESPEasy-Plugin
//  http://arnemauer.nl/ducobox-gateway/
//#######################################################################################################

#define PLUGIN_152
#define PLUGIN_ID_152           152
#define PLUGIN_NAME_152         "DUCO Serial Box Sensors"
#define PLUGIN_VALUENAME1_152   "BOX_TEMP"     // sensorinfo
#define PLUGIN_VALUENAME2_152   "BOX_RH"       // sensorinfo
#define PLUGIN_VALUENAME3_152   "BOX_CO2_PPM"  // sensorinfo
#define PLUGIN_READ_TIMEOUT_152   1200 // DUCOBOX askes "live" CO2 sensor info, so answer takes sometimes a second.
#define PLUGIN_LOG_PREFIX_152   String("[P152] DUCO BOX SENSOR: ")

boolean Plugin_152_init = false;

typedef enum {
    P152_DATA_BOX_TEMP = 0,
    P152_DATA_BOX_RH = 1,
    P152_DATA_BOX_CO2_PPM = 2,
    P152_DATA_LAST = 3,
} P152DucoDataTypes;
float p152_duco_data[P152_DATA_LAST];

typedef enum {
    P152_CONFIG_IDX_BOX_TEMP = 0,
    P152_CONFIG_IDX_BOX_RH = 1,
    P152_CONFIG_IDX_BOX_CO2_PPM = 2,
} P152PluginConfigs;


boolean Plugin_152(byte function, struct EventStruct *event, String& string)
{
  boolean success = false;

  switch (function)
  {

    case PLUGIN_DEVICE_ADD:
      {
        Device[++deviceCount].Number = PLUGIN_ID_152;
        Device[deviceCount].Type = DEVICE_TYPE_DUMMY;
        Device[deviceCount].VType = SENSOR_TYPE_TRIPLE;
        Device[deviceCount].Ports = 0;
        Device[deviceCount].PullUpOption = false;
        Device[deviceCount].InverseLogicOption = false;
        Device[deviceCount].FormulaOption = false;
        Device[deviceCount].DecimalsOnly = true;
        Device[deviceCount].ValueCount = 3;
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

    case PLUGIN_GET_DEVICEVALUENAMES:
      {
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_152));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[1], PSTR(PLUGIN_VALUENAME2_152));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[2], PSTR(PLUGIN_VALUENAME3_152));
        break;
      }

    case PLUGIN_WEBFORM_LOAD:
      {
        addFormNote(F("For use with domoticz you can define an idx per value. If you use this you can ignore 'Send to Controller' below."));
        addFormNumericBox(F("IDX BOX-sensor temperature"), F("Plugin_152_IDX1"), PCONFIG(P152_CONFIG_IDX_BOX_TEMP), 0, 5000);
        addFormNumericBox(F("IDX BOX-sensor relative humidity"), F("Plugin_152_IDX2"), PCONFIG(P152_CONFIG_IDX_BOX_RH), 0, 5000);
        addFormNumericBox(F("IDX BOX-sensor CO2 PPM"), F("Plugin_152_IDX3"), PCONFIG(P152_CONFIG_IDX_BOX_CO2_PPM), 0, 5000);

        success = true;
        break;
      }

    case PLUGIN_WEBFORM_SAVE:
      {
        PCONFIG(P152_CONFIG_IDX_BOX_TEMP) = getFormItemInt(F("Plugin_152_IDX1"));
        PCONFIG(P152_CONFIG_IDX_BOX_RH) = getFormItemInt(F("Plugin_152_IDX2"));
        PCONFIG(P152_CONFIG_IDX_BOX_CO2_PPM) = getFormItemInt(F("Plugin_152_IDX3"));
        success = true;
        break;
      }
      
    case PLUGIN_INIT:{
            LoadTaskSettings(event->TaskIndex);

            Serial.begin(115200, SERIAL_8N1);
            Plugin_152_init = true;

            p152_duco_data[P152_DATA_BOX_TEMP] = NAN; // box temp
            p152_duco_data[P152_DATA_BOX_RH] = NAN; // box rh
            p152_duco_data[P152_DATA_BOX_CO2_PPM] = NAN; // box CO2 ppm

            success = true;
            break;
        }

    case PLUGIN_READ:
      {
        if (Plugin_152_init){
                addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_152 + "read box sensors");
                readBoxSensors();

                if (UserVar[event->BaseVarIndex + P152_DATA_BOX_TEMP] != p152_duco_data[P152_DATA_BOX_TEMP]) {
                    UserVar[event->BaseVarIndex + P152_DATA_BOX_TEMP] = p152_duco_data[P152_DATA_BOX_TEMP];
                    DucoSerialSendUpdate(PLUGIN_LOG_PREFIX_152, event->TaskIndex, event->BaseVarIndex, P152_DATA_BOX_TEMP, PCONFIG(P152_CONFIG_IDX_BOX_TEMP), 1);
                }

                if (UserVar[event->BaseVarIndex + P152_DATA_BOX_RH] != p152_duco_data[P152_DATA_BOX_RH]) {
                    UserVar[event->BaseVarIndex + P152_DATA_BOX_RH] = p152_duco_data[P152_DATA_BOX_RH];
                    DucoSerialSendUpdate(PLUGIN_LOG_PREFIX_152, event->TaskIndex, event->BaseVarIndex, P152_DATA_BOX_RH, PCONFIG(P152_CONFIG_IDX_BOX_RH), 2);
                }

                if(UserVar[event->BaseVarIndex + P152_DATA_BOX_CO2_PPM] != p152_duco_data[P152_DATA_BOX_CO2_PPM]){
                    UserVar[event->BaseVarIndex + P152_DATA_BOX_CO2_PPM] = p152_duco_data[P152_DATA_BOX_CO2_PPM];
                    DucoSerialSendUpdate(PLUGIN_LOG_PREFIX_152, event->TaskIndex, event->BaseVarIndex, P152_DATA_BOX_CO2_PPM, PCONFIG(P152_CONFIG_IDX_BOX_CO2_PPM), 0);
                }               
            }

        success = true;
        break;
      }
  }
  return success;
}

void readBoxSensors(){
    addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_152 + "Start readBoxSensors");
    
    /* Reset values to NAN; only update values we can correctly read */
    for (int i = 0; i < P152_DATA_LAST; i++) {
        p152_duco_data[i] = NAN;
    }

    /*
     * Read box sensor information; this could contain CO2, Temperature and
     * Relative Humidity values, depending on the installed box sensors.
     */
    char command[] = "sensorinfo\r\n";
    int commandSendResult = DucoSerialSendCommand(PLUGIN_LOG_PREFIX_152, command);
    if (commandSendResult) {
        if (DucoSerialReceiveData(PLUGIN_LOG_PREFIX_152, PLUGIN_READ_TIMEOUT_152, false)) {
            if (DucoSerialCheckCommandInResponse(PLUGIN_LOG_PREFIX_152, "sensorinfo", false)) {
                addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_152 + "Received correct response on sensorinfo");
                /* Example output:
                    [SENSOR] INFO
                    CO2 :  627 [ppm] (0)
                    RH : 3246 [.01%] (0)
                    TEMP :  235 [.1°C] (0)
                 */
                /* Loop over all lines; make sure string is nul-terminated */
                if (duco_serial_bytes_read == 0 || duco_serial_bytes_read > sizeof(duco_serial_buf)) {
                    addLog(LOG_LEVEL_ERROR, PLUGIN_LOG_PREFIX_152 + "Invalid bytes read returned: " + duco_serial_bytes_read);
                    return;
                }

                duco_serial_buf[duco_serial_bytes_read] = '\0';

                char *buf = (char*)duco_serial_buf;
	              char *token = strtok(buf, "\r");
                unsigned int raw_value;
                while (token != NULL) {
                    char logBuf[30];

                    if (sscanf(token, "  CO2 : %u", &raw_value) == 1) {
                        unsigned int co2_ppm = raw_value; /* No conversion required */
                        p152_duco_data[P152_DATA_BOX_CO2_PPM] = co2_ppm;

                        snprintf(logBuf, sizeof(logBuf), "CO2 PPM: %u = %u PPM", raw_value, co2_ppm);
                        addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_152 + logBuf);
                    }

                    if (sscanf(token, "  RH : %u", &raw_value) == 1) {
                        float rh = (float) raw_value / 100.;
                        p152_duco_data[P152_DATA_BOX_RH] = rh;

                        snprintf(logBuf, sizeof(logBuf), "RH: %u = %.2f%%", raw_value, rh);
                        addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_152 + logBuf);
                    }

                    if (sscanf(token, "  TEMP : %u", &raw_value) == 1) {
                        float temp = (float) raw_value / 10.;
                        p152_duco_data[P152_DATA_BOX_TEMP] = temp;

                        snprintf(logBuf, sizeof(logBuf), "TEMP: %u = %.1f°C", raw_value, temp);
                        addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_152 + logBuf);
                    }
                    token = strtok(NULL, "\r");
                }
            } else { 
                addLog(LOG_LEVEL_INFO, PLUGIN_LOG_PREFIX_152 + "Received invalid response on sensorinfo");
            }
        }
    }
}
