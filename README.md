### Orologio Matrice
 
Semplice orologio/calendario/condizioni meteo utilizzando un NodeMCU, 2 blocchi da 4 display a matrice 8x8 e un sensore umidità temperatura DHT22.
Librerie da installare in Arduino IDE:

- EasyNTPClient by Harsha Alva
- TimeLib by Paul Stoffregen
- MD_Parola by Majic Designs
- MD_MAX72xx by Majic Designs
- Adafruit Unified Sensor Driver
- Adafruit DHT library

#### Componenti utilizzati
I seguenti link contengono un codice affiliazione Amazon solo per utenti Italiani:

- [Display matrice 8x32](https://amzn.to/3nwu8EC) (confezione da 3, ve ne avanzerà 1 ma si risparmia)
- [NodeMCU ESP8266](https://amzn.to/3GF7Ecf)
- [DHT22](https://amzn.to/3FtreqF)
- [Cavetti Jumper](https://amzn.to/3rmOeSX)

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
Dovete modificare il file .ino mettendo nome e password della vostra rete WiFi in quanto l'orario viene prelevato da un server NTP. In caso di problemi (il programma non riesce ad aggiornare l'orario):
- spegnete e riaccendete il router
- se il riavvio del router non funziona, provate ad utilizzare un indirizzo IP statico (segui istruzioni nel codice)

#### Case
Ho disegnato un case per alloggiare il display e il NodeMCU sul retro, lo renderò disponibile a breve ma è necessario avere il NodeMCU che ho messo nell'elenco in quanto le misure dei vari NodeMCU sul mercato sono diverse tra loro.

#### Video
[https://youtu.be/LpFm4PYXK0A](https://youtu.be/LpFm4PYXK0A)
