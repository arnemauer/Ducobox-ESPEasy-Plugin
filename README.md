# Ducobox ESPEasy Plugin
Plugin voor ESPEasy om een Ducobox Silent / Focus aan te sturen / sensoren uit te lezen. 


Benodigde hardware:
- WEMOS D1 of een ander ESP8266 variant
- CC1101 868Mhz transmitter (aliexpress.com)



|CC11xx pin    |ESP pins|Description                                        |
|:-------------|:-------|:--------------------------------------------------|
|1 - VCC       |VCC     |3v3                                                |
|2 - GND       |GND     |Ground                                             |
|3 - MOSI      |13=D7   |Data input to CC11xx                               |  
|4 - SCK       |14=D5   |Clock pin                                          |
|5 - MISO/GDO1 |12=D6   |Data output from CC11xx / serial clock from CC11xx |
|6 - GDO2      |04=D1  |output as a symbol of receiving or sending data    |
|7 - GDO0      |X        |output as a symbol of receiving or sending data    |
|8 - CSN      |15=D8   |Chip select / (SPI_SS)                             |


Delen vn de code zijn gebaseerd op de library gemaakt door 'supersjimmie', 'Thinkpad', 'Klusjesman' and 'jodur'. https://github.com/supersjimmie/IthoEcoFanRFT/tree/master/Master/Itho 

 
## Commando's:

**1. Veranderd ventilatiemode commando**: VENT,mode,override 

	**mode**
	- AUTO
	- LOW
	- MIDDLE
	- HIGH
	- NOTHOME

	**override**
	- percentage 0 - 100%

	LET OP: dit percentage wordt overgenomen ongeacht de ventilatiemodus.


**2. Join commando**: JOIN

**3. Disjoin commando**: DISJOIN


### Aanmelden ESP8266 

Om de ventilator aan te sturen moet je deze aanmelden bij de Ducobox. Zet de Ducobox daarvoor in de "installermode" via de drukknop op de printplaat van de Ducobox of via een van de componenten.
Start daarna het join commando. Na enkele seconden is het netwerkid en deviceid ingesteld.