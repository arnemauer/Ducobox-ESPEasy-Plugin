/*
 * Author: Arne Mauer
 * Parts of this code is based on the -library made by 'supersjimmie', 'Thinkpad', 'Klusjesman' and 'jodur'.
 */

#include "DucoCC1101.h"
#include <string.h>
#include <Arduino.h>
#include <SPI.h>


// TODO: 
// - Hoe reageert een bedieningsschakelaar wanneer er een networkcall is voor zijn network address?
// bijvoorbeeld gw address = 5
// Received message: SRC:1; DEST:0; ORG.SRC:1; ORG.DEST:0; Ntwrk:0001f947;Type:0; Bytes:10; Counter:12; RSSI:-68 (0x0D);
// DATA: FF,05,  <<<< 





// default constructor
DucoCC1101::DucoCC1101(uint8_t counter, uint8_t sendTries) : CC1101()
{
	this->outDucoPacket.counter = counter;
	this->sendTries = sendTries;
	this->deviceAddress = 0;
	this->radioPower = 0xC1; // default radio power 0xC1 = 10,3dBm @ 868mhz
	this->waitingForAck = 0; 
	this->ackTimer = 0;
	this->ackRetry = 0;

	this->testCounter = 0; // temp 
	
	this->messageCounter = 1; // for messages out NEVER ZERO!
	this->lastRssiByte = 0;

	this->numberOfLogmessages = 0;

	this->installerModeActive = false;

} //DucoCC1101

// default destructor
DucoCC1101::~DucoCC1101()
{
} //~DucoCC1101

void DucoCC1101::setLogMessage(const __FlashStringHelper* flashString)
{
    String s(flashString);
    setLogMessage(s.c_str());
}

void DucoCC1101::setLogMessage(const char *newLogMessage){
	snprintf(logMessages[this->numberOfLogmessages], sizeof(logMessages[this->numberOfLogmessages]), "%lu - %s", millis(), newLogMessage);
	//strncpy(logMessages[this->numberOfLogmessages], newLogMessage, sizeof( (logMessages[this->numberOfLogmessages]) ) );

	if(this->numberOfLogmessages == 9){
		this->numberOfLogmessages = 0;
	}else{
		this->numberOfLogmessages++;
	}
}

uint8_t DucoCC1101::getNumberOfLogMessages(){
	uint8_t tempNumber = this->numberOfLogmessages;
	this->numberOfLogmessages = 0;
	return tempNumber;
}


void DucoCC1101::reset(){
	writeCommand(CC1101_SRES);
	this->ducoDeviceState = ducoDeviceState_notInitialised;

	// TODO: reset/delete all variables
	return;
}

void DucoCC1101::initReceive()
{
	/*
	Configuration reverse engineered from RFT print.
	
	Base frequency		868.299866MHz
	Channel				0
	Channel spacing		199.951172kHz
	Carrier frequency	868.299866MHz
	Xtal frequency		26.000000MHz
	Data rate			38.3835kBaud
	RX filter BW		325.000000kHz
	Manchester			disabled
	Modulation			2-FSK
	Deviation			50.781250kHz
	TX power			0x6F,0x26,0x2E,0x7F,0x8A,0x84,0xCA,0xC4
	PA ramping			enabled
	Whitening			disabled
	*/	
	this->ducoDeviceState = ducoDeviceState_notInitialised;
	writeCommand(CC1101_SRES);

	writeRegister(CC1101_IOCFG0 ,0x2E);	//!!! 	//High impedance (3-state)
	writeRegister(CC1101_IOCFG1 ,0x2E);	//!!    //High impedance (3-state)
	//writeRegister(CC1101_IOCFG2 ,0x07);	//!!! duco 0x07= Asserts when a packet has been received with CRC OK. De-asserts when the first byte is read from the RX FIFO.
	
	writeRegister(CC1101_IOCFG2 ,0x06);	
	/* 6 (0x06) = Asserts when sync word has been sent / received, and de-asserts at the end of the packet. 
	=> In RX, the pin will also deassert when a packet is discarded due to address or maximum length filtering 
      or when the radio enters RXFIFO_OVERFLOW state. 
			=>> on rising -> DO NOTHING
			=>> on falling -> check 

   => In TX the pin will de-assert if the TX FIFO underflows.
	      =>> TODO: disable interrupt
	*/

	writeRegister(CC1101_FIFOTHR, 0x07); // default value. FIFO treshold TX=33;RX=32
	writeRegister(CC1101_SYNC1 ,0xD3);	//duco 0xD3 /   		//message2 byte6
	writeRegister(CC1101_SYNC0 ,0x91);	//duco 0x91	/  	//message2 byte7

	writeRegister(CC1101_PKTLEN ,0x20); // duco 0x20 / maximum packet size is 32 bytes,
	/* In variable packet length mode, PKTCTRL0.LENGTH_CONFIG=1, the packet length is configured by the first byte after the
sync word. The packet length is defined as the payload data, excluding the length byte and the optional CRC. The PKTLEN register is
used to set the maximum packet length allowed in RX. Any packet received with a length byte with a value greater than PKTLEN will be discarded.*/


	writeRegister(CC1101_PKTCTRL1 ,0x0C); //duco 0x0C=00001100 /  	//ADR_CHK[1:0]=No address check, APPEND_STATUS= two status bytes will be appended to the payload of the packet. The status bytes contain RSSI and LQI values, as well as CRC OK.; CRC_AUTOFLUSH = Enable automatic flush of RX FIFO when CRC is not OK.
	writeRegister(CC1101_PKTCTRL0 ,0x05); //duco 0x05=00000101 /  		//Variable packet length mode. Packet length configured by the first byte after sync word, CRC calculation in TX and CRC check in RX enabled, No data whitening, Normal mode, use FIFOs for RX and TX
	
	writeRegister(CC1101_ADDR ,0x00); // duco/ NOT USED: Address used for packet filtration. 
	writeRegister(CC1101_CHANNR ,0x00); // duco/

	writeRegister(CC1101_FSCTRL1 ,0x06); // duco/
	writeRegister(CC1101_FSCTRL0 ,0x00); // duco/

	writeRegister(CC1101_FREQ2 ,0x21); // duco/
	writeRegister(CC1101_FREQ1 ,0x65); // duco/
	writeRegister(CC1101_FREQ0 ,0x5C); //  0x6A / duco: 0x5C

	writeRegister(CC1101_MDMCFG4 ,0xCA); // duco 0xCA   / 0xE8 
	writeRegister(CC1101_MDMCFG3 ,0x83); // duco 0x83   / 0x43 
	writeRegister(CC1101_MDMCFG2 ,0x13); // duco 0x13   / 0x00 		//Enable digital DC blocking filter before demodulator, 2-FSK, Disable Manchester encoding/decoding, No preamble/sync 
	writeRegister(CC1101_MDMCFG1 ,0x22); // duco/		//Disable FEC
	writeRegister(CC1101_MDMCFG0 ,0xF8); // duco/
	
	writeRegister(CC1101_DEVIATN ,0x35); // duco 0x35   / 0x40 

	writeRegister(CC1101_MCSM2 ,0x07);  
	writeRegister(CC1101_MCSM1 ,0x2F); //Next state after finishing packet reception/transmission => RX
	writeRegister(CC1101_MCSM0 ,0x08);  //no auto calibrate // 0x18 / duco 0x08

	writeRegister(CC1101_FOCCFG ,0x16); // duco/
	writeRegister(CC1101_BSCFG ,0x6C); // duco/
	writeRegister(CC1101_AGCCTRL2 ,0x43);  // duco/
	writeRegister(CC1101_AGCCTRL1 ,0x40); // duco/
	writeRegister(CC1101_AGCCTRL0 ,0x91); // duco/

	writeRegister(CC1101_FREND1 ,0x56); // duco/ 0x56
	writeRegister(CC1101_FREND0 ,0x10); // duco 0x10   / 0x17


	writeRegister(CC1101_FSCAL3 ,0xE9); // duco 0xE9 /  0xA9
	writeRegister(CC1101_FSCAL2 ,0x2A); // duco/
	writeRegister(CC1101_FSCAL1 ,0x00); // duco/
	writeRegister(CC1101_FSCAL0 ,0x1F); // duco/

	writeRegister(CC1101_FSTEST ,0x59); // duco/
	writeRegister(CC1101_TEST2 ,0x81); // duco/
	writeRegister(CC1101_TEST1 ,0x35); // duco/
	writeRegister(CC1101_TEST0 ,0x09); // duco 0x09 / 0x0B

	writeRegister(CC1101_IOCFG0 ,0x07);	//!!! 	//High impedance (3-state)

	
	//0x6F,0x26,0x2E,0x7F,0x8A,0x84,0xCA,0xC4
	// DUCO: 0xC5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00


	uint8_t ducoPaTableReceive[8] = {this->radioPower, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

	writeBurstRegister(CC1101_PATABLE | CC1101_WRITE_BURST, (uint8_t*)ducoPaTableReceive, 8);
	
	writeCommand(CC1101_SIDLE);
	writeCommand(CC1101_SIDLE);

	writeCommand(CC1101_SCAL);

	//wait for calibration to finish
	//while ((readRegisterWithSyncProblem(CC1101_MARCSTATE, CC1101_STATUS_REGISTER)) != CC1101_MARCSTATE_IDLE) yield();
	// ADDED: Timeout added because device will loop when there is no response from cc1101. 
	unsigned long startedWaiting = millis();
	while((readRegisterWithSyncProblem(CC1101_MARCSTATE, CC1101_STATUS_REGISTER)) != CC1101_MARCSTATE_IDLE && millis() - startedWaiting <= 1000)
	{
		//esp_yield();
		delay(0); // delay will call esp_yield()
	}

	if((readRegisterWithSyncProblem(CC1101_MARCSTATE, CC1101_STATUS_REGISTER)) != CC1101_MARCSTATE_IDLE){
		this->ducoDeviceState = ducoDeviceState_initialisationCalibrationFailed;
		return;
	}


	writeCommand(CC1101_SRX);
	// wait till in rx mode (0x1F)
	//while ((readRegisterWithSyncProblem(CC1101_MARCSTATE, CC1101_STATUS_REGISTER)) != CC1101_MARCSTATE_RX) yield();
	// ADDED: Timeout added because device will loop when there is no response from cc1101. 
	startedWaiting = millis();
	while((readRegisterWithSyncProblem(CC1101_MARCSTATE, CC1101_STATUS_REGISTER)) != CC1101_MARCSTATE_RX && millis() - startedWaiting <= 1000)
	{
		delay(0); // delay will call esp_yield()
	}

	if((readRegisterWithSyncProblem(CC1101_MARCSTATE, CC1101_STATUS_REGISTER)) != CC1101_MARCSTATE_RX){
		this->ducoDeviceState = ducoDeviceState_initialisationRxmodeFailed;
		return;
	}	



//	if(this->deviceAddress != 0x00){
	this->ducoDeviceState = ducoDeviceState_initialised;
//	}


}


void DucoCC1101::sendDataToDuco(CC1101Packet *packet){
	// DEBUG
	if(this->logRFMessages){
		char bigLogBuf[250];
		arrayToString(packet->data, min(static_cast<uint>(packet->length),sizeof(bigLogBuf)) , bigLogBuf);
		setLogMessage(bigLogBuf);
	}
	// DEBUG
	sendData(packet);
}



bool DucoCC1101::pollNewDeviceAddress(){
	if(this->ducoDeviceState == ducoDeviceState_joinSuccessful) {
		this->ducoDeviceState = ducoDeviceState_initialised;
		return 1;
	}else if(this->ducoDeviceState == ducoDeviceState_disjointed){
		this->ducoDeviceState = ducoDeviceState_notInitialised;
		return 1;
	}
	return 0;
}


  char DucoCC1101::valToHex(uint8_t val) {
    if ((val & 0x0f) < 10)
      return ('0' + val);
    else
      return ('a' + (val - 10));
  }

  String DucoCC1101::byteToHexString(uint8_t b) {
    String buffer = "";
    buffer += valToHex(b & 0x0f);
    b >>= 4;
    buffer = valToHex(b & 0x0f) + buffer;
    return buffer;
  }


void DucoCC1101::arrayToString(byte array[], unsigned int len, char buffer[])
{
    for (unsigned int i = 0; i < len; i++)
    {
        byte nib1 = (array[i] >> 4) & 0x0F;
        byte nib2 = (array[i] >> 0) & 0x0F;
        buffer[i*3+0] = nib1  < 0xA ? '0' + nib1  : 'A' + nib1  - 0xA;
        buffer[i*3+1] = nib2  < 0xA ? '0' + nib2  : 'A' + nib2  - 0xA;
		buffer[i*3+2] = 0x2C; // comma
    }
    buffer[len*3] = '\0';
}


bool DucoCC1101::checkForNewPacket()
{
	if (receiveData(&inMessage)){
		//getMarcState();
		if(inMessage.length < 8 ){
			//discard package? length is smaller dan minimal packet length.... 
			// resulting in negative uint8_t inDucoPacket.dataLength :S
			setLogMessage(F("Invalid packed length (<8)."));
			return false;
		}

		// reset dataLength
		inDucoPacket.dataLength = 0;

		inDucoPacket.messageType = inMessage.data[0];

		// Fill Duco packet with data
		memcpy(inDucoPacket.networkId, &inMessage.data[1], sizeof inDucoPacket.networkId);
		inDucoPacket.sourceAddress = (inMessage.data[5] >> 3); // first 5 bits 
		inDucoPacket.destinationAddress = ( (inMessage.data[5] & 0b00000111) << 2) | (inMessage.data[6] >> 6); // get last 3 bits and shift left.
		inDucoPacket.originalSourceAddress = ( (inMessage.data[6] & 0b00111110) >> 1); // first 5 bits 
		inDucoPacket.originalDestinationAddress = ( (inMessage.data[6] & 0b00000001) << 4) | (inMessage.data[7] >> 4); // get last 3 bits and shift left.
		inDucoPacket.counter = (inMessage.data[7] & 0b00001111);

		inDucoPacket.crc_ok = inMessage.crc_ok;
		inDucoPacket.rssi = inMessage.rssi;
		inDucoPacket.lqi = inMessage.lqi;

		memcpy(&inDucoPacket.data, &inMessage.data[8],(inMessage.length-8));
		inDucoPacket.dataLength = (inMessage.length-8);

		// save RSSI value (we need this later for our reply on network messages )
		//lastRssiByte = inDucoPacket.data[inDucoPacket.dataLength-1];

		// CREATE LOG ENTRY OF RECEIVED PACKET
		if(this->logRFMessages){
  			char bigLogBuf[250];
		
			snprintf(bigLogBuf, sizeof(bigLogBuf), "Received message: SRC:%u; DEST:%u; ORG.SRC:%u; ORG.DEST:%u; Ntwrk:%02x%02x%02x%02x;",
				  inDucoPacket.sourceAddress,inDucoPacket.destinationAddress, inDucoPacket.originalSourceAddress, inDucoPacket.originalDestinationAddress,inDucoPacket.networkId[0], inDucoPacket.networkId[1], inDucoPacket.networkId[2], inDucoPacket.networkId[3]);
			setLogMessage(bigLogBuf);
			memset(bigLogBuf, 0, sizeof(bigLogBuf)); // reset char bigLogBuf

			snprintf(bigLogBuf, sizeof(bigLogBuf), "Type:%u; Bytes:%u; Counter:%u; RSSI:%d (0x%02X); ",
				  	inDucoPacket.messageType, inMessage.length, inDucoPacket.counter,	 convertRssiHexToDBm(), inDucoPacket.rssi);
			setLogMessage(bigLogBuf);

			memset(bigLogBuf, 0, sizeof(bigLogBuf)); // reset char bigLogBuf

			//log received bytes 
			arrayToString(inDucoPacket.data, min(static_cast<uint>(inDucoPacket.dataLength),sizeof(bigLogBuf)) , bigLogBuf);
			setLogMessage(bigLogBuf);
		}

		// check if network ID is our network ID
		/*YES: 
		- ducomsg_network
		- ducomsg_ack
		- ducomsg_message

		 IF NOT:
		- ducomsg_join2
		- ducomsg_join4
		*/ 
		if(matchingNetworkId(inDucoPacket.networkId)){ // check for network id
				
			// if destinationAddress is broadcastaddress = 0, then repeat the message
			if(inDucoPacket.destinationAddress == 0x00 && inDucoPacket.sourceAddress == 0x01 && inDucoPacket.messageType == ducomsg_network){
				setLogMessage(F("Received messagetype: network0"));
				processNetworkPacket();

			// check if the message is send to this device
			}else if(inDucoPacket.destinationAddress == this->deviceAddress){

				//  if originalDestinationAddress is our address, we need to process the packet.
				//  otherwise repeat the message
				if(inDucoPacket.originalDestinationAddress == this->deviceAddress){
		
					switch(inDucoPacket.messageType){
						case ducomsg_network:{
							// if we receive a network message send to a specific node we most send an Ack!
							setLogMessage(F("Received messagetype: network0 (addressed to this node!)"));
							sendAck(); 
							processNetworkPacket(); // also resend network package if it is addressed to this node?
							break;
						}
						case ducomsg_ack:{
							setLogMessage(F("Received messagetype: ACK"));
							
							// stop retry sending message
							if(outDucoPacket.counter == inDucoPacket.counter){
								waitingForAck = 0;
								setLogMessage(F("Ack received!"));

								// if we've send a disjoin message and receive an ACK, set ducoDeviceState to the next status
								if(ducoDeviceState == ducoDeviceState_disjoinWaitingForAck){
									ducoDeviceState = ducoDeviceState_disjoinWaitingForConfirmation;
								}
							}else{
									setLogMessage(F("Ack received but counter doesnt match!"));
							}					
							break;
						}

						case ducomsg_message:{
							setLogMessage(F("Received messagetype: Normal message"));
							sendAck(); 
							parseMessageCommand();	
							break;
						}

						case ducomsg_join4:{
							setLogMessage(F("Received messagetype: JOIN4"));
							// if we are waiting for disjoin confirmation, finish disjoin
							if(ducoDeviceState == ducoDeviceState_disjoinWaitingForConfirmation){
								finishDisjoin();

							// if we are waiting for join confirmation, finish joining
							}else if(ducoDeviceState == ducoDeviceState_join3){
								processJoin4Packet();
							}else{
								// if ducobox didnt receive the first ack (from sendJoinFinish()), check if address in packet is the same and send ack again!
								setLogMessage(F("Received join4 message but join already finished, check address and resend ACK."));
								if(inDucoPacket.data[5] == this->deviceAddress){
									sendAck();
									setLogMessage(F("sendJoin4FinishPacket: another ACK sent!"));
								}else{
									setLogMessage(F("No match between join4 address and our deviceid. Ignoring message."));
								}
							}
							break;
						}

						default:{
							setLogMessage(F("Received messagetype: unknown"));
							break;
						}
					} // end switch

				}else{
				// destinationAddress is our address but originalDestinationAddress is an other device
				// as repeater we need to repeat the message (send to inDucoPacket.originalDestinationAddress)
					repeatMessage();
				}
						
			}
		
		}else
		{
			// network ID doesnt match with our network ID => are we joining a network?
			switch(inDucoPacket.messageType){
				case ducomsg_join2:{
					setLogMessage(F("Received messagetype: JOIN2"));
					if(ducoDeviceState == ducoDeviceState_join1){
						sendJoin3Packet();
					}else{
						setLogMessage(F("Ignoring join 2 message because gateway ducoDeviceState isn't JOIN1."));
					}
					break;
				}
				default:{
					setLogMessage(F("Ignore message, not our networkID."));
					break;
				}
			}//switch

		}
		
		return true;
	}	// end if(receiveData(&inMessage)){
	
	return false;
}


uint8_t DucoCC1101::updateMessageCounter(){
	if(this->messageCounter == 15){
		this->messageCounter = 1;
	}else{
		this->messageCounter++;
	}
	return this->messageCounter;
}


uint8_t DucoCC1101::getRssi(){
	if (inDucoPacket.rssi >= 128){
        //rssi_dec = ( rssi_byte - 256) / 2) - 74;
		return (inDucoPacket.rssi - 128);
	}else{
        //rssi_dec = (rssi_byte / 2) - 74;
		return (inDucoPacket.rssi + 128);
	}
}

int DucoCC1101::convertRssiHexToDBm(){
	int rssi_dec = 0;
	if (inDucoPacket.rssi >= 128){
      rssi_dec = (( inDucoPacket.rssi - 256) / 2) - 74;
	}else{
      rssi_dec = (inDucoPacket.rssi / 2) - 74;
	}
	return rssi_dec;

}

void DucoCC1101::processNetworkPacket(){
	/* first data byte of a network package:
		- 0x00 = installermode off
		- 0x01 = installermode on
		- 0x06 = ?????
	*/
	switch(inDucoPacket.data[1]){
		case 0x00: 
			installerModeActive = false;
			// no log message here because we often receive a networkpacket
			break;
		case 0x01:
			installerModeActive = true;
			setLogMessage(F("Installermode activated!"));
		break;
		default:
			/* 
			If databyte is higer than 1, it is a broadcast for a specific the node number!
			Duco packettype = LINK (coreloglevel debug)
			*/
			setLogMessage(F("Network message -> call to specific node"));
		break;
	}


	// after receiving a networkpacket, each node repeats the network packet with RSSI value
	uint8_t ducoRssi;
	ducoRssi = getRssi();
	
	outDucoPacket.commandLength = 0;
	
	// copy data from incoming network package 
	outDucoPacket.data[0] = inDucoPacket.data[1];
	outDucoPacket.dataLength = 1;
 	outDucoPacket.messageType = ducomsg_network;

 	prefillDucoPacket(&outDucoPacket, 0x00, inDucoPacket.originalSourceAddress, inDucoPacket.originalDestinationAddress); 

	outDucoPacket.counter = inDucoPacket.counter;
	ducoToCC1101Packet(&outDucoPacket, &outMessage);

	// packet 9 = footer, needs to be rssi value.
	outMessage.data[9] = ducoRssi;

	sendDataToDuco(&outMessage);
	setLogMessage(F("Send processNetworkPacket done!"));
}




void DucoCC1101::sendDisjoinPacket(){
	setLogMessage(F("sendDisjoinPacket()"));

	ducoDeviceState = ducoDeviceState_disjoinRequest;
	outDucoPacket.messageType = ducomsg_message;

	outDucoPacket.command[0] = 0x10;
	outDucoPacket.command[1] = 0x00;
	outDucoPacket.command[2] = 0x3B;

	outDucoPacket.commandLength = 3;
	outDucoPacket.dataLength = 0;
 
  	prefillDucoPacket(&outDucoPacket, 0x01); // to ducobox
	outDucoPacket.counter = updateMessageCounter();
	ducoToCC1101Packet(&outDucoPacket, &outMessage);

	sendDataToDuco(&outMessage);
	setLogMessage(F("SEND disjoin packet done!"));
	ducoDeviceState = ducoDeviceState_disjoinWaitingForAck;
}

void DucoCC1101::finishDisjoin(){
	setLogMessage(F("FinishDisjoin()"));
	if(matchingNetworkId(inDucoPacket.networkId)){
		if(matchingDeviceAddress(inDucoPacket.destinationAddress)){
			sendAck(); 						// then send ack
			setLogMessage(F("Device disjoining finished!"));
			// remove networkID and deviceID
			this->networkId[0] = 0x00;
			this->networkId[1] = 0x00;
			this->networkId[2] = 0x00;
			this->networkId[3] = 0x00;

			this->deviceAddress = 0x00;
			ducoDeviceState = ducoDeviceState_disjointed;

		}
	}
}

void DucoCC1101::sendJoinPacket(){
	setLogMessage(F("SendJoinPacket()"));

	// remove networkID and deviceID, to receive the join messages me need to be in network 0000000 and address 0
	this->networkId[0] = 0x00;
	this->networkId[1] = 0x00;
	this->networkId[2] = 0x00;
	this->networkId[3] = 0x00;

	this->deviceAddress = 0x00;

	outDucoPacket.messageType = ducomsg_join1;	
	outDucoPacket.sourceAddress =  0x01;
	outDucoPacket.destinationAddress = 0x00;
	outDucoPacket.originalSourceAddress =  0x01;
	outDucoPacket.originalDestinationAddress = 0x00;
	outDucoPacket.counter = messageCounter;

	//co2 = 00 00 7C 3E -- Batt Remote = 00 00 00 00
	for(int i=0; i<4;i++)
	outDucoPacket.networkId[i] = joinCO2NetworkId[i];

	outDucoPacket.command[0] = 0x0c;
	outDucoPacket.commandLength = 1;
	outDucoPacket.dataLength = 0;

	ducoToCC1101Packet(&outDucoPacket, &outMessage);

	sendDataToDuco(&outMessage);
	//finishTransfer();
	setLogMessage(F("Joinpacket sent. DucoDeviceState = ducoDeviceState_join1"));
	ducoDeviceState = ducoDeviceState_join1;
}



// TODO: split this function in receiveJoin2Packet and sendJoin3Packet
void DucoCC1101::processJoin2Packet(){
	setLogMessage(F("processJoin2Packet()"));

	// check if joinCO2NetworkId is in data
	if(joinPacketValidNetworkId()){
		setLogMessage(F("SendJoin3Packet: valid join2 packet received!"));
		sendJoin3Packet();
	}else{
		// cant find joinCO2NetworkId in data
		setLogMessage(F("SendJoin3Packet: INVALID join2 packet received!"));
	}
}


void DucoCC1101::sendJoin3Packet(){
	outDucoPacket.sourceAddress =  0x00;
	outDucoPacket.destinationAddress = 0x01;
	outDucoPacket.originalSourceAddress =  0x00;
	outDucoPacket.originalDestinationAddress = 0x01;

	// update networkid
	memcpy(this->networkId, &inDucoPacket.networkId,4);

	// send response
	outDucoPacket.messageType = ducomsg_join3;
		
	memcpy(outDucoPacket.networkId,networkId,4 );

	//co2 = 00 00 7C 3E -- Batt Remote = 00 00 00 00
	for(int i=0; i<4;i++)
	outDucoPacket.command[i] = joinCO2NetworkId[i];

	outDucoPacket.commandLength = 4;
	outDucoPacket.data[0] = 0x0C;
	outDucoPacket.dataLength = 1;

	outDucoPacket.counter = 0x0D; // counter is always 13 for join3 message! (for battery remote!!!)
	ducoToCC1101Packet(&outDucoPacket, &outMessage);

	sendDataToDuco(&outMessage);
	setLogMessage(F("sendJoin3Packet: join3 message sent. DucoDeviceState = ducoDeviceState_join3"));
	ducoDeviceState = ducoDeviceState_join3;
}


bool DucoCC1101::joinPacketValidNetworkId(){
	setLogMessage(F("joinPacketValidNetworkId()"));
		if((inDucoPacket.sourceAddress == 0x01) && (inDucoPacket.destinationAddress == 0x00)){
			// check if network id is in command
			for(int i=0; i<4;i++){
				if(inDucoPacket.data[1+i] == joinCO2NetworkId[i]){
					if(i==3){ 
						return true;
					}
				}else{
					return false;
				}
			}
		}
	return false;
}



void DucoCC1101::processJoin4Packet(){
	// A join4 message is send to the networkID of the Ducobox (networkID is set by a join2 message). Check for matching network ID
	if(matchingNetworkId(inDucoPacket.networkId)){
		if(joinPacketValidNetworkId()){
			sendJoin4FinishPacket();
		}else{
			setLogMessage(F("processJoin4Packet: invalid join4 packet received, can't find joinCO2NetworkId in data."));
		}
	}else{
			setLogMessage(F("processJoin4Packet: invalid join4 packet received, not our network ID."));
	}
}


// data of a join4 message: 00 00 7C 3E XX YY
// XX = network address
// YY = node number
void DucoCC1101::sendJoin4FinishPacket(){
	ducoDeviceState = ducoDeviceState_join4;

	setLogMessage(F("sendJoinFinish: valid join4 packet received!"));
	this->deviceAddress = inDucoPacket.data[5]; // = new address	
	//this->nodeNumber = inDucoPacket.data[6]; // = nodenumber -> do we need to save this somewhere?

	char logBuf[50];
	snprintf(logBuf, sizeof(logBuf), "sendJoinFinish: new device address is: %u;",this->deviceAddress);
	setLogMessage(logBuf);

	// send ack! from new deviceaddress to address of sender.
	sendAck();
	setLogMessage(F("sendJoinFinish: ACK sent!"));
	ducoDeviceState = ducoDeviceState_joinSuccessful;

}




void DucoCC1101::waitForAck(){
	setLogMessage(F("Start waiting for ack..."));
	this->ackTimer = millis();
	this->ackRetry = 0;
	this->waitingForAck = 1;
}

// check if we are waiting for an ACK, 
void DucoCC1101::checkForAck(){
	if(this->waitingForAck){

		unsigned long mill = millis();
 		if (mill - this->ackTimer >= 500){ // wait for 300 ms (standard duco),
		 	if(this->ackRetry <= this->sendTries){
				setLogMessage(F("CheckforAck: still waiting for ACK. Sending message again..."));
				//resend message
				sendDataToDuco(&outMessage);
				//setLogMessage("CheckforAck: message resent");

				this->ackTimer = millis();
				this->ackRetry++;
			 }else{
				 this->waitingForAck = 0;
				setLogMessage(F("CheckforAck: no ack received, cancel retrying."));

			 }
		 }
	}
}

bool DucoCC1101::matchingNetworkId(uint8_t id[4]) 
{
	for (uint8_t i=0; i<=3;i++){
		if (id[i] != this->networkId[i]){
			return false;
		}
	}
	return true;
}


bool DucoCC1101::matchingDeviceAddress(uint8_t compDeviceAddress){
	if(this->deviceAddress == compDeviceAddress){
		return true;
	}else{
		return false;
	}
}

 


bool DucoCC1101::checkForNewPacketInRXFifo(){
	uint8_t rxBytes = readRegisterWithSyncProblem(CC1101_RXBYTES, CC1101_STATUS_REGISTER);
	
	char LogBuf[20];
	snprintf(LogBuf, sizeof(LogBuf), "STATUS: %02X",rxBytes);
	setLogMessage(LogBuf);

	rxBytes = rxBytes & CC1101_BITS_RX_BYTES_IN_FIFO;

	if(rxBytes > 0){
		return true;
	}else{
		return false;
	}
}


uint8_t DucoCC1101::getMarcState(bool noLogMessage){
	uint8_t result = readRegisterWithSyncProblem(CC1101_MARCSTATE, CC1101_STATUS_REGISTER);

if(!noLogMessage){
	char LogBuf[20];
	snprintf(LogBuf, sizeof(LogBuf), "MARCST: %02X",result);
	setLogMessage(LogBuf);
}
return result;
}



uint8_t DucoCC1101::checkForBytesInRXFifo(){
	uint8_t rxBytes = readRegisterWithSyncProblem(CC1101_RXBYTES, CC1101_STATUS_REGISTER);
	rxBytes = rxBytes & CC1101_BITS_RX_BYTES_IN_FIFO;

return rxBytes;

}




void DucoCC1101::parseMessageCommand()
{
	// first byte of command
	switch(inDucoPacket.data[1] ){
		// Ping vanuit ducobox naar node. Het antwoord van de node is de byte uit de request plus 1 (decimaal).	
		//0x10 0x00 
		case 0x10:
			if(inDucoPacket.data[2] == 0x00){
				setLogMessage(F("Received ping command"));
				if(inDucoPacket.data[3] == 0x38){
					// do nothing
				}else{
					sendPing();
					waitForAck();
				}
			}
		break;

		//Wijziging huidige ventilatiestand (door ducobox)	
		// 0x20 0x00 0x12
		case 0x20:
		if( (inDucoPacket.data[2] == 0x00) && (inDucoPacket.data[3] == 0x12)){
			setLogMessage(F("Received change ventilation command"));

			if((inDucoPacket.data[4] < 30)){ //CHECK IF ventilationmode has a valid value
				currentVentilationMode = inDucoPacket.data[4];

				//  Bevestiging van wijziging  ventilatiestand (door node)	0x40 0x00 0x22
				sendConfirmationNewVentilationMode();
				waitForAck();
			}
		}
		break;

		//Verzoek van node om ventilatiestand te wijzigen (bijv. door druk op knop of CO2 te hoog).	
		//0x30 0x00 0x22 0x00 !!!niet van toepassing omdat deze enkel vanuit de node wordt verstuurd!!!
		//Opvragen parameter van node	
		//0x30 0x00 0x40 0x00
		case 0x30:
			if((inDucoPacket.data[2] == 0x00) && (inDucoPacket.data[3] == 0x40) && (inDucoPacket.data[4] == 0x00)){
					setLogMessage(F("Received request for parameter value command"));
					sendNodeParameterValue();
					waitForAck();
			}
		break;
		
		//Opvragen meerdere parameters van node (maximaal 3)
		//0x33 0x33 for 4 parameters OR 0x33 0x00 for 2 parameters
		case 0x33:
			if( (inDucoPacket.data[2] == 0x33) || (inDucoPacket.data[2] == 0x00)){
				setLogMessage(F("Received request for multiple parameters value command"));
				sendMultipleNodeParameterValue();
				waitForAck();
			}
		break;
		case DEFAULT:{
			setLogMessage(F("Unknown command received"));
			break;
		}
	}
}


	// destinationAddress is our address but originalDestinationAddress is an other device
	// as repeater we need to repeat the message (send to inDucoPacket.originalDestinationAddress)
void DucoCC1101::repeatMessage(){

	outDucoPacket.commandLength = 0;

	// copy messageType and counter
	outDucoPacket.messageType = inDucoPacket.messageType;
	outDucoPacket.counter = inDucoPacket.counter;

	// copy data from incoming packet to outgoing packet
	memcpy(&outDucoPacket.data, &inMessage.data, inDucoPacket.dataLength);
	outDucoPacket.dataLength = inDucoPacket.dataLength;

  	prefillDucoPacket(&outDucoPacket, inDucoPacket.originalDestinationAddress, inDucoPacket.originalSourceAddress, inDucoPacket.originalDestinationAddress); 
	ducoToCC1101Packet(&outDucoPacket, &outMessage);

	// packet 9 = RSSI value for networkpackages and repeater messaages
	// each node repeats the packet with RSSI value in byte 
	uint8_t ducoRssi;
	ducoRssi = getRssi();
	outMessage.data[9] = ducoRssi;

	sendDataToDuco(&outMessage);
	setLogMessage(F("SEND repeatMessage() done!"));
}





void DucoCC1101::sendConfirmationNewVentilationMode(){
//40	0	22 +	ventilationmode +	0c	79
	uint8_t parameter = inDucoPacket.data[4];

	outDucoPacket.command[0] = 0x40;
	outDucoPacket.command[1] = 0x00;
	outDucoPacket.command[2] = 0x22;
	outDucoPacket.command[3] = 0x00;
	
	outDucoPacket.commandLength = 4;
	outDucoPacket.data[0] = parameter;
	outDucoPacket.data[1] = 0x6d;
	//outDucoPacket.data[2] = 0x79;
	outDucoPacket.dataLength = 2;
	
	outDucoPacket.messageType = ducomsg_message;

  	prefillDucoPacket(&outDucoPacket, inDucoPacket.sourceAddress, inDucoPacket.originalDestinationAddress, inDucoPacket.originalSourceAddress); 
	outDucoPacket.counter = updateMessageCounter();
	ducoToCC1101Packet(&outDucoPacket, &outMessage);

	sendDataToDuco(&outMessage);
	setLogMessage(F("SEND sendConfirmationNewVentilationMode done!"));
}




void DucoCC1101::sendNodeParameterValue(){
	uint8_t parameter = inDucoPacket.data[5];

	outDucoPacket.command[0] = 0x50;
	outDucoPacket.command[1] = 0x00;
	outDucoPacket.command[2] = 0x41;
	outDucoPacket.command[3] = 0x00;
	outDucoPacket.commandLength = 4;

	switch(parameter){

		case 0x01:{
			outDucoPacket.data[0] = parameter;
			outDucoPacket.data[1] = 0x00;
			outDucoPacket.data[2] = 0x01;
			outDucoPacket.dataLength = 3;
		break;

		}
		case 0x05:{ // parameter 5 = ?????, 2-byte response always 0x00 0x06
			outDucoPacket.data[0] = parameter;
			outDucoPacket.data[1] = 0x00;
			outDucoPacket.data[2] = 0x06;
			outDucoPacket.dataLength = 3;
		break;
		}
		//parameter 137 = CO2 Setpoint (ppm) (2-byte response)
		case 0x89:{ 
			int CO2Setpoint = 850;
			outDucoPacket.data[0] = parameter;
			outDucoPacket.data[1] = ((CO2Setpoint >> 8) & 0xff);
			outDucoPacket.data[2] =  (CO2Setpoint & 0xff);
			outDucoPacket.dataLength = 3;
		break;
		}

		//case 0x16:{ // parameter 22 = ?????, one byte response always 0x02
		//	outDucoPacket.data[0] = parameter;
		//	outDucoPacket.data[1] = 0x02;
		//	outDucoPacket.dataLength = 2;		
		//break;
		//}

		case 0x49:{ // parameter 73 = temp, 2-byte response 210 = 21.0C
			int currentTemp = 210;
			outDucoPacket.data[0] = parameter;
			outDucoPacket.data[1] = ((currentTemp >> 8) & 0xff);
			outDucoPacket.data[2] =  (currentTemp & 0xff);
			outDucoPacket.dataLength = 3;
		break;
		}

		case 0x4A:{ // parameter 74 = CO2 value (ppm), 2-byte response 500 = 500 ppm
			int currentCO2 = 500;
			outDucoPacket.data[0] = parameter;
			outDucoPacket.data[1] = ((currentCO2 >> 8) & 0xff);
			outDucoPacket.data[2] =  (currentCO2 & 0xff);
			outDucoPacket.dataLength = 3;
		break;
		}
		case 0x80:{  // parameter 128 = ?????, 2-byte response always 0x00 0x00
			outDucoPacket.data[0] = parameter;
			outDucoPacket.data[1] = 0x00;
			outDucoPacket.data[2] = 0x00;
			outDucoPacket.dataLength = 3;
		break;
		}
		default:{
		//parameter does not exist response: 0x40	0x00	0x42	0x00 + parameterbyte + 0x02
			setLogMessage(F("sendNodeParameterValue(); Requested parameter does not exist!"));

			outDucoPacket.command[0] = 0x40;
			outDucoPacket.command[1] = 0x00;
			outDucoPacket.command[2] = 0x42;
			outDucoPacket.command[3] = 0x00;
			outDucoPacket.commandLength = 4;

			//outDucoPacket.data[0] = 0x00;
			outDucoPacket.data[0] = parameter;
			outDucoPacket.data[1] = 0x02;
			outDucoPacket.dataLength = 2;
		break;
		}
	}

	outDucoPacket.messageType = ducomsg_message;

  	prefillDucoPacket(&outDucoPacket, inDucoPacket.sourceAddress, inDucoPacket.originalDestinationAddress, inDucoPacket.originalSourceAddress); 
	outDucoPacket.counter = updateMessageCounter();
	ducoToCC1101Packet(&outDucoPacket, &outMessage);

	sendDataToDuco(&outMessage);

	char logBuf[30];
	snprintf(logBuf, sizeof(logBuf), "SEND parameter %u done", parameter);
	setLogMessage(logBuf);
}




void DucoCC1101::sendMultipleNodeParameterValue(){
	//setLogMessage("sendMultipleNodeParameterValue()");

	//commando opvragen apparaatinfo (meerdere adresen per uitleesactie)					33	33	40	registeraddress byte1	registeraddress byte2	40	registeraddress byte1	registeraddress byte2	40	registeraddress byte1	registeraddress byte2	40	registeraddress byte1	registeraddress byte2								
	//antwoord					55	55	41	registeraddress byte1	registeraddress byte2	waarde byte 1	waarde byte 2	41	registeraddress byte1	registeraddress byte2	waarde byte 1	waarde byte 2	41	registeraddress byte1	registeraddress byte2	waarde byte 1	waarde byte 2	41	0	3	0	0
	outDucoPacket.messageType = ducomsg_message;

	//uint8_t requestedParameters[3] = {};
	//uint8_t counterParameters = 0;

	outDucoPacket.command[0] = 0x55;

	if(inDucoPacket.data[2]== 0x33){
		outDucoPacket.command[1] = 0x55;
	}else{
		outDucoPacket.command[1] = 0x00;
	}
		outDucoPacket.commandLength = 2;
		outDucoPacket.dataLength = 0;

	for(int i=0; i<4;i++){
		if( (3+(i*3)) < (inDucoPacket.dataLength-2)){ // -2 bytes for rssi, CRC ok bytes

			if(inDucoPacket.data[(3+(i*3))] == 0x40){
				outDucoPacket.data[outDucoPacket.dataLength] = 0x41;
				outDucoPacket.data[outDucoPacket.dataLength+1] = 0x00;
				outDucoPacket.data[outDucoPacket.dataLength+2] = inDucoPacket.data[(5+(i*3))];

				uint8_t parameterValue[2];
				uint8_t parameter = inDucoPacket.data[(5+(i*3))];
				getParameterValue(parameter,&parameterValue[0]);
				outDucoPacket.data[outDucoPacket.dataLength+3] = parameterValue[0];
				outDucoPacket.data[outDucoPacket.dataLength+4] = parameterValue[1];

				outDucoPacket.dataLength = outDucoPacket.dataLength+5;
			}
		}
	}

  	prefillDucoPacket(&outDucoPacket, inDucoPacket.sourceAddress, inDucoPacket.originalDestinationAddress, inDucoPacket.originalSourceAddress); 
	outDucoPacket.counter = updateMessageCounter();
	ducoToCC1101Packet(&outDucoPacket, &outMessage);

	sendDataToDuco(&outMessage);
	setLogMessage(F("Send multiple parameter done!"));
}


void DucoCC1101::getParameterValue(uint8_t parameter, uint8_t *value){
	//device serial: RS1521002390 (ascii -> hex)
	const uint8_t deviceSerial[12] = {0x52, 0x53, 0x31, 0x35, 0x32, 0x31, 0x30, 0x30, 0x32, 0x33, 0x39, 0x31};

	switch(parameter){
		// 0x00 = product id, value=12030 (0x2E 0xFE)
		case 0x00:{ value[0]= 0x2E; value[1]= 0xFE; break; }
		// 0x01 = softwareversion >X<.x.x, value= 1 (0x00 0x01)
		case 0x01:{ value[0]= 0x00; value[1]= 0x01; break; }
		// 0x02 = softwareversion x.>X<.x, value= 2 (0x00 0x02)
		case 0x02:{ value[0]= 0x00; value[1]= 0x02; break; }
		// 0x03 = softwareversion x.x.>X<, value= 0 (0x00 0x00)
		case 0x03:{ value[0]= 0x00; value[1]= 0x00; break; }

		// deviceserial
		case 0x08:{ value[0]= deviceSerial[1]; value[1]= deviceSerial[0]; break; }
		case 0x09:{ value[0]= deviceSerial[3]; value[1]= deviceSerial[2]; break; }
		case 0x0A:{ value[0]= deviceSerial[5]; value[1]= deviceSerial[4]; break; }
		case 0x0B:{ value[0]= deviceSerial[7]; value[1]= deviceSerial[6]; break; }
		case 0x0C:{ value[0]= deviceSerial[9]; value[1]= deviceSerial[8]; break; }
		case 0x0D:{ value[0]= deviceSerial[11]; value[1]= deviceSerial[10]; break; }
		default: {value[0]= 0x00; value[1]= 0x00; break; }
	}
}

void DucoCC1101::sendPing(){
	
	outDucoPacket.messageType = ducomsg_message;

	outDucoPacket.command[0] = 0x10;
	outDucoPacket.command[1] = 0x00;
	outDucoPacket.commandLength = 2;
	outDucoPacket.data[0] = (inDucoPacket.data[3] +1);
	outDucoPacket.dataLength = 1;

  	prefillDucoPacket(&outDucoPacket, inDucoPacket.sourceAddress, inDucoPacket.originalDestinationAddress, inDucoPacket.originalSourceAddress); 
	outDucoPacket.counter = inDucoPacket.counter;
	ducoToCC1101Packet(&outDucoPacket, &outMessage);

	sendDataToDuco(&outMessage);
	setLogMessage(F("SEND PING done!"));
}




void DucoCC1101::sendAck(){
	//setLogMessage("Send Ack...");

	outDucoPacket.messageType = ducomsg_ack;
	outDucoPacket.commandLength = 0;

	outDucoPacket.dataLength = 0;

	prefillDucoPacket(&outDucoPacket, inDucoPacket.sourceAddress, inDucoPacket.originalDestinationAddress, inDucoPacket.originalSourceAddress); // address duco
	outDucoPacket.counter = inDucoPacket.counter;
	ducoToCC1101Packet(&outDucoPacket, &outMessage);

	sendDataToDuco(&outMessage);
	setLogMessage(F("SEND ACK done!"));
}

void DucoCC1101::prefillDucoPacket(DucoPacket *ducoOutPacket, uint8_t destinationAddress){
	prefillDucoPacket(ducoOutPacket, destinationAddress, this->deviceAddress, destinationAddress);
}


void DucoCC1101::prefillDucoPacket(DucoPacket *ducoOutPacket, uint8_t destinationAddress, uint8_t originalSourceAddress, uint8_t originalDestinationAddress ){
	ducoOutPacket->sourceAddress =  this->deviceAddress;
	ducoOutPacket->destinationAddress = destinationAddress;
	ducoOutPacket->originalSourceAddress =  originalSourceAddress;
	ducoOutPacket->originalDestinationAddress = originalDestinationAddress;
	memcpy(ducoOutPacket->networkId,networkId,4 );
}


void DucoCC1101::ducoToCC1101Packet(DucoPacket *duco, CC1101Packet *packet)
{
	packet->data[0] = 0;
	packet->data[1] = duco->messageType;
	packet->data[2] = duco->networkId[0];	
	packet->data[3] = duco->networkId[1];	
	packet->data[4] = duco->networkId[2];	
	packet->data[5] = duco->networkId[3];	

	//address
	packet->data[6] = (duco->sourceAddress << 3) | (duco->destinationAddress >> 2);
	packet->data[7] = (duco->destinationAddress << 6) | (duco->sourceAddress << 1) | (duco->originalDestinationAddress >> 4);
	packet->data[8] = (duco->originalDestinationAddress << 4) | duco->counter;

	// footer
	packet->data[9] = 0xFF; 

	for(uint8_t i=0; i < duco->commandLength; i++){
		packet->data[10+i] = duco->command[i];
	}

	for(uint8_t i=0; i < duco->dataLength; i++){
		packet->data[10+duco->commandLength+i] = duco->data[i];
	}

	packet->length = 10 + duco->commandLength + duco->dataLength;




}



void DucoCC1101::requestVentilationMode(uint8_t ventilationMode, uint8_t percentage, uint8_t temp){
	requestVentilationMode(ventilationMode, percentage, temp, false);
}

void DucoCC1101::requestVentilationMode(uint8_t ventilationMode, uint8_t percentage, uint8_t temp, bool activateInstallerMode){
	//setLogMessage("requestVentilationMode()");

	uint8_t installerModeBit = activateInstallerMode ? 0x10 : 0x00; // 0x10 = 0001.0000

	outDucoPacket.messageType = ducomsg_message;

	outDucoPacket.command[0] = 0x40;
	outDucoPacket.command[1] = 0x00;
	outDucoPacket.command[2] = 0x22;
	outDucoPacket.commandLength = 3;
	outDucoPacket.data[0] = percentage;
	outDucoPacket.data[1] = (installerModeBit | 0x08 | ventilationMode); // 0x08 = 0000.1000
	outDucoPacket.data[2] = (temp -100); // temp = 21.0c = 21 - 10 = 11 (dec) = 0x0B
	outDucoPacket.dataLength = 3;
 
 //prefillDucoPacket(DucoPacket *ducoOutPacket, uint8_t destinationAddress, uint8_t originalSourceAddress, uint8_t originalDestinationAddress 
	prefillDucoPacket(&outDucoPacket, 0x01); // address duco

	outDucoPacket.counter = updateMessageCounter();
	ducoToCC1101Packet(&outDucoPacket, &outMessage);

	sendDataToDuco(&outMessage);

	char logBuf[120];
	snprintf(logBuf, sizeof(logBuf), "Send requestventilationmode done. mode: %u, override percentage: %u%%, temp: %u C, installermode: %u", ventilationMode, percentage, temp, activateInstallerMode);
	setLogMessage(logBuf);

	waitForAck();

	// change current ventilation mode => DONT, just wait for confirmation message!
	//currentVentilationMode = ventilationMode; 
return;
}





void DucoCC1101::disableInstallerMode(){
	//setLogMessage("disableInstallerMode()");
	outDucoPacket.messageType = ducomsg_network;

	outDucoPacket.commandLength = 0;
	outDucoPacket.data[0] = 0x00; // 0x00 = disable installer mode, 0x01 = installermode is enabled
	outDucoPacket.dataLength = 1;
 
	prefillDucoPacket(&outDucoPacket, 0x01); // address duco

	// increase counter!
	// function like getCounter()???
	outDucoPacket.counter = updateMessageCounter();
	ducoToCC1101Packet(&outDucoPacket, &outMessage);

	sendDataToDuco(&outMessage);
	setLogMessage(F("SEND disableInstallerMode done!"));

	waitForAck();

}



/* TEST FUNCTIONS! */

uint8_t DucoCC1101::TEST_getVersion(){
	uint8_t version = readRegister(0xF1);
	return version;
}

uint8_t DucoCC1101::TEST_getPartnumber(){
	uint8_t version = readRegister(0xF0);
	return version;
}

void DucoCC1101::TEST_GDOTest(){
	writeRegister(CC1101_IOCFG0 ,0x2E);	//High impedance (3-state)
	writeRegister(CC1101_IOCFG1 ,0x2E); //High impedance (3-state)
	writeRegister(CC1101_IOCFG2 ,0x3F);	// 135-141 kHz clock output (XOSC frequency divided by 192).
}



void DucoCC1101::sendTestMessage(){

	outDucoPacket.messageType = ducomsg_message;
	outDucoPacket.commandLength = 0;

	outDucoPacket.data[0] = 0x47; // G
	outDucoPacket.data[1] = 0x57; // W
	outDucoPacket.data[2] = 0x54; // T
	outDucoPacket.data[3] = 0x45; // E
	outDucoPacket.data[4] = 0x53; // S
	outDucoPacket.data[5] = 0x54; // T

	outDucoPacket.dataLength = 6;
 
	prefillDucoPacket(&outDucoPacket, 0x01); // address duco

	outDucoPacket.counter = 0x08;
	ducoToCC1101Packet(&outDucoPacket, &outMessage);

	sendDataToDuco(&outMessage);
	return;
}


void DucoCC1101::sendRawPacket(uint8_t messageType, uint8_t sourceAddress, uint8_t destinationAddress, uint8_t originalSourceAddress, uint8_t originalDestinationAddress, uint8_t *data, uint8_t length){
	
	outDucoPacket.sourceAddress =  sourceAddress;
	outDucoPacket.destinationAddress = destinationAddress;
	outDucoPacket.originalSourceAddress =  originalSourceAddress;
	outDucoPacket.originalDestinationAddress = originalDestinationAddress;
	outDucoPacket.messageType = messageType;
	outDucoPacket.commandLength = 0;

	for(uint8_t i=0; i < length;i++){
		outDucoPacket.data[i] = data[i];
	}

	outDucoPacket.dataLength = length;

	memcpy(outDucoPacket.networkId,this->networkId,4);
	outDucoPacket.counter = inDucoPacket.counter;

	ducoToCC1101Packet(&outDucoPacket, &outMessage);

	sendDataToDuco(&outMessage);
	setLogMessage(F("Send raw Duco packet done!"));
}
