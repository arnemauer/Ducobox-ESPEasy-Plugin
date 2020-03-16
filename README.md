# Ducobox ESPEasy Plugin
Plugin voor ESPEasy om een Ducobox Silent / Focus aan te sturen / sensoren uit te lezen. 
Er zijn twee plugins: een ducobox RF gateway en ducobox serial gateway.

## Ducobox RF gateway (P150)
Met de RF gateway kun je de ventilatiestand (auto,laag,middel,hoog) van de Ducobox wijzigen. De RF gateway maakt gebruik van een CC1101 om de ducobox draadloos aan te sturen. De unit gedraagt zich als een CO2 bedieningsschakelaar. De unit registreert de actuele ventilatiemodus, kan de ventilatiemodus wijzigen en kan een override percentage instellen (ventilatiesnelheid ongeacht de gekozen modus).

## Ducobox serial gateway
De serial gateway sluit je met een kabel aan op de print van de Ducobox. Hierdoor is meer informatie op te vragen zoals de huidige status, de ingestelde snelheid van de ventilator, de CO2 waarde van een CO2 sensor en de temperatuur van een CO2 sensor. Het is helaas niet mogelijk om de ventilatiemodus aan te passen. Als je de ventilatiemodus wilt wijzigen combineer je de serial gateway met de RF gateway.

Er zijn drie plugins voor de serial gateway:

**DUCO Serial Gateway (P151)** 

Deze plugin leest data over de ducobox zelf uit. Je kan hiermee vier waarden uitlezen:
1. Ventilatiestand (auto,laag,midden,hoog)
2. Ventilatie percentage (0 - 100%)
3. Ventilator percentage (snelheid van de ventilator (0 - 100%))
4. Countdown (in seconden voordat de ducobox van de manuele stand zal terugschakelen naar AUTO)

**DUCO Serial GW - Box Sensor - CO2 (koolstofdioxide in PPM) (P152)**
**DUCO Serial GW - Box Sensor - CO2 (temperatuur) & luchtvochtigheidssensor (temperatuur en luchtvochtigheid) (P153)**
Duco heeft een aantal boxsensoren die met een kabel aangesloten worden op de Ducobox. Deze boxsensoren kunnen elk afzonderlijk in een specifieke kanaalopening van de afvoerbox geklikt worden. Er is een CO2 boxsensor en een vocht boxsensor. Door deze plugin meerdere keren te activeren kun je meerdere boxsensoren uitlezen.

- Uitlezen van een CO2 boxsensor: activeer P152 om de CO2 uit te lezen en activeer P153 om de temperatuur uit te lezen.
- Uitlezen van een luchtvochtigheidsboxsensor: activeer P153 om de temperatuur en luchtvochtigheid uit te lezen.

**DUCO Serial GW - External CO2 Sensor (P154)**
Er zijn diverse bedieningsschakelaars die je aan de Ducobox kan koppelen. Sommige bedieningsschakelaars hebben ook een sensor aan boord. Dit zijn de 'CO₂ Ruimtesensor - Bedieningsschakelaar' en de 'Vocht Ruimtesensor - Bedieningsschakelaar'. Alle bedieningsschakelaars vind je [hier](https://www.duco.eu/nl-nl-producten/nl-nl-luchtafvoer/nl-nl-sturingscomponenten/nl-nl-bedieningsschakelaar). 

Met deze plugin kun je de CO2 en temperatuur uitlezen van een externe CO2 schakelaar. Door deze plugin meerdere keren te activeren kun je meerdere Bedieningsschakelaars uitlezen.

**DUCO Serial GW - External Humidity Sensor (P155)**
Met deze plugin kun je het vochtheidsgehalte en temperatuur uitlezen van een vochtruimtesensor bedieningsschakelaar. Door deze plugin meerdere keren te activeren kun je meerdere Bedieningsschakelaars uitlezen.

## Benodigde hardware:
Je hebt in iedergeval een WEMOS D1 of een ander ESP8266 variant nodig. Je kan de RF gateway en de serial gateway combineren op één esp8266. Wat je nodig hebt voor de RF gateway vind je [hier](https://github.com/arnemauer/Ducobox-ESPEasy-Plugin/wiki/3.-RF-Gateway---Hardware) en voor de serial gateway vind je [hier](https://github.com/arnemauer/Ducobox-ESPEasy-Plugin/wiki/6.-Serial-GW---Hardware).

Ik heb een aantal volledige gateways over (RF en serial). Als je intresse hebt kun je deze [hier aanvragen](https://forms.gle/QiHtNV9kgV45jKJh8).

<img src="https://lh3.googleusercontent.com/8azWteHwdkUwZEbfpm3KQFrbueGk_BfumJB3W-Lc2TxvxVRz8y6pe8aM5TCT5qbIoEQRvA52vejgsPj5eKMWKzezNE3t-M4v8dK7BUFs3j8R4Ma_XhQnv9FcH2Ab57_8G4KVMIvFdWpQaV5EKEiB333v_LX35ozgbhsD1LTaGCPZZ6HvIlw_DLLEPp_IKZrqKA_G_u8MUVSIz0tqes-GICjkhzgqUdYIZWfDlPwa-VB8y03NIlfVbd9inAQW-qyOyHumjzqOiEUMQnm8JGBZNCTe4_VERnp7uJUGfEjdfHE9GVNgSfoWegcbY7amy9FtIrcJYLB4bbJpiQ4u67PnjV9wp0upGlEEjspQnTiSIJ1vj48TPr6P-GZlflim1CMEq6R4M2e3-NOX0MfhmXzqj40yQZN0yuI2uTpsjKmp_zNx_lG20HI_L6xgnS0uL_tlosomtS3APT0Vqqb2Hw039wEIwI59GKcodg-nIWmBkFfDQXhTeqRtqGOM1g2s8w2cQK7jBmBVwntbX6AllVb1iOoub5WZE28HLZo95C3_RY2za98LBMRYqmBve8bIFTx5iGvwcJAogwpnhqmqspjqJB-t7XKmuaZnIa2kAiBMf0MoHW9awUn5chu2c1bAu0MDdYO9lMN2dj12cnHlGApQmr7CP1-twj65wF2_cvP5V_VlSZ-jBjUUCo3Asxh_p4spA-F0XdQdziZf8ECUKs4qSQd1vOQM0ejn_t1hVA4uZ_kMVmXlOA=w200">  

## Screenshots ESP8266 als CO2 sensor in DUCO Network Tool ###
De Duco Network Tool is een programma waarmee je via een computer de ducobox kan uitlezen en configureren. 
Op deze [wikipagina](https://github.com/arnemauer/Ducobox-ESPEasy-Plugin/wiki/DUCO-Network-tool) vind je screenshots waarop je kan zien hoe de unit zich als CO2 sensor aanmeld bij de ducobox.

Delen van de code zijn gebaseerd op de library gemaakt door '[supersjimmie](https://github.com/supersjimmie/IthoEcoFanRFT/tree/master/Master/Itho )', 'Thinkpad', 'Klusjesman' and 'jodur'. 

