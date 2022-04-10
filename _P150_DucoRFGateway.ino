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

//This extra settings struct is needed because the default settingsstruct doesn't support strings
struct PLUGIN_150_ExtraSettingsStruct
{	uint8_t networkId[4];
} PLUGIN_150_ExtraSettings;

DucoCC1101 PLUGIN_150_rf;

// extra for interrupt handling
int PLUGIN_150_State=0; // after startup it is assumed that the fan is running in automatic mode
int PLUGIN_150_OldState=1;

int8_t Plugin_150_IRQ_pin=-1;
int8_t Plugin_150_LED_pin=-1;
bool Plugin_150_LED_status=false; // false = off, true=on
bool PLUGIN_150_InitRunned = false;

#define PLUGIN_150
#define PLUGIN_ID_150         150
#define PLUGIN_NAME_150       "DUCO ventilation remote"
#define PLUGIN_VALUENAME1_150 "Ventilationmode"
#define PLUGIN_LOG_PREFIX_150   String("[P150] DUCO RF GW: ")

typedef enum {
	P150_CONFIG_DEVICE_ADDRESS = 0,
    P150_CONFIG_LOG_RF = 1,
	P150_CONFIG_RADIO_POWER = 2,
	P150_CONFIG_HARDWARE_TYPE = 3,
} P150PluginConfigs;


//interrupt 
void PLUGIN_150_interruptHandler() ICACHE_RAM_ATTR;
volatile uint8_t PLUGIN_150_IRQ = false; //!<  irq flag
volatile unsigned long test_interrupt_counter;

// DUCO default = 0xC1 and HIGH = 0xC0
typedef enum {
	P150_RADIO_POWER_DEFAULT = 0, // 0,6 dBm = 0x8D
	P150_RADIO_POWER_1 = 1, //  10,3 dBm = 0xC1
	P150_RADIO_POWER_2 = 2, //   5,0 dBm = 0x81
	P150_RADIO_POWER_3 = 3, // -5.0 dBm = 0x67
	P150_RADIO_POWER_4 = 4, // -20 dBm  = 0x0F
} P150RadioPowerValues;

uint8_t P150_radio_power_value[5] = {0x8D, 0xC1, 0x81, 0x67, 0x0F};

#define P150_RADIO_POWER_OUTPUT_OPTIONS 5
String Plugin_150_radiopower_valuename(byte value_nr) {
	switch (value_nr) {
		case P150_RADIO_POWER_DEFAULT: return F("Default - radio power 0,6 dBm (0x8D)");
		case P150_RADIO_POWER_1:  return F("Radio power 10,3 dBm (0xC1)");
		case P150_RADIO_POWER_2:  return F("Radio power 5,0 dBm (0x81)");
		case P150_RADIO_POWER_3:  return F("Radio power -5,0 dBm (0x67)");
		case P150_RADIO_POWER_4:  return F("Radio power -20 dBm (0x0F) - lowest");
		default:
      break;
  	}
  	return "";
}


typedef enum {
	P150_HARDWARE_DIY = 0,
	P150_HARDWARE_83_AND_LOWER = 1,
	P150_HARDWARE_84_AND_HIGHER = 2,
} P150HardwareTypes;

#define P150_HARDWARE_NR_OUTPUT_OPTIONS 3
String Plugin_150_hardware_type(byte value_nr) {
	switch (value_nr) {
		case P150_HARDWARE_DIY:  return F("DIY esp8266 hardware");
		case P150_HARDWARE_83_AND_LOWER:  return F("Ventilation gateway V8.3 and lower");
		case P150_HARDWARE_84_AND_HIGHER: return F("Ventilation gateway V8.4 and higher");
		default:
      break;
  	}
  	return "";
}



boolean Plugin_150(byte function, struct EventStruct *event, String& string)
{
	boolean success = false;

	switch (function){

		case PLUGIN_DEVICE_ADD:{
			Device[++deviceCount].Number = PLUGIN_ID_150;
         	Device[deviceCount].Type = DEVICE_TYPE_DUAL;
         	Device[deviceCount].VType = Sensor_VType::SENSOR_TYPE_SINGLE;
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

		case PLUGIN_GET_DEVICENAME:{
			string = F(PLUGIN_NAME_150);
			break;
		}

		case PLUGIN_GET_DEVICEVALUENAMES:{
			strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_150));
			break;
		}

		case PLUGIN_GET_DEVICEGPIONAMES:{
        	event->String1 = formatGpioName_input(F("Interrupt pin (CC1101)"));
        	event->String2 = formatGpioName_output(F("Status LED"));
        	break;
      }

    case PLUGIN_SET_DEFAULTS:{
    	PCONFIG(P150_CONFIG_DEVICE_ADDRESS) = 0;
		PCONFIG(P150_CONFIG_LOG_RF) = 0;
    	PCONFIG(P150_CONFIG_RADIO_POWER) = P150_RADIO_POWER_DEFAULT;

    	success = true;
    	break;
  	}
  		
		case PLUGIN_INIT: {
		
			LoadCustomTaskSettings(event->TaskIndex, (byte*)&PLUGIN_150_ExtraSettings, sizeof(PLUGIN_150_ExtraSettings));
			addLog(LOG_LEVEL_INFO, PLUGIN_LOG_PREFIX_150 + F("Extra Settings PLUGIN_150 loaded"));

			// check if task is enable and IRQ pin is set, if not than reset CC1101
			if (Settings.TaskDeviceEnabled[event->TaskIndex] == false || Settings.TaskDevicePin1[event->TaskIndex] == -1) {
				addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_150 + F("Task disabled, checking if CC1101 needs a reset."));
				if(PLUGIN_150_rf.getDucoDeviceState() != ducoDeviceState_notInitialised ){
					PLUGIN_150_rf.reset(); // reset CC1101
					detachInterrupt(Settings.TaskDevicePin1[event->TaskIndex]);
				}
			}else{


				//If configured interrupt pin differs from configured, release old pin first
				if ((Settings.TaskDevicePin1[event->TaskIndex]!=Plugin_150_IRQ_pin) && (Plugin_150_IRQ_pin!=-1))
				{
					addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_150 + F("IO-PIN changed, deatachinterrupt old pin"));
					detachInterrupt(Plugin_150_IRQ_pin);
				}

			

								
				PLUGIN_150_rf.setLogRFMessages(PCONFIG(P150_CONFIG_LOG_RF));
				PLUGIN_150_rf.init();
				PLUGIN_150_rf.setDeviceAddress(PCONFIG(P150_CONFIG_DEVICE_ADDRESS));
				PLUGIN_150_rf.setNetworkId(PLUGIN_150_ExtraSettings.networkId);
				PLUGIN_150_rf.setRadioPower(P150_radio_power_value[PCONFIG(P150_CONFIG_RADIO_POWER)]);
				PLUGIN_150_rf.setTemperature(210); // = 21.0 C
								
				String log4 = PLUGIN_LOG_PREFIX_150 + "Values set from config. DeviceID: ";
				log4 += PLUGIN_150_rf.getDeviceAddress();
				log4 += F(", networkId: ");
				for (uint8_t i=0; i<=3; i++){
					log4 += String(PLUGIN_150_ExtraSettings.networkId[i], HEX);
				}
				log4 += F(", radio power: ");
				log4 += Plugin_150_radiopower_valuename(PCONFIG(P150_CONFIG_RADIO_POWER));
				addLog(LOG_LEVEL_INFO, log4);

				// set pinmode for CC1101 interrupt pin
				Plugin_150_IRQ_pin = Settings.TaskDevicePin1[event->TaskIndex];
				pinMode(Plugin_150_IRQ_pin, INPUT);

				// set pinmode for status led
				Plugin_150_LED_pin = Settings.TaskDevicePin2[event->TaskIndex];
				if(Plugin_150_LED_pin != -1 && PCONFIG(P150_CONFIG_HARDWARE_TYPE) != P150_HARDWARE_84_AND_HIGHER){
					pinMode(Plugin_150_LED_pin, OUTPUT);	
					digitalWrite(Plugin_150_LED_pin, HIGH);
				}

				if(PCONFIG(P150_CONFIG_HARDWARE_TYPE) == P150_HARDWARE_84_AND_HIGHER){
					P151_PCF_set_pin_output(6, HIGH); // PCF8574; P6 LED_YELLOW	= HIGH (Led off)
					P151_PCF_set_pin_output(7, HIGH); // PCF8574; P7 LED_BLUE		= HIGH (Led off)
				}

				// start initiating the CC1101 radio module
				PLUGIN_150_rf.initReceive();
				attachInterrupt(Plugin_150_IRQ_pin, PLUGIN_150_interruptHandler, FALLING);

				// check if succesfully initialised CC1101 radio module
				switch(PLUGIN_150_rf.getDucoDeviceState()){
					case 0x14: { // initialistion succesfull!
						PLUGIN_150_InitRunned=true;
						success = true;
						PLUGIN_150_rf.sendSubscribeMessage(); // subscribe to ducobox to get latest ventilation mode.
						addLog(LOG_LEVEL_INFO, PLUGIN_LOG_PREFIX_150 + F("CC1101 868Mhz transmitter initialized"));
						break;
					}
					case 0x15: {
						addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_150 + F("Initialisation -> calibration failed. No response from CC1101 or status not idle."));
					break;
					}
					case 0x16: {
						addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_150 + F("Initialisation -> set RXmode failed. No response from CC1101 or status not rxmode."));
					break;
					}
					default: {
						addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_150 + F("Initialisation -> unexpected device state: ") + PLUGIN_150_rf.getDucoDeviceState());
					break;
					}

				}

				if(PLUGIN_150_rf.getDucoDeviceState() != 0x14){
					success = false;
					break;
				}


			}
			success = true;
			break;
		}

		case PLUGIN_EXIT:{
			addLog(LOG_LEVEL_INFO, PLUGIN_LOG_PREFIX_150 + F("EXIT PLUGIN_150"));
			//remove interupt when plugin is removed
			detachInterrupt(Plugin_150_IRQ_pin);
			PLUGIN_150_rf.reset(); // reset CC1101


			if(PCONFIG(P150_CONFIG_HARDWARE_TYPE) == P150_HARDWARE_84_AND_HIGHER){
				//P151_PCF_set_pin_output(6, HIGH); // PCF8574; P6 LED_YELLOW	= HIGH (Led off)
				P151_PCF_set_pin_output(7, HIGH); // PCF8574; P7 LED_BLUE		= HIGH (Led off)
			}

			clearPluginTaskData(event->TaskIndex); // clear plugin taskdata
			ClearCustomTaskSettings(event->TaskIndex); // clear networkID settings

			
			success = true;
			break;
		}


		case PLUGIN_FIFTY_PER_SECOND: {
			// check for interrupt 
			if(PLUGIN_150_InitRunned){
				if(PLUGIN_150_IRQ){
					if (PCONFIG(P150_CONFIG_HARDWARE_TYPE) == P150_HARDWARE_84_AND_HIGHER){ // status led
						P151_PCF_set_pin_output(7, LOW); // PCF8574; P7 LED_BLUE		= LOW (Led on)
						Plugin_150_LED_status = true;
					}else if(Plugin_150_LED_pin != -1){
						digitalWrite(Plugin_150_LED_pin, LOW);
						Plugin_150_LED_status = true;
					}
					PLUGIN_150_IRQ = false; 
				}
				PLUGIN_150_DUCOcheck();
			}

			
			success = true;
			break;
		}


		case PLUGIN_TEN_PER_SECOND: {

			// set statusled off
			if(Plugin_150_LED_status){
				if (PCONFIG(P150_CONFIG_HARDWARE_TYPE) == P150_HARDWARE_84_AND_HIGHER){ // status led
					P151_PCF_set_pin_output(7, HIGH); // PCF8574; P7 LED_BLUE		= HIGH (Led off)
					Plugin_150_LED_status = false;
				}else if(Plugin_150_LED_pin != -1){
					digitalWrite(Plugin_150_LED_pin, HIGH);
					Plugin_150_LED_status = false;
				}
			}

		
			PLUGIN_150_rf.checkForAck();

			if(PLUGIN_150_rf.pollNewDeviceAddress()){
				memcpy(PLUGIN_150_ExtraSettings.networkId, PLUGIN_150_rf.getnetworkID(), 4); //convert char array to uint8_t
				PCONFIG(P150_CONFIG_DEVICE_ADDRESS) = PLUGIN_150_rf.getDeviceAddress();
				SaveCustomTaskSettings(event->TaskIndex, (byte*)&PLUGIN_150_ExtraSettings, sizeof(PLUGIN_150_ExtraSettings));
				SaveTaskSettings(event->TaskIndex);
    			SaveSettings();
			}
			


			uint8_t numberOfLogMessages = PLUGIN_150_rf.getNumberOfLogMessages();
			for(int i=0; i< numberOfLogMessages;i++){
				addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_150 + PLUGIN_150_rf.logMessages[i]);
			}

			success = true;
			break;
		}

    
		case PLUGIN_ONCE_A_SECOND:{ 
			//Publish new data when vars are changed or init has runned
			if  (PLUGIN_150_OldState!=PLUGIN_150_State){
				addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_150 + F("ventilation mode changed -> UPDATE by PLUGIN_ONCE_A_SECOND"));
				PLUGIN_150_Publishdata(event);
				sendData(event);

				//Remeber current state for next cycle
				PLUGIN_150_OldState=PLUGIN_150_State;
			}
			success = true;
			break;
    	}
    
	
    	case PLUGIN_READ: {    
        	// This ensures that even when Values are not changing, data is send at the configured interval for aquisition 
			addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_150 + F("UPDATE by PLUGIN_READ"));
			PLUGIN_150_Publishdata(event);
        	success = true;
        	break;
    	}  
    
		case PLUGIN_WRITE: {
			String tmpString = string;
			String cmd = parseString(tmpString, 1);
			String param1 = parseString(tmpString, 2);
				if (cmd.equalsIgnoreCase(F("VENTMODE"))) {
					// check if task is enabled
					if (Settings.TaskDeviceEnabled[event->TaskIndex]) {
							//override ventilation with percentage
						String param2 = parseString(tmpString, 3); 
						uint8_t percentage = atoi(param2.c_str()); // if empty percentage will be 0
						if( (percentage < 0) || (percentage > 100) ){ percentage = 0; }

						String param3 = parseString(tmpString, 4); 
						uint8_t buttonPresses = atoi(param3.c_str()); // if empty buttonpresses will be 0
						if( (buttonPresses < 1) || (buttonPresses > 3) ){ buttonPresses = 1; }

						uint8_t ventilationMode = 0x99;
						bool permanentVentilationMode = false;
						if (param1.equalsIgnoreCase(F("AUTO")))	{ 		  				ventilationMode = 0x00;
						}else if (param1.equalsIgnoreCase(F("LOW")))	{ 				ventilationMode = 0x04;
						}else if (param1.equalsIgnoreCase(F("MIDDLE")))	{ 				ventilationMode = 0x05;
						}else if (param1.equalsIgnoreCase(F("HIGH")))	{ 				ventilationMode = 0x06;					
						}else if (param1.equalsIgnoreCase(F("NOTHOME"))){ 				ventilationMode = 0x07; 
						}else if (param1.equalsIgnoreCase(F("PERMANENTLOW")))	{ 		ventilationMode = 0x04; permanentVentilationMode = true;
						}else if (param1.equalsIgnoreCase(F("PERMANENTMIDDLE")))	{ 	ventilationMode = 0x05; permanentVentilationMode = true;
						}else if (param1.equalsIgnoreCase(F("PERMANENTHIGH")))	{ 		ventilationMode = 0x06; permanentVentilationMode = true;
						}

						if(ventilationMode == 0x99){
							addLog(LOG_LEVEL_INFO, PLUGIN_LOG_PREFIX_150 + F("Command 'VENTMODE' - unknown ventilation mode."));
							printWebString += PLUGIN_LOG_PREFIX_150 + F("Command 'VENTMODE' - Unknown ventilation mode.");
						}else{
							String log5 = PLUGIN_LOG_PREFIX_150 + F("Sent command for 'VENTMODE' & '") + param1 + F("' to DUCO unit");
							addLog(LOG_LEVEL_INFO, log5);
							printWebString += log5;

							PLUGIN_150_rf.requestVentilationMode(ventilationMode, permanentVentilationMode, percentage, buttonPresses);
						}

						success = true;

					}else{
						addLog(LOG_LEVEL_INFO, PLUGIN_LOG_PREFIX_150 + F("Command ignored, please enable the task."));
						printWebString += PLUGIN_LOG_PREFIX_150 + F("Command ignored, please enable the task.");
					}

				}

				else if (cmd.equalsIgnoreCase(F("JOIN")))
					{
						// check if task is enabled
						if (Settings.TaskDeviceEnabled[event->TaskIndex]) {

								//override ventilation with percentage
							addLog(LOG_LEVEL_INFO, PLUGIN_LOG_PREFIX_150 + F("Sent command for 'join' to DUCO unit"));
							printWebString += PLUGIN_LOG_PREFIX_150 + F("Sent command for 'join' to DUCO unit");
							PLUGIN_150_rf.sendJoinPacket();
							success = true;

						}else{
							addLog(LOG_LEVEL_INFO, PLUGIN_LOG_PREFIX_150 + F("Command ignored, please enable the task."));
							printWebString += PLUGIN_LOG_PREFIX_150 + F("Command ignored, please enable the task.");
						}

					}
				else if (cmd.equalsIgnoreCase(F("DISJOIN")))
					{
						// check if task is enabled
						if (Settings.TaskDeviceEnabled[event->TaskIndex]) {
							addLog(LOG_LEVEL_INFO, PLUGIN_LOG_PREFIX_150 + F("Sent command for 'disjoin' to DUCO unit"));
							printWebString += PLUGIN_LOG_PREFIX_150 + F("Sent command for 'disjoin' to DUCO unit");
							PLUGIN_150_rf.sendDisjoinPacket();
							success = true;
						}else{
							addLog(LOG_LEVEL_INFO, PLUGIN_LOG_PREFIX_150 + F("Command ignored, please enable the task."));
							printWebString += PLUGIN_LOG_PREFIX_150 + F("Command ignored, please enable the task.");
						}
					}		
				else if (cmd.equalsIgnoreCase(F("INSTALLERMODE")))
					{

					// check if task is enabled
					if (Settings.TaskDeviceEnabled[event->TaskIndex]) {
						if(param1.equalsIgnoreCase(F("OFF"))){
							PLUGIN_150_rf.disableInstallerMode();
							addLog(LOG_LEVEL_INFO, PLUGIN_LOG_PREFIX_150 + F("Sent command for disabling installermode."));
							printWebString += PLUGIN_LOG_PREFIX_150 + F("Sent command for disabling installermode.");
							success = true;
						}else if(param1.equalsIgnoreCase(F("ON"))){
							uint8_t ventilationState = PLUGIN_150_rf.getCurrentVentilationMode();
							PLUGIN_150_rf.enableInstallerMode();
							addLog(LOG_LEVEL_INFO, PLUGIN_LOG_PREFIX_150 + F("Sent command for enabling installermode."));
							printWebString += PLUGIN_LOG_PREFIX_150 + F("Sent command for enabling installermode.");
							success = true;
						}
					}else{
						addLog(LOG_LEVEL_INFO, PLUGIN_LOG_PREFIX_150 + F("Command ignored, please enable the task."));
						printWebString += PLUGIN_LOG_PREFIX_150 + F("Command ignored, please enable the task.");
					}
			


					}		
					else if (cmd.equalsIgnoreCase(F("DUCOTESTMESSAGE")))
					{

						
						// MESSAGETYPE
						uint8_t messageType = atoi(param1.c_str()); // if empty value will be 0

						// sourceAddress
						String param2 = parseString(tmpString, 3);  // 
						uint8_t sourceAddress = atoi(param2.c_str()); // if empty value will be 0

						// destinationAddress
						String param3 = parseString(tmpString, 4); 
						uint8_t destinationAddress = atoi(param3.c_str()); // if empty value will be 0

						// originalSourceAddress
						String param4 = parseString(tmpString, 5); 
						uint8_t originalSourceAddress = atoi(param4.c_str()); // if empty value will be 0

						// originalDestinationAddress
						String param5 = parseString(tmpString, 6); 
						uint8_t originalDestinationAddress = atoi(param5.c_str()); // if empty value will be 0

						String param6 = parseString(tmpString, 7); 
						char hexValues[40];
						param6.toCharArray(hexValues, 40);
						
						uint8_t hexArraySize = 20;
						unsigned char hexArray[hexArraySize];
						uint8_t dataBytes = 0;
						PLUGIN_150_hexstringToHex(hexValues, strlen(hexValues), hexArray, hexArraySize, &dataBytes);

						addLog(LOG_LEVEL_INFO, PLUGIN_LOG_PREFIX_150 + dataBytes);

						uint8_t* myuint8array = (uint8_t*)hexArray;
						PLUGIN_150_rf.sendRawPacket(messageType, sourceAddress, destinationAddress, originalSourceAddress, originalDestinationAddress, myuint8array, dataBytes);

							String log6 = PLUGIN_LOG_PREFIX_150 + F("Sent DUCOTESTMESSAGE to Duco Network. ");
							log6 += "Databytes: ";
							log6 += dataBytes;

							addLog(LOG_LEVEL_INFO, log6);
							printWebString += log6;
							success = true;
					}


					uint8_t numberOfLogMessages = PLUGIN_150_rf.getNumberOfLogMessages();
					for(int i=0; i< numberOfLogMessages;i++){
						addLog(LOG_LEVEL_INFO, PLUGIN_LOG_PREFIX_150 + PLUGIN_150_rf.logMessages[i]);
					}

			
	  		break; 
		}

  		case PLUGIN_WEBFORM_SHOW_VALUES: {

			String ventMode = "";
			if(PLUGIN_150_InitRunned){
				switch(PLUGIN_150_State){
					case 0:  {  ventMode = "Automatic (0)"; break; } // modbus 0 = auto                   convert to duco 0  = "auto" = AutomaticMode;
					case 1:  {  ventMode = "ManualMode1 (1)"; break; } // modbus 4 = Manuele laagstand      convert to duco 1  = "man1" = ManualMode1;
					case 2:  {  ventMode = "ManualMode2 (2)"; break; } // modbus 5 = Manuele middenstand    convert to duco 2  = "man2" = ManualMode2; 
					case 3:  {  ventMode = "ManualMode3 (3)"; break; } // modbus 6 = Manuele hoogstand      convert to duco 3  = "man3" = ManualMode3; 
					case 4:  {  ventMode = "EmptyHouse (4)"; break; } // modbus 7 = Niet-thuis-stand       convert to duco 4  = "empt" = EmptyHouse;

					case 11:  {  ventMode = "PermanentManualMode1 (11)"; break; } // 	= continu LOW		       convert to duco 4  = "cnt1" = PermanentManualMode1;
					case 12:  {  ventMode = "PermanentManualMode2 (12)"; break; } //   = continu MIDDLE       convert to duco 4  = "cnt2" = PermanentManualMode2;
					case 13:  {  ventMode = "PermanentManualMode3 (13)"; break; } //   = continu HIGH       convert to duco 4  = "cnt3" = PermanentManualMode3;

					default: {  ventMode = "NA"; break; };
				}
			}else{
				ventMode = "Disabled"; 
			}
			// can't use pluginWebformShowValue because ajax will refresh the value with original value (number)
			//addHtml(pluginWebformShowValue(event->TaskIndex, 0, F("Ventilationmode"), ventMode));

			addHtml(F("<div class=\"div_l\" id=\"installermode\">Ventilationmode:</div>"));
			addHtml(F("<div class=\"div_r\" id=\"ventilationModeValue\">"));
			addHtml(ventMode);
			addHtml(F("</div>"));
			addHtml(F("<div class=\"div_br\"></div>"));

			if( PLUGIN_150_InitRunned ){
				if(PCONFIG(P150_CONFIG_DEVICE_ADDRESS) != 0){

					if(PLUGIN_150_rf.getInstallerModeActive()){
						addHtml(F("<div class=\"div_l\" id=\"installermode\">Installermode (<a style='color:#07D; text-decoration: none;' href='/tools?cmd=INSTALLERMODE,OFF'>deactivate</a>):</div>"));
						addHtml(F("<div class=\"div_r\" style=\"background-color: #c00000;\" id=\"installermodeactive\">ACTIVE</div>"));
					}else{
						addHtml(F("<div class=\"div_l\" id=\"installermode\">Installermode (<a style='color:#07D; text-decoration: none;' href='/tools?cmd=INSTALLERMODE,ON'>activate</a>):</div>"));
						addHtml(F("<div class=\"div_r\" style=\"background-color: #080;\" id=\"installermodenactive\">NOT ACTIVE</div>"));
					}
					addHtml(F("<div class=\"div_br\"></div>"));
				}

				addHtml(F("<div class=\"div_l\" id=\"installermode\">Actions:</div>"));

				// if init runned and device has no addressid than show join button
				if(PCONFIG(P150_CONFIG_DEVICE_ADDRESS) == 0 ){
					addHtml(F("<div class=\"div_r\" style=\"background-color: #07D;\"><a style='color:#fff; text-decoration: none;' href='/tools?cmd=JOIN'>Join</a></div>"));
				}else{
					addHtml(F("<div class=\"div_r\" style=\"background-color: #07D;\"><a style='color:#fff; text-decoration: none;' href='/tools?cmd=VENTMODE,AUTO'>AUTO</a></div>"));
					addHtml(F("<div class=\"div_r\" style=\"background-color: #07D;\"><a style='color:#fff; text-decoration: none;' href='/tools?cmd=VENTMODE,LOW'>LOW</a></div>"));
					addHtml(F("<div class=\"div_r\" style=\"background-color: #07D;\"><a style='color:#fff; text-decoration: none;' href='/tools?cmd=VENTMODE,MIDDLE'>MID</a></div>"));
					addHtml(F("<div class=\"div_r\" style=\"background-color: #07D;\"><a style='color:#fff; text-decoration: none;' href='/tools?cmd=VENTMODE,HIGH'>HIGH</a></div>"));
					
					addHtml(F("<div class=\"div_br\"></div>"));

					addHtml(F("<div class=\"div_r\" style=\"background-color: #07D;\"><a style='color:#fff; text-decoration: none;' href='/tools?cmd=VENTMODE,PERMANENTLOW'>Perm. LOW</a></div>"));
					addHtml(F("<div class=\"div_r\" style=\"background-color: #07D;\"><a style='color:#fff; text-decoration: none;' href='/tools?cmd=VENTMODE,PERMANENTMIDDLE'>Perm. MID</a></div>"));
					addHtml(F("<div class=\"div_r\" style=\"background-color: #07D;\"><a style='color:#fff; text-decoration: none;' href='/tools?cmd=VENTMODE,PERMANENTHIGH'>Perm. HIGH</a></div>"));
				
				}
				addHtml(F("<div class=\"div_br\"></div>"));
			}

				success = true; // The call to PLUGIN_WEBFORM_SHOW_VALUES should only return success = true when no regular values should be displayed

			break;
		}



    	case PLUGIN_WEBFORM_LOAD: {

			addRowLabel(F("Hardware type"));
			addSelector_Head(PCONFIG_LABEL(P150_CONFIG_HARDWARE_TYPE));

			for (byte x = 0; x < P150_HARDWARE_NR_OUTPUT_OPTIONS; x++){
				addSelector_Item(Plugin_150_hardware_type(x), x, PCONFIG(P150_CONFIG_HARDWARE_TYPE) == x, false, "");
			}
			addSelector_Foot();


			addFormSubHeader(F("Remote RF Controls (automaticly filled after succesfull join)"));
			char tempNetworkId [9];
			char tempBuffer[20];

			LoadCustomTaskSettings(event->TaskIndex, (byte*)&PLUGIN_150_ExtraSettings, sizeof(PLUGIN_150_ExtraSettings));

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

			// Selector for Radio power
			addRowLabel(F("Radio Power"));
			addSelector_Head(PCONFIG_LABEL(P150_CONFIG_RADIO_POWER));

			for (byte x = 0; x < P150_RADIO_POWER_OUTPUT_OPTIONS; x++){
				String name     = Plugin_150_radiopower_valuename(x);
				addSelector_Item(name, x, PCONFIG(P150_CONFIG_RADIO_POWER) == x, false, "");
			}
			addSelector_Foot();
			// Selector for Radio power

			addFormCheckBox(F("Log rf messages to syslog"), PCONFIG_LABEL(P150_CONFIG_LOG_RF), PCONFIG(P150_CONFIG_LOG_RF));

			success = true;
			break;
    	}

    	case PLUGIN_WEBFORM_SAVE:{

			PCONFIG(P150_CONFIG_HARDWARE_TYPE) = getFormItemInt(PCONFIG_LABEL(P150_CONFIG_HARDWARE_TYPE), 0);

			unsigned long number = strtoul( web_server.arg(F("PLUGIN_150_NETWORKID")).c_str(), nullptr, 16);
			for(int i=3; i>=0; i--){    // start with lowest byte of number
				PLUGIN_150_ExtraSettings.networkId[i] = number & 0xFF;  // or: = byte( number);
				number >>= 8;            // get next byte into position
			}

			addLog(LOG_LEVEL_DEBUG, PLUGIN_LOG_PREFIX_150 + PLUGIN_150_ExtraSettings.networkId[0] + "-" +  PLUGIN_150_ExtraSettings.networkId[1] +"-" +  PLUGIN_150_ExtraSettings.networkId[2] +"-" +  PLUGIN_150_ExtraSettings.networkId[3]);


			PCONFIG(P150_CONFIG_DEVICE_ADDRESS) = atoi(web_server.arg(F("PLUGIN_150_DEVICEADDRESS")).c_str());
			PCONFIG(P150_CONFIG_RADIO_POWER) = getFormItemInt(PCONFIG_LABEL(P150_CONFIG_RADIO_POWER), 0);
			PCONFIG(P150_CONFIG_LOG_RF) = isFormItemChecked(PCONFIG_LABEL(P150_CONFIG_LOG_RF));

			SaveCustomTaskSettings(event->TaskIndex, (byte*)&PLUGIN_150_ExtraSettings, sizeof(PLUGIN_150_ExtraSettings));
			
			success = true;
			break;
    	}	
	
}

return success;
} 









// IRQ handler:
void PLUGIN_150_interruptHandler(void)
{
	if(PLUGIN_150_rf.checkForNewPacket()){
		PLUGIN_150_IRQ = true; // for led

	}
}

void PLUGIN_150_DUCOcheck() {

/*
	// for debug purpose -> save millis to see how long it takes to run PLUGIN_150_DUCOcheck()
	char testintlog[40];
	unsigned long mill = millis();
	unsigned long duur = mill - test_interrupt_counter;
	snprintf(testintlog, sizeof(testintlog),"%lu %lu %lu", mill, test_interrupt_counter, duur );
	addLog(LOG_LEVEL_DEBUG,PLUGIN_LOG_PREFIX_150 + F("TIMER => ") + testintlog );
	// for debug purpose -> save millis to see how long it takes to run PLUGIN_150_DUCOcheck()

	addLog(LOG_LEVEL_DEBUG,PLUGIN_LOG_PREFIX_150 + F("Start of RF signal received"));
	//noInterrupts();
*/

	PLUGIN_150_rf.processNewMessages();



		int ventilationState = PLUGIN_150_rf.getCurrentVentilationMode();
		bool permanentMode = PLUGIN_150_rf.getCurrentPermanentMode();

      //  convert modbus status to "normal" duco status numbers
		if(!permanentMode){
			switch(ventilationState){
				case 0: { PLUGIN_150_State = 0; break; } // modbus 0 	= auto                   convert to duco 0  = "auto" = AutomaticMode;
				case 4: { PLUGIN_150_State = 1; break; } // modbus 4 	= Manuele laagstand      convert to duco 1  = "man1" = ManualMode1;
				case 5: { PLUGIN_150_State = 2; break; } // modbus 5 	= Manuele middenstand    convert to duco 2  = "man2" = ManualMode2; 
				case 6: { PLUGIN_150_State = 3; break; } // modbus 6 	= Manuele hoogstand      convert to duco 3  = "man3" = ManualMode3; 
				case 7: { PLUGIN_150_State = 4; break; } // modbus 7 	= Niet-thuis-stand       convert to duco 4  = "empt" = EmptyHouse;
				default: { 	addLog(LOG_LEVEL_DEBUG,PLUGIN_LOG_PREFIX_150 + F("Unknown ventilationmode")); }
			}
		}else{
			switch(ventilationState){
				case 4:  {  PLUGIN_150_State = 11; break; } // 	  		= continu LOW		     	convert to duco 11  = "cnt1" = PermanentManualMode1;
				case 5:  {  PLUGIN_150_State = 12; break; } //   	  	= continu MIDDLE       	convert to duco 12  = "cnt2" = PermanentManualMode2;
				case 6:  {  PLUGIN_150_State = 13; break; } //      	= continu HIGH       	convert to duco 13  = "cnt3" = PermanentManualMode3;
				default: { 	addLog(LOG_LEVEL_DEBUG,PLUGIN_LOG_PREFIX_150 + F("Unknown ventilationmode")); }
			}
		}

		// CC1101 automaticly discards packets when CRC isn't OK.
	//	addLog(LOG_LEVEL_DEBUG,PLUGIN_LOG_PREFIX_150 + F("Ignoring RF noise"));


	uint8_t numberOfLogMessages = PLUGIN_150_rf.getNumberOfLogMessages();

	for(int i=0; i< numberOfLogMessages;i++){
		addLog(LOG_LEVEL_INFO, PLUGIN_LOG_PREFIX_150 + PLUGIN_150_rf.logMessages[i]);
	}

}


  
void PLUGIN_150_Publishdata(struct EventStruct *event) {
   UserVar[event->BaseVarIndex]=PLUGIN_150_State;
   String log = PLUGIN_LOG_PREFIX_150 + " State: ";
   log += UserVar[event->BaseVarIndex];
   addLog(LOG_LEVEL_DEBUG, log);
}



void PLUGIN_150_hexstringToHex(char *hexString, uint8_t hexStringSize, unsigned char *hexArray, uint8_t hexArraySize, uint8_t *dataBytes ){
	 const char *pos = hexString;


     /* WARNING: no sanitization or error-checking whatsoever */
    for (size_t count = 0; count < min(hexStringSize/2, (int)hexArraySize); count++) {
        sscanf(pos, "%2hhx", &hexArray[count]);
        pos += 2;
		  (*dataBytes)++; 
		  addLog(LOG_LEVEL_INFO, PLUGIN_LOG_PREFIX_150 + F("COUNT") + count);
    }

}