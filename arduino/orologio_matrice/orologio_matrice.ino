/*
 * Dot Matrix clock/calendar/weather
 * on 8 8x8 dot matrix displays driven from MAX7219
 * and NodeMCU (ESP8266)
 * This example comes out from "Parola_Zone_TimeMsg"
 * included in the "MD_Parola" library
 * I've only modified that example to suit my needs
 * 
 * MD parola wiki: https://majicdesigns.github.io/MD_Parola/index.html
 * 
 * Giovanni Bernardo (@cyb3rn0id)
 * https://www.settorezero.com
 */
 
#include <ESP8266WiFi.h> // standard library
#include <WiFiUdp.h> // standard library
#include <SPI.h> // standard library

#include <EasyNTPClient.h> // by Harsha Alva (https://github.com/aharshac/EasyNTPClient) - MIT
#include <TimeLib.h> // by Paul Stoffregen (https://github.com/PaulStoffregen/Time) - no license
#include <MD_Parola.h> // by Majic Designs (https://github.com/MajicDesigns/MD_Parola) - LGPL 2.1
#include <MD_MAX72xx.h> // by Majic Designs (https://github.com/MajicDesigns/MD_MAX72XX) - LGPL 2.1
#include <Adafruit_Sensor.h> // Adafruit Unified Sensor Driver (https://github.com/adafruit/Adafruit_Sensor) - Apache 2.0
#include <DHT.h> // Adafruit DHT library (https://github.com/adafruit/DHT-sensor-library) - MIT
#include <DHT_U.h> // As above

#include "myfont.h" // font I've modified from the standard one, 

#define DHTPIN D2 // Temperature/Humidity sensor connected on NodeMCU D2 
#define DHTTYPE DHT22 // DHT22 aka AM2302
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 8
#define CLK_PIN   D5
#define DATA_PIN  D7
#define CS_PIN    D8

// your wifi settings
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// NTP server
const char* ntpserver = "time.inrim.it";
uint16_t timeoffset=3600; // time offset in seconds
 
// uncomment if you want to use static IPs
//#define USE_STATIC

#ifdef USE_STATIC
IPAddress ip(192,168,1,35); // ip address you want to assign to your board
IPAddress gateway(192,168,1,1); // router address
IPAddress subnet(255,255,255,0); // subnet mask
IPAddress dns1(1,1,1,1); // DNS is required for EasyNTP client using a static IP
IPAddress dns2(1,0,0,1);
#endif

#define SPEED_TIME  75
#define PAUSE_TIME  0
#define MAX_MESG  40

MD_Parola P = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
WiFiUDP ntpUDP;
EasyNTPClient ntpClient(ntpUDP, ntpserver, timeoffset); 
DHT_Unified dht(DHTPIN, DHTTYPE);

// Global variables
char szTime[9];    // array used for the time (mm:ss\0)
char szMesg[MAX_MESG+1] = ""; // array used for other messages
uint8_t degC[] = { 6, 3, 3, 56, 68, 68, 68 }; // Degree symbol for temperature in °C

// month to string function
char *mon2str(uint8_t mon, char *psz, uint8_t len)
  {
  static const char str[][4] PROGMEM = {"Gen", "Feb", "Mar", "Apr", "Mag", "Giu", "Lug", "Ago", "Set", "Ott", "Nov", "Dic"};
  *psz = '\0';
   mon--;
  if (mon < 12)
    {
    strncpy_P(psz, str[mon], len); // strncpy_P => from Progmem to RAM (dest, source, length)
    psz[len] = '\0';
    }
  return(psz);
  }

// day of week to string
char *dow2str(uint8_t code, char *psz, uint8_t len)
  {
  static const char str[][10] PROGMEM = {"Domenica", "Lunedi", "Martedi", "Mercoledi", "Giovedi", "Venerdi", "Sabato"};
  *psz = '\0';
  code--;
  if (code < 7)
    {
    strncpy_P(psz, str[code], len);
    psz[len] = '\0';
    }
  return(psz);
  }

// returns hh:mm, if f=true => hour separator showed (used for flashing)
void getTime(char *psz, bool f = true)
  {
  sprintf(psz, "%02d%c%02d", hour(), (f ? ':' : ' '), minute());
  }

// return date as dd MMMM YYYY
void getDate(char *psz)
  {
  char m[4]=""; // array for storing month name
  mon2str(month(), m, 4);
  sprintf(psz, "%02d %s %d", day(), m, year());
  }

// return day of week name
void getDayOfWeek(char *psz)
  {
  char d[10]=""; // array for storing day of week name
  dow2str(weekday(), psz, 10);
  }

void setup(void)
{
  Serial.begin(57600);
  dht.begin();
  P.begin(2); // matrix init as 2 different zones
  P.setZone(0, 0, 3); // zone 0 (most right): from display 0 to display 3 (display 0 is where data line is connected)
  P.setZone(1, 4, 7); // zone 1 (most left): from display 4 to display 7
  P.setFont(myFont); // font for both zones (you can specify a different font for the 2 zones too if you want)
  P.setInvert(0, false);
  P.setInvert(1, false);
  P.setIntensity(9);

  wifiConnect();
  
  Serial.println("Connecting to NTP server");
  unsigned long t=ntpClient.getUnixTime();
  t==0?Serial.println("Time not updated"):Serial.println("Time updated");
  setTime(t);
  
  // zone 0 (right): will use the "szMesg" array for showing scrolling messages
  P.displayZoneText(0, szMesg, PA_CENTER, SPEED_TIME, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
  
  // zone 1 (left) : will use the "szTime" array for showing always the clock, no effects, no scrolling, static writings
  P.displayZoneText(1, szTime, PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT); 
  
  P.addChar('$', degC); // add degree symbol to the font definition in place of the '$' char
  getTime(szTime); // save actual time in szTime array for showing it quickly
}

void loop(void)
{
  static uint32_t lastTime = 0; // millis() memory
  static uint8_t  messagen = 0;  // message number to show in the most right part
  static bool flasher = false;  // flag used for flashing the hour separator
  sensors_event_t event; // DHT22 event object
  
  P.displayAnimate(); // perform display effects

  if (P.getZoneStatus(0)) // zone 0 has completed the animation
    {
    switch (messagen)
      {
      case 0: // Day of week
        messagen++;
        getDayOfWeek(szMesg);
        break;
        
      case 1:  // calendar
        messagen++;
        getDate(szMesg);
        break;
      
      case 2: // Temperature
        dht.temperature().getEvent(&event);
        messagen++;
        dtostrf(event.temperature, 4, 1, szMesg);
        strcat(szMesg, "$");
        break;

      default: // Relative Humidity
        dht.humidity().getEvent(&event);
        messagen=0;
        dtostrf(event.relative_humidity, 4, 1, szMesg);
        strcat(szMesg, "% RH");
        break;
      }
    P.displayReset(0); // reset zone 0
  }

  // write time
  if (millis() - lastTime >= 1000)
  {
    lastTime = millis();
    getTime(szTime, flasher);
    flasher = !flasher; // flashing time separator
    P.displayReset(1); // reset zone 1
  }
}

void wifiConnect(void)
  {
  uint8_t i=0;
  WiFi.mode(WIFI_STA);
 
  Serial.print("Connecting to: ");
  Serial.println(ssid);

  WiFi.hostname("Led_Matrix_Clock");
  #ifdef USE_STATIC
  WiFi.config(ip,gateway,subnet,dns1,dns2);
  #endif
  WiFi.begin(ssid, password);

  // Connect to WiFi network
  while ((WiFi.status() != WL_CONNECTED) && (i<100))
    {
    delay(500);
    Serial.print(".");
    i++;
    }
  
  if (WiFi.status() == WL_CONNECTED)
    {
    Serial.println("WiFi Connected.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Subnet Mask: ");
    Serial.println(WiFi.subnetMask());
    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("MAC Address: ");
    Serial.println(WiFi.macAddress());
   }
  else
    {
    P.print("No WiFi");
    Serial.println("Not connected");
    }
}
