//#################################### Plugin 153: DUCO Serial Box Sensors ##################################
//
//  DUCO Serial Gateway to read out the exernal installed Box Sensors
//
//  https://github.com/arnemauer/Ducobox-ESPEasy-Plugin
//  http://arnemauer.nl/ducobox-gateway/
//#######################################################################################################

// TODO: add a device parser for device droplist.

#define PLUGIN_153
#define PLUGIN_ID_153           153
#define PLUGIN_NAME_153         "DUCO Serial External Sensors"
#define PLUGIN_VALUENAME1_153   "Temperature"     // sensorinfo
#define PLUGIN_VALUENAME2_153   "Relative_humidity"       // sensorinfo
#define PLUGIN_VALUENAME3_153   "CO2_PPM"  // sensorinfo
#define PLUGIN_READ_TIMEOUT_153   1200 // DUCOBOX askes "live" CO2 sensor info, so answer takes sometimes a second.
#define PLUGIN_LOG_PREFIX_153   String("[P153] DUCO EXTERNAL SENSOR: ")

boolean Plugin_153_init = false;

typedef enum {
    P153_DATA_SENSOR_TEMP = 0,
    P153_DATA_SENSOR_RH = 1,
    P153_DATA_SENSOR_CO2_PPM = 2,
    P153_DATA_LAST = 3,
} P153DucoDataTypes;
float p153_duco_data[P153_DATA_LAST];

typedef enum {
    P153_CONFIG_IDX_SENSOR_TEMP = 0,
    P153_CONFIG_IDX_SENSOR_RH = 1,
    P153_CONFIG_IDX_SENSOR_CO2_PPM = 2,
    P153_CONFIG_DEVICE_TYPE = 3,
    P153_CONFIG_NODE_ADDRESS = 4,
} P153PluginConfigs;

typedef enum {
    P153_DUCO_DEVICE_NA = 0,
    P153_DUCO_DEVICE_CO2 = 1,
    P153_DUCO_DEVICE_RH = 2,
      /* add more device:
        - Sensorless Control valve
        - CO2 Control valve
        - Humidity Control valve
        - window ventilator
        - Actuatorprint
      */
} P153DucoSensorDeviceTypes;

typedef enum {
    P153_DUCO_PARAMETER_TEMP = 73,
    P153_DUCO_PARAMETER_CO2 = 74,
    P153_DUCO_PARAMETER_RH = 75,
      /* add more device:
        - Sensorless Control valve
        - CO2 Control valve
        - Humidity Control valve
        - window ventilator
        - Actuatorprint
      */
} P153DucoParameters;

boolean Plugin_153(byte function, struct EventStruct *event, String& string)
{
  boolean success = false;

  switch (function)
  {

    case PLUGIN_DEVICE_ADD:
      {
        Device[++deviceCount].Number = PLUGIN_ID_153;
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
        string = F(PLUGIN_NAME_153);
        break;
      }

    case PLUGIN_GET_DEVICEVALUENAMES:
      {
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_153));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[1], PSTR(PLUGIN_VALUENAME2_153));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[2], PSTR(PLUGIN_VALUENAME3_153));
        break;
      }

    case PLUGIN_WEBFORM_LOAD:
      {

        String options[3];
        options[P153_DUCO_DEVICE_NA] = "";
        options[P153_DUCO_DEVICE_CO2] = F("CO2 Sensor");
        options[P153_DUCO_DEVICE_RH] = F("Humidity Sensor");

        addHtml(F("<TR><TD>Sensor type:<TD>"));
        byte choice = PCONFIG(P153_CONFIG_DEVICE_TYPE);
        addSelector(String(F("Plugin_153_DEVICE_TYPE")), 3, options, NULL, NULL, choice, true);

        if(choice != P153_DUCO_DEVICE_NA){
          addFormNumericBox(F("Sensor Node address"), F("Plugin_153_NODE_ADDRESS"), PCONFIG(P153_CONFIG_NODE_ADDRESS), 0, 5000);

          addFormNote(F("For use with domoticz you can define an idx per value. If you use this you can ignore 'Send to Controller' below."));
          addFormNumericBox(F("IDX Sensor temperature"), F("Plugin_153_IDX1"), PCONFIG(P153_CONFIG_IDX_SENSOR_TEMP), 0, 5000);
        }
        if(choice == P153_DUCO_DEVICE_CO2){
          addFormNumericBox(F("IDX Sensor CO2 PPM"), F("Plugin_153_IDX3"), PCONFIG(P153_CONFIG_IDX_SENSOR_CO2_PPM), 0, 5000);
        }else if(choice == P153_DUCO_DEVICE_RH){
          addFormNumericBox(F("IDX Sensor relative humidity"), F("Plugin_153_IDX2"), PCONFIG(P153_CONFIG_IDX_SENSOR_RH), 0, 5000);
        }

        success = true;
        break;
      }

    case PLUGIN_WEBFORM_SAVE:
      {

        PCONFIG(P153_CONFIG_DEVICE_TYPE) = getFormItemInt(F("Plugin_153_DEVICE_TYPE"));

        if(PCONFIG(P153_CONFIG_DEVICE_TYPE) =! P153_DUCO_DEVICE_NA){
          PCONFIG(P153_CONFIG_NODE_ADDRESS) = getFormItemInt(F("Plugin_153_NODE_ADDRESS"));
          PCONFIG(P153_CONFIG_IDX_SENSOR_TEMP) = getFormItemInt(F("Plugin_153_IDX1"));
        }

        if(PCONFIG(P153_CONFIG_DEVICE_TYPE) == P153_DUCO_DEVICE_CO2){
          PCONFIG(P153_CONFIG_IDX_SENSOR_CO2_PPM) = getFormItemInt(F("Plugin_153_IDX3"));
        } else if(PCONFIG(P153_CONFIG_DEVICE_TYPE) == P153_DUCO_DEVICE_RH){
          PCONFIG(P153_CONFIG_IDX_SENSOR_RH) = getFormItemInt(F("Plugin_153_IDX2"));
        }

        success = true;
        break;
      }
      
    case PLUGIN_INIT:{
            LoadTaskSettings(event->TaskIndex);        

            // maybe we can change the valuecount based on the selecter sensortype. But... this isnt possible because espeasy searches in Device for the pluginnumber (getDeviceIndex)
            // so... if there are multiple plugins activated we only get de index of the first device.
            /*
            byte DeviceIndex = getDeviceIndex(Settings.TaskDeviceNumber[event->TaskIndex]);
            char logBuf[30];
            snprintf(logBuf, sizeof(logBuf), "taskindex: %u; deviceindex: %u", event->TaskIndex, DeviceIndex);
            addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_153 + logBuf);
            */


            Serial.begin(115200, SERIAL_8N1);
            Plugin_153_init = true;

            p153_duco_data[P153_DATA_SENSOR_TEMP] = NAN; // box temp
            p153_duco_data[P153_DATA_SENSOR_RH] = NAN; // box rh
            p153_duco_data[P153_DATA_SENSOR_CO2_PPM] = NAN; // box CO2 ppm

            success = true;
            break;
        }

    case PLUGIN_READ:
      {
        if (Plugin_153_init){
                addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_153 + "read external duco sensors");

                if(PCONFIG(P153_CONFIG_DEVICE_TYPE) == P153_DUCO_DEVICE_CO2){
                  readExternalSensors(P153_DATA_SENSOR_TEMP, PCONFIG(P153_CONFIG_NODE_ADDRESS));
                  readExternalSensors(P153_DATA_SENSOR_CO2_PPM, PCONFIG(P153_CONFIG_NODE_ADDRESS));
                } else if(PCONFIG(P153_CONFIG_DEVICE_TYPE) == P153_DUCO_DEVICE_RH){
                  readExternalSensors(P153_DATA_SENSOR_TEMP, PCONFIG(P153_CONFIG_NODE_ADDRESS));
                  readExternalSensors(P153_DATA_SENSOR_RH, PCONFIG(P153_CONFIG_NODE_ADDRESS));
                }
                

                if (UserVar[event->BaseVarIndex + P153_DATA_SENSOR_TEMP] != p153_duco_data[P153_DATA_SENSOR_TEMP]) {
                    UserVar[event->BaseVarIndex + P153_DATA_SENSOR_TEMP] = p153_duco_data[P153_DATA_SENSOR_TEMP];
                    DucoSerialSendUpdate(PLUGIN_LOG_PREFIX_153, event->TaskIndex, event->BaseVarIndex, P153_DATA_SENSOR_TEMP, PCONFIG(P153_CONFIG_IDX_SENSOR_TEMP), 1);
                }

                if (UserVar[event->BaseVarIndex + P153_DATA_SENSOR_RH] != p153_duco_data[P153_DATA_SENSOR_RH]) {
                    UserVar[event->BaseVarIndex + P153_DATA_SENSOR_RH] = p153_duco_data[P153_DATA_SENSOR_RH];
                    DucoSerialSendUpdate(PLUGIN_LOG_PREFIX_153, event->TaskIndex, event->BaseVarIndex, P153_DATA_SENSOR_RH, PCONFIG(P153_CONFIG_IDX_SENSOR_RH), 2);
                }

                if(UserVar[event->BaseVarIndex + P153_DATA_SENSOR_CO2_PPM] != p153_duco_data[P153_DATA_SENSOR_CO2_PPM]){
                    UserVar[event->BaseVarIndex + P153_DATA_SENSOR_CO2_PPM] = p153_duco_data[P153_DATA_SENSOR_CO2_PPM];
                    DucoSerialSendUpdate(PLUGIN_LOG_PREFIX_153, event->TaskIndex, event->BaseVarIndex, P153_DATA_SENSOR_CO2_PPM, PCONFIG(P153_CONFIG_IDX_SENSOR_CO2_PPM), 0);
                }               
            }

        success = true;
        break;
      }
  }
  return success;
}

void readExternalSensors(uint8_t dataType, int nodeAddress){
    char logBuf[34];

    /*
     * Read box sensor information; this could contain CO2, Temperature and
     * Relative Humidity values, depending on the installed box sensors.
     */
        // SEND COMMAND: nodeparaget <Node> 74
    char command[20] = ""; /* 17 bytes + max 2 byte nodenumber + \r\n */
    uint8_t parameter;

    if(dataType == P153_DATA_SENSOR_CO2_PPM){
      parameter = P153_DUCO_PARAMETER_CO2;
      snprintf(logBuf, sizeof(logBuf), "NodeAddress: %u. Type: CO2", nodeAddress);
    }else if(dataType == P153_DATA_SENSOR_TEMP){
      parameter = P153_DUCO_PARAMETER_TEMP;
      snprintf(logBuf, sizeof(logBuf), "NodeAddress: %u. Type: temp", nodeAddress);
    }else if(dataType == P153_DATA_SENSOR_RH){
      parameter = P153_DUCO_PARAMETER_RH;
      snprintf(logBuf, sizeof(logBuf), "NodeAddress: %u. Type: RH", nodeAddress);
    }else{
      return;
    }
    
          
    /* Reset value to NAN; only update values we can correctly read */
        p153_duco_data[dataType] = NAN;

    addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_153 + "Start read external sensor. " + logBuf );

    snprintf_P(command, sizeof(command), "nodeparaget %d %d\r\n", nodeAddress, parameter);
    int commandSendResult = DucoSerialSendCommand(PLUGIN_LOG_PREFIX_153, command);

    if (commandSendResult) {
        if (DucoSerialReceiveData(PLUGIN_LOG_PREFIX_153, PLUGIN_READ_TIMEOUT_153, false)) {
            /* Expected response is command minus the \n */
            command[strlen(command) - 1] = '\0';
            if (DucoSerialCheckCommandInResponse(PLUGIN_LOG_PREFIX_153, command, false)) {
                addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_153 + "Received correct response on sensorinfo");
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
                if (duco_serial_bytes_read == 0 || duco_serial_bytes_read > sizeof(duco_serial_buf)) {
                    addLog(LOG_LEVEL_ERROR, PLUGIN_LOG_PREFIX_153 + "Invalid bytes read returned: " + duco_serial_bytes_read);
                    return;
                }

                duco_serial_buf[duco_serial_bytes_read] = '\0';

                char *buf = (char*)duco_serial_buf;
	            char *token = strtok(buf, "\r");
                unsigned int raw_value;
                while (token != NULL) {
                    char logBuf[30];
                    if (sscanf(token, "  --> %u", &raw_value) == 1) {

                      if(dataType == P153_DATA_SENSOR_CO2_PPM){
                        unsigned int co2_ppm = raw_value; /* No conversion required */
                        p153_duco_data[P153_DATA_SENSOR_CO2_PPM] = co2_ppm;
                        snprintf(logBuf, sizeof(logBuf), "CO2 PPM: %u = %u PPM", raw_value, co2_ppm);

                      }else if(dataType == P153_DATA_SENSOR_TEMP){
                        float temp = (float) raw_value / 10.;
                        p153_duco_data[P153_DATA_SENSOR_TEMP] = temp;
                        snprintf(logBuf, sizeof(logBuf), "TEMP: %u = %.1f°C", raw_value, temp);

                      }else if(dataType == P153_DATA_SENSOR_RH){
                        float rh = (float) raw_value / 100.;
                        p153_duco_data[P153_DATA_SENSOR_RH] = rh;
                        snprintf(logBuf, sizeof(logBuf), "RH: %u = %.2f%%", raw_value, rh);

                      }
                      addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_153 + logBuf);
                    }

                    token = strtok(NULL, "\r");
                }
            } else { 
                addLog(LOG_LEVEL_INFO, PLUGIN_LOG_PREFIX_153 + "Received invalid response on sensorinfo");
            }
        }
    }
}


