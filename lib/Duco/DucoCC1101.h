/*
 * Author: Arne Mauer
 * Parts of this code is based on the itho-library made by 'supersjimmie', 'Thinkpad', 'Klusjesman' and 'jodur'.
 */

#ifndef __DUCOCC1101_H__
#define __DUCOCC1101_H__

#include <stdio.h>
#include "CC1101.h"
#include "DucoPacket.h"


#define DUCO_PARAMETER							0x05
#define CC1101_WRITE_BURST						0x40
#define CC1101_WRITE_BURST						0x40

//pa table settings
const uint8_t ducoPaTableReceive[8] = {0xC5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

const uint8_t joinCO2NetworkId[4] = {0x00,0x00,0x7C,0x3E};

const uint8_t ducoDeviceState_notInitialised = 0x00;	
const uint8_t ducoDeviceState_join1 = 0x01;	
const uint8_t ducoDeviceState_join2 = 0x02;	
const uint8_t ducoDeviceState_join3 = 0x03;	
const uint8_t ducoDeviceState_join4 = 0x04;	
const uint8_t ducoDeviceState_joinSuccessful = 0x05;	

const uint8_t ducoDeviceState_disjoinRequest = 0x0A;	
const uint8_t ducoDeviceState_disjoinWaitingForAck = 0x0B;	
const uint8_t ducoDeviceState_disjoinWaitingForConfirmation = 0x0C;	
const uint8_t ducoDeviceState_disjointed = 0x0D;	

const uint8_t ducoDeviceState_initialised = 0x14;	
const uint8_t ducoDeviceState_initialisationCalibrationFailed = 0x15;
const uint8_t ducoDeviceState_initialisationRxmodeFailed = 0x16;


		// 0x00 = not initialised
		// 0x01 = join -> step 1, join1 message sent, waiting for join2 message
		// 0x02 = join -> step 2, join2 message received
		// 0x03 = join -> step 3, join3 message sent, waiting for join4 message
		// 0x04 = join -> step 4, join4 message received,
		// 0x05 = join succesfull, new networkid and deviceid
		
		// 0x0A = disjoin command sent
		// 0x0B = waiting for ack
		// 0x0C = ducobox disjoin confirmation
		// 0x0D = device disjointed

		// 0x14 = initialised with address/networkid from config
		
	const uint8_t ducomsg_network 	= 0x00;
	const uint8_t ducomsg_join1		= 0x01;
	const uint8_t ducomsg_join2		= 0x02;
	const uint8_t ducomsg_join3 	= 0x03;
	const uint8_t ducomsg_join4 	= 0x04;
	const uint8_t ducomsg_ack 		= 0x06;
	const uint8_t ducomsg_message 	= 0x07;

class DucoCC1101 : protected CC1101
{
	private:
		//receive
		CC1101Packet inMessage;												//temp storage message
		CC1101Packet outMessage;
		
		DucoPacket inDucoPacket;												//stores last received message data
		
		//send
		DucoPacket outDucoPacket;												//stores state of "remote"

		//settings
		uint8_t sendTries;														//number of times a command is send at one button press
		
		void arrayToString(byte array[], unsigned int len, char buffer[]);


		uint8_t deviceAddress;
		uint8_t networkId[4];

		uint8_t pingCounter;

		uint8_t messageCounter;
		uint8_t currentVentilationMode;

		bool waitingForAck; 
		uint8_t counterInAck;
		uint8_t ackRetry;

		uint8_t ducoDeviceState;
		// 0x00 = not initialised
		// 0x01 = join -> step 1, join1 message sent, waiting for join2 message
		// 0x02 = join -> step 2, join2 message received
		// 0x03 = join -> step 3, join3 message sent
		// 0x04 = join -> step 4, join4 message received,
		// 0x05 = join succesfull, new networkid and deviceid
		
		// 0x0A = disjoin command sent
		// 0x0B = waiting for ack
		// 0x0C = ducobox disjoin confirmation
		// 0x0D = device disjoined

		// 0x14 = initialised with address/networkid from config
		// 0x15 = initialisation failed stage 1 -> calibration failed or no response from cc1101
		// 0x16 = initialisation failed stage 2 -> set RX mode failed or no response from cc1101

		uint8_t testCounter;
		uint8_t ducoboxLastRssiByte;

		//String logMessages[10]; // to store some log messages from library
		#define NUMBER_OF_LOG_STRING 10
		#define MAX_LOG_STRING_SIZE 600

		uint8_t numberOfLogmessages = 0;
		String logline; // used to build a log message

		unsigned long ackTimer;
	//functions
	public:
		DucoCC1101(uint8_t counter = 0, uint8_t sendTries = 2);		//set initial counter value
		~DucoCC1101();
		
		//init
		void init() { CC1101::init(); }											//init,reset CC1101
		void initReceive();
		uint8_t getLastCounter() { return outDucoPacket.counter; }				//counter is increased before sending a command
		void setSendTries(uint8_t sendTries) { this->sendTries = sendTries; }
		
		uint8_t getCurrentVentilationMode() { return this->currentVentilationMode; }						//retrieve last received/parsed command from remote	
		uint8_t getDucoDeviceState() { return this->ducoDeviceState; } // retrieve radio/duco state
		



		//deviceid
		void setDeviceAddress(uint8_t deviceAddress){ this->deviceAddress = deviceAddress; }
		uint8_t getDeviceAddress() { return deviceAddress; }	

		//deviceid
		void setNetworkId(uint8_t newNetworkId[]){ 
			memcpy(networkId, newNetworkId, 4);
		}
		uint8_t* getnetworkID() { return networkId; }	

		uint8_t getNumberOfLogMessages();
		char logMessages[NUMBER_OF_LOG_STRING][MAX_LOG_STRING_SIZE];


		void checkForAck();

		void sendJoinPacket();
		void sendDisjoinPacket();
		//receive
		bool checkForNewPacket(bool logRFMessages);												//check RX fifo for new data
		bool checkForNewPacketInRXFifo();
		uint8_t checkForBytesInRXFifo();

		void requestVentilationMode(uint8_t ventilationMode, uint8_t percentage, uint8_t temp);


		DucoPacket getLastPacket() { return inDucoPacket; }						//retrieve last received/parsed packet from remote
		uint8_t getLastInCounter() { return inDucoPacket.counter; }						//retrieve last received/parsed command from remote

		bool pollNewDeviceAddress();
				
	protected:
	private:
		DucoCC1101( const DucoCC1101 &c);
		DucoCC1101& operator=( const DucoCC1101 &c);

		void setLogMessage(const char *newLogMessage);



  		char valToHex(uint8_t val);

 		String byteToHexString(uint8_t b);

		// joinCO2NetworkId
		void sendJoin3Packet();
		void sendJoinFinish();
		void finishDisjoin();

		void processNetworkPacket();

		uint8_t updateMessageCounter();
		uint8_t getRssi();

		//parse received message
		void parseReceivedPackets();
		void parseMessageCommand();
				
		void sendPing();
		void sendConfirmationNewVentilationMode();
		void sendNodeParameterValue();
		void sendMultipleNodeParameterValue();


		bool matchingDeviceAddress(uint8_t compDeviceAddress);

		bool matchingNetworkId(uint8_t id[]);

		void getParameterValue(uint8_t parameter, uint8_t *value);

		//send
		void ducoToCC1101Packet(DucoPacket *duco, CC1101Packet *packet);
		void prefillDucoPacket(DucoPacket *ducoOutPacket, uint8_t receiverAddress);
		void sendAck();
		void waitForAck();
		
		//test
		void testCreateMessage();

}; //DucoCC1101

#endif //__DUCOCC1101_H__
