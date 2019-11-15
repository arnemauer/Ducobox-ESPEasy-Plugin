# Ducobox ESPEasy Plugin
Plugin voor ESPEasy om een Ducobox Silent / Focus aan te sturen / sensoren uit te lezen. 
Er zijn twee plugins: een ducobox RF gateway en ducobox serial gateway.

## Ducobox RF gateway (P150)
Met de RF gateway kun je de ventilatiestand (auto,laag,middel,hoog) van de Ducobox wijzigen. De RF gateway maakt gebruik van een CC1101 om de ducobox draadloos aan te sturen. De unit gedraagt zich als een CO2 bedieningsschakelaar. De unit registreert de actuele ventilatiemodus, kan de ventilatiemodus wijzigen en kan een override percentage instellen (ventilatiesnelheid ongeacht de gekozen modus).

## Ducobox serial gateway
De serial gateway sluit je met een kabel aan op de print van de Ducobox. Hierdoor is meer informatie op te vragen zoals de huidige status, de ingestelde snelheid van de ventilator, de CO2 waarde van een CO2 sensor en de temperatuur van een CO2 sensor. Het is helaas niet mogelijk om de ventilatiemodus aan te passen. Als je de ventilatiemodus wilt wijzigen combineer je de serial gateway met de RF gateway.

Er zijn drie plugins voor de serial gateway:

**DucoSerialGateway (P151)** 

Deze plugin leest data over de ducobox zelf uit. Je kan hiermee de flow oftewel de snelheid van de ventilator (0 - 100%) uitlezen en de ventilatiestand (auto,laag,midden,hoog).

**DucoSerialBoxSensors (P152)**

Duco heeft een aantal boxsensoren die met een kabel aangesloten worden op de Ducobox. Deze boxsensoren kunnen elk afzonderlijk in een specifieke kanaalopening van de afvoerbox geklikt worden. Er is een CO2 boxsensor en een vocht boxsensor. Door deze plugin meerdere keren te activeren kun je meerdere boxsensoren uitlezen.

**DucoSerialExternalSensors (P153)**

Er zijn diverse bedieningsschakelaars die je aan de Ducobox kan koppelen. Sommige bedieningsschakelaars hebben ook een sensor aan boord. Dit zijn de 'CO₂ Ruimtesensor - Bedieningsschakelaar' en de 'Vocht Ruimtesensor - Bedieningsschakelaar'. Alle bedieningsschakelaars vind je [hier](https://www.duco.eu/nl-nl-producten/nl-nl-luchtafvoer/nl-nl-sturingscomponenten/nl-nl-bedieningsschakelaar). Met deze plugin kun je de temperatuur, CO2 en vochtheidsgehalte uitlezen (afhankelijk van het type schakelaar). Door deze plugin meerdere keren te activeren kun je meerdere Bedieningsschakelaars uitlezen.


## Benodigde hardware:
Je hebt in iedergeval een WEMOS D1 of een ander ESP8266 variant nodig. Je kan de RF gateway en de serial gateway combineren op één esp8266. Wat je nodig hebt voor de RF gateway vind je [hier](https://github.com/arnemauer/Ducobox-ESPEasy-Plugin/wiki/3.-RF-Gateway---Hardware) en voor de serial gateway vind je [hier](https://github.com/arnemauer/Ducobox-ESPEasy-Plugin/wiki/6.-Serial-GW---Hardware).

Ik heb een aantal volledige gateways over (RF en serial). Als je intresse hebt kun je deze [hier aanvragen](https://forms.gle/QiHtNV9kgV45jKJh8).

<img src="https://lh3.googleusercontent.com/0fI6IxprpmEwW6kx5X_tSSoJS0TYV-Q07oT_ziOtLwpLDJNW01bCB9eZ2GzR8OqxKwnYWI5EP9L7IPNLjq2j26ZRZGzZqTCDyiMXdUQuD-yKaqmvAVYixdqD_9csVTxBillpgDZAxDFC1AxEzaFJKX6oHZ-bMghR40u9lSAC8P5zHsZVjxWrOhuZ6jJl-hJLs6yfNt_yLiewiTOrDUpY5v3Hew6GBr7jqjU2C8yiZ8sFIJ0dTiTPxj1L2uy0o33SWGA7D0yXdVbteEF7z9qKHpL3b0d3ztqrR55zP3qjvfyHdQPAnAH-9XK4NAYRKCX5CBXT0ORztqCECx2_vVpF73KFTJc7I9xfCXReJ6b15vDhVrXGDF2gEQSlb3PVzpK0Kgr96-Kbh-ZYxUtgUAdbiH7HlqPB4GtAVjvPSQeXQ2hH7lmb0cNjXkS0b5O7V3_GmQ5lcm6w1F0FIdPGAN1LWnApoQznlrw3c72uWPbHanuBkC9mdIl5euGa5JWeC21VvehGwiPg3YWJ6HxLI5koo1ZKSD3aHJZwAq7jD23SVYRVoQEYy_K-iye5eAa0ohyp7-Vv04wr0V4F8eUq5JyjdcWRIo0AjQWZotTIpvm35YJ46nMhLDnLWU6Wg6zzwPYV1LMHXKTX3AkTRkM_-01KxXA5MFuCxw9o7gS--xiWpIaIWHIQra7tCu4PR-2yvHFYeasQEloXzduChpKVHdh2m_IkzjOyoMuN8D0ntzQTFxN-Wda2OQ=w200">  

## Screenshots ESP8266 als CO2 sensor in DUCO Network Tool ###
De Duco Network Tool is een programma waarmee je via een computer de ducobox kan uitlezen en configureren. 
Op deze [wikipagina](https://github.com/arnemauer/Ducobox-ESPEasy-Plugin/wiki/DUCO-Network-tool) vind je screenshots waarop je kan zien hoe de unit zich als CO2 sensor aanmeld bij de ducobox.

Delen van de code zijn gebaseerd op de library gemaakt door '[supersjimmie](https://github.com/supersjimmie/IthoEcoFanRFT/tree/master/Master/Itho )', 'Thinkpad', 'Klusjesman' and 'jodur'. 

