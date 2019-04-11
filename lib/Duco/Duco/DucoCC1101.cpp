/*
 * Author: Arne Mauer
 * Parts of this code is based on the -library made by 'supersjimmie', 'Thinkpad', 'Klusjesman' and 'jodur'.
 */

#include "DucoCC1101.h"
#include <string.h>
#include <Arduino.h>
#include <SPI.h>

// default constructor
DucoCC1101::DucoCC1101(uint8_t counter, uint8_t sendTries) : CC1101()
{
	this->outDucoPacket.counter = counter;
	this->sendTries = sendTries;
	this->deviceAddress = 0;
	this->waitingForAck = 0; 
	this->ackTimer = 0;
	this->ackRetry = 0;

	this->testCounter = 0; // temp 
	
	this->messageCounter = 1; // for messages out NEVER ZERO!
	this->ducoboxLastRssiByte = 0;

	this->numberOfLogmessages = 0;

} //DucoCC1101

// default destructor
DucoCC1101::~DucoCC1101()
{
} //~DucoCC1101


String DucoCC1101::getLogMessage(uint8_t messageNumber){
	return logMessages[messageNumber];
}

void DucoCC1101::setLogMessage(String newLogMessage){
	if(this->numberOfLogmessages == 9){
		this->numberOfLogmessages = 0;
	}else{
		this->numberOfLogmessages++;
	}

	logMessages[this->numberOfLogmessages] = newLogMessage;
}

uint8_t DucoCC1101::getNumberOfLogMessages(){
	uint8_t tempNumber = this->numberOfLogmessages;
	this->numberOfLogmessages = 0;
	return tempNumber;

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
	writeCommand(CC1101_SRES);

	writeRegister(CC1101_IOCFG0 ,0x2E);	//!!! 	//High impedance (3-state)
	writeRegister(CC1101_IOCFG1 ,0x2E);	//!!    //High impedance (3-state)
	writeRegister(CC1101_IOCFG2 ,0x07);	//!!! duco 0x07 /  0x00		//Assert when RX FIFO is filled or above the RX FIFO threshold. Deassert when (0x00): RX FIFO is drained below threshold, or (0x01): deassert when RX FIFO is empty.
	// ivm omruilen gpio's anders dan duco 
	// gpio0 = niet gebruiken, gpio2 = bij geldig pakket

	writeRegister(CC1101_FIFOTHR, 0x07); // default value. FIFO treshold TX=33;RX=32
	writeRegister(CC1101_SYNC1 ,0xD3);	//duco 0xD3 /   		//message2 byte6
	writeRegister(CC1101_SYNC0 ,0x91);	//duco 0x91	/  	//message2 byte7
	writeRegister(CC1101_PKTLEN ,0x20); // duco 0x20 / 
	writeRegister(CC1101_PKTCTRL1 ,0x0C); //duco 0x0C /  	//No address check, Append two bytes with status RSSI/LQI/CRC OK, 
	writeRegister(CC1101_PKTCTRL0 ,0x05); //duco 0x05 /  		//Infinite packet length mode, CRC disabled for TX and RX, No data whitening, Asynchronous serial mode, Data in on GDO0 and data out on either of the GDOx pins 
	
	writeRegister(CC1101_ADDR ,0x00); // duco/
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

	
	//0x6F,0x26,0x2E,0x7F,0x8A,0x84,0xCA,0xC4
	// DUCO: 0xC5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	writeBurstRegister(CC1101_PATABLE | CC1101_WRITE_BURST, (uint8_t*)ducoPaTableReceive, 8);
	
	writeCommand(CC1101_SIDLE);
	writeCommand(CC1101_SIDLE);

	writeCommand(CC1101_SCAL);

	//wait for calibration to finish
	while ((readRegisterWithSyncProblem(CC1101_MARCSTATE, CC1101_STATUS_REGISTER)) != CC1101_MARCSTATE_IDLE) yield();
	
	writeCommand(CC1101_SRX);
	
	// wait till in rx mode (0x1F)
	while ((readRegisterWithSyncProblem(CC1101_MARCSTATE, CC1101_STATUS_REGISTER)) != CC1101_MARCSTATE_RX) yield();
	
	if(this->deviceAddress != 0x00){
		this->ducoDeviceState = ducoDeviceState_initialised;
	}
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



bool DucoCC1101::checkForNewPacket()
{
	if (receiveData(&inMessage)){
		inDucoPacket.messageType = inMessage.data[0];

		// Fill Duco packet with data
		memcpy(inDucoPacket.networkId, &inMessage.data[1], sizeof inDucoPacket.networkId);
		inDucoPacket.deviceIdSender = (inMessage.data[5] >> 3); // first 5 bits 
		inDucoPacket.deviceIdReceiver = ( (inMessage.data[5] & 0b00000111) << 2) | (inMessage.data[6] >> 6); // get last 3 bits and shift left.
		inDucoPacket.deviceIdSender2 = ( (inMessage.data[6] & 0b00111110) >> 1); // first 5 bits 
		inDucoPacket.deviceIdReceiver2 = ( (inMessage.data[6] & 0b00000001) << 4) | (inMessage.data[7] >> 4); // get last 3 bits and shift left.
		inDucoPacket.counter = (inMessage.data[7] & 0b00001111);

		memcpy(&inDucoPacket.data, &inMessage.data[8],(inMessage.length-8));
		inDucoPacket.dataLength = (inMessage.length-8);

		// CREATE LOG ENTRY OF RECEIVED PACKET
		String log = F("[P150] DUCO RF GW: Received message [SNDR:");
		log += String(inDucoPacket.deviceIdSender, DEC);
		log += F("; RECVR: ");
		log += String(inDucoPacket.deviceIdReceiver, DEC);
		log += F("; SNDR2:");
		log += String(inDucoPacket.deviceIdSender2, DEC);
		log += F("; RECVR2: ");
		log += String(inDucoPacket.deviceIdReceiver2, DEC);
		log += F("] NetworkID: [");
		log += String(inDucoPacket.networkId[0], HEX) + String(inDucoPacket.networkId[1], HEX) + String(inDucoPacket.networkId[2], HEX) + String(inDucoPacket.networkId[3], HEX);
		log += F("] Type: [");
		log += String(inDucoPacket.messageType, HEX);
		log += F("] Bytes:[");
		log += String(inMessage.length, DEC);
		log += F("] Counter:[");
		log += String(inDucoPacket.counter, HEX);
		log += F("] Message:[");

		//log received bytes 
		for (int i = 0; i < inDucoPacket.dataLength; i++) {
			log += byteToHexString(inDucoPacket.data[i]);
			log += F(", ");
		}
		log += F("]");

		setLogMessage(log);

		//save RSSI value if message received from ducobox
		if(inDucoPacket.deviceIdSender == 0x01){
			ducoboxLastRssiByte = inDucoPacket.data[inDucoPacket.dataLength-1];
		}


		switch(inDucoPacket.messageType){
			case ducomsg_network:{
			    setLogMessage(F("[P150] DUCO RF GW: Messagetype: network0"));
					if(matchingNetworkId(inDucoPacket.networkId)){
						if(inDucoPacket.deviceIdSender == 0x01){
							processNetworkPacket();
						}
					}
			break;
			}

			case ducomsg_join2:{
			  setLogMessage(F("[P150] DUCO RF GW: messagetype: JOIN2"));
				sendJoin3Packet();
				break;
			}

			case ducomsg_join4:{
			  setLogMessage(F("[P150] DUCO RF GW: messagetype: JOIN4"));
				// if we are waiting for disjoin confirmation, finish disjoin
				if(ducoDeviceState == ducoDeviceState_disjoinWaitingForConfirmation){
					finishDisjoin();

				// if we are waiting for join confirmation, finish joining
				}else if(ducoDeviceState == ducoDeviceState_join3){
					sendJoinFinish();	
				}else{
					// if ducobox didnt receive the first ack, check if address in packet is the same and send ack again!
					setLogMessage(F("[P150] DUCO RF GW: received join4 message but join already finished, check address and resend ACK."));
					if(inDucoPacket.data[5] == this->deviceAddress){
						sendAck();
						setLogMessage(F("[P150] DUCO RF GW: SendJoinFinish: another ACK sent!"));
					}else{
						setLogMessage(F("[P150] DUCO RF GW: No match between join4 address and our deviceid. ingoring message..."));
					}
			    setLogMessage(F("[P150] DUCO RF GW: Ignore JOIN4. No join or disjoin action running..."));
				}
				break;
			}

			case ducomsg_ack:{
				setLogMessage(F("[P150] DUCO RF GW: messagetype: ACK"));
				// stop retry sending message
				if(matchingNetworkId(inDucoPacket.networkId)){
					if(matchingDeviceAddress(inDucoPacket.deviceIdReceiver)){
						if(outDucoPacket.counter == inDucoPacket.counter){
							waitingForAck = 0;
							setLogMessage(F("[P150] DUCO RF GW: Ack received!"));
							if(ducoDeviceState == ducoDeviceState_disjoinWaitingForAck){
								ducoDeviceState = ducoDeviceState_disjoinWaitingForConfirmation;
							}
						}else{
							setLogMessage(F("[P150] DUCO RF GW: Ack received but counter doesnt match!"));
						}
					}
				}
				break;
			}

			case ducomsg_message:{
			    setLogMessage(F("[P150] DUCO RF GW: messagetype: Normal message"));
				// check if address is address of this device
					if(matchingNetworkId(inDucoPacket.networkId)){
						if(matchingDeviceAddress(inDucoPacket.deviceIdReceiver)){
							sendAck(); 						// then send ack
							parseMessageCommand();						// take action! :) 
						}
					}
				break;
			}

			case DEFAULT:{
				setLogMessage(F("[P150] DUCO RF GW: messagetype: unknown"));
				break;
			}
		}

		return true;
	}	
	
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



void DucoCC1101::processNetworkPacket(){
	uint8_t duco_rssi;
	if (ducoboxLastRssiByte >= 128){
        //rssi_dec = ( rssi_byte - 256) / 2) - 74;
		duco_rssi = (ducoboxLastRssiByte - 128);
	}else{
        //rssi_dec = (rssi_byte / 2) - 74;
		duco_rssi = (ducoboxLastRssiByte + 128);
	}
	// convert to duco RSSI byte
	//duco_rssi = (((rssi_dec+74) * 2 ) + 128);

	outDucoPacket.commandLength = 0;
	outDucoPacket.data[0] = inDucoPacket.data[1];
	outDucoPacket.dataLength = 1;
 
	prefillDucoPacket(&outDucoPacket, 0x00); // address duco
	outDucoPacket.counter = inDucoPacket.counter;
	ducoToCC1101Packet(&outDucoPacket, &outMessage);

	// packet 9 = footer, needs to be rssi value.
	outMessage.data[9] = duco_rssi;

	sendData(&outMessage);
	setLogMessage(F("[P150] DUCO RF GW: SEND processNetworkPacket done!"));
}




void DucoCC1101::sendDisjoinPacket(){
	setLogMessage(F("[P150] DUCO RF GW: sendDisjoinPacket()"));

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

	sendData(&outMessage);
	setLogMessage(F("[P150] DUCO RF GW: SEND disjoin packet done!"));
	ducoDeviceState = ducoDeviceState_disjoinWaitingForAck;
}

void DucoCC1101::finishDisjoin(){
	setLogMessage(F("[P150] DUCO RF GW: FinishDisjoin()"));
	if(matchingNetworkId(inDucoPacket.networkId)){
		if(matchingDeviceAddress(inDucoPacket.deviceIdReceiver)){
			sendAck(); 						// then send ack
			setLogMessage(F("[P150] DUCO RF GW: Device disjoining finished!"));
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
	setLogMessage(F("[P150] DUCO RF GW: sendJoinPacket()"));

	outDucoPacket.messageType = ducomsg_join1;	
	outDucoPacket.deviceIdSender =  0x01;
	outDucoPacket.deviceIdReceiver = 0x00;
	outDucoPacket.deviceIdSender2 =  0x01;
	outDucoPacket.deviceIdReceiver2 = 0x00;
	outDucoPacket.counter = messageCounter;

	//co2 = 00 00 7C 3E -- Batt Remote = 00 00 00 00
	for(int i=0; i<4;i++)
	outDucoPacket.networkId[i] = joinCO2NetworkId[i];

	outDucoPacket.command[0] = 0x0c;
	outDucoPacket.commandLength = 1;
	outDucoPacket.dataLength = 0;

	ducoToCC1101Packet(&outDucoPacket, &outMessage);

	sendData(&outMessage);
	//finishTransfer();
	setLogMessage(F("[P150] DUCO RF GW: Joinpacket sent"));
	ducoDeviceState = ducoDeviceState_join1;
}

void DucoCC1101::sendJoin3Packet(){
	setLogMessage(F("[P150] DUCO RF GW: FUNC: sendJoin3Packet"));

	bool validJoin2Packet = false;

	if((inDucoPacket.deviceIdSender == 0x01) && (inDucoPacket.deviceIdReceiver == 0x00)){

		// check if network id is in command
		for(int i=0; i<4;i++){
			if(inDucoPacket.data[1+i] == joinCO2NetworkId[i]){
				validJoin2Packet = true;
				ducoDeviceState = ducoDeviceState_join2;

			}else{
				validJoin2Packet = false;
				break;
			}
		}
	}

	if(validJoin2Packet){
		setLogMessage(F("[P150] DUCO RF GW: SendJoin3Packet: valid join2 packet received!"));

		outDucoPacket.deviceIdSender =  0x00;
		outDucoPacket.deviceIdReceiver = 0x01;
		outDucoPacket.deviceIdSender2 =  0x00;
		outDucoPacket.deviceIdReceiver2 = 0x01;

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

		outDucoPacket.counter = 0x0D; // counter is always 13 for join3 message!
		ducoToCC1101Packet(&outDucoPacket, &outMessage);

		sendData(&outMessage);
		setLogMessage(F("[P150] DUCO RF GW: sendJoinFinish: join3 message sent!"));
		ducoDeviceState = ducoDeviceState_join3;

	}else{
		setLogMessage(F("[P150] DUCO RF GW: sendJoin3Packet: INVALID join2 packet received!"));
	}
}

void DucoCC1101::sendJoinFinish(){
	setLogMessage(F("[P150] DUCO RF GW: FUNC: sendJoinFinish"));
	bool validJoin4Packet = false;

	if(matchingNetworkId(inDucoPacket.networkId)){

		if((inDucoPacket.deviceIdSender == 0x01) && (inDucoPacket.deviceIdReceiver == 0x00)){
			// check if network id is in command
			for(int i=0; i<4;i++){
				if(inDucoPacket.data[1+i] == joinCO2NetworkId[i]){
					validJoin4Packet = true;
					ducoDeviceState = ducoDeviceState_join4;

				}else{
					validJoin4Packet = false;
					break;
				}
			}
		}
	}

	if(validJoin4Packet){
		setLogMessage(F("[P150] DUCO RF GW: sendJoinFinish: valid join4 packet received!"));
		if(inDucoPacket.data[5] == inDucoPacket.data[6]){
			this->deviceAddress = inDucoPacket.data[5]; // = new address	

			String log2 = F("[P150] DUCO RF GW: sendJoinFinish: new device address is ");
			log2 += String( this->deviceAddress, DEC);
			setLogMessage(log2);
			// send ack! from new deviceaddress to address of sender.
			sendAck();
			setLogMessage(F("[P150] DUCO RF GW: sendJoinFinish: ACK sent!"));
			ducoDeviceState = ducoDeviceState_joinSuccessful;
		}
	}else{
		setLogMessage(F("[P150] DUCO RF GW: sendJoinFinish: INVALID join4 packet received!"));
	}
}




void DucoCC1101::waitForAck(){
	setLogMessage(F("[P150] DUCO RF GW: Start waiting for ack..."));
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
				setLogMessage(F("[P150] DUCO RF GW: CheckforAck: still waiting for ACK"));
				//resend message
				sendData(&outMessage);
				setLogMessage(F("[P150] DUCO RF GW: CheckforAck: message resent"));

				this->ackTimer = millis();
				this->ackRetry++;
			 }else{
				 this->waitingForAck = 0;
				setLogMessage(F("[P150] DUCO RF GW: CheckforAck: no ack received, cancel retrying."));

			 }
		 }
	}
}

bool DucoCC1101::matchingNetworkId(uint8_t id[])
{
	for (uint8_t i=0; i<3;i++){
		if (id[i] != this->networkId[i]){
			return false;
		}
	}
	return true;
}


bool DucoCC1101::matchingDeviceAddress(uint8_t compDeviceAddress){
	if(deviceAddress == compDeviceAddress){
		return true;
	}else{
		return false;
	}
}

 


bool DucoCC1101::checkForNewPacketInRXFifo(){
	uint8_t rxBytes = readRegisterWithSyncProblem(CC1101_RXBYTES, CC1101_STATUS_REGISTER);
	rxBytes = rxBytes & CC1101_BITS_RX_BYTES_IN_FIFO;

	if(rxBytes > 0){
		return true;
	}else{
		return false;
	}
}


void DucoCC1101::parseMessageCommand()
{
	// first byte of command
	switch(inDucoPacket.data[1] ){
		// Ping vanuit ducobox naar node. Het antwoord van de node is de byte uit de request plus 1 (decimaal).	
		//0x10 0x00 
		case 0x10:
			if(inDucoPacket.data[2] == 0x00){
				setLogMessage(F("[P150] DUCO RF GW: received ping command"));
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
			setLogMessage(F("[P150] DUCO RF GW: received change ventilation command"));

			if((inDucoPacket.data[4] < 7)){
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
					setLogMessage(F("[P150] DUCO RF GW: received request for parameter value command"));
					sendNodeParameterValue();
					waitForAck();
			}
		break;
		
		//Opvragen meerdere parameters van node (maximaal 3)
		//0x33 0x33 for 4 parameters OR 0x33 0x00 for 2 parameters
		case 0x33:
			if( (inDucoPacket.data[2] == 0x33) || (inDucoPacket.data[2] == 0x00)){
				setLogMessage(F("[P150] DUCO RF GW: received request for multiple parameters value command"));
				sendMultipleNodeParameterValue();
				waitForAck();
			}
		break;
		case DEFAULT:{
			setLogMessage(F("[P150] DUCO RF GW: unknown command received"));
			break;
		}
	}
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

	prefillDucoPacket(&outDucoPacket, inDucoPacket.deviceIdSender);
	outDucoPacket.counter = updateMessageCounter();
	ducoToCC1101Packet(&outDucoPacket, &outMessage);

	sendData(&outMessage);
	setLogMessage(F("[P150] DUCO RF GW: SEND sendConfirmationNewVentilationMode done!"));
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
			setLogMessage(F("[P150] DUCO RF GW: sendNodeParameterValue(); Requested parameter does not exist!"));

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

	prefillDucoPacket(&outDucoPacket, inDucoPacket.deviceIdSender);
	outDucoPacket.counter = updateMessageCounter();
	ducoToCC1101Packet(&outDucoPacket, &outMessage);

	sendData(&outMessage);

	String log2 = F("[P150] DUCO RF GW: SEND parameter: ");
	log2 += String( parameter, DEC);
	setLogMessage(log2);
}




void DucoCC1101::sendMultipleNodeParameterValue(){
	setLogMessage(F("[P150] DUCO RF GW: FUNC: sendMultipleNodeParameterValue"));

	//commando opvragen apparaatinfo (meerdere adresen per uitleesactie)					33	33	40	registeraddress byte1	registeraddress byte2	40	registeraddress byte1	registeraddress byte2	40	registeraddress byte1	registeraddress byte2	40	registeraddress byte1	registeraddress byte2								
	//antwoord					55	55	41	registeraddress byte1	registeraddress byte2	waarde byte 1	waarde byte 2	41	registeraddress byte1	registeraddress byte2	waarde byte 1	waarde byte 2	41	registeraddress byte1	registeraddress byte2	waarde byte 1	waarde byte 2	41	0	3	0	0
	outDucoPacket.messageType = ducomsg_message;

	uint8_t requestedParameters[3] = {};
	uint8_t counterParameters = 0;

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

	prefillDucoPacket(&outDucoPacket, inDucoPacket.deviceIdSender);
	outDucoPacket.counter = updateMessageCounter();
	ducoToCC1101Packet(&outDucoPacket, &outMessage);

	sendData(&outMessage);
	setLogMessage(F("[P150] DUCO RF GW: send multiple parameter done!"));
}


void DucoCC1101::getParameterValue(uint8_t parameter, uint8_t *value){
	//device serial: RS1521002390 (ascii -> hex)
	const uint8_t deviceSerial[12] = {0x52, 0x53, 0x31, 0x35, 0x32, 0x31, 0x30, 0x30, 0x32, 0x33, 0x39, 0x31};

	switch(parameter){
		// 0x00 = product id, value=12030 (0x2E 0xFE)
		case 0x00:{ value[0]= 0x2E; value[1]= 0xFE; break; }
		// 0x01 = softwareversion X.x.x, value= 1 (0x00 0x01)
		case 0x01:{ value[0]= 0x00; value[1]= 0x01; break; }
		// 0x02 = softwareversion x.X.x, value= 2 (0x00 0x02)
		case 0x02:{ value[0]= 0x00; value[1]= 0x02; break; }
		// 0x03 = softwareversion x.x.X, value= 0 (0x00 0x00)
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
 
	prefillDucoPacket(&outDucoPacket, inDucoPacket.deviceIdSender);
	outDucoPacket.counter = inDucoPacket.counter;
	ducoToCC1101Packet(&outDucoPacket, &outMessage);

	sendData(&outMessage);
	setLogMessage(F("[P150] DUCO RF GW: SEND PING done!"));
}




void DucoCC1101::sendAck(){
	setLogMessage(F("[P150] DUCO RF GW: Send Ack..."));

	outDucoPacket.messageType = ducomsg_ack;
	outDucoPacket.commandLength = 0;

	outDucoPacket.dataLength = 0;


	prefillDucoPacket(&outDucoPacket, inDucoPacket.deviceIdSender);
	outDucoPacket.counter = inDucoPacket.counter;
	ducoToCC1101Packet(&outDucoPacket, &outMessage);

	sendData(&outMessage);
	setLogMessage(F("[P150] DUCO RF GW: SEND ACK done!"));
}


void DucoCC1101::prefillDucoPacket(DucoPacket *ducoOutPacket, uint8_t receiverAddress){
	ducoOutPacket->deviceIdSender =  deviceAddress;
	ducoOutPacket->deviceIdReceiver = receiverAddress;
	ducoOutPacket->deviceIdSender2 =  deviceAddress;
	ducoOutPacket->deviceIdReceiver2 = receiverAddress;
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
	packet->data[6] = (duco->deviceIdSender << 3) | (duco->deviceIdReceiver >> 2);
	packet->data[7] = (duco->deviceIdReceiver << 6) | (duco->deviceIdSender << 1) | (duco->deviceIdReceiver2 >> 4);
	packet->data[8] = (duco->deviceIdReceiver2 << 4) | duco->counter;

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
	setLogMessage(F("[P150] DUCO RF GW: requestVentilationMode()"));

	outDucoPacket.messageType = ducomsg_message;

	outDucoPacket.command[0] = 0x40;
	outDucoPacket.command[1] = 0x00;
	outDucoPacket.command[2] = 0x22;
	outDucoPacket.commandLength = 3;
	outDucoPacket.data[0] = percentage;
	outDucoPacket.data[1] = (ventilationMode | 0x08);
	outDucoPacket.data[2] = (temp -100); // temp = 21.0c = 21 - 10 = 11 (dec) = 0x0B
	outDucoPacket.dataLength = 3;
 
	prefillDucoPacket(&outDucoPacket, 0x01); // address duco
	outDucoPacket.counter = inDucoPacket.counter;
	ducoToCC1101Packet(&outDucoPacket, &outMessage);

	sendData(&outMessage);
	setLogMessage(F("[P150] DUCO RF GW: SEND requestventilationmode done!"));

	String log2 = F("[P150] DUCO RF GW: SEND requestventilationmode, set mode to: ");
	log2 += String( ventilationMode, DEC);
	log2 += F(", override percentage: ");
	log2 += String( percentage, DEC);
	log2 += F("%, temp: ");
	log2 += String( temp, DEC);
	log2 += F("C");
	setLogMessage(log2);
	waitForAck();

}



