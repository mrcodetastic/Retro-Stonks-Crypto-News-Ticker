#ifndef TICKER_CONFIG_H
#define TICKER_CONFIG_H
#include <TimeLib2.hpp>

#define EEPROM_BYTES_RESERVE 2048

/********************************************* CONFIG CONSTANTS **********************************************/

// Config definitions
#define SYSTEM_CONFIG_VER  2            // Change this every time the format of the struct changes
#define USER_CONFIG_VER    2              // Change this every time the format of the struct changes

// User config items
#define BRIGHTNESS_MODE_ADAPTIVE 0
#define BRIGHTNESS_MODE_MIN 1
#define BRIGHTNESS_MODE_MAX 2

#define TICKER_SCROLL_SPEED_NORMAL 0
#define TICKER_SCROLL_SPEED_SLOW 1
#define TICKER_SCROLL_SPEED_FAST 2
#define TICKER_SCROLL_SPEED_INSANE 3

#define TICKER_CONTENT_FREQ_LOOP     0
#define TICKER_CONTENT_FREQ_HOURLY   1
#define TICKER_CONTENT_FREQ_RANDOM   2
#define TICKER_CONTENT_FREQ_NEVER    3

#define LED_MODE_CRYPTO_PULSATE 0
#define LED_MODE_CRYPTO_FIXED 1
#define LED_MODE_CRYPTO_THERMOMETER 2
#define LED_MODE_RANDOM 5

#define SLEEP_WAKE_MODE_LIGHT       0
#define SLEEP_WAKE_MODE_ALWAYS_ON   1
#define SLEEP_WAKE_MODE_TIME        2
#define SLEEP_WAKE_WEEKDAY_WEEKEND 3

#define CUSTOM_MESSAGE_FREQ_RANDOM 0

///#define DEFAULT_DEVICE_PASSWORD     "123456"

// Default Crypto
#define DEFAULT_TICKER_1 "BTC"
#define DEFAULT_TICKER_2 "ETH"
#define DEFAULT_TICKER_3 "LTC"

// Random stonk
#define DEFAULT_STONK_1 "ASX:LKE"

// Default News - Sky World and Coin Desk - confirm this in the index.php file for the IoT Proxy Script
#define DEFAULT_NEWS_CODES "BT" 
#define DEFAULT_NEWS_LIMIT 10


/********************************************* IOT ENDPOINTS ***********************************************/

const char* PRI_IOT_ENDPOINT_HOSTNAME = "tty.us.to";
const char* SEC_IOT_ENDPOINT_HOSTNAME = "tty.us.to";

const char* IOT_ENDPOINT_PATH = "/iot/ticker/";

/********************************************* CONFIG STRUCTS **********************************************/

// How to manage header files
// http://www.cplusplus.com/forum/articles/10627/
// https://stackoverflow.com/questions/228684/how-to-declare-a-structure-in-a-header-that-is-to-be-used-by-multiple-files-in-c

// For EEPROM address writing, memory position, do not change!!
static const int eeprom_addr_SystemConfig = 0;
static const int eeprom_addr_TickerConfig = 128; 



// System config that isn't overwritten constantly
struct SystemConfig // 312 bytes
{
  char      device_id[32];

  time_t    last_firmware_check_time_t;
  bool      filesystem_needs_update; // Is the FS image stale?  

  int config_version;  

};


// https://www.arduino.cc/en/Reference/EEPROMPut
struct TickerConfig  // aprox 480 bytes in size using sizeof
{
  char owner_name[64];   

  char clock_timezone[64];
  char clock_2_timezone[64];
  char clock_3_timezone[64];

  char login_username[9];
  char login_password[9];

  // For Crypto Stuff.
  char crypto_ccy[8];
  //unsigned int ticker_mode; 

  char crypto_1[16];
  char crypto_2[16];  
  char crypto_3[16];  
  char crypto_4[16];  
  char crypto_5[16];  
  char crypto_6[16];  

  // Stock
  char stock_1[16];
  char stock_2[16];  
  char stock_3[16];  
  char stock_4[16];  
  char stock_5[16];  
  char stock_6[16];    

  // News
  char news_codes[128];
  unsigned int  news_limit;

  // Weather
  char weather_city_id  [32]; // irrelevant to the ticker, but used for weather :-)
  char weather_city_name[64];

  // Device Behaviour
  unsigned int wakeup_hour;
  unsigned int wakeup_minute;
  unsigned int sleep_hour;
  unsigned int sleep_minute;

  unsigned int scroll_speed;
  
  unsigned int ticker_content_freq_date;
  unsigned int ticker_content_freq_crypto;  
  unsigned int ticker_content_freq_news;    
  unsigned int ticker_content_freq_weather;      
  unsigned int ticker_content_freq_stock;   

  unsigned int matrix_brightness_mode;
  unsigned int device_awake_weekends;  
  unsigned int wake_sleep_mode;

  // Countdown
  unsigned int ticker_content_freq_countdown;     
  char countdown_name[64];
  unsigned long countdown_datetime;

  int config_version;

};

// In memory struct only, not EEPROM
// Ensure the maximum message size is aligned in ConfigHandlers.h
struct CustomMessage
{
    
    time_t message_timestamp;     
    time_t message_display_freq;   // how often to show in seconds
    time_t message_expiry; // how long for in seconds

    // message_timestamp + message_desplay_expiry = cutoff time

    unsigned int message_length;      
    char message[255];    

    time_t message_last_displayed;    

};


#endif
