/*
 * Dot Matrix clock/calendar/weather
 * on 8 8x8 dot matrix displays driven from MAX7219
 * and NodeMCU (ESP8266)
 * This example comes out from "Parola_Zone_TimeMsg"
 * included in the "MD_Parola" library
 * I've only modified that example to suit my needs
 * MD parola wiki: https://majicdesigns.github.io/MD_Parola/index.html
 * 
 * Updates 24/07/2022:
 * + added MQTT for transmitting Temperature/Humidity value 
 * + added automatic DST
 *
 * Giovanni Bernardo (@cyb3rn0id)
 * https://www.settorezero.com
 *
 * Project Published on Hackster.io:
 * https://www.hackster.io/CyB3rn0id/yet-another-dot-matrix-clock-ee98f3
 */

#define DEBUG // uncomment for verbose output on serial. Used most for testing DST function
#define MQTT  // uncomment if you want to use MQTT
//#define USE_STATIC_IP // uncomment if you want to use static IPs

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
#ifdef MQTT
 #include <PubSubClient.h> // by Nick O' Leary (https://github.com/knolleary/pubsubclient) - MIT
#endif

#include "myfont.h" // font I've modified from the standard one
#include "secret.h" // user/passwords for wifi and mqtt

// DHT22 settings
#define DHTPIN D2 // Temperature/Humidity sensor connected on NodeMCU D2 
#define DHTTYPE DHT22 // DHT22 aka AM2302

// Matrix displays settings
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 8 // 8 matrix displays
#define CLK_PIN   D5
#define DATA_PIN  D7
#define CS_PIN    D8

// MD Parola settings
#define SPEED_TIME 75
#define PAUSE_TIME 0
#define MAX_MESG 40

// NTP server
const char* ntpserver = "time.inrim.it";
uint16_t timeoffset=3600; // time offset in seconds for standard solar time (ROME: UTC+1 = UTC+3600 seconds)

// IP addresses for static ip
#ifdef USE_STATIC_IP
 IPAddress ip(192,168,1,35); // ip address you want to assign to your board
 IPAddress gateway(192,168,1,1); // router address
 IPAddress subnet(255,255,255,0); // subnet mask
 IPAddress dns1(1,1,1,1); // Looks like DNS is required for EasyNTP client using a static IP
 IPAddress dns2(1,0,0,1);
#endif

// MQTT settings
#ifdef MQTT
 #define MQTT_USE_PASSWORD // Comment this if your MQTT server does not require user/password
 #define MQTT_REFRESH 10 // update MQTT every 10 seconds
 IPAddress mqtt_server(192,168,1,101); // this is the address of your MQTT server
 const uint16_t mqtt_port=1883; // this is the port where your MQTT server is listening. 1883 is the standard value
 const char* mqtt_clientID = "YADM_Clock_by_Cyb3rn0id"; // unique name for this MQTT client
 const char* mqtt_topic="garage"; // name for MQTT topic (will be added /temperature and /humidity to this)
 uint16_t mqtttime=MQTT_REFRESH; // update MQTT every 10 second
#endif

MD_Parola P = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
WiFiUDP ntpUDP;
EasyNTPClient ntpClient(ntpUDP, ntpserver); 
DHT_Unified dht(DHTPIN, DHTTYPE);
#ifdef MQTT
 WiFiClient MQTT_WiFi_Client;
 PubSubClient MQTTClient(mqtt_server, mqtt_port, MQTT_WiFi_Client);
#endif

// Global variables
char szTime[9]; // array used for the time (mm:ss\0)
char szMesg[MAX_MESG+1] = ""; // array used for other messages
uint8_t degC[] = { 6, 3, 3, 56, 68, 68, 68 }; // Degree symbol for temperature in °C
bool timeupdated=false; // keep track of time updating successful
bool mqttconnection=true; // keep track of mqtt connection successful. true also if mqtt not used for clock stuff
bool wifiok=false; // keep track of wifi status

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

 // connect to wifi
 wifiok=wifiConnect();

 // if wifi connected => update time and connect (eventually) to MQTT broker
 if (wifiok)
  {
  timeupdated=timeupdate(timeoffset);
  if (timeupdated)
    {
    // re-load time if we're in DST
    if (checkDST())
      {
      delay(500);
      timeupdated=timeupdate(timeoffset*2);    
      }
    }
#ifdef MQTT
  mqttconnection=mqtt_connect();
#endif
  }
else // No wifi, so time cannot update and MQTT connection cannot be done
  {
  timeupdated=false;
#ifdef MQTT
  mqttconnection=false;
#endif
  }

 // zone 0 (right): will use the "szMesg" array for showing scrolling messages
 P.displayZoneText(0, szMesg, PA_CENTER, SPEED_TIME, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
 // zone 1 (left) : will use the "szTime" array for showing always the clock, no effects, no scrolling, static writings
 P.displayZoneText(1, szTime, PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT); 
 // add degree symbol to the font definition in place of the '$' char
 P.addChar('$', degC);
 // save actual time in szTime array for showing it quickly
 getTime(szTime); 
}

void loop(void)
 {
 static uint32_t lastTime = 0; // millis() memory for time updating
 static uint32_t lastTimeMQTT = 0; // millis() memory for MQTT updating
 static uint8_t  messagen = 0;  // message number to show in the most right part
 static bool flasher = false;  // flag used for flashing the hour separator
 static float temp; // DHT temperature
 static float hum; // DHT humidity
 static bool firststart=true; // update DHT values over MQTT first time
 static int prevhour=hour(); // keep track of hour for DST/no DST stuff
 sensors_event_t event; // DHT22 event object

 P.displayAnimate(); // perform display effects
 
#ifdef MQTT
 if (mqttconnection) 
    {
    mqttconnection=MQTTClient.loop(); // mantain active MQTT connection, returns connection status
    if (firststart) // send DHT22 at first start
      {
      delay(50);
      dht.temperature().getEvent(&event);
      temp=event.temperature;
      delay(100);
      dht.humidity().getEvent(&event);
      hum=event.relative_humidity;
      firststart=false;
      mqtttime=0; // refresh quick
      }
    }
  else
    {
    mqtttime=60; // try to reconnect every minute  
    }
 // retrieve both temperature and humidity at start for updating mqtt at startup
 
#endif // use MQTT

 // check for DST change/update time every hour only if wifi working
 if ((hour()!=prevhour) && (wifiok))
    {
    prevhour=hour();
    if (checkDST())
      {
      timeupdated=timeupdate(timeoffset+3600); // additional 1 hour for DST  
      }
    else
      {
      timeupdated=timeupdate(timeoffset);  
      }
    }

 // update clock - scrolling message
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
        messagen++;
        dht.temperature().getEvent(&event);
        temp=event.temperature;
        dtostrf(temp, 4, 1, szMesg);
        strcat(szMesg, "$"); // add grade symbol
        break;
      case 3: // Relative Humidity
        // add further message if no wifi or no mqtt
        messagen=((wifiok && mqttconnection)?0:(messagen+1));
        dht.humidity().getEvent(&event);
        hum=event.relative_humidity;
        dtostrf(hum, 4, 1, szMesg);
        strcat(szMesg, "% RH");
        break;
       case 4: // no wifi or no mqtt
        messagen=0;
        if (!wifiok) // if no wifi, it's logic that there isn't mqtt (and time updates)
          {
          strncpy_P(szMesg, "WiFi Failed!", 13);
          }
        else // no mqtt but there is wifi: restart your mqtt broker or router
          { 
          strncpy_P(szMesg, "MQTT Failed!", 13);
          }
        break;
      } // switch
    P.displayReset(0); // reset zone 0
    } // zone 0 completed animation

 // update clock - time (every second)
 if (millis() - lastTime >= 1000)
    {
    lastTime = millis();
    getTime(szTime, flasher);
    flasher = !flasher; // flashing time separator
    P.displayReset(1); // reset zone 1
    // if time not updated, try every second if wifi is connected
    if (!timeupdated && wifiok)
       {
       timeupdated=timeupdate(timeoffset);
       }
    } // update clock every second

  // send new DHT22 values over MQTT or try to connect to MQTT Broker if no connecition
  // check also wifi connection every this time so I can report on display if connection broke
#ifdef MQTT
  if (millis() - lastTimeMQTT >= (mqtttime*1000))
     {
     if (WiFi.status() == WL_CONNECTED) wifiok=true;
     lastTimeMQTT = millis();
     if (!mqttconnection) // try to reconnect
        {
        if (wifiok) mqttconnection=mqtt_connect();
        }
     else
        {
        mqtttime=MQTT_REFRESH; // refresh every 10sec
        // numbers must to be converted in arrays in order to be passed to mqtt_publish function
	      char value[5]; // array used for numbers 
        char fulltopic[50]; // array used for the full topic
        dtostrf(temp,4,1,value); // decimal to array
        strcpy(fulltopic, mqtt_topic);
        strcat(fulltopic, "/temperature"); // append '/temperature' to topic
#ifdef DEBUG
        if (MQTTClient.publish(fulltopic, value))
            {
	          Serial.print("Temperature published: ");
            Serial.println(value);
            }
        else
            {
            Serial.println("Temperature NOT published");  
            }
#endif
	      dtostrf(hum,4,1,value);
        strcpy(fulltopic, mqtt_topic);
        strcat(fulltopic, "/humidity");
#ifdef DEBUG     
	      if (MQTTClient.publish(fulltopic, value))
            {
            Serial.print("Humidity published: ");
            Serial.println(value);
            }
        else
            {
            Serial.println("Humidity NOT published");  
            }
#endif
      } // mqtt connection is ok
   } // time passed for mqtt
#endif // MQTT used
 } // loop

// connect to NTP server, set time offset, returns success or not
bool timeupdate(int offset)
  {
  bool timeok=false;
#ifdef DEBUG  
  Serial.print("Connecting to NTP server, time offset=");
  Serial.print(offset);
  Serial.println(" seconds");
#endif
  unsigned long t=ntpClient.getUnixTime();
  if (t==0)
    {
    Serial.println("Time not updated");
    timeok=false;
    }
  else
    {
    Serial.println("Time updated");
    timeok=true;
    }
  // I'll set time offset from TimeLib, since using setTimeOffset to the EasyNTP seems not working the second time
  setTime(t+offset);
  return (timeok);
  }

bool wifiConnect(void)
  {
  uint8_t i=0;
  WiFi.mode(WIFI_STA);
 
#ifdef DEBUG
  Serial.print("Connecting to: ");
  Serial.println(ssid);
#endif
  WiFi.hostname("Led_Matrix_Clock");
#ifdef USE_STATIC_IP
  WiFi.config(ip,gateway,subnet,dns1,dns2);
#endif
  WiFi.begin(ssid, password);

  // Connect to WiFi network
  while ((WiFi.status() != WL_CONNECTED) && (i<100))
    {
    delay(300);
#ifdef DEBUG
    Serial.print(".");
#endif    
	i++;
    }
  // check and return wifi status 
  if (WiFi.status() == WL_CONNECTED)
    {
#ifdef DEBUG  
	  Serial.println("WiFi Connected.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Subnet Mask: ");
    Serial.println(WiFi.subnetMask());
    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("MAC Address: ");
    Serial.println(WiFi.macAddress());
#endif
    return true;
   }
  else
    {
#ifdef DEBUG
    Serial.println();
    Serial.println("Not connected");
#endif
    return false;
    }
  }

#ifdef MQTT
bool mqtt_connect(void) 
  {
#ifdef DEBUG    
	Serial.println("Trying to connect to MQTT Broker");  
#endif    
	// Attempt to connect
#ifdef MQTT_USE_PASSWORD
        if (MQTTClient.connect(mqtt_clientID,mqtt_user,mqtt_password)) 
#else
        if (MQTTClient.connect(mqtt_clientID))  
#endif
        {
#ifdef DEBUG        
		    Serial.println("MQTT connected");
#endif        
		    return (true);
        } 
    else 
        {
#ifdef DEBUG      
	      Serial.print("MQTT failed, rc=");
        Serial.println(MQTTClient.state());
#endif   
        return (false);     
	      }
    }
#endif

// check if actual time is Standard Time ("ora solare") or Daylight Saving Time ("ora legale")
// returns DST status (true if we're between the 2:00AM of the last Sunday of March
// and the 03:00AM of the last Sunday of October)
bool checkDST(void)
  {
  // In italy DST goes from last Sunday of March (we must set UTC+2 at 2:00AM)
  // until last sunday of October (we must set UTC+1 at 3:00AM)
#ifdef DEBUG  
  Serial.println("Checking DST");
#endif
  // Month is Apr,May,Jun,Jul,Aug,Sep => for sure we're in DST
  if ((month()>3) && (month()<10))  
    {
#ifdef DEBUG      
  Serial.println("Month is Between April and September: we're in DST");
#endif    
  return (true);
    }
  // Month is March or October: we must check day accurately
  if ((month()==3) || (month()==10))
    {
#ifdef DEBUG      
  Serial.println("Month is March or October: we must check day");
#endif
    // Last sunday can occurr only after day 24, March and October have both 31 days:
    // if 24 is Sunday, last Sunday will occurr on 31th (24+7 = 31), so last Sunday is
    // always from 25 to 31
    if (day()<25) // if day is <25 and we're in March, cannot be DST, but if we're in October, it's still DST
      {
      if (month()==3)
        {
        // march 1 to 24 : cannot be DST
#ifdef DEBUG          
    Serial.println("Month is March and day is from 1 to 24: we're NOT in DST");
#endif        
    return false;
        }
      else
        {
        // october 1 to 24 : still DST
#ifdef DEBUG         
      Serial.println("Month is October and day is from 1 to 24: we're in DST");
#endif
        return true;
        }
      }
    // if we're here, means day is from 25 to 31
    // so... today is sunday or sunday is already passed?
    // In TimeLib Sunday is 1 and Saturday is 7 (this is called "weekday()")
    // the value (31-actual day number+weekday number) is a number from 1 to 7 if today is Sunday or
    // Sunday is already passed. Is a number between 8 and 13 if Sunday has to come
    if (((31-day())+weekday()) <8) // It's Sunday or Sunday is already passed
      {
      // today is Sunday and it's the 2:00AM or 2:00AM are passed if in March? 
      // or is Sunday and it's the 3:00AM or 3:00 AM are passed if in October?
      // if (sunday + (march + hour>1 OR october + hour>2))
#ifdef DEBUG  
      Serial.println("It's the last Sunday or last Sunday is already passed");
#endif      
    if (weekday()==1 && (((month()==3) && (hour()>1)) || ((month()==10) && (hour()>2)))) //&& hour()>1) // 
        {
#ifdef DEBUG          
    Serial.println("It's the last Sunday of March or October and is the right hour too!");
#endif        
    // If March, we're still in DST, if October, DST has ended
        if (month()==3)
          {
#ifdef DEBUG            
      Serial.println("We're in March, we're in DST");
#endif          
      return true;
          }
        else
          {
#ifdef DEBUG            
      Serial.println("We're in October, we're NOT in DST");
#endif          
      return false;  
          }
        }
      // it's not sunday, but sunday is passed
      else if (weekday()>1)
        {
#ifdef DEBUG          
    Serial.println("Last Sunday is passed");
#endif        
    // If March, we're in DST, if October, DST has ended
        if (month()==3)
          {
#ifdef DEBUG            
      Serial.println("It's March: we're in DST");
#endif          
      return true;  
          }
        else
          {
#ifdef DEBUG            
      Serial.println("It's October: we're NOT in DST");
#endif          
      return false;  
          }
        }
      else
        // it's Sunday but are not the 2:00AM in March nor the 3:00AM in October
        {
        // If March, no DST, if October, we're still in DST
#ifdef DEBUG          
    Serial.println("It's the last Sunday but it's not the right hour");
#endif        
    if (month()==3)
          {
#ifdef DEBUG            
      Serial.println("It's March, we're NOT in DST");
#endif          
      return false;  
          }
        else
          {
#ifdef DEBUG            
      Serial.println("It's October, we're in DST");
#endif          
      return true;
          }
        }
      }
    else
      // it's not sunday or sunday has to come
      // If March, no DST, if October, we're still in DST
      {
#ifdef DEBUG        
    Serial.println("Last Sunday has to come");
#endif      
    if (month()==3)
        {
#ifdef DEBUG          
    Serial.println("It's March: we're NOT in DST");
#endif        
    return false;  
        }
      else
        {
#ifdef DEBUG          
    Serial.println("It's October: we're in DST");
#endif        
    return true;  
        }
      }
    } // this month is 3 or 10
  // in all other cases there is no DST (Month is Jan,Feb,Nov,Dec)
#ifdef DEBUG    
  Serial.println("Month is Jan, Feb, Nov or Dec: we're NOT in DST");
#endif  
  return false;
  }
