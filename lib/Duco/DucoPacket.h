/*
 * Author: Arne Mauer
 * Parts of this code is based on the itho-library made by 'supersjimmie', 'Thinkpad', 'Klusjesman' and 'jodur'.
 */

#ifndef DUCOPACKET_H_
#define DUCOPACKET_H_

class DucoPacket
{
	public:

		uint8_t destinationAddress;   // destination address
		uint8_t sourceAddress;     // source address
		uint8_t originalDestinationAddress; // oringal destination address
		uint8_t originalSourceAddress; // original source address

		uint8_t networkId[4];

		uint8_t messageType;
		
		uint8_t command[4];
		uint8_t commandLength;

		uint8_t data[40];
		uint8_t dataLength;
		
		uint8_t counter;		//0-255, counter is increased on every remote button press

		/**
     	* CRC OK flag
     	*/
    	bool crc_ok;

    	/**
     	* Received Strength Signal Indication
     	*/
    	unsigned char rssi;

    	/**
     	* Link Quality Index
     	*/
    	unsigned char lqi;
};


#endif /* DUCOPACKET_H_ */