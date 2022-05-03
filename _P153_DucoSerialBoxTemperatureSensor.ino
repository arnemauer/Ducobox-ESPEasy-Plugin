//#################################### Plugin 153: DUCO Serial Box Sensors ##################################
//
//  DUCO Serial Gateway to read out the optionally installed Box Sensors
//
//  https://github.com/arnemauer/Ducobox-ESPEasy-Plugin
//  http://arnemauer.nl/ducobox-gateway/
//#######################################################################################################

#define PLUGIN_153
#define PLUGIN_ID_153           153
#define PLUGIN_NAME_153         "DUCO Serial GW - Box Sensor - Temperature & humidity sensor"
#define PLUGIN_READ_TIMEOUT_153   1000 // DUCOBOX askes "live" CO2 sensor info, so answer takes sometimes a second.
#define PLUGIN_LOG_PREFIX_153   String("[P153] DUCO BOX SENSOR: ")

boolean Plugin_153_init = false;
// when calling 'PLUGIN_READ', if serial port is in use set this flag and check in PLUGIN_ONCE_A_SECOND if serial port is free.
bool P153_waitingForSerialPort = false;

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
		addSelector_Head(PCONFIG_LABEL(P153_CONFIG_DEVICE));

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
		if(!ventilation_gateway_disable_serial){
			Serial.begin(115200, SERIAL_8N1);
		}
		Plugin_153_init = true;
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

	case PLUGIN_READ:{
		if (Plugin_153_init && !ventilation_gateway_disable_serial){

			addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_153 + F("start read, eventid:") +event->TaskIndex);


       // check if serial port is in use by another task, otherwise set flag.
			if(serialPortInUseByTask == 255){
				serialPortInUseByTask = event->TaskIndex;
            addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_153 + F("Start readBoxSensors"));
            startReadBoxSensors(PLUGIN_LOG_PREFIX_153);
         }else{
				addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_153 + F("Serial port in use, set flag to read data later."));
				P153_waitingForSerialPort = true;
			}
		}
      success = true;
      break;
   }

	
	case PLUGIN_ONCE_A_SECOND:{
		if(!ventilation_gateway_disable_serial){
			if(P153_waitingForSerialPort){
				if(serialPortInUseByTask == 255){
					Plugin_153(PLUGIN_READ, event, string);
					P153_waitingForSerialPort = false;
				}
			}

			if(serialPortInUseByTask == event->TaskIndex){
				if( (millis() - ducoSerialStartReading) > PLUGIN_READ_TIMEOUT_153){
					addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_153 + F("Serial reading timeout"));
					DucoTaskStopSerial(PLUGIN_LOG_PREFIX_153);
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
			uint8_t result = 0;
			bool stop = false;
			
			while( (result = DucoSerialInterrupt()) != DUCO_MESSAGE_FIFO_EMPTY && stop == false){
				switch(result){
					case DUCO_MESSAGE_ROW_END: {
                  readBoxSensorsProcessRow(PLUGIN_LOG_PREFIX_153, PCONFIG(P153_CONFIG_DEVICE), event->BaseVarIndex, PCONFIG(P153_CONFIG_LOG_SERIAL));
						duco_serial_bytes_read = 0; // reset bytes read counter
						break;
					}
					case DUCO_MESSAGE_END: {
				      DucoThrowErrorMessage(PLUGIN_LOG_PREFIX_153, result);
						DucoTaskStopSerial(PLUGIN_LOG_PREFIX_153);
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
				DucoSerialSendCommand(PLUGIN_LOG_PREFIX_153);
			}
		}

	   success = true;
    	break;
  	}



  }
  return success;
}

