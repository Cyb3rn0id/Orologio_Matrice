### Orologio Matrice aka (Yet Another) Dot Matrix Clock
 
Semplice orologio/calendario/condizioni meteo utilizzando un NodeMCU, 2 blocchi da 4 display a matrice 8x8 e un sensore umidità temperatura DHT22. Ho pubblicato il [progetto completo su Hackster](https://www.hackster.io/CyB3rn0id/yet-another-dot-matrix-clock-ee98f3). Su Hackster potete trovare anche l'enclosure da stampare in 3D.

Update 24/07/2022 : L'orologio esegue il cambio automatico ora solare/ora legale (DST), una routine aggiorna l'orologio ogni ora o se all'avvio non c'è stato l'aggiornamento, e i dati di umidità e temperatura possono essere inviati, opzionalmente, ad un server MQTT.

[![Video Orologio matrice](orologio_matrice.jpg)](https://www.youtube.com/watch?v=LpFm4PYXK0A)

Librerie da installare in Arduino IDE:

- EasyNTPClient by Harsha Alva
- TimeLib by Paul Stoffregen
- MD_Parola by Majic Designs
- MD_MAX72xx by Majic Designs
- Adafruit Unified Sensor Driver
- Adafruit DHT library
- (opzionale per MQTT) PubSubClient by Nick O' Leary


#### Componenti utilizzati
I link sia ad Amazon che a Futura Elettronica contengono un codice di affiliazione:

- Display matrice 8x32: [Amazon](https://amzn.to/3nwu8EC) (confezione da 3, ve ne avanzerà 1 ma si risparmia) / [Futura Elettronica](https://www.futurashop.it/componenti-sensori-breakoutboard-cavi-contenitori/optoelettronica/display/display-a-matrice/8300-YB302?tracking=5f004a6ba8be7)
- NodeMCU ESP8266: [Amazon](https://amzn.to/3GF7Ecf) / [Futura Elettronica](https://www.futurashop.it/componenti-sensori-breakoutboard-cavi-contenitori/optoelettronica/display/display-a-matrice/8300-YB302?tracking=5f004a6ba8be7)
- DHT22: [Amazon](https://amzn.to/3FtreqF) / [Futura Elettronica](https://www.futurashop.it/modulo-sensore-di-temperatura-umidita-dht22-8300-MODDHT22?tracking=5f004a6ba8be7)
- Cavetti Jumper: [Amazon](https://amzn.to/3rmOeSX) / [Futura Elettronica](https://www.futurashop.it/confezione-50-jumper-femmina-femmina-vari-colori-7300-jumper50?tracking=5f004a6ba8be7)

#### Collegamenti
Unire insieme due display a matrice per formarne un'unico da 8 elementi. I display hanno un header maschio 5 pin sulla destra (DIN) e 5 pad liberi sulla sinistra (DOUT): attaccarne due in cascata come meglio ritenete opportuno collegando i 5 pin di uno con i 5 dell'altro aventi lo stesso nome (solo i pin DIN e DOUT hanno nome diverso e vanno comunque collegati insieme).
Al NodeMCU andrà collegato solo l'header maschio rimasto libero.

collegamento display -> NodeMCU  
CLK -> D5  
CS -> D8  
DIN -> D7  
GND -> GND  
VCC - > VU (o VV o VUSB)  

collegamento DHT22 -> NodeMCU  
Avendo il DHT22 frontale (ovvero con la griglia rivolta a voi), il pin 1 è quello sulla sinistra  
PIN 1 (VCC) -> 3V3  
PIN 2 (Data) -> D2  
PIN 3 (NC) : non connesso  
PIN 4 (GND) : GND  
  
Il NodeMCU andrà alimentato dalla porta USB.  

#### Setup
Dovete modificare il file secret.h mettendo nome e password della vostra rete WiFi in quanto l'orario viene prelevato da un server NTP. In caso di problemi (il programma non riesce ad aggiornare l'orario):
- spegnete e riaccendete il router
- se il riavvio del router non funziona, provate ad utilizzare un indirizzo IP statico (segui istruzioni nel codice)
Modificate il file .ino commentando la riga #define MQTT se non intendete usare un server MQTT. Se invece intendete usarlo, modificate anche la riga `const char* mqtt_topic="garage";` mettendo un vostro topic. Supponendo di avere impostato il topic a `garage`, ritroverete il valore di umidità sul topic `garage/humidity` e il valore di temperatura su `garage/temperature`.
