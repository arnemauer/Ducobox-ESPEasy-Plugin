/*
 * Author: Arne Mauer
 * Parts of this code is based on the itho-library made by 'supersjimmie', 'Thinkpad', 'Klusjesman' and 'jodur'.
 */

#ifndef DUCOPACKET_H_
#define DUCOPACKET_H_

class DucoPacket
{
	public:
		uint8_t deviceIdReceiver;
		uint8_t deviceIdSender;
		uint8_t deviceIdReceiver2;
		uint8_t deviceIdSender2;
		uint8_t networkId[4];

		uint8_t messageType;
		
		uint8_t command[4];
		uint8_t commandLength;

		uint8_t data[40];
		uint8_t dataLength;
		
		uint8_t counter;		//0-255, counter is increased on every remote button press
};


#endif /* DUCOPACKET_H_ */