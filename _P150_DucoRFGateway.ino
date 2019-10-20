//#################################### Plugin 150: DUCO RF Gateway ##################################
//
//  DUCO RF Gateway to read Ducobox data
//  https://github.com/arnemauer/Ducobox-ESPEasy-Plugin
//
//  Parts of this code is based on the itho-library made by 'supersjimmie', 'Thinkpad', 'Klusjesman' and 'jodur'.
//#######################################################################################################



#include <SPI.h>
#include "DucoCC1101.h"
#include "DucoPacket.h"
#include <Ticker.h>

//This extra settings struct is needed because the default settingsstruct doesn't support strings
struct PLUGIN_150_ExtraSettingsStruct
{	uint8_t networkId[4];
//	uint8_t deviceAddress;
} PLUGIN_150_ExtraSettings;


DucoCC1101 PLUGIN_150_rf;
void PLUGIN_150_DUCOinterrupt() ICACHE_RAM_ATTR;

// extra for interrupt handling
Ticker PLUGIN_150_DUCOticker;
int PLUGIN_150_State=0; // after startup it is assumed that the fan is running low
int PLUGIN_150_OldState=1;

int8_t Plugin_150_IRQ_pin=-1;
int8_t Plugin_150_LED_pin=-1;
bool Plugin_150_LED_status=false; // false = off, true=on
bool PLUGIN_150_InitRunned = false;

bool Plugin_150_logRF = true;


#define PLUGIN_150
#define PLUGIN_ID_150         150
#define PLUGIN_NAME_150       "DUCO ventilation remote"
#define PLUGIN_VALUENAME1_150 "Ventilationmode"
#define PLUGIN_LOG_PREFIX_150   String("[P150] DUCO RF GW: ")

typedef enum {
	P150_CONFIG_DEVICE_ADDRESS = 0,
    P150_CONFIG_LOG_RF = 1,
} P150PluginConfigs;



boolean Plugin_150(byte function, struct EventStruct *event, String& string)
{
	boolean success = false;

	switch (function)
	{

	case PLUGIN_DEVICE_ADD:
		{
			Device[++deviceCount].Number = PLUGIN_ID_150;
            Device[deviceCount].Type = DEVICE_TYPE_DUAL;
            Device[deviceCount].VType = SENSOR_TYPE_SINGLE;
			Device[deviceCount].Ports = 0;
			Device[deviceCount].PullUpOption = false;
			Device[deviceCount].InverseLogicOption = false;
			Device[deviceCount].FormulaOption = false;
			Device[deviceCount].ValueCount = 1;
			Device[deviceCount].SendDataOption = true;
			Device[deviceCount].TimerOption = true;
			Device[deviceCount].GlobalSyncOption = true;
		   break;
		}

	case PLUGIN_GET_DEVICENAME:
		{
			string = F(PLUGIN_NAME_150);
			break;
		}

	case PLUGIN_GET_DEVICEVALUENAMES:
		{
			strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_150));
			break;
		}

	case PLUGIN_GET_DEVICEGPIONAMES:
      {
        event->String1 = formatGpioName_input(F("Interrupt pin (CC1101)"));
        event->String2 = formatGpioName_output(F("Status LED"));
        break;
      }

  
  		
	case PLUGIN_INIT: {
		noInterrupts();
		detachInterrupt(Plugin_150_IRQ_pin);
	
		Plugin_150_logRF = PCONFIG(P150_CONFIG_LOG_RF);

		//If configured interrupt pin differs from configured, release old pin first
		if ((Settings.TaskDevicePin1[event->TaskIndex]!=Plugin_150_IRQ_pin) && (Plugin_150_IRQ_pin!=-1))
		{
			addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_150 + "IO-PIN changed, deatachinterrupt old pin");
			detachInterrupt(Plugin_150_IRQ_pin);
		}

	
		LoadCustomTaskSettings(event->TaskIndex, (byte*)&PLUGIN_150_ExtraSettings, sizeof(PLUGIN_150_ExtraSettings));
		addLog(LOG_LEVEL_INFO, PLUGIN_LOG_PREFIX_150 + "Extra Settings PLUGIN_150 loaded");
						
		PLUGIN_150_rf.init();
		PLUGIN_150_rf.setDeviceAddress(PCONFIG(P150_CONFIG_DEVICE_ADDRESS));
		PLUGIN_150_rf.setNetworkId(PLUGIN_150_ExtraSettings.networkId);

		String log4 = PLUGIN_LOG_PREFIX_150 + "Values set from config: DeviceID: ";
		log4 += PLUGIN_150_rf.getDeviceAddress();
		log4 += F(" and networkId");
		for (uint8_t i=0; i<=3; i++){
			log4 += String(PLUGIN_150_ExtraSettings.networkId[i], HEX);
			log4 +=F(",");
		}
		addLog(LOG_LEVEL_INFO, log4);

		Plugin_150_IRQ_pin = Settings.TaskDevicePin1[event->TaskIndex];
		pinMode(Plugin_150_IRQ_pin, INPUT);

			Plugin_150_LED_pin = Settings.TaskDevicePin2[event->TaskIndex];
		if(Plugin_150_LED_pin != -1){
			pinMode(Plugin_150_LED_pin, OUTPUT);	
			digitalWrite(Plugin_150_LED_pin, HIGH);
		
		}

		String log3 = PLUGIN_LOG_PREFIX_150 + "Interrupt cc1101 initialized: IRQ-pin ";
		log3 += Plugin_150_IRQ_pin;
		addLog(LOG_LEVEL_INFO, log3);

		PLUGIN_150_rf.initReceive();

		// check if succesfully initialised radio cc1101
		switch(PLUGIN_150_rf.getDucoDeviceState()){
			case 0x14: { // initialistion succesfull!
				PLUGIN_150_InitRunned=true;
				success = true;
				addLog(LOG_LEVEL_INFO, PLUGIN_LOG_PREFIX_150 + "CC1101 868Mhz transmitter initialized");
				attachInterrupt(Plugin_150_IRQ_pin, PLUGIN_150_DUCOinterrupt, RISING);
				break;
			}
			case 0x15: {
				addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_150 + "Initialisation -> calibration failed. No response from CC1101 or status not idle.");
			break;
			}
			case 0x16: {
				addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_150 + " Initialisation -> set RXmode failed. No response from CC1101 or status not rxmode.");
			break;
			}

		}




		if(PLUGIN_150_rf.getDucoDeviceState() == 0x14){
			success = true;

		}else{
			// initialisation failed...
			success = false;
		}

		break;
	}

	case PLUGIN_EXIT:
	{
		addLog(LOG_LEVEL_INFO, PLUGIN_LOG_PREFIX_150 + "EXIT PLUGIN_150");
		//remove interupt when plugin is removed
		detachInterrupt(Plugin_150_IRQ_pin);
		success = true;
		break;
	}


	case PLUGIN_TEN_PER_SECOND: {

		// set statusled off
		if(Plugin_150_LED_pin != -1){
			if(Plugin_150_LED_status){
				digitalWrite(Plugin_150_LED_pin, HIGH);
				Plugin_150_LED_status = false;
			}
		}

		noInterrupts();

		PLUGIN_150_rf.checkForAck();
		if(PLUGIN_150_rf.pollNewDeviceAddress()){
			memcpy(PLUGIN_150_ExtraSettings.networkId, PLUGIN_150_rf.getnetworkID(), 4); //convert char array to uint8_t
			PCONFIG(P150_CONFIG_DEVICE_ADDRESS) = PLUGIN_150_rf.getDeviceAddress();
			SaveCustomTaskSettings(event->TaskIndex, (byte*)&PLUGIN_150_ExtraSettings, sizeof(PLUGIN_150_ExtraSettings));
		}
		
		interrupts();

		success = true;
     	break;
	}

    
    case PLUGIN_ONCE_A_SECOND:
    {
		      
      //Publish new data when vars are changed or init has runned or timer is running (update every 2 sec)
      if  ((PLUGIN_150_OldState!=PLUGIN_150_State) || PLUGIN_150_InitRunned)
      {
		addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_150 + "ventilation mode changed -> UPDATE by PLUGIN_ONCE_A_SECOND");
		PLUGIN_150_Publishdata(event);
        sendData(event);

		//reset flag set by init
		PLUGIN_150_InitRunned = false;
      }  

      //Remeber current state for next cycle
      PLUGIN_150_OldState=PLUGIN_150_State;
	  success = true;
      break;
    }
    
	
    case PLUGIN_READ: {    
        // This ensures that even when Values are not changing, data is send at the configured interval for aquisition 
		addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_150 + "UPDATE by PLUGIN_READ");
		PLUGIN_150_Publishdata(event);
        success = true;
        break;
    }  
    
	case PLUGIN_WRITE: {
		String tmpString = string;
		String cmd = parseString(tmpString, 1);
		String param1 = parseString(tmpString, 2);

		if (cmd.equalsIgnoreCase(F("VENTMODE"))) {
			//override ventilation with percentage
			String param2 = parseString(tmpString, 3);
			uint8_t percentage = atoi(param2.c_str());

			uint8_t ventilationMode = 0x99;
			if (param1.equalsIgnoreCase(F("AUTO")))	{ 		  ventilationMode = 0x00;
		//	}else if (param1.equalsIgnoreCase(F("HIGH10")))	{ ventilationMode = 0x01;
		//	}else if (param1.equalsIgnoreCase(F("HIGH20")))	{ ventilationMode = 0x02;
		//	}else if (param1.equalsIgnoreCase(F("HIGH30")))	{ ventilationMode = 0x03;
			}else if (param1.equalsIgnoreCase(F("LOW")))	{ ventilationMode = 0x04;
			}else if (param1.equalsIgnoreCase(F("MIDDLE")))	{ ventilationMode = 0x05;
			}else if (param1.equalsIgnoreCase(F("HIGH")))	{ ventilationMode = 0x06;
			}else if (param1.equalsIgnoreCase(F("NOTHOME"))){ ventilationMode = 0x07; 
			}

			if(ventilationMode == 0x99){
				addLog(LOG_LEVEL_INFO, PLUGIN_LOG_PREFIX_150 + "Command 'VENTMODE' - unknown ventilation mode.");
				printWebString += PLUGIN_LOG_PREFIX_150 + "Command 'VENTMODE' - Unknown ventilation mode.";
			}else{
				String log5 = PLUGIN_LOG_PREFIX_150 + "Sent command for 'VENTMODE' & '" + param1 + "' to DUCO unit";
				addLog(LOG_LEVEL_INFO, log5);
				printWebString += log5;

				PLUGIN_150_rf.requestVentilationMode(ventilationMode,percentage,0xD2); // temp=210 = 21.0C
				
			}

			success = true;
		}

		else if (cmd.equalsIgnoreCase(F("JOIN")))
			{
				addLog(LOG_LEVEL_INFO, PLUGIN_LOG_PREFIX_150 + "Sent command for 'join' to DUCO unit");
				printWebString += PLUGIN_LOG_PREFIX_150 + "Sent command for 'join' to DUCO unit";
				PLUGIN_150_rf.sendJoinPacket();
				success = true;
			}
		else if (cmd.equalsIgnoreCase(F("DISJOIN")))
			{
				addLog(LOG_LEVEL_INFO, PLUGIN_LOG_PREFIX_150 + "Sent command for 'disjoin' to DUCO unit");
				printWebString += PLUGIN_LOG_PREFIX_150 + "Sent command for 'disjoin' to DUCO unit";
				PLUGIN_150_rf.sendDisjoinPacket();
				success = true;
			}
			
		 
	  break; 
	}


    case PLUGIN_WEBFORM_LOAD: {
		addFormSubHeader(F("Remote RF Controls"));
		char tempNetworkId [9];
		char tempBuffer[20];
		for(int i=0; i<=3; i++){    // start with lowest byte of number
			sprintf(&tempBuffer[0],"%02X",PLUGIN_150_ExtraSettings.networkId[i]); //converts to hexadecimal base.
			tempNetworkId[(i*2)] = tempBuffer[0];
			tempNetworkId[(i*2)+1] = tempBuffer[1];
		}
		tempNetworkId[8] = '\0';
        addFormTextBox( F("Network ID (HEX)"), F("PLUGIN_150_NETWORKID"), tempNetworkId, 8);

		char tempDeviceAddress[3];
		sprintf(&tempDeviceAddress[0],"%d", PCONFIG(P150_CONFIG_DEVICE_ADDRESS));

        addFormTextBox( F("Device Address"), F("PLUGIN_150_DEVICEADDRESS"), tempDeviceAddress, 3);
		addFormCheckBox(F("Log rf messages to syslog"), F("Plugin150_log_rf"), PCONFIG(P150_CONFIG_LOG_RF));

        success = true;
        break;
    }

    case PLUGIN_WEBFORM_SAVE:{
		byte CardNumberByte[4];
		unsigned long number = strtoul( WebServer.arg(F("PLUGIN_150_NETWORKID")).c_str(), nullptr, 16);
		for(int i=3; i>=0; i--){    // start with lowest byte of number
			PLUGIN_150_ExtraSettings.networkId[i] = number & 0xFF;  // or: = byte( number);
			number >>= 8;            // get next byte into position
		}
			
		Serial.println(PLUGIN_150_ExtraSettings.networkId[0], HEX);
		Serial.println(PLUGIN_150_ExtraSettings.networkId[1], HEX);
		Serial.println(PLUGIN_150_ExtraSettings.networkId[2], HEX);
		Serial.println(PLUGIN_150_ExtraSettings.networkId[3], HEX);

		PCONFIG(P150_CONFIG_DEVICE_ADDRESS) = atoi(WebServer.arg(F("PLUGIN_150_DEVICEADDRESS")).c_str());
        PCONFIG(P150_CONFIG_LOG_RF) = isFormItemChecked(F("Plugin150_log_rf"));


		SaveCustomTaskSettings(event->TaskIndex, (byte*)&PLUGIN_150_ExtraSettings, sizeof(PLUGIN_150_ExtraSettings));
		success = true;
        break;
    }	
	
}

return success;
} 



void PLUGIN_150_DUCOinterrupt()
{
	PLUGIN_150_DUCOticker.once_ms(1, PLUGIN_150_DUCOcheck);	
}


void PLUGIN_150_DUCOcheck() {

	if(Plugin_150_LED_pin != -1){
		digitalWrite(Plugin_150_LED_pin, LOW);
		Plugin_150_LED_status = true;
	}
	
	noInterrupts();
	addLog(LOG_LEVEL_DEBUG,PLUGIN_LOG_PREFIX_150 + "RF signal received");

	if (PLUGIN_150_rf.checkForNewPacket(Plugin_150_logRF)){
		int ventilationState = PLUGIN_150_rf.getCurrentVentilationMode();

        //  convert modbus status to "normal" duco status numbers
        switch(ventilationState){
            case 0: { PLUGIN_150_State = 0; break; } // modbus 0 = auto                   convert to duco 0  = "auto" = AutomaticMode;
            case 1: { PLUGIN_150_State = 21; break; } // modbus 1 = 10 minuten hoogstand   convert to duco 21 = "aut1" = Boost10min;
            case 2: { PLUGIN_150_State = 22; break; } // modbus 2 = 20 minuten hoogstand   convert to duco 22 = "aut2" = Boost20min;
            case 3: { PLUGIN_150_State = 23; break; } // modbus 3 = 30 minuten hoogstand   convert to duco 23 = "aut3" = Boost30min;
            case 4: { PLUGIN_150_State = 1; break; } // modbus 4 = Manuele laagstand      convert to duco 1  = "man1" = ManualMode1;
            case 5: { PLUGIN_150_State = 2; break; } // modbus 5 = Manuele middenstand    convert to duco 2  = "man2" = ManualMode2; 
            case 6: { PLUGIN_150_State = 3; break; } // modbus 6 = Manuele hoogstand      convert to duco 3  = "man3" = ManualMode3; 
            case 7: { PLUGIN_150_State = 4; break; } // modbus 7 = Niet-thuis-stand       convert to duco 4  = "empt" = EmptyHouse;
        }

		uint8_t numberOfLogMessages = PLUGIN_150_rf.getNumberOfLogMessages();
		for(int i=0; i< numberOfLogMessages;i++){
			addLog(LOG_LEVEL_INFO, PLUGIN_LOG_PREFIX_150 + PLUGIN_150_rf.getLogMessage(i));
		}
		
		// If new package is arrived while reading FIFO CC1101 there is no new interrupt
		uint8_t bytesInFifo = PLUGIN_150_rf.checkForBytesInRXFifo();
		if(bytesInFifo > 0){
			//addLog(LOG_LEVEL_DEBUG, F("[P150] DUCO RF GW: Bytes left in RX FIFO"));
  			char logBuf[40];
			snprintf(logBuf, sizeof(logBuf), "%u bytes left in RX FIFO", bytesInFifo);
   			addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_150 + logBuf);

			PLUGIN_150_DUCOinterrupt();
		}
	}

	interrupts();
}
  
void PLUGIN_150_Publishdata(struct EventStruct *event) {
    UserVar[event->BaseVarIndex]=PLUGIN_150_State;
    String log = PLUGIN_LOG_PREFIX_150 + " State: ";
    log += UserVar[event->BaseVarIndex];
    addLog(LOG_LEVEL_DEBUG, log);
}