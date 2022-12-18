#define DEVICE_UPDATE_CHECK_FREQ (60*60*24*7)

//#define DEBUG_MODE 1

/* Automatically determine board and build type.
 * Defined in https://github.com/platformio/platform-espressif8266/blob/develop/boards/ 
 */ 

#ifdef ARDUINO_ESP8266_WEMOS_D1MINI  
    #define DEVICE_MODEL "WMD1"
    #pragma message "Compiling for production WeMos D1 R2 and mini board!"
    #define CS_PIN D8           // Slave Select
    #define RGB_LEDS_PIN D2     // WS2812B LED PIN
#else
    #pragma message "UNSUPPORTED BOARD!!"
#endif

// Important Debug Class - Redirect Output for Development Purposes
#include "TickerDebug.hpp"
#include "TickerConfigStructs.hpp"  /* Can only include after the global variables have been declared.
                                     because we unfortuantely need to adjust these variables in the functions within.. */
#include <map>
#include <list>

/*------------------------------ LITTLEFS LIBRARIES ------------------------------*/
#include <FS.h>
#include <LittleFS.h>

FS* fileSystem      = &LittleFS;
bool lfs_OK = false;


/*------------------------------ ARDUINO LIBRARIES ------------------------------*/
#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>             // Library for handle teh EEPROM
#include <ESP8266WiFi.h>        // ESP library for all WiFi functions
#include <ESP8266WebServer.h>   // used in AP mode (config mode)
#include <AutoConnect.h>
#include <ESP8266httpUpdate.h>  // For HTTP based remote firmware update
#include <ESP8266HTTPClient.h>  // For proxy connections instead of using WiFiclient directly
#include <TimeLib.h>            // For Time Management //http://www.arduino.cc/playground/Code/Time - https://github.com/PaulStoffregen/Time - Known issue with ESP8266 -> http://forum.arduino.cc/index.php?topic=96891.30

#include "JsonProcessor.hpp"

/*--------------------------- NETWORK CONFIGURATION ------------------------------*/
#define MDNS_LOCAL_PREFIX "ticker" // this will result in "ticker.local" as the mDNS 
#define FEED_UPDATE_FREQUENCY_MS (2 * 60 * 60 * 1000)

/*----------------------------- TOP LED CONFIG -----------------------------------*/
#define FASTLED_ESP8266_RAW_PIN_ORDER // need to define this before include per: https://github.com/FastLED/FastLED/wiki/ESP8266-notes
//#include <FastLED.h>

#define NUM_LEDS    3
#define BRIGHTNESS  64
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define FRAMES_PER_SECOND  30
//CRGB    leds[NUM_LEDS];

/*--------------------------  FRONT MATRIX DEFINES -------------------------------*/
#include <MD_MAX72xx.h>         // Library to control the Maxim MAX7219 chip on the dot matrix module   https://github.com/MajicDesigns/MD_MAX72XX
#include <MD_Parola.h>          // Parola library to scroll and display text on the display (needs MD_MAX72xx library)  https://github.com/MajicDesigns/MD_Parola

#define   MAX_DEVICES         8                   
#define   HARDWARE_TYPE       MD_MAX72XX::FC16_HW
#define   NUM_ZONES           2   // Number of Parola Zones

#define   ZONE_RIGHT 0             // Most right zone
#define   ZONE_LEFT 1             // Most left zone

#define   ESP8266LED_PIN      D4 // Inbuilt LED
#define   MAX_TICKER_LENGTH   256 // maximum length of the crypto ticker string

// Scroll speeds based on TickerConfig balues
#define SCROLL_SPEED_MS_DELAY_SLOW 50
#define SCROLL_SPEED_MS_DELAY_NORMAL 30
#define SCROLL_SPEED_MS_DELAY_FAST 20
#define SCROLL_SPEED_MS_DELAY_INSANE 8


/*---------------------------- Class Instances -------------------------------*/
ESP8266WebServer    webServer(80);
AutoConnect         Portal(webServer);
AutoConnectConfig   PortalConfig;
WiFiClient          client;
HTTPClient          http; 
MD_Parola           Parola = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES); 

/*--------------------------- DISPLAY STATES -----------------------------*/
// New displaystate container
struct displayState
{
    time_t last_run;
    int    frequency;
    int    scroll_speed;
    char   c_name[16];

    displayState(time_t lr, int f, int s, const char * n) : last_run(lr), frequency(f), scroll_speed(s) { strncpy(c_name, n, 15); }
};

enum  displayStates {UNKNOWN, BLACKOUT, S_TIME, S_DATE, S_WEATHER_C, S_WEATHER_F, S_NEWS, S_CRYPTO, S_STOCK, S_COUNTDOWN, S_MESSAGE, S_NO_WIFI};
displayStates currentDisplayState = UNKNOWN;
displayStates previousDisplayState = UNKNOWN;
bool  displayStateCompleted       = true;  // for multi-loop display
time_t displayStateStartTS = 0;
int  displayStateActivityStep    = 0;  // if the displayState has multiple parts
unsigned int  tmp_counter = 0;

std::map<displayStates, displayState> userDisplayStates;
std::map<displayStates, displayState>::iterator uds_it;

// For Data Structure Display using Parola
std::list<std::string>::iterator string_list_itr;
std::list<TickerInstance>::iterator crypto_list_itr;

/*--------------------------- GLOBAL VARIABLES -----------------------------*/
// System and Ticker Configuration
SystemConfig systemConfig;
TickerConfig tickerConfig;

// Global IOT Ticker Host and Path
char  global_endpoint_host[65]= {'\0'};
char  global_endpoint_path[65]= {'\0'};

bool  first_setup = false;
bool  internet_up = true; // can we connect to the internet?

// Current Custom User Message
CustomMessage  customMessage;

// Define global variables and const
time_t        gmt_offset    		      = 0;
bool          reload_required         = true; // need to do this on first boot
int           parola_display_speed 	  = 0;
int           parola_display_pause 	  = 0;
int           previous_hour           = 0;

// Char Buffers
char          parolaBuffer[MAX_TICKER_LENGTH] = {0};        // Generic Char Buffer for the Parola

// For main loop event scheduling / duration calcuation
unsigned long current_millisecond                 = 0;
unsigned long previous_update_millisecond                = 0;
//unsigned long leds_last_changed_millisecond       = 0; // leds (no longer operated)
unsigned long last_brightness_change_millisecond  = 0; // matrix display

// Light Sensor (analogue read) - Moving average
const int adc_average_length    = 6; // 6 readings, over about sis seconds.
int       adc_readings[adc_average_length]; // all are zero as a start
int       adc_current_position  = 0;
long      adc_current_sum = 0;
long      adc_last_sampled_millisecond = 0;
int       adc_sample_size = sizeof(adc_readings) / sizeof(int);
int       adc_maverage    = 0;
bool      sprint_adc = false; // print to serial


// FOR AUTO ON AND OFF - Automatic determination of 'darkness' or light
const int       adc_darkness_threshold_minimum    = 25; // What is the absolute minimum ADC / Light value that we consider 'darkness' regardless of our 24hr calculated amount.
const int       adc_darkness_led_noise_adjustment = 15; // How much we add to the ADC value to compensate for our own light?
unsigned long   adc_darkness_threshold_maverage_age_millisecond = 0;
int             temp_adc_lowest_maverage = 2000;     // Start off higher than MAX ADC return value
int             adc_darkness_threshold_maverage = 0; // lowest


/*------------------------- Custom Includes ----------------------------------*/
#include "LittleFSBrowser.hpp"        // Need to include this after the WebServer has been declared
#include "TickerHTTPHandlers.hpp"   // EEPROM and HTTPServer Configuration Handlers
//#include "CustomFastLED.h"    // custom gradient definition
#include "CustomParola.hpp"
#include "TickerSerialRead.hpp" // Custom actions 
#include "UtilFunctions.hpp"



/*---------------------------------- FUNCTION DEFS ----------------------------------*/
bool getAllFeedData();
bool getWeatherForecastData();
bool getWeatherCurrentData();
bool is_valid_symbol (char *symbol);

bool getCryptoData();
bool getStonksData();
bool getNewsFeedData();
bool getTimeFromServer();

bool get_json_and_parse_v3(JsonProcessor &parser, const String& action_str, const String& params_str);

void getFormattedDateToCharBuffer(char * parolaBuffer);
void getFormattedTimeToCharBuffer(char * parolaBuffer);

void checkForFirmwareUpdate();
void performFilesystemUpdate();



/*---------------------------------- SETUP -----------------------------------*/
void displayActivate()
{
 // Setup and flush Dot Matrix Display immediately 
  Parola.begin(NUM_ZONES);
  Parola.displayClear();
  Parola.displaySuspend(false);
  Parola.setInvert(false);

  /* Zones start from RIGHT to LEFT so Zone 0 is actually the most right zone.
   * |------ Zone LEFT (1)----- Zone RIGHT (0) ------|
   * 
   * We don't use Zone 0 yet */
  setDefaultZoneSizes();

  // ADC Reading
  int adc_ldr = analogRead(A0);
  //Sprint(F("Initial ADC Reading: ")); SprintlnDEC(adc_ldr, DEC);
  int intensity = (adc_ldr > 1000) ? 8:0;

  // Override
  if(tickerConfig.matrix_brightness_mode == BRIGHTNESS_MODE_MIN) {
      intensity = 0;
  } else if (tickerConfig.matrix_brightness_mode == BRIGHTNESS_MODE_MAX) {
      intensity = 15;
  }
  
  // Have the define the zones above before you can set the intensity...
  Parola.setIntensity(ZONE_RIGHT, intensity);  // Set to minimum on boot 
  Parola.setIntensity(ZONE_LEFT, intensity);  // Set to minimum on boot     

}


// This gets called if captive portal is required.
bool atDetect(IPAddress& softapIP) 
{
  first_setup = true;

  Sprintln(F(" * No WiFi configuration found. Starting Portal."));

  // 'RetroTicker' AP will be availabe on your mobile phone, use the password 12345678 to connect
  // and then configure the proper WiFi accesspoint for the device to connect to.
  //Serial.println("Captive portal started, SoftAP IP:" + softapIP.toString());
  Sprintln(F("Captive Portal started."));
  printTextOrScrollLeft("************ No WiFi! Please connect to 'RetroTicker' on your phone to configure. ************", false, 25);  
  printTextOrScrollLeft("Configure WiFi", true);  
  return true;
}

void setup()
{ 
  delay(200);
  // Setup the Serial
  Serial.begin(115200, SERIAL_8N1); // The default is 8 data bits, no parity, one stop bit. 

  // Setup Parola
  displayActivate();
  showStartBranding(); // CustomParola.h  
  setDefaultZoneSizes();    

  /************** CORE SETUP **************/
  Sprintln(F(" * Allocating / Loading EEPROM"));
  EEPROM.begin(EEPROM_BYTES_RESERVE); // reserve memory for EEPROM
  delay(50);

  // Get the current Ticker Configuration from EEPROMGet, and configure if it's garbage
  // https://www.arduino.cc/en/Tutorial/EEPROMGet
  EEPROM_SystemConfig_Start();
  EEPROM_TickerConfig_Start();

    // Close EEPROM, release some memory, config contained within structures
  Sprintln(F(" * Releasing EEPROM"));
  EEPROM.end();
  delay(50);

/*
  // Setup LEDs
  FastLED.addLeds<LED_TYPE, RGB_LEDS_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(  BRIGHTNESS );
  fill_solid(leds, NUM_LEDS, CRGB::Black); // set to black
  FastLED.show();
*/

  /*-------------------- INIT LITTLE FS --------------------*/
  lfs_OK = fileSystem->begin(); 
  delay(5);
  if (!fileSystem->check())
  {
    Serial.println(F("************* FILESYSTEM ERROR ***************"));
  }

  /*-------------------- START THE NETWORKING --------------------*/
  Sprintln(F(" * Starting WiFi Connection Manager"));

  PortalConfig.apid = "RetroTicker";
  PortalConfig.title = "Configure WiFi";
  PortalConfig.menuItems = AC_MENUITEM_CONFIGNEW;

  Portal.config(PortalConfig);

  // Starts user web site included the AutoConnect portal.
  Portal.onDetect(atDetect);
  if (Portal.begin()) {
    //Serial.println("Started, IP:" + WiFi.localIP().toString());
  }
  else {
    
    Sprintln(F("Something went wrong?"));
    while (true) { 
      #if defined(ESP8266)          
              yield(); // keep the watchdog fed. Removed: NOT relevant to ESP32
      #endif         
      } // Wait.
  }

  // Got here after using the portal. Lets flush memory by resetting.
  if (first_setup) {
    ESP.restart();
  }

  /* Once the WiFi network is up and running, and the HTTP server is going the next most important thing is to get the time!
   * If this fails then we can assume we can't seem to connect to the internet for some reason. If we can't connect
   * to the internet then we halt all further progress... unless we have a REAL TIME ClOCK!
   */

  // We boot the webserver incase we need t reset the config or change the IoT endpoint!
  // HTTP Handler - Default Page
  Sprintln(F(" * Starting Webserver"));
    webServer.on("/", []() {
    if (!handleFileRead("/index.html"))                  // send it if it exists
      webServer.send(404, "text/plain", "404: Not Found"); // otherwise, respond with a 404 (Not Found) error, which shouldn't happen!
  });

  //webServer.on("/message",        HTTPMessageHandler);  // If valid login, then show, config
  webServer.on(F("/message_submit"),   HTTPMessageSubmitHandler);  // If valid login, then show, config
  webServer.on(F("/config.json"),      HTTPGetConfigJSONHandler);  // Get the current configuration
  webServer.on(F("/config_submit"),    HTTPConfigSubmitHandler);   // Save config
  webServer.on(F("/reset"),            HTTPConfigResetHandler);    // Flush or reset all configuation?
  webServer.on(F("/reset_submit"),     HTTPConfigResetSubmitHandler);
  webServer.on(F("/advanced"),         HTTPConfigAdvancedHandler);    // Flush or reset all configuation?
  //webServer.on(F("/advanced_submit"),  HTTPConfigAdvancedSubmitHandler);
  webServer.on(F("/update"),           HTTPUpdateHandler);

  // Final webserver catch-all
  webServer.onNotFound([]() {                              // If the client requests any URI
    if (!handleFileRead(webServer.uri()))                  // send it if it exists (such as bootstrap.min.js)
      webServer.send(404, "text/plain", "404: Not Found"); // otherwise, respond with a 404 (Not Found) error
  });

  webServer.begin();

  Sprintln(F(" * Getting Time From Internet Endpoint "));
  while (!getTimeFromServer() )  // Stay in the loop until we get internet connection.
  { // while this is false        
        internet_up = false;  
        Parola.displayZoneText(ZONE_LEFT, "Could not connect to the Internet. Please check your Internet connection.", PA_LEFT, SCROLL_SPEED_MS_DELAY_SLOW, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);                     

        while ( !allZoneAnimationsCompleted() )
        {
              Parola.displayAnimate();
              webServer.handleClient();       // Handle any requests as they come.
              handleSerialRead(); // defined in TickerSerialRead.h

              #if defined(ESP8266)          
                        yield(); // keep the watchdog fed. Removed: NOT relevant to ESP32
              #endif   
        }        
  }
  internet_up = true;
  
  
  // Display configuration
  userDisplayStates.insert({S_TIME,       displayState(0,TICKER_CONTENT_FREQ_LOOP,-1,"TIME")});  // always show
  userDisplayStates.insert({S_DATE,       displayState(0,tickerConfig.ticker_content_freq_date,-1,"DATE")});
  userDisplayStates.insert({S_WEATHER_C,  displayState(0,tickerConfig.ticker_content_freq_weather,-1,"WEATHER")});
  userDisplayStates.insert({S_NEWS,       displayState(0,tickerConfig.ticker_content_freq_news,-1,"NEWS")});
  userDisplayStates.insert({S_CRYPTO,     displayState(0,tickerConfig.ticker_content_freq_crypto,-1,"CRYPTO")});
  userDisplayStates.insert({S_STOCK,      displayState(0,tickerConfig.ticker_content_freq_stock,-1,"STOCKS")});
  userDisplayStates.insert({S_COUNTDOWN,  displayState(0,tickerConfig.ticker_content_freq_countdown,-1,"COUNTDOWN")}); 

  Sprintln(F("Setup completed!"));
  show_starting_greeting();

  Parola.displayClear();
  sprintf_P(parolaBuffer, "Access this device on your local network by going to: http://%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  printTextOrScrollLeft(parolaBuffer, false, 25);  
  printTextOrScrollLeft("Loading...");

} // end Setup


/*---------------------------- DETERMINE DISPLAY STATE ---------------------------*/
void determineNextDisplayState()
{
        time_t current_timestamp = now();

        previousDisplayState = currentDisplayState; // record this

        // https://stackoverflow.com/questions/26281979/c-loop-through-map
        uds_it = userDisplayStates.find(currentDisplayState);

        if ( uds_it == userDisplayStates.end() ) { // not a user display state
          //Sprintln(F("Got some bullshit display state. Moving to time."));
          currentDisplayState = S_TIME; // Go to time
          return;
        }

        // Update lastrun of current user display state
        //uds_it = userDisplayStates.find(currentDisplayState);
        uds_it->second.last_run = current_timestamp;

        unsigned int counter = 0;
        while (true)
        {
            // Increment to next displaystate, but also check if we're at the end now
            if (++uds_it == userDisplayStates.end()) {
                uds_it = userDisplayStates.begin(); // got to the start again
            }

            //Sprintln(F("Calculating the next suitable display state..."));

            // While Statement
            if ( uds_it->second.frequency == TICKER_CONTENT_FREQ_LOOP) {
                 Sprintln(F("TICKER_CONTENT_FREQ_LOOP"));
                Sprintln(uds_it->second.c_name);
                currentDisplayState = uds_it->first;
                break;
            } else if  ( 
                (uds_it->second.frequency == TICKER_CONTENT_FREQ_HOURLY) && ( abs(uds_it->second.last_run - current_timestamp) > 3600)
            ) {
                 Sprintln(F("TICKER_CONTENT_FREQ_HOURLY"));
                Sprintln(uds_it->second.c_name);
              currentDisplayState = uds_it->first;
              break;
            } else if  ( (uds_it->second.frequency == TICKER_CONTENT_FREQ_RANDOM) && ( (rand() % 10) == 1 ) ) {
               Sprintln(F("TICKER_CONTENT_FREQ_RANDOM"));
              Sprintln(uds_it->second.c_name);
              currentDisplayState = uds_it->first;
              break;
            } 
            else { } // never

       

            if (counter++ > userDisplayStates.size()) {  // Tried to find something but failed miserably
              currentDisplayState = UNKNOWN;
               Sprintln(F("Couldn't find a suitable next display state. Is the config stuffed?"));
              break; 
            }

        } // end selection loop

} // determine next display state

/* 
 * Function to display and iterate through a list of string for each. Parola
 * itertion loop. Pass by reference to avoid copy.
 * https://arstechnica.com/civis/viewtopic.php?t=722588
 */
void displayStringList(std::list<std::string> &string_list)
{
  
          if (string_list.size() == 0) {
           Sprintln(F("No items in this list... breaking."));
          return;
        }
  
        if (displayStateCompleted) { // we've come in from some other displayState having completed
          string_list_itr = string_list.begin(); 
          displayStateCompleted = false;
          tmp_counter = 0;
        }

        Parola.displayZoneText(ZONE_LEFT, string_list_itr->c_str(), PA_RIGHT, parola_display_speed, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);   

        if (++string_list_itr == string_list.end()) // iterate to next forecast.. but check that there isn't a next one
            displayStateCompleted = true;      

        tmp_counter++;

} // end generic display list function


/*---------------------------------- MAIN LOOP ----------------------------------*/
void loop()
{
    time_t current_timestamp  = now();  

    current_millisecond       = millis(); // Note: Only provides system uptime in milliseconds.
    webServer.handleClient();       // Handle any requests as they come.

    Portal.handleRequest();   // Need to handle AutoConnect menu.
    if (WiFi.status() == WL_IDLE_STATUS) {
    #if defined(ARDUINO_ARCH_ESP8266)
        ESP.reset();
    #elif defined(ARDUINO_ARCH_ESP32)
        ESP.restart();
    #endif    
      delay(1000);
    }

    handleSerialRead(); // defined in TickerSerialRead.h

    // Do do an analogue read every second, check to see brightness and calculate moving average
    if ( ((unsigned long)(current_millisecond - adc_last_sampled_millisecond) >= (1000)) )
    {
      // Sprint("It's time to do an ADC sample: ");
      int current_adc_reading  = analogRead(A0);
      adc_maverage             = movingAvg(adc_readings, &adc_current_sum, adc_current_position, adc_sample_size, current_adc_reading);
      adc_current_position++;

      // We've filled the array, start again
      if (adc_current_position >= adc_sample_size)
      {
          adc_current_position = 0;

          // Do this check every time the moving average array has looped (i.e. full data set), also check that
          // we have been taking ADC samples for > 10 seconds, then start logging
          if ( current_millisecond > 20000) {
            temp_adc_lowest_maverage = min(temp_adc_lowest_maverage, adc_maverage); // what's lower, current adc, or historical in past 24 hours?
          }

          // It has been 24 hours (in millisecnds)
          if ( (unsigned long)(current_millisecond - adc_darkness_threshold_maverage_age_millisecond) > (86400000)) {
            adc_darkness_threshold_maverage                 = temp_adc_lowest_maverage*1.05; // add five percent for the hell of it.
            adc_darkness_threshold_maverage_age_millisecond = current_millisecond; // Set the current milisecond
            temp_adc_lowest_maverage                        = 2000; // reset minimum to above maximum ADC return value         
          }
      }

      // Change the intensity, but only every thirty seconds or so, and if the moving average permits.
      if ((unsigned long)(current_millisecond - last_brightness_change_millisecond) > (30 * 1000) ) { // don't keep flip flopping
        if (tickerConfig.matrix_brightness_mode == BRIGHTNESS_MODE_ADAPTIVE) {
            if (adc_maverage < 400) {  Parola.setIntensity(0); } 
            else if (adc_maverage < 700) {  Parola.setIntensity(1); } 
            else if (adc_maverage > 1000) { Parola.setIntensity(12); }
            else { Parola.setIntensity(3); }
                 
          last_brightness_change_millisecond = current_millisecond;
        } // we are using adaptive brightness
      } // last brightness change / flipflop check (minimum every 30 seconds)

      if (sprint_adc)
      {
        Serial.printf_P("ADC Mavg: %d, Spot: %d\n", adc_maverage, current_adc_reading);
        //Sprint("ADC Mavg: "); SprintlnDEC(adc_maverage, DEC);
      }
      adc_last_sampled_millisecond = current_millisecond;
    } // end ADC value check / check every second



    // If we're still animating, then no action required.
    if ( !allZoneAnimationsCompleted() ) 
    {
      Parola.displayAnimate();
      return;
    }


    // We have finised the current display state (because some last more that one loop of parola.)
    if (displayStateCompleted) 
    {
      //Sprintln(F("Display State completed: So determining next displayState"));
      determineNextDisplayState();

      // Do we need to get new data?
      if (  ( (current_millisecond - previous_update_millisecond) >= FEED_UPDATE_FREQUENCY_MS ) || reload_required) 
      { 
           Sprintln(F("Performing an update of feed data..."));         

          // Do we still have internet?               
          if (WiFi.status() != WL_CONNECTED) { 
                internet_up = false; 
          } else {
                /* If this completes we assume the internet is up when in reality the user could
                 * have chosen to show no content other than the TIME!
                 */
                internet_up = getAllFeedData(); 
          }

          if (internet_up ) { // Internet is up
                previous_update_millisecond = current_millisecond;            
                reload_required = false;
          } else { // Internet is down, try again in 15 minutes
              previous_update_millisecond += 60*1000*15; // add ten minutes
               Sprintln(F("No Interenet, will try again in 15 minutes."));
          }
        
      } // end feed update check

    } // end displayState completed check


    //*************************** WAKE / SLEEP **************************/   
    // These will override and set the state to BLACKOUT
    if (tickerConfig.wake_sleep_mode == SLEEP_WAKE_MODE_LIGHT) {
        if ( (adc_maverage < adc_darkness_threshold_minimum ) || (adc_maverage < adc_darkness_threshold_maverage) ) {
          currentDisplayState = BLACKOUT;
          //Sprintln(F("We are BELOW low light threshold. Turning OFF."));
        } else if (currentDisplayState == BLACKOUT) {  // We're above threshold and off
          currentDisplayState = S_TIME; // Start with the time
          //Sprintln(F("We are ABOVE low light threshold. Turning ON."));          
        }
    } // end sleep wake mode - light

    if (tickerConfig.wake_sleep_mode == SLEEP_WAKE_MODE_TIME) {
        bool _result = false;
        // Are we after the wakeup hour or minute
        if (  hour() > int(tickerConfig.wakeup_hour) ) {  _result = true; }
        if ( (hour() == int(tickerConfig.wakeup_hour)) && (minute() >= int(tickerConfig.wakeup_minute)  ) ) { _result = true; }

        // but after we after the sleep time?
        if (  hour() > int(tickerConfig.sleep_hour) ) {  _result = false; }
        if ( (hour() == int(tickerConfig.sleep_hour)) && (minute() >= int(tickerConfig.sleep_minute)  ) ) { _result = false; }

        if ( _result == false ) { // go to sleep
            currentDisplayState = BLACKOUT;
          Sprintln(F("We are outside our operating time. Turning off."));            
        } else if (currentDisplayState == BLACKOUT) {  // We're above threshold and off
          currentDisplayState = S_TIME; // Start with the time
          Sprintln(F("We are inside our operating time. Turning on."));                      
        }
    } // end sleep wake mode  - time  

    if (tickerConfig.wake_sleep_mode == SLEEP_WAKE_WEEKDAY_WEEKEND) {
        if (  ( weekday() == 1 || weekday() == 7) ) { // Weekend Day
          if (hour() > 9 && hour() < 22) { // between 9am and 10pm
            if (currentDisplayState == BLACKOUT) {  currentDisplayState = S_TIME;
                Sprintln(F("It's a weekend and inside operating hours. Turning ON."));                
             }
          } else { currentDisplayState = BLACKOUT; 
                 Sprintln(F("It's a weekend and outside operating hours. Turning OFF."));                
          } // outside weekend hours
        } // end weekend check

        if ( weekday() > 1 && weekday() < 6 ) { // it's monday to friday
          if ( (hour() > 7 && hour() < 11) || ( hour() > 17 && hour() < 20) ) { // between work hours
            if (currentDisplayState == BLACKOUT) {  currentDisplayState = S_TIME; 
                Sprintln(F("It's a weekday and inside operating hours. Turning ON."));                
            }
          } else { currentDisplayState = BLACKOUT;                 
                   Sprintln(F("It's a weekday and outside operating hours. Turning OFF.")); 
          } // outside working hours
        } // end monday to friday check
    } // end sleep wake mode  - working week  

    // SO what do we do? We shouldn't have an active display
    if ( currentDisplayState == BLACKOUT)
           Parola.displayClear();


    //************************** CUSTOM MESSAGE ***************************/
    if ( customMessage.message_length != 0 ) { // we have a message
      if ( customMessage.message_last_displayed == 0 ) { // show immediately

          customMessage.message_last_displayed = current_timestamp;          
           Sprintln(F("Display message immediately."));
          currentDisplayState = S_MESSAGE;           
      } else if ( customMessage.message_display_freq > 0 ) { // display more than once?

        if ( (current_timestamp - customMessage.message_last_displayed) > customMessage.message_display_freq ) 
          if ( ( current_timestamp < (customMessage.message_timestamp + customMessage.message_expiry ) )
               || customMessage.message_expiry == 0 ) { // but has it NOT expired yet?

               Sprintln(F("Message hasn't expired yet, and we're within the interval. Displaying."));
              // Update the last displayed
              customMessage.message_last_displayed = current_timestamp;
              currentDisplayState = S_MESSAGE;              
          } // check expiry
        } // check frequency
    } // check if a message exists


    //*************************** PAROLA STUFF ***************************/

    // Configure the Ticker Scroll Speed
    switch(tickerConfig.scroll_speed) {
      case TICKER_SCROLL_SPEED_SLOW:
        parola_display_speed = SCROLL_SPEED_MS_DELAY_SLOW;
        parola_display_pause = 30 * 1000; 
        break;
      
      case TICKER_SCROLL_SPEED_NORMAL:
        parola_display_speed = SCROLL_SPEED_MS_DELAY_NORMAL;
        parola_display_pause = 15 * 1000; 
        break;

      case TICKER_SCROLL_SPEED_FAST:
        parola_display_speed = SCROLL_SPEED_MS_DELAY_FAST;
        parola_display_pause = 5 * 1000;      
        break;

      case TICKER_SCROLL_SPEED_INSANE:
        parola_display_speed = SCROLL_SPEED_MS_DELAY_INSANE;
        parola_display_pause = 1000;      
        break;        
    }

    //*************************** DISPLAY STATES ***************************/
    switch (currentDisplayState)
    {
      case S_TIME: // Current Time
      {
        Sprintln(F("Showing Time."));      
        getFormattedTimeToCharBuffer(parolaBuffer);

        if ( ((previousDisplayState == currentDisplayState) && (previous_hour != hour())) || ( (rand() % 100) == 42)  ) // show a animation
        { // Borrowed from Parola_Sprites_Simple
            Parola.displayZoneText(ZONE_LEFT, parolaBuffer, PA_CENTER, parola_display_speed, 1000, PA_PRINT, PA_SPRITE);

            if ((rand() % 100) > 35 ) {
              Parola.setSpriteData(pacman1, W_PMAN1, F_PMAN1, pacman1, W_PMAN1, F_PMAN1);                      
            } else {
              Parola.setSpriteData(rocket, W_ROCKET, F_ROCKET, rocket, W_ROCKET, F_ROCKET);             
            }

        } else {
          Parola.displayZoneText(ZONE_LEFT, parolaBuffer, PA_CENTER, parola_display_speed, parola_display_pause, PA_PRINT); 
        }

        previous_hour = hour();
        displayStateCompleted = true;    
      }   
        break;     

      case S_DATE: // Current Date
      {
        Sprintln(F("Showing Date."));
        getFormattedDateToCharBuffer(parolaBuffer);
        Parola.displayZoneText(ZONE_LEFT, parolaBuffer, PA_CENTER, parola_display_speed, parola_display_pause, PA_PRINT);
        displayStateCompleted = true;
      }
        break;        

      case S_WEATHER_C: // Current Weather
      {
        Sprintln(F("Showing Weather."));   
        Parola.displayClear(ZONE_LEFT);   
        Parola.displayZoneText(ZONE_LEFT, CurrentWeather_str.c_str(), PA_RIGHT, parola_display_speed, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);              
        displayStateCompleted = true;
      }
        break;

      case S_WEATHER_F: // Forecast Weather
      {
        if (WeatherForecasts_str.size() == 0) {
           Sprintln(F("No Weather... breaking."));
          break;
        }
        displayStringList(WeatherForecasts_str);
      }
        break;

      case S_NEWS:
      {
        if (NewsHeadlines_str.size() == 0) {
           Sprintln(F("No News... breaking."));
          break;
        }

        if ( tmp_counter > tickerConfig.news_limit) {
           Sprintln(F("Gone past the news we care about..."));
          displayStateCompleted = true;
          break;
        }

        // else show news
        displayStringList(NewsHeadlines_str);
      }
        break;

      case S_CRYPTO:
      { 
        // https://stackoverflow.com/questions/11578936/getting-a-bunch-of-crosses-initialization-error
        if (CryptoTicker_str.size() == 0) {
           Sprintln(F("No Crypto... breaking."));
          break;
        } 

        if (displayStateCompleted == true)
        {
          displayStateActivityStep = 0; // sub actions
          crypto_list_itr = CryptoTickers.begin(); 
          displayStateCompleted = false;

          Parola.setZone(ZONE_RIGHT, 0, 0);               // Zone 0 we don't use just yet...
          Parola.setZone(ZONE_LEFT, 1, MAX_DEVICES-1);   
       
        }

          Sprint(F("Showing crypto: ")); Sprintln(crypto_list_itr->name);
          Sprint(F("displayStateActivityStep: ")); SprintlnDEC(displayStateActivityStep, DEC);

          textEffect_t text_effect;

          // Price go up or down?
          text_effect = (crypto_list_itr->percent_change_24h > 0) ? PA_SCROLL_UP:PA_SCROLL_DOWN;          

            switch(displayStateActivityStep)
            {
              case 0:
                Parola.displayZoneText(ZONE_LEFT, crypto_list_itr->name, PA_CENTER, 0, 2000, PA_RANDOM, PA_NO_EFFECT);  // Name                   
                Parola.displayClear(ZONE_RIGHT); // clear the new zone
                displayStateActivityStep++;
                break;

              case 1:
                Parola.displayZoneText(ZONE_RIGHT, crypto_list_itr->parola_upordown_code, PA_CENTER, parola_display_speed, 2000, text_effect, PA_NO_EFFECT);  // Arrow
                displayStateActivityStep++;
                break;

              case 2:
                snprintf_P(parolaBuffer, sizeof(parolaBuffer), "%s%0.2lf", crypto_list_itr->parola_currency_code, crypto_list_itr->price_reporting_ccy); 
                Parola.displayZoneText(ZONE_LEFT, parolaBuffer, PA_CENTER, parola_display_speed, 2000, text_effect, text_effect);  // Price
                displayStateActivityStep++;
                break;

              case 3:
                snprintf_P(parolaBuffer, sizeof(parolaBuffer), "%0.2lf%%",crypto_list_itr->percent_change_24h); 
                Parola.displayZoneText(ZONE_LEFT, parolaBuffer, PA_CENTER, parola_display_speed, 2000, text_effect, text_effect);  // Price
                displayStateActivityStep++;
                break;

              default:
                if (++crypto_list_itr == CryptoTickers.end()) { // iterate to next forecast.. but check that there isn't a next one
                    displayStateCompleted = true;   
                    setDefaultZoneSizes(); // back to normal
                    Parola.displayClear();
                }                

                  displayStateActivityStep = 0;
                  Sprintln("Setting to zero...");
                break;
            }

          // Display old school strings
          //displayStringList(CryptoTicker_str);
       }
        break;       


      case S_COUNTDOWN:
      { // https://stackoverflow.com/questions/11578936/getting-a-bunch-of-crosses-initialization-error
        // Not ideal to in
        if ( strlen(tickerConfig.countdown_name) < 3 || (tickerConfig.countdown_datetime < current_timestamp) ) {
           Sprintln(F("No valid countdown... breaking."));
          break;
        } 

        if (displayStateCompleted == true)
        {
          displayStateActivityStep = 0; // sub actions
          displayStateCompleted = false;
          displayStateStartTS = current_timestamp;
        }

        // Difference from now.
        int difference = tickerConfig.countdown_datetime - current_timestamp;

        // From DateTime.h
        int days = elapsedDays(difference);
        int hours = numberOfHours(difference);
        int minutes = numberOfMinutes(difference);
        int seconds = numberOfSeconds(difference);   

        if (displayStateActivityStep == 0)
        {
             displayStateStartTS = current_timestamp;          
            snprintf_P(parolaBuffer, sizeof(parolaBuffer), "Countdown to %s \x10 \x10", tickerConfig.countdown_name);
            printTextOrScrollLeft(parolaBuffer, false, parola_display_speed);  
            displayStateActivityStep++; // only do this once
        }
        else
        {
          if (days > 0) {
            snprintf_P(parolaBuffer, sizeof(parolaBuffer), "%dd %02dh %02dm", days, hours, minutes);            
          } else {
            /* code */
            // TODO: https://forum.arduino.cc/index.php?topic=645252.0
            // Fix shift of pixels due to '1' being a smaller character
            snprintf_P(parolaBuffer, sizeof(parolaBuffer), "%02dh %02dm %02ds", hours, minutes, seconds);
          }

            Parola.displayZoneText(ZONE_LEFT, parolaBuffer, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);    
            //printTextOrScrollLeft(parolaBuffer, true);  // print in centre    
            delay(100);
        }
                    //Serial.println( (current_timestamp - displayStateStartTS), DEC)            ;

        if ( (current_timestamp - displayStateStartTS) > (parola_display_pause/1000) )
        {
                    displayStateCompleted = true;   
                    displayStateActivityStep = 0;   
                    break;    
         }

       } // end countdown
      break;       


      case S_STOCK:
      {
        if (EquitiesTicker_str.size() == 0) {
           Sprintln(F("No Stocks... breaking."));
          break;
        }
        displayStringList(EquitiesTicker_str);  
      }      
        break;        

      case S_MESSAGE:
      {
        Sprintln(F("Showing Message."));   

        // message is grater than about 10 characters... will need to scroll
        if (customMessage.message_length > 10) {
          Parola.displayZoneText(ZONE_LEFT, customMessage.message, PA_LEFT, SCROLL_SPEED_MS_DELAY_SLOW, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);              
        } else {
          Parola.displayZoneText(ZONE_LEFT, customMessage.message, PA_CENTER, SCROLL_SPEED_MS_DELAY_SLOW, parola_display_pause, PA_PRINT);                        
        } 
        displayStateCompleted = true;
      }     
        break;

      case S_NO_WIFI:
      {
        printTextOrScrollLeft("No WiFi !", true);
         Sprintln(F("No Wifi !"));
        delay(5000);
      }
        break;

      default:
        break;

    } // end Switch Statement
} // end loop

bool getAllFeedData()
{
  bool success = true;

  if (WiFi.status() != WL_CONNECTED)
  {
    Sprintln (F("Error: getAllFeedData - Not connected to WiFi!"));
    success = false;
  }

  if ( tickerConfig.ticker_content_freq_weather != TICKER_CONTENT_FREQ_NEVER) {
    if (success) success &= getWeatherCurrentData();    
    if (success) success &= getWeatherForecastData();
  } else { Sprintln(F("getAllFeedData(): User configuration skips weather data.")); }

  if ( tickerConfig.ticker_content_freq_crypto != TICKER_CONTENT_FREQ_NEVER) {
    if (success) success &= getCryptoData();
  } else { Sprintln(F("getAllFeedData(): User configuration skips crypto data.")); }

  if ( tickerConfig.ticker_content_freq_news != TICKER_CONTENT_FREQ_NEVER) {
    if (success) success &= getNewsFeedData();
  } else { Sprintln(F("getAllFeedData(): User configuration skips news data.")); }

  if ( tickerConfig.ticker_content_freq_stock != TICKER_CONTENT_FREQ_NEVER) {
    if (success) success &= getStonksData();
  } else { Sprintln(F("getAllFeedData(): User configuration skips stocks data.")); }  

  return success;

} // get all feed data



/******************************* DATA GATHERING FUNCTIONS  *********************************/
// Query to get current weather: http://api.openweathermap.org/data/2.5/weather?id=2643743&appid=700650b72d30a4662143cc010c89f700&units=metric
// We use ArduinoJSON for this as the returned data size is small enough.
bool getWeatherCurrentData() {
   Sprintln(F("Obtaining Current Weather Data..."));
   
   CurrentWeatherProcessor processor;

   processor.set_gmt_offset(gmt_offset);

  if ( 
        !get_json_and_parse_v3( processor,"weather",
                      String("city_id=" + String(tickerConfig.weather_city_id) + "&city_name=" + String(tickerConfig.weather_city_name))
      ))  {  
            Sprintln(F("Failed to request and parse current weather."));
           return false;
   }

   if( strcmp(CurrentWeather.city_id, tickerConfig.weather_city_id) != 0)
   {
      Sprintln(F("Open Weather Maps City ID's don't match. Updating EEPROM"));
     EEPROM_cache_weather_city_id(CurrentWeather.city_id, sizeof(CurrentWeather.city_id));
   }
    return true;
} // get current weather data

// Query to get forecast:  http://api.openweathermap.org/data/2.5/forecast?id=2643743&appid=700650b72d30a4662143cc010c89f700&units=metric
bool getWeatherForecastData() {

  ForecastWeatherProcessor processor;  // Open Weather Map JSON Streaming Parser

  processor.set_gmt_offset(gmt_offset);

  // Populate this into the global httpResponse variable
  if ( !get_json_and_parse_v3(processor, "weather_forecast", String("city_id=" + String(tickerConfig.weather_city_id) + "&city_name=" + String(tickerConfig.weather_city_name))) )  {
         Sprintln(F("Failed to request and parse weather forecast."));
        return false;
  }

  return true;
} // end weather forecast.

// Get the Crypto Data
bool getCryptoData() {
   // Sprintln(F("Using the new Crypto Ticker data gathering function..."));

  // 1. Do we have at least one ticker value and a currency?
  if (  !(is_valid_symbol(tickerConfig.crypto_1) || 
        is_valid_symbol(tickerConfig.crypto_2) ||
        is_valid_symbol(tickerConfig.crypto_3) ||
        is_valid_symbol(tickerConfig.crypto_4) ||
        is_valid_symbol(tickerConfig.crypto_5) ||
        is_valid_symbol(tickerConfig.crypto_6))
    )  {
     Sprintln(F("getCryptoData: No ticker symbols provided."));
    return true;
  }

  // 2. Setup JSON Parser
  
  TickerProcessor processor;
  processor.set_crypto_mode(true); // poplate to crypto

  // 3. Parse
  String http_params; http_params.reserve(128);
  http_params = "currency=USD"; //" + String(tickerConfig.ticker_currency); // by default the iot proxy script will return the top 20 or so.
  http_params += "&symbol=" + 
                    String(tickerConfig.crypto_1) + "," +
                    String(tickerConfig.crypto_2) + "," +
                    String(tickerConfig.crypto_3) + "," +
                    String(tickerConfig.crypto_4) + "," +
                    String(tickerConfig.crypto_5) + "," +                    
                    String(tickerConfig.crypto_6);
  
  // 4. Obtain data.
  if ( !get_json_and_parse_v3(processor, "ticker", http_params) ) {
       Sprintln(F("Failed to get Crpyto Information"));
      return false;
   }

   return true;
} // getCryptoData

// Get the Stocks Data
bool getStonksData() {
  // 1. Do we have at least one ticker value and a currency?
  if (  !(strlen(tickerConfig.stock_1) > 3) )  {
     Sprintln(F("getStocksData: No valid stock symbols provided"));
    return true;
  }

  // 2. Setup JSON Parser
  TickerProcessor processor;
  processor.set_crypto_mode(false); // populate stock std::list

  // 3. Parse
  String http_params; http_params.reserve(128);
  http_params = "symbol=" + 
                    String(tickerConfig.stock_1) + "," +
                    String(tickerConfig.stock_2) + "," +
                    String(tickerConfig.stock_3) + "," +
                    String(tickerConfig.stock_4) + "," +
                    String(tickerConfig.stock_5) + "," +                    
                    String(tickerConfig.stock_6);
  
  // 4. Obtain data.
  if ( !get_json_and_parse_v3(processor, "stock", http_params) ) {
       Sprintln(F("Failed to get Stock Information"));
      return false;
   }

   return true;

} // getStocks

// Get the data from our IoT RSS Proxy
bool getNewsFeedData() {
  
  // We actually have any news to share?
  if ( strlen(tickerConfig.news_codes) < 2) {
     Sprintln(F("No news feeds have been selected"));
    return true;
  }

  NewsProcessor processor;

  if (   !get_json_and_parse_v3(processor, "news", String("feed=" + String(tickerConfig.news_codes)))   )  {
       Sprintln(F("Failed to get news!"));
      return false;
  }

  return true;

} // end news parser

// Bootstrap the device and get the time from remote API server
bool getTimeFromServer() 
{
    TimeProcessor processor;

    time_t unix_timestamp = 0;
    strncpy(global_endpoint_path,   IOT_ENDPOINT_PATH,          sizeof(global_endpoint_path) );

    strncpy(global_endpoint_host,   PRI_IOT_ENDPOINT_HOSTNAME,  sizeof(global_endpoint_host) );

    if (   get_json_and_parse_v3(processor, "time", "") )  
    {
        Sprintln(F("Got time from primary endpoint."));
        processor.update(unix_timestamp, gmt_offset);

        // Set the global time via TimeLib.h, set to GMT
        setTime(unix_timestamp);

        // Set the offset for this TZ
        adjustTime(gmt_offset);

        return true;
    }

    strncpy(global_endpoint_host,   SEC_IOT_ENDPOINT_HOSTNAME,  sizeof(global_endpoint_host) );

    if (   get_json_and_parse_v3(processor, "time", ""))  
    {
        Sprintln(F("Got time from secondary endpoint."));
        processor.update(unix_timestamp, gmt_offset);

        // Set the global time via TimeLib.h, set to GMT
        setTime(unix_timestamp);

        // Set the offset for this TZ
        adjustTime(gmt_offset);

        return true;
    }

    return false;

} // end getTimeFrom Server

// Stream based parser of v3 API feed
bool get_json_and_parse_v3(JsonProcessor &parser, const String& action_str, const String& params_str)
{
    client.flush();

    String url = "http://" + String(global_endpoint_host) +  String(global_endpoint_path) + "?action=" + action_str + "&did=" + String(systemConfig.device_id) + "&" + params_str;
    Sprint(F("> Getting JSON data from URL: ")); Sprintln(url);

    http.setUserAgent("RetroTicker/2.0 (ESP8266 " DEVICE_MODEL ")"); 
    http.setTimeout(8000);
    http.begin(client, url);
    http.useHTTP10(true);        
        // start connection and send HTTP header
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) 
    {    
      // Get a reference to the stream in HTTPClient
      Stream& response = http.getStream();

      // Allocate the JsonDocument in the heap
      DynamicJsonDocument doc(10000);

      // Deserialize the JSON document in the response
      deserializeJson(doc, response);
      
      bool parser_res = parser.process_json_document(doc);

      if (parser_res) {
          Sprintln("Parsed JSON OK.");
      }
      else
      {
        Sprintln("Failed to parse JSON!");
      }

      // Disconnect
      http.end();      
      return parser_res;  

    } else {
      Serial.printf("[HTTP] GET... failed, error: %s \r\n", http.errorToString(httpCode).c_str());
      http.end();  
     return false;
    }
}


/********************************** DATE/TIME HELPER UTILS  *************************************/
// NOTE: Ensure destination buffer at least 12 characters.
void getFormattedTimeToCharBuffer(char * parolaBuffer)
{
  /*
    unsigned long rawTime = now();
    int hours             = (rawTime % 86400L) / 3600;
    int minutes           = (rawTime % 3600)   / 60;
  */
  char am_or_pm[3];

  // Convert to 12 hour time
  if (isAM() == false)
  {
    strcpy(am_or_pm, "PM");
  }
  else
  {
    strcpy(am_or_pm, "AM");
  }

  sprintf((char *)parolaBuffer, "%02d:%02d %s",  hourFormat12(), minute(), am_or_pm );

} // end getFormattedTimeToCharBuffer

// NOTE: Ensure destination buffer at least 12 characters.
void getFormattedDateToCharBuffer(char * parolaBuffer)
{

  // Weird duplication issue when using the Time library character pointers!?
  // Answer: Because these functions return just a pointer to the same buffer, and sprintf
  // runs both then attempts to substitute. Unfortunately only the last operation is then in
  // the buffer, so we need to duplicate.
  // https://forum.arduino.cc/index.php?topic=367763.0

  /*
    Sprintln("The current weekday is: " + String(dayStr(weekday())));
    Sprintln("The current weekday number is: " + String(weekday()));
    Sprintln(dayStr(weekday()));
    Sprintln(day());
    Sprintln(monthShortStr(month()));
  */

  //sprintf((char *)parolaBuffer, "%s %d %s %s",  dayStr(weekday()), day(), monthShortStr(month()));

  char weekday_str[6] = { '\0' };
  strcpy(weekday_str, dayShortStr(weekday()));
  sprintf((char *)parolaBuffer, "%s %d %s",  weekday_str, day(),  monthShortStr(month()) );

} // end getFormattedDateToCharBuffer