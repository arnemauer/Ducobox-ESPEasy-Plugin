/*
 * Author: Arne Mauer
 * Parts of this code is based on the itho-library made by 'supersjimmie', 'Thinkpad', 'Klusjesman' and 'jodur'.
 */

#ifndef DUCOPACKET_H_
#define DUCOPACKET_H_

class DucoPacket {
	public:
		uint8_t destinationAddress;   // destination address
		uint8_t sourceAddress;     // source address
		uint8_t originalDestinationAddress; // oringal destination address
		uint8_t originalSourceAddress; // original source address

		uint8_t networkId[4];

		uint8_t messageType;
		uint8_t rssiSender;
		uint8_t data[23]; // max packet is 32 bytes (- 9 bytes for addressing) = 23 bytes for data
		uint8_t dataLength;
		uint8_t length; // length of the totale message (only for receiving)
		
		uint8_t counter;		//0-15, MessageCounter is increased before sending a new message

		/**
     	* CRC OK flag
     	*/
    	bool crc_ok;

    	/**
     	* Received Strength Signal Indication
     	*/
    	uint8_t rssi;

    	/**
     	* Link Quality Index
     	*/
    	unsigned char lqi;
};

#endif /* DUCOPACKET_H_ */