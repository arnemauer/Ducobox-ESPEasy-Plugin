//#######################################################################################################
//#################################### Plugin 150: DUCO VENTILATION REMOTE ##############################
//#######################################################################################################
// Parts of this code is based on the itho-library made by 'supersjimmie', 'Thinkpad', 'Klusjesman' and 'jodur'.

#include <SPI.h>
#include "DucoCC1101.h"
#include "DucoPacket.h"
#include <Ticker.h>

//This extra settings struct is needed because the default settingsstruct doesn't support strings
struct PLUGIN_150_ExtraSettingsStruct
{	uint8_t networkId[4];
	uint8_t deviceAddress;
} PLUGIN_150_ExtraSettings;



DucoCC1101 PLUGIN_150_rf;
void PLUGIN_150_DUCOinterrupt() ICACHE_RAM_ATTR;

// extra for interrupt handling
Ticker PLUGIN_150_DUCOticker;
int PLUGIN_150_State=1; // after startup it is assumed that the fan is running low
int PLUGIN_150_OldState=1;

int PLUGIN_150_Timer=0;
int PLUGIN_150_LastIDindex = 0;
int PLUGIN_150_OldLastIDindex = 0;
long PLUGIN_150_LastPublish=0; 
int8_t Plugin_150_IRQ_pin=-1;
bool PLUGIN_150_InitRunned = false;


#define PLUGIN_150
#define PLUGIN_ID_150         150
#define PLUGIN_NAME_150       "DUCO ventilation remote"
#define PLUGIN_VALUENAME1_150 "State"
#define PLUGIN_VALUENAME2_150 "Timer"
#define PLUGIN_VALUENAME3_150 "LastIDindex"

// Timer values for hardware timer in Fan
#define PLUGIN_150_Time1      10*60
#define PLUGIN_150_Time2      20*60
#define PLUGIN_150_Time3      30*60

boolean Plugin_150(byte function, struct EventStruct *event, String& string)
{
	boolean success = false;

	switch (function)
	{

	case PLUGIN_DEVICE_ADD:
		{
			Device[++deviceCount].Number = PLUGIN_ID_150;
            Device[deviceCount].Type = DEVICE_TYPE_SINGLE;
            Device[deviceCount].VType = SENSOR_TYPE_TRIPLE;
			Device[deviceCount].Ports = 0;
			Device[deviceCount].PullUpOption = false;
			Device[deviceCount].InverseLogicOption = false;
			Device[deviceCount].FormulaOption = false;
			Device[deviceCount].ValueCount = 3;
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
			strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[1], PSTR(PLUGIN_VALUENAME2_150));
			strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[2], PSTR(PLUGIN_VALUENAME3_150));
			break;
		}
  
  		
	case PLUGIN_INIT:
		{
				noInterrupts();
				detachInterrupt(Plugin_150_IRQ_pin);

			//If configured interrupt pin differs from configured, release old pin first
			if ((Settings.TaskDevicePin1[event->TaskIndex]!=Plugin_150_IRQ_pin) && (Plugin_150_IRQ_pin!=-1))
			{
				addLog(LOG_LEVEL_DEBUG, F("IO-PIN changed, deatachinterrupt old pin"));
				detachInterrupt(Plugin_150_IRQ_pin);
			}
			LoadCustomTaskSettings(event->TaskIndex, (byte*)&PLUGIN_150_ExtraSettings, sizeof(PLUGIN_150_ExtraSettings));
			addLog(LOG_LEVEL_INFO, F("Extra Settings PLUGIN_150 loaded"));
						
			PLUGIN_150_rf.init();
			PLUGIN_150_rf.setDeviceAddress(PLUGIN_150_ExtraSettings.deviceAddress);
			//uint8_t tempNetworkId[] = {};
			//memcpy(tempNetworkId, &PLUGIN_150_ExtraSettings.networkId, 4); //convert char array to uint8_t
			PLUGIN_150_rf.setNetworkId(PLUGIN_150_ExtraSettings.networkId);

			String log4 = F("Values set from config: DeviceID ");
			log4 += PLUGIN_150_rf.getDeviceAddress();
			log4 += F(" and networkId");
			for (uint8_t i=0; i<=3; i++)
			log4 += String(PLUGIN_150_ExtraSettings.networkId[i], HEX);
			log4 +=F(",");
			addLog(LOG_LEVEL_INFO, log4);


			Plugin_150_IRQ_pin = Settings.TaskDevicePin1[event->TaskIndex];
			pinMode(Plugin_150_IRQ_pin, INPUT);
			
			String log3 = F("interrupt cc1101 initialized: ");
			log3 += Plugin_150_IRQ_pin;
			addLog(LOG_LEVEL_INFO, log3);

			PLUGIN_150_rf.initReceive();
			PLUGIN_150_InitRunned=true;
			success = true;
			addLog(LOG_LEVEL_INFO, F("CC1101 868Mhz transmitter initialized"));
						attachInterrupt(Plugin_150_IRQ_pin, PLUGIN_150_DUCOinterrupt, RISING);


			break;
		}

	case PLUGIN_EXIT:
	{
		addLog(LOG_LEVEL_INFO, F("EXIT PLUGIN_150"));
		//remove interupt when plugin is removed
		detachInterrupt(Plugin_150_IRQ_pin);
		success = true;
		break;
	}


	case PLUGIN_TEN_PER_SECOND:
	{
			noInterrupts();
		PLUGIN_150_rf.checkForAck();
		if(PLUGIN_150_rf.pollNewDeviceAddress()){
			memcpy(PLUGIN_150_ExtraSettings.networkId, PLUGIN_150_rf.getnetworkID(), 4); //convert char array to uint8_t
			PLUGIN_150_ExtraSettings.deviceAddress = PLUGIN_150_rf.getDeviceAddress();
			SaveCustomTaskSettings(event->TaskIndex, (byte*)&PLUGIN_150_ExtraSettings, sizeof(PLUGIN_150_ExtraSettings));
		}
			interrupts();

		success = true;
     	break;
	}

    
    case PLUGIN_ONCE_A_SECOND:
    {
		
      //decrement timer when timermode is running
      //if (PLUGIN_150_State>=10) PLUGIN_150_Timer--;
      
      //if timer has elapsed set Fan state to low
     // if ((PLUGIN_150_State>=10) && (PLUGIN_150_Timer<=0)) 
      // { PLUGIN_150_State=1;
       //  PLUGIN_150_Timer=0;
       //} 
      
      //Publish new data when vars are changed or init has runned or timer is running (update every 2 sec)
      if  ((PLUGIN_150_OldState!=PLUGIN_150_State))
      {
		addLog(LOG_LEVEL_DEBUG, F("UPDATE by PLUGIN_ONCE_A_SECOND"));
		PLUGIN_150_Publishdata(event);
        sendData(event);
      }  
      //Remeber current state for next cycle
      PLUGIN_150_OldState=PLUGIN_150_State;
	  //PLUGIN_150_OldLastIDindex =PLUGIN_150_LastIDindex;
	  success = true;
      break;
    }
    
	
    case PLUGIN_READ: {    
         
         // This ensures that even when Values are not changing, data is send at the configured interval for aquisition 
		 addLog(LOG_LEVEL_DEBUG, F("UPDATE by PLUGIN_READ"));
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

			uint8_t ventilationMode = 0x00;
			if (param1.equalsIgnoreCase(F("AUTO")))	{ 		  ventilationMode = 0x00;
		//	}else if (param1.equalsIgnoreCase(F("HIGH10")))	{ ventilationMode = 0x01;
		//	}else if (param1.equalsIgnoreCase(F("HIGH20")))	{ ventilationMode = 0x02;
		//	}else if (param1.equalsIgnoreCase(F("HIGH30")))	{ ventilationMode = 0x03;
			}else if (param1.equalsIgnoreCase(F("LOW")))	{ ventilationMode = 0x04;
			}else if (param1.equalsIgnoreCase(F("MIDDLE")))	{ ventilationMode = 0x05;
			}else if (param1.equalsIgnoreCase(F("HIGH")))	{ ventilationMode = 0x06;
			}else if (param1.equalsIgnoreCase(F("NOTHOME"))){ ventilationMode = 0x07; }

			addLog(LOG_LEVEL_INFO, F("Sent command for 'VENTMODE' to DUCO unit"));
			printWebString += F("Sent command for 'VENTMODE' to DUCO unit");

			PLUGIN_150_rf.requestVentilationMode(ventilationMode,percentage,0xD2); // temp=210 = 21.0C
			success = true;
		}

		else if (param1.equalsIgnoreCase(F("JOIN")))
			{
				addLog(LOG_LEVEL_INFO, F("Sent command for 'join' to DUCO unit"));
				printWebString += F("Sent command for 'join' to DUCO unit");
				PLUGIN_150_rf.sendJoinPacket();
				success = true;
			}
		else if (param1.equalsIgnoreCase(F("DISJOIN")))
			{
				addLog(LOG_LEVEL_INFO, F("Sent command for 'disjoin' to DUCO unit"));
				printWebString += F("Sent command for 'disjoin' to DUCO unit");
				PLUGIN_150_rf.sendDisjoinPacket();
				success = true;
			}
			
		 
	  break; 
	}


      case PLUGIN_WEBFORM_LOAD:
        {
		  addFormSubHeader(F("Remote RF Controls"));
		  char tempNetworkId [9];
		  //char *ptr = &tempNetworkId[0];
			char tempBuffer[20];
			for(int i=0; i<=3; i++){    // start with lowest byte of number
		  		sprintf(&tempBuffer[0],"%02X",PLUGIN_150_ExtraSettings.networkId[i]); //converts to hexadecimal base.
				tempNetworkId[(i*2)] = tempBuffer[0];
				tempNetworkId[(i*2)+1] = tempBuffer[1];
			}
			tempNetworkId[8] = '\0';
          addFormTextBox( F("Network ID (HEX)"), F("PLUGIN_150_NETWORKID"), tempNetworkId, 8);

		  char tempDeviceAddress[3];
			sprintf(&tempDeviceAddress[0],"%d", PLUGIN_150_ExtraSettings.deviceAddress);

          addFormTextBox( F("Device Address"), F("PLUGIN_150_DEVICEADDRESS"), tempDeviceAddress, 3);
          success = true;
          break;
        }

      case PLUGIN_WEBFORM_SAVE:
        {

			//	char *CardNumber = "B763AB23";
			byte CardNumberByte[4];
			// Use 'nullptr' or 'NULL' for the second parameter.
			unsigned long number = strtoul( WebServer.arg(F("PLUGIN_150_NETWORKID")).c_str(), nullptr, 16);
			Serial.println(F(">>"));
			for(int i=3; i>=0; i--)    // start with lowest byte of number
			{
				PLUGIN_150_ExtraSettings.networkId[i] = number & 0xFF;  // or: = byte( number);
				//Serial.println(F("-"));
				//Serial.println(number & 0xFF, HEX);
				number >>= 8;            // get next byte into position
			}
			
			Serial.println(PLUGIN_150_ExtraSettings.networkId[0], HEX);
			Serial.println(PLUGIN_150_ExtraSettings.networkId[1], HEX);
			Serial.println(PLUGIN_150_ExtraSettings.networkId[2], HEX);
			Serial.println(PLUGIN_150_ExtraSettings.networkId[3], HEX);

			PLUGIN_150_ExtraSettings.deviceAddress = atoi(WebServer.arg(F("PLUGIN_150_DEVICEADDRESS")).c_str());

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

void PLUGIN_150_DUCOcheck()
{
	noInterrupts();
	
	//PLUGIN_150_rf.readRegister(CC1101_RSSI, CC1101_STATUS_REGISTER)

	addLog(LOG_LEVEL_DEBUG, "RF signal received\n");
	if (PLUGIN_150_rf.checkForNewPacket())
	{
		
		PLUGIN_150_State = PLUGIN_150_rf.getCurrentVentilationMode();

		// If new package is arrived while reading FIFO CC1101 there is no new interrupt
		if(PLUGIN_150_rf.checkForNewPacketInRXFifo()){
			addLog(LOG_LEVEL_DEBUG, "DUCO: bytes left in RX FIFO\n");
			PLUGIN_150_DUCOinterrupt();
		}
	}



	interrupts();
}
  
void PLUGIN_150_Publishdata(struct EventStruct *event)
{
    UserVar[event->BaseVarIndex]=PLUGIN_150_State;
    UserVar[event->BaseVarIndex+1]=PLUGIN_150_Timer;
	UserVar[event->BaseVarIndex+2]=PLUGIN_150_LastIDindex;
    PLUGIN_150_LastPublish=millis();
    String log = F("State: ");
    log += UserVar[event->BaseVarIndex];
    addLog(LOG_LEVEL_DEBUG, log);
    log = F("Timer: ");
    log += UserVar[event->BaseVarIndex+1];
    addLog(LOG_LEVEL_DEBUG, log);
	log = F("LastIDindex: ");
	log += UserVar[event->BaseVarIndex+2];
	addLog(LOG_LEVEL_DEBUG, log);
}