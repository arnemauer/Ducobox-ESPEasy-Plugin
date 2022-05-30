//#################################### Plugin 150: DUCO RF Gateway ##################################
//
//  DUCO RF Gateway to read Ducobox data
//  https://github.com/arnemauer/Ducobox-ESPEasy-Plugin
//
//  Parts of this code is based on the itho-library made by 'supersjimmie', 'Thinkpad', 'Klusjesman' and 'jodur'.
//#######################################################################################################

#include "_Plugin_Helper.h"
#include <SPI.h>
#include "DucoCC1101.h"
#include "DucoPacket.h"

uint8_t Plugin_150_NetworkId[4];
DucoCC1101 PLUGIN_150_rf;

// extra for interrupt handling
int PLUGIN_150_State=0; // after startup it is assumed that the fan is running in automatic mode
int PLUGIN_150_OldState=1;

int8_t Plugin_150_IRQ_pin=-1;
int8_t Plugin_150_LED_pin=-1;
bool Plugin_150_LED_status=false; // false = off, true=on
bool PLUGIN_150_InitRunned = false;
bool PLUGIN_150_SubscribeMessageSend = false;

#define PLUGIN_150
#define PLUGIN_ID_150         150
#define PLUGIN_NAME_150       "DUCO ventilation remote"
#define PLUGIN_VALUENAME1_150 "Ventilationmode"
#define PLUGIN_LOG_PREFIX_150   String("[P150] RF GW: ")

typedef enum {
	P150_CONFIG_DEVICE_ADDRESS = 0,
    P150_CONFIG_LOG_RF = 1,
	P150_CONFIG_RADIO_POWER = 2,
	P150_CONFIG_HARDWARE_TYPE = 3,
	P150_CONFIG_NETWORKID_BYTE_1_2 = 4,
	P150_CONFIG_NETWORKID_BYTE_3_4 = 5,
} P150PluginConfigs;

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
		case P150_HARDWARE_84_AND_HIGHER: return F("Ventilation gateway V8.4");
		default:
      break;
  	}
  	return "";
}

//interrupt 
volatile uint8_t PLUGIN_150_IRQ = false; //!<  irq flag
volatile unsigned long PLUGIN_150_Int_time = 0;

// Forward declarations
void PLUGIN_150_DUCOcheck();
void PLUGIN_150_Publishdata(struct EventStruct *event);
void PLUGIN_150_hexstringToHex(char *hexString, uint8_t hexStringSize, unsigned char *hexArray, uint8_t hexArraySize, uint8_t *dataBytes );
void PLUGIN_150_GetRfLog();

// IRQ handler:
ICACHE_RAM_ATTR void PLUGIN_150_interruptHandler(void)
{
		PLUGIN_150_IRQ = true; 
		PLUGIN_150_Int_time = millis(); // cant be removed in the future
}

boolean Plugin_150(byte function, struct EventStruct *event, String& string)
{
	boolean success = false;

	switch (function){

		case PLUGIN_DEVICE_ADD:{
			Device[++deviceCount].Number 			= PLUGIN_ID_150;
         	Device[deviceCount].Type 				= DEVICE_TYPE_DUAL; // interruptpin and led pin
         	Device[deviceCount].VType 				= Sensor_VType::SENSOR_TYPE_SINGLE;
			Device[deviceCount].Ports 				= 0;
			Device[deviceCount].PullUpOption 		= false;
			Device[deviceCount].InverseLogicOption 	= false;
			Device[deviceCount].FormulaOption 		= false;
			Device[deviceCount].ValueCount 			= 1;
			Device[deviceCount].SendDataOption 		= true;
			Device[deviceCount].TimerOption 		= true;
			Device[deviceCount].TimerOptional      	= false;
			Device[deviceCount].GlobalSyncOption 	= true;
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
			PCONFIG(P150_CONFIG_DEVICE_ADDRESS) 	= 0;
			PCONFIG(P150_CONFIG_LOG_RF) 			= 0;
			PCONFIG(P150_CONFIG_RADIO_POWER) 		= P150_RADIO_POWER_DEFAULT;
			PCONFIG(P150_CONFIG_NETWORKID_BYTE_1_2) = 0;
			PCONFIG(P150_CONFIG_NETWORKID_BYTE_3_4) = 0;
			success = true;
			break;
		}
  		
		case PLUGIN_INIT: {
			// reset CC1101 when CC1101 is initialised and task is disabled or IRQ pin is not selected. CONFIG_PIN1 
			if (Settings.TaskDeviceEnabled[event->TaskIndex] == false) { // || Settings.TaskDevicePin1[event->TaskIndex] == -1
				String log = PLUGIN_LOG_PREFIX_150;
				log += F("Task disabled, checking if CC1101 needs a reset.");
				addLogMove(LOG_LEVEL_DEBUG, log);
				if(PLUGIN_150_rf.getDucoDeviceState() != ducoDeviceState_notInitialised ){
					PLUGIN_150_rf.reset(); // reset CC1101
					detachInterrupt(Plugin_150_IRQ_pin);
				}

			}else{

				//If configured interrupt pin differs from configured, release old pin first
				if ((Settings.TaskDevicePin1[event->TaskIndex]!=Plugin_150_IRQ_pin) && (Plugin_150_IRQ_pin!=-1)){
					String log = PLUGIN_LOG_PREFIX_150;
					log += F("IO-PIN changed, deatachinterrupt old pin");
					addLogMove(LOG_LEVEL_DEBUG, log);
					detachInterrupt(Plugin_150_IRQ_pin);
				}
				
				// check if interrupt pin is set.
				if (Settings.TaskDevicePin1[event->TaskIndex] == -1){
					String log = PLUGIN_LOG_PREFIX_150;
					log += F("IO-PIN changed, deatachinterrupt old pin");
					addLogMove(LOG_LEVEL_DEBUG, log);
				}else{
					Plugin_150_NetworkId[0] = (PCONFIG(P150_CONFIG_NETWORKID_BYTE_1_2) >> 8);
					Plugin_150_NetworkId[1] = (PCONFIG(P150_CONFIG_NETWORKID_BYTE_1_2) & 0xff);
					Plugin_150_NetworkId[2] = (PCONFIG(P150_CONFIG_NETWORKID_BYTE_3_4) >> 8);
					Plugin_150_NetworkId[3] = (PCONFIG(P150_CONFIG_NETWORKID_BYTE_3_4) & 0xff);
	
					PLUGIN_150_rf.setLogRFMessages(PCONFIG(P150_CONFIG_LOG_RF));
					PLUGIN_150_rf.init();
					PLUGIN_150_rf.setGatewayAddress(PCONFIG(P150_CONFIG_DEVICE_ADDRESS));
					PLUGIN_150_rf.setNetworkId(Plugin_150_NetworkId);
					PLUGIN_150_rf.setRadioPower(P150_radio_power_value[PCONFIG(P150_CONFIG_RADIO_POWER)]);
					PLUGIN_150_rf.setTemperature(210); // = 21.0 C
									
					String log = PLUGIN_LOG_PREFIX_150;
					log += F("Values set from config. DeviceID: ");
					log += PLUGIN_150_rf.getDeviceAddress();
					log += F(", networkId: ");
					for (uint8_t i=0; i<=3; i++){
						log += String(Plugin_150_NetworkId[i], HEX);
					}
					log += F(", radio power: ");
					log += Plugin_150_radiopower_valuename(PCONFIG(P150_CONFIG_RADIO_POWER));
					addLogMove(LOG_LEVEL_INFO, log);

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
						Wire.begin();
						P151_PCF_set_pin_output(6, HIGH); // PCF8574; P6 LED_YELLOW	= HIGH (Led off)
						P151_PCF_set_pin_output(7, HIGH); // PCF8574; P7 LED_BLUE		= HIGH (Led off)
					}


					// start initiating the CC1101 radio module
					PLUGIN_150_rf.initReceive();
					attachInterrupt(Plugin_150_IRQ_pin, PLUGIN_150_interruptHandler, RISING);

					// check if succesfully initialised CC1101 radio module
					switch(PLUGIN_150_rf.getDucoDeviceState()){
						case 0x14: { // initialistion succesfull!
							PLUGIN_150_InitRunned=true;
							success = true;

							log = PLUGIN_LOG_PREFIX_150;
							log += F("CC1101 868Mhz transmitter initialized");
							addLogMove(LOG_LEVEL_INFO, log );

							// only send a subscribemessage when device is joined and has an address.
							if(PCONFIG(P150_CONFIG_DEVICE_ADDRESS) != 0){
								PLUGIN_150_rf.sendSubscribeMessage(); // subscribe to ducobox to get latest ventilation mode.
							}

							// get log messages
							PLUGIN_150_GetRfLog();
							break;
						}
						case 0x15: {
							log = PLUGIN_LOG_PREFIX_150;
							log += F("Initialisation -> calibration failed. No response from CC1101 or status not idle.");
							addLogMove(LOG_LEVEL_INFO, log );
							break;
						}
						case 0x16: {
							log = PLUGIN_LOG_PREFIX_150;
							log += F("Initialisation -> set RXmode failed. No response from CC1101 or status not rxmode.");
							addLogMove(LOG_LEVEL_INFO, log );
							break;
						}
						default: {
							log = PLUGIN_LOG_PREFIX_150;
							log += F("Initialisation -> unexpected device state: ");
							log += PLUGIN_150_rf.getDucoDeviceState();
							addLogMove(LOG_LEVEL_INFO, log );
							break;
						}

					}

					if(PLUGIN_150_rf.getDucoDeviceState() != 0x14){
						success = false;
						break;
					}
				}
			}
			success = true;
			break;
		}

		case PLUGIN_EXIT:{
			//remove interupt when plugin is removed
			if (loglevelActiveFor(LOG_LEVEL_DEBUG)) {
				String log = PLUGIN_LOG_PREFIX_150;
				log += F("EXIT PLUGIN_150");
				addLogMove(LOG_LEVEL_INFO, log);
			}
			detachInterrupt(Plugin_150_IRQ_pin);
			PLUGIN_150_rf.reset(); // reset CC1101

			if(PCONFIG(P150_CONFIG_HARDWARE_TYPE) == P150_HARDWARE_84_AND_HIGHER){
				//P151_PCF_set_pin_output(6, HIGH); // PCF8574; P6 LED_YELLOW	= HIGH (Led off)
				P151_PCF_set_pin_output(7, HIGH); // PCF8574; P7 LED_BLUE		= HIGH (Led off)
			}

			success = true;
			break;
		}


		case PLUGIN_FIFTY_PER_SECOND: {
			// check for interrupt 
			if(PLUGIN_150_InitRunned){
				if(PLUGIN_150_IRQ){
					PLUGIN_150_IRQ = false; 
					if (loglevelActiveFor(LOG_LEVEL_DEBUG)) {
						String log;
						unsigned long current_time = millis();
						log += PLUGIN_150_Int_time;
						log += F(" - ");
						log += current_time;
						log += F(" = ");
						log += (current_time - PLUGIN_150_Int_time);
						addLogMove(LOG_LEVEL_INFO, log);
					}
					if(PLUGIN_150_rf.checkForNewPacket()){
						
						if (PCONFIG(P150_CONFIG_HARDWARE_TYPE) == P150_HARDWARE_84_AND_HIGHER){ // status led
							P151_PCF_set_pin_output(7, LOW); // PCF8574; P7 LED_BLUE		= LOW (Led on)
							Plugin_150_LED_status = true;
						}else if(Plugin_150_LED_pin != -1){
							digitalWrite(Plugin_150_LED_pin, LOW);
							Plugin_150_LED_status = true;
						}

						PLUGIN_150_DUCOcheck();
						PLUGIN_150_GetRfLog(); // get log messages
					}
				}

				PLUGIN_150_rf.checkForAck();
				PLUGIN_150_GetRfLog();  // get log messages
			}
			
			success = true;
			break;
		}


    	case PLUGIN_I2C_HAS_ADDRESS:{
			if(PCONFIG(P150_CONFIG_HARDWARE_TYPE) == P150_HARDWARE_84_AND_HIGHER){
				success = true;
		 	}
      		break;
    	}

		case PLUGIN_TEN_PER_SECOND: {
			if(PLUGIN_150_InitRunned){
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

				// if ESP reacts to slow on CC1101 interrupt its possible a RXFIFO_OVERFLOW occurse. 
				// IRQ pin goes low after reading the first byte from RXfifo
				// If the pin is still high the CC1101 received another message.
				if(PLUGIN_150_rf.checkAndResetRxFifoOverflow()){
					if (loglevelActiveFor(LOG_LEVEL_DEBUG)) {
						String log;
						log += PLUGIN_LOG_PREFIX_150;
						log += F("RX FIFO OVERFLOW -> idle, flush RX FIFO and switch to RX state.");
						addLogMove(LOG_LEVEL_DEBUG, log);
					}
				}
		
				if(PLUGIN_150_rf.pollNewDeviceAddress()){
					if (loglevelActiveFor(LOG_LEVEL_DEBUG)) {
						String log;
						log += PLUGIN_LOG_PREFIX_150;
						log += F("Set new address and network ID after succesfull join.");
						addLogMove(LOG_LEVEL_DEBUG, log);
					}
					memcpy(Plugin_150_NetworkId, PLUGIN_150_rf.getnetworkID(), 4); //convert char array to uint8_t
					PCONFIG(P150_CONFIG_DEVICE_ADDRESS) = PLUGIN_150_rf.getDeviceAddress();
					PCONFIG(P150_CONFIG_NETWORKID_BYTE_1_2) = ((uint16_t)Plugin_150_NetworkId[0] << 8) | Plugin_150_NetworkId[1];
					PCONFIG(P150_CONFIG_NETWORKID_BYTE_3_4) = ((uint16_t)Plugin_150_NetworkId[2] << 8) | Plugin_150_NetworkId[3];
					SaveSettings();
				}
				
				// get log messages
				PLUGIN_150_GetRfLog();
			}
			success = true;
			break;
		}

		case PLUGIN_ONCE_A_SECOND:{ 
			if(PLUGIN_150_InitRunned && !PLUGIN_150_SubscribeMessageSend){
				// only send a subscribemessage when device is joined and has an address.
				if(PCONFIG(P150_CONFIG_DEVICE_ADDRESS) != 0){
					PLUGIN_150_SubscribeMessageSend = true;
					PLUGIN_150_rf.sendSubscribeMessage(); // subscribe to ducobox to get latest ventilation mode.
				}
			}

			//Publish new data when vars are changed or init has runned
			if (PLUGIN_150_OldState!=PLUGIN_150_State){
				if (loglevelActiveFor(LOG_LEVEL_DEBUG)) {
					String log;
					log += PLUGIN_LOG_PREFIX_150;
					log += F("ventilation mode changed - UPDATE by PLUGIN_ONCE_A_SECOND");
					addLogMove(LOG_LEVEL_DEBUG, log);
				}
				UserVar[event->BaseVarIndex]=PLUGIN_150_State;
				sendData(event);
			
				//Remeber current state for next cycle
				PLUGIN_150_OldState=PLUGIN_150_State;
			}

			success = true;
			break;
    	}
    
    	case PLUGIN_READ: {    
        	// This ensures that even when Values are not changing, data is send at the configured interval for aquisition 
			if (loglevelActiveFor(LOG_LEVEL_DEBUG)) {
				String log;
				log += PLUGIN_LOG_PREFIX_150;
				log += F("UPDATE by PLUGIN_READ");
				addLogMove(LOG_LEVEL_DEBUG, log);
			}
			UserVar[event->BaseVarIndex]=PLUGIN_150_State;
        	success = true;
        	break;
    	}  
    
		case PLUGIN_WRITE: {
			String tmpString = string;
			String cmd = parseString(tmpString, 1);
			String param1 = parseString(tmpString, 2);
			String log;
			log += PLUGIN_LOG_PREFIX_150;

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
						log += F("Command 'VENTMODE' - unknown ventilation mode.");
						addLogMove(LOG_LEVEL_INFO, log);
						printWebString += log;
					}else{
						log += F("Sent command 'VENTMODE ");
						log += param1;
						log += F("' to DUCO unit");
						addLogMove(LOG_LEVEL_INFO, log);
						printWebString += log;
						PLUGIN_150_rf.requestVentilationMode(ventilationMode, permanentVentilationMode, percentage, buttonPresses);
					}

					success = true;

					}else{
							log += F("Command ignored, please enable the task.");
							addLogMove(LOG_LEVEL_INFO, log);
							printWebString += log;
					}

				}else if (cmd.equalsIgnoreCase(F("JOIN"))){
					// check if task is enabled
					if (Settings.TaskDeviceEnabled[event->TaskIndex]) {
						//override ventilation with percentage
						log += F("Sent command for 'join' to DUCO unit");
						addLogMove(LOG_LEVEL_INFO, log);
						printWebString += log;
						PLUGIN_150_rf.sendJoinPacket();
						success = true;
					}else{
						log += F("Command ignored, please enable the task.");
						addLogMove(LOG_LEVEL_INFO, log);
						printWebString += log;
					}

				} else if (cmd.equalsIgnoreCase(F("DISJOIN"))){
					// check if task is enabled
					if (Settings.TaskDeviceEnabled[event->TaskIndex]) {
						log += F("Sent command for 'disjoin' to DUCO unit");
						addLogMove(LOG_LEVEL_INFO, log);
						printWebString += log;
						PLUGIN_150_rf.sendDisjoinPacket();
						success = true;
					}else{
						log += F("Command ignored, please enable the task.");
						addLogMove(LOG_LEVEL_INFO, log);
						printWebString += log;
					}
				}else if (cmd.equalsIgnoreCase(F("INSTALLERMODE"))){
					// check if task is enabled
					if (Settings.TaskDeviceEnabled[event->TaskIndex]) {
						log += F("Sent command for ");
						if(param1.equalsIgnoreCase(F("OFF"))){
							PLUGIN_150_rf.disableInstallerMode();
							log += F("disabling");
						}else if(param1.equalsIgnoreCase(F("ON"))){
							log += F("enabling");
							PLUGIN_150_rf.enableInstallerMode();
						}
						
						log += F(" installermode.");
						success = true;

					}else{
						log += F("Command ignored, please enable the task.");
					}
					addLogMove(LOG_LEVEL_INFO, log);
					printWebString += log;
				}else if (cmd.equalsIgnoreCase(F("CC1101STATUS"))){

					uint8_t marcState = PLUGIN_150_rf.getMarcState(true);
					char LogBuf[20];
					snprintf(LogBuf, sizeof(LogBuf), "getMarcState: %02X",marcState);

					log += LogBuf;
					addLogMove(LOG_LEVEL_INFO, log);
					printWebString += log;
					success = true;

				}else if (cmd.equalsIgnoreCase(F("RXBYTES"))){

					uint8_t marcState = PLUGIN_150_rf.TEST_getRxBytes();

					char LogBuf[20];
					snprintf(LogBuf, sizeof(LogBuf), "rxBYTES: %02X",marcState);
					log += LogBuf;
					addLogMove(LOG_LEVEL_INFO, log);
					printWebString += log;
					success = true;

				}else if (cmd.equalsIgnoreCase(F("DUCOTESTMESSAGE"))){
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

					// databytes
					String param6 = parseString(tmpString, 7); 
					char hexValues[40];
					param6.toCharArray(hexValues, 40);
					
					uint8_t hexArraySize = 20;
					unsigned char hexArray[hexArraySize];
					uint8_t dataBytes = 0;
					PLUGIN_150_hexstringToHex(hexValues, strlen(hexValues), hexArray, hexArraySize, &dataBytes);

					uint8_t* myuint8array = (uint8_t*)hexArray;
					PLUGIN_150_rf.sendRawPacket(messageType, sourceAddress, destinationAddress, originalSourceAddress, originalDestinationAddress, myuint8array, dataBytes);

					log +=  F("Sent DUCOTESTMESSAGE to Duco Network. Databytes: ");
					log += dataBytes;
					addLogMove(LOG_LEVEL_INFO, log);
					printWebString += log;
					success = true;
				}

				PLUGIN_150_GetRfLog();			
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
			String log;
			log += PLUGIN_LOG_PREFIX_150;
			log += F("PLUGIN_WEBFORM_LOAD");
			addLogMove(LOG_LEVEL_DEBUG, log);

			addRowLabel(F("Hardware type"));
			addSelector_Head(F("p150_hardware_type"));

			for (byte x = 0; x < P150_HARDWARE_NR_OUTPUT_OPTIONS; x++){
				addSelector_Item(Plugin_150_hardware_type(x), x, PCONFIG(P150_CONFIG_HARDWARE_TYPE) == x, false, "");
			}
			addSelector_Foot();

			addFormSubHeader(F("Remote RF Controls (automaticly filled after succesfull join)"));
			char tempNetworkId [9];
			char tempBuffer[20];
			char tempByte[4];
			tempByte[0] = PCONFIG(P150_CONFIG_NETWORKID_BYTE_1_2) >> 8;
			tempByte[1] = PCONFIG(P150_CONFIG_NETWORKID_BYTE_1_2) & 0xff;
			tempByte[2] = PCONFIG(P150_CONFIG_NETWORKID_BYTE_3_4) >> 8;
			tempByte[3] = PCONFIG(P150_CONFIG_NETWORKID_BYTE_3_4) & 0xff;

			for(int i=0; i<=3; i++){    // start with lowest byte of number
				sprintf(&tempBuffer[0],"%02X", tempByte[i]); //converts to hexadecimal base.
				tempNetworkId[(i*2)] = tempBuffer[0];
				tempNetworkId[(i*2)+1] = tempBuffer[1];
			}
			tempNetworkId[8] = '\0';
			addFormTextBox( F("Network ID (HEX)"), F("p150_network_id"), tempNetworkId, 8);
			addFormNumericBox(F("Device Address"), F("p150_deviceaddress"), PCONFIG(P150_CONFIG_DEVICE_ADDRESS), 0, 255);

			// Selector for Radio power
			addRowLabel(F("Radio Power"));
			addSelector_Head(F("p150_radio_power"));
			for (byte x = 0; x < P150_RADIO_POWER_OUTPUT_OPTIONS; x++){
				String name     = Plugin_150_radiopower_valuename(x);
				addSelector_Item(name, x, PCONFIG(P150_CONFIG_RADIO_POWER) == x, false, "");
			}
			addSelector_Foot();
			// Selector for Radio power

			addFormCheckBox(F("Log rf messages to syslog"), F("p150_log"), PCONFIG(P150_CONFIG_LOG_RF));

			success = true;
			break;
    	}

    	case PLUGIN_WEBFORM_SAVE:{
			PCONFIG(P150_CONFIG_HARDWARE_TYPE) = getFormItemInt(F("p150_hardware_type"), 0);

			unsigned long number = strtoul( web_server.arg(F("p150_network_id")).c_str(), nullptr, 16);
			PCONFIG(P150_CONFIG_NETWORKID_BYTE_3_4) = number & 0xFFFF;  // or: = byte( number);
			number >>= 16;            // get next byte into position
			PCONFIG(P150_CONFIG_NETWORKID_BYTE_1_2) = number & 0xFFFF;  // or: = byte( number);

			PCONFIG(P150_CONFIG_DEVICE_ADDRESS) = getFormItemInt(F("p150_deviceaddress"));
			PCONFIG(P150_CONFIG_RADIO_POWER) = getFormItemInt(F("p150_radio_power"));
			PCONFIG(P150_CONFIG_LOG_RF) = isFormItemChecked(F("p150_log"));

			success = true;
			break;
    	}	

	}
	return success;
} 




void PLUGIN_150_GetRfLog(){
	uint8_t numberOfLogMessages = PLUGIN_150_rf.getNumberOfLogMessages();
	String log;

    if (loglevelActiveFor(LOG_LEVEL_DEBUG) && log.reserve(144)) {
		for(int i=0; i< numberOfLogMessages;i++){
			log += PLUGIN_LOG_PREFIX_150;
			log += PLUGIN_150_rf.logMessages[i];
			addLogMove(LOG_LEVEL_DEBUG, log);
		}
	}
}



void PLUGIN_150_DUCOcheck() {
	PLUGIN_150_rf.processNewMessages();

		uint8_t ventilationState = PLUGIN_150_rf.getCurrentVentilationMode();
		bool permanentMode = PLUGIN_150_rf.getCurrentPermanentMode();

     	//  convert modbus status to "normal" duco status numbers
		if(!permanentMode){
			switch(ventilationState){
				case 0: { PLUGIN_150_State = 0; break; } // modbus 0 	= auto                   convert to duco 0  = "auto" = AutomaticMode;
				case 4: { PLUGIN_150_State = 1; break; } // modbus 4 	= Manuele laagstand      convert to duco 1  = "man1" = ManualMode1;
				case 5: { PLUGIN_150_State = 2; break; } // modbus 5 	= Manuele middenstand    convert to duco 2  = "man2" = ManualMode2; 
				case 6: { PLUGIN_150_State = 3; break; } // modbus 6 	= Manuele hoogstand      convert to duco 3  = "man3" = ManualMode3; 
				case 7: { PLUGIN_150_State = 4; break; } // modbus 7 	= Niet-thuis-stand       convert to duco 4  = "empt" = EmptyHouse;
				default: { 	String log = PLUGIN_LOG_PREFIX_150; log += F("Unknown ventilationmode"); addLogMove(LOG_LEVEL_INFO, log); }
			}
		}else{
			switch(ventilationState){
				case 4:  {  PLUGIN_150_State = 11; break; } // 	  		= continu LOW		    convert to duco 11  = "cnt1" = PermanentManualMode1;
				case 5:  {  PLUGIN_150_State = 12; break; } //   	  	= continu MIDDLE       	convert to duco 12  = "cnt2" = PermanentManualMode2;
				case 6:  {  PLUGIN_150_State = 13; break; } //      	= continu HIGH       	convert to duco 13  = "cnt3" = PermanentManualMode3;
				default: {  String log = PLUGIN_LOG_PREFIX_150; log += F("Unknown ventilationmode"); addLogMove(LOG_LEVEL_INFO, log); }
			}
		}

		PLUGIN_150_GetRfLog();

}

void PLUGIN_150_hexstringToHex(char *hexString, uint8_t hexStringSize, unsigned char *hexArray, uint8_t hexArraySize, uint8_t *dataBytes ){
	const char *pos = hexString;
	String log;
	log += PLUGIN_LOG_PREFIX_150;
    /* WARNING: no sanitization or error-checking whatsoever */
    for (uint8_t count = 0; count < min(hexStringSize/2, (int)hexArraySize); count++) {
        sscanf(pos, "%2hhx", &hexArray[count]);
        pos += 2;
		(*dataBytes)++; 
		log += count;
		addLogMove(LOG_LEVEL_INFO, log);
    }
}