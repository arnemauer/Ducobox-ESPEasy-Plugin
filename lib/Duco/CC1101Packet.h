/*
 * Author: Arne Mauer
 * Parts of this code is based on the itho-library made by 'supersjimmie', 'Thinkpad', 'Klusjesman' and 'jodur'.
 */

#ifndef CC1101PACKET_H_
#define CC1101PACKET_H_

#include <stdio.h>

#define CC1101_BUFFER_LEN        64
#define CC1101_DATA_LEN          CC1101_BUFFER_LEN - 3

class CC1101Packet{
	public:
		uint8_t length;
		uint8_t data[32]; 	// The CC1101 is configured with a maximum packet size of 32 bytes (PKTLEN register)
							//The packet length is defined as the payload data, excluding the length byte and the optional CRC.
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

#endif /* CC1101PACKET_H_ */