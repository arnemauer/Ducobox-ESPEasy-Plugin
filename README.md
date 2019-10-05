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

Ik heb een aantal volledige gateways over (RF en serial). Als je intresse hebt kun je deze (hier aanvragen)[https://forms.gle/QiHtNV9kgV45jKJh8].

<img src="https://lh3.googleusercontent.com/BwMq_kRBSWiwzLijpkmwPA1R2sJZLi7zy4GyY_how3kKHnGUe9z8nwNkQxhgmt_3JARkmPTbv7FDmz99WHIDQ6mGINUzBPpCQxCJR7BuxoFqu5GyYdGNuIPVW96-RQD9ebi2uOOi0qQMArJFxurA-l80G9yedwmcSlkiwf3SRvTiT6x6ZZG6vA01nDOni1H-FuWfYvo7SWxxftWyZ-NyQvb3njz-nzTb0cMnRHbCEaqT4UBUpOQsn-rpI0YrKb8rvKOaej1RvaKkod9okyXCCn8X40NpM0o04w3SbqwSpESJ7hqvjz--geNArxl7PCEA0UW3I3IfthfQQJU_csnE970Vhe4jU0bwnGglWz2aplGbeDiMzHTiZzZPQh7z5NSvGXKWVCCnN63BEo6vuuTTtMcAY0CYZ1ES1yyu0iymhjRiZbabDb6GXdfPrybYpEv4iB5yM7qDF8k6ruhUFaU8mYdV-I1Z_eVRadBLM7QW-XyTBrdH7gM0OG1KhW3qdEt1Ysr6_0GKrQ7MHowRg-kUCMh-8XDiBYesPoEjRMJKZ0ps6zgtTaIdvBWv-5qbaxoA8tkoS_zBqwKdmUqTlLbR1Gdcrcgdejus7f3S6mF4PyTGx_hg2v_EsrepZOmgZljLt41QRuViZft-SJQR0X0TU57VdC7-c2VgbV325QjTjPjNF2VC0NkvolwzNDdHPYAtpYrBypOkmGv5lghzeqkYk-uGWJHcq-78vN3ARnh8IvCegAtiDA=w300">  

## Screenshots ESP8266 als CO2 sensor in DUCO Network Tool ###
De Duco Network Tool is een programma waarmee je via een computer de ducobox kan uitlezen en configureren. 
Op deze [wikipagina](https://github.com/arnemauer/Ducobox-ESPEasy-Plugin/wiki/DUCO-Network-tool) vind je screenshots waarop je kan zien hoe de unit zich als CO2 sensor aanmeld bij de ducobox.

Delen van de code zijn gebaseerd op de library gemaakt door '[supersjimmie](https://github.com/supersjimmie/IthoEcoFanRFT/tree/master/Master/Itho )', 'Thinkpad', 'Klusjesman' and 'jodur'. 

