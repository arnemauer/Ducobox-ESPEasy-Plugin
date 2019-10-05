# Ducobox ESPEasy Plugin
Plugin voor ESPEasy om een Ducobox Silent / Focus aan te sturen / sensoren uit te lezen. 
Er zijn twee plugins: een ducobox RF gateway en ducobox serial gateway.

**Ducobox RF gateway (_P150_DucoRFGateway.ino)**

De RF gateway maakt gebruik van een CC1101 om de ducobox draadloos aan te sturen. De unit gedraagt zich als een CO2 bedieningsschakelaar. De unit registreert de actuele ventilatiemodus, kan de ventilatiemodus wijzigen en kan een override percentage instellen (ventilatiesnelheid ongeacht de gekozen modus).

**Ducobox serial gateway**

De serial gateway sluit je met een kabel aan op de print van de Ducobox. Hierdoor is meer informatie op te vragen zoals de huidige status, de ingestelde snelheid van de ventilator, de CO2 waarde van een CO2 sensor en de temperatuur van een CO2 sensor. Het is helaas niet mogelijk om de ventilatiemodus aan te passen. Als je de ventilatiemodus wilt wijzigen combineer je de serial gateway met de RF gateway.

Er zijn drie plugins voor de serial gateway:
** DucoSerialGateway (P151) ** 
Deze plugin leest data over de ducobox zelf uit. Je kan hiermee de flow oftewel de snelheid van de ventilator (0 - 100%) uitlezen en de ventilatiestand (auto,laag,midden,hoog).


** DucoSerialBoxSensors (P152) **
Duco heeft een aantal boxsensoren die met een kabel aangesloten worden op de Ducobox. Deze boxsensoren kunnen elk afzonderlijk in een specifieke kanaalopening van de afvoerbox geklikt worden. Er is een CO2 boxsensor en een vocht boxsensor. Door deze plugin meerdere keren te activeren kun je meerdere boxsensoren uitlezen.


** DucoSerialExternalSensors (P153) **

Door deze plugin meerdere keren te activeren kun je meerdere Bedieningsschakelaars uitlezen.


Delen van de code zijn gebaseerd op de library gemaakt door '[supersjimmie](https://github.com/supersjimmie/IthoEcoFanRFT/tree/master/Master/Itho )', 'Thinkpad', 'Klusjesman' and 'jodur'. 

## Benodigde hardware:
- WEMOS D1 of een ander ESP8266 variant

- Voor RF gateway: 
   * CC1101 868Mhz transmitter
   * 868Mhz SMA antenne
   * SMA Female Jack Connector For 1.6mm Solder Edge PCB Straight Mount 

- Voor Serial gateway: 
   * Mini Micro JST 2.0 PH 5-Pin Connector plug met Draden
   * PJ-307 3.5mm Stereo Jack Socket Audio Jack Connector PCB - 5PIN 

<img src="https://github.com/arnemauer/Ducobox-ESPEasy-Plugin/raw/master/Screenshots/HARDWARE.jpg" width="500">  

## Screenshots ESP8266 als CO2 sensor in DUCO Network Tool ###
De Duco Network Tool is een programma waarmee je via een computer de ducobox kan uitlezen en configureren. 
Op deze [wikipagina](https://github.com/arnemauer/Ducobox-ESPEasy-Plugin/wiki/DUCO-Network-tool) vind je screenshots waarop je kan zien hoe de unit zich als CO2 sensor aanmeld bij de ducobox.
