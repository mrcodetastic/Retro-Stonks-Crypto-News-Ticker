#include "TickerConfigStructs.hpp"
#include "TickerDebug.hpp"

/* 
 * For Message Management //http://www.arduino.cc/playground/Code/Time - https://github.com/PaulStoffregen/Time - 
 * Known issue with ESP8266 -> http://forum.arduino.cc/index.php?topic=96891.30
 */
#include <TimeLib2.hpp>          

extern void checkForFirmwareUpdate(); // defined in the main .cpp file
extern TimeLib2 clockMain;


/************************************* EEPROM UTILS  ****************************************/
// Flush the while EEPROM section
void EEPROM_clear_all()
{
  Sprintln("EEPROM_clear_all"); // per: http://esp8266.github.io/Arduino/versions/2.0.0/doc/libraries.html
  EEPROM.begin(EEPROM_BYTES_RESERVE); // reserve memory for EEPROM

  for (unsigned int i = 0 ; i < EEPROM.length() ; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
}


void EEPROM_SystemConfig_Start() // function assume EEPROM.begin has been called
{

  // Load System Config into struct
  EEPROM.get(0, systemConfig);  // global

  Sprintln("Device ID: " + String(systemConfig.device_id));

  Sprintln("Performing EEPROM System Configuration Check...");
  if (systemConfig.config_version == SYSTEM_CONFIG_VER) return;
  
  Sprintln("System configuration invalid. Creating.");

  EEPROM_clear_all();

  // Start afresh
  SystemConfig newConfig;   

  // After MUCH PAIN it was discovered that you need to flush out the entire struct 
  // as when uninitialised, there's no control on what you will get.
  // https://www.go4expert.com/forums/default-values-c-struct-fields-t8264/
  newConfig = { 0 };  

  // Set to configured
  newConfig.config_version = SYSTEM_CONFIG_VER;

  // Use system chip ID and store has string of hex
  sprintf( newConfig.device_id, "%X", system_get_chip_id() );

  EEPROM.put(eeprom_addr_SystemConfig, newConfig);
  EEPROM.commit(); // very important

  // Now refresh the current in-memory config
  systemConfig = newConfig;
  
  Sprintln("Generated Device ID: " + String(systemConfig.device_id));               
    

} // end System Config Start


void EEPROM_TickerConfig_Start (bool flush = false) // function assume EEPROM.begin has been called
{
  // Load Ticker Config into struct
  EEPROM.get(eeprom_addr_TickerConfig, tickerConfig);  // global
 
  Sprintln("Performing EEPROM Ticker Configuration Check...");


  if ( (tickerConfig.config_version == USER_CONFIG_VER) && (flush == false) ) return;   

  Sprintln("User configuration invalid. Creating.");


  // Start afresh
  TickerConfig newConfig;   

  // After MUCH PAIN it was discovered that you need to flush out the entire struct 
  // as when uninitialised, there's no control on what you will get.
  // https://www.go4expert.com/forums/default-values-c-struct-fields-t8264/
  newConfig = { 0 };  

  // Set to configured
  newConfig.config_version  = USER_CONFIG_VER;

  strncpy(newConfig.clock_timezone, "Europe/London", sizeof(newConfig.clock_timezone));
  strncpy(newConfig.weather_city_name, "London,UK", sizeof(newConfig.weather_city_name) );

  // Make a fresh copy of the default password
  //strcpy(newConfig.login_password,  DEFAULT_DEVICE_PASSWORD );   

  // Set the default Crypto Tickers and the News Feeds
  // newConfig.ticker_mode      = TICKER_MODE_TOP;    // by default use pull what is the top marketcap
  strcpy(newConfig.crypto_1,  DEFAULT_TICKER_1 );   
  strcpy(newConfig.crypto_2,  DEFAULT_TICKER_2 );       
  strcpy(newConfig.crypto_3,  DEFAULT_TICKER_3 );  

  strcpy(newConfig.stock_1,   DEFAULT_STONK_1 );      

  strcpy(newConfig.news_codes,  DEFAULT_NEWS_CODES ); 
  newConfig.news_limit = DEFAULT_NEWS_LIMIT;

  // Not used :-)
  strcpy(newConfig.login_username, "USERNAME"); 

  // Countdown is disabled by default
  newConfig.ticker_content_freq_countdown = TICKER_CONTENT_FREQ_NEVER;

  EEPROM.put(eeprom_addr_TickerConfig, newConfig);
  EEPROM.commit(); // very important

  // Now refresh the current in-memory config
  tickerConfig = newConfig;
  //EEPROM.get(0, tickerConfig); // Let's hope what's in eeprom is in the right order for the struct.

         

  
} // ticker configuration start


// We only do this ONCE when we've got the city id from Open Weather Maps
// This avoids the ticker querying the IOT proxy web script by City String Name
// which we can't cache with our proxy web script.
void EEPROM_cache_weather_city_id(char *weather_city_id, int size) // assume EEPROM begin has NOT been called
{
     Sprint("We recieved the following CITY ID from Open Weather Maps: ");
     Sprintln(weather_city_id);

     memset(tickerConfig.weather_city_id, 0, sizeof(tickerConfig.weather_city_id));
     strncpy(tickerConfig.weather_city_id, weather_city_id, size);

     Sprint("Updated EEPROM weather city id is: "); Sprintln(tickerConfig.weather_city_id);

     EEPROM.begin(EEPROM_BYTES_RESERVE); // reserve memory for EEPROM
     EEPROM.put(eeprom_addr_TickerConfig, tickerConfig);  // Update the config
     Sprintln("Caching City ID in the EEPROM..");
     EEPROM.end(); // very important    
}

// Assuming EEPROM is pen at the moment
void EEPROM_SystemConfig_UpdateLastFirmwareCheckTime()
{
    /* Note: EEPROM is already opened within setup()  where this function is 
     *       called from.
     */

     // Refer to TimeLib.h - 'now()' returns the current time as seconds since Jan 1 1970 
     systemConfig.last_firmware_check_time_t  = clockMain.getEpochSecond(); 
     //systemConfig.filesystem_needs_update     = false;

     Sprint("Updating last firmware check time to: ");
     SprintlnDEC(systemConfig.last_firmware_check_time_t, DEC);

     EEPROM.begin(EEPROM_BYTES_RESERVE); // reserve memory for EEPROM
     EEPROM.put(eeprom_addr_SystemConfig, systemConfig);
     EEPROM.commit(); // very important   
     EEPROM.end(); // very important     
}

void EEPROM_set_filesystem_needs_update(bool flag)
{
     systemConfig.filesystem_needs_update = flag; //set the flag

     EEPROM.begin(EEPROM_BYTES_RESERVE); // reserve memory for EEPROM
     EEPROM.put(eeprom_addr_SystemConfig, systemConfig);
     EEPROM.commit(); // very important 
     EEPROM.end(); // very important     
}

void EEPROM_set_firmware_needs_update()
{
     systemConfig.last_firmware_check_time_t = 0; //set the flag

     EEPROM.begin(EEPROM_BYTES_RESERVE); // reserve memory for EEPROM
     EEPROM.put(eeprom_addr_SystemConfig, systemConfig);
     EEPROM.commit(); // very important 
     EEPROM.end(); // very important     
}

// For ESP8266 FSupdate, updater callback
void EEPROM_set_filesystem_stale_callback()
{
   EEPROM_set_filesystem_needs_update(true);
}


/**************************************** HTTP HANDLERS ****************************************/
void HTTPGetConfigJSONHandler()
{

    char input_wakeup_time[8];
    char input_sleep_time[8];
    
    // Need to get the formatting right for the Javascript form-autofill code ie 0X_0X
    sprintf(input_wakeup_time, "%02d_%02d",tickerConfig.wakeup_hour,  tickerConfig.wakeup_minute);
    sprintf(input_sleep_time,  "%02d_%02d",tickerConfig.sleep_hour,   tickerConfig.sleep_minute);    

    String json_output;
    json_output.reserve(1024);
    json_output = 
        // String quotes on consequtive lines are concatenated
        // https://forum.arduino.cc/index.php?topic=73212.0
        // { "input_owner_name":"John Smith",              "input_weather_city_name":"London,UK",              "input_timezone":"Australia/Brisbane",              "input_wakeup_time":"09_00",              "input_sleep_time":"22_20",               "input_ticker_currency":"AUD",              "input_ticker_1":"a",               "input_qty_1":"1",              "input_ticker_2":"b",               "input_qty_2":"2",              "input_ticker_3":"c",               "input_qty_3":"3",              "input_ticker_4":"d",               "input_qty_4":"4",              "input_ticker_5":"e",               "input_qty_5":"5",              "input_ticker_6":"f",               "input_qty_6":"6",              "input_ticker_7":"g",               "input_qty_7":"7",              "input_ticker_8":"h",               "input_qty_8":"8",              "input_ticker_9":"i",               "input_qty_9":"9",              "input_ticker_10":"j",              "input_qty_10":"00",              "input_matrix_brightness_mode":"1",               "input_rgb_mode":"1",               "input_custom_message_interval":"60",               "input_wake_sleep_mode":"2"}
        "{"
        "\"input_owner_name\":\""         + String(tickerConfig.owner_name)         + "\","          
        "\"input_weather_city_name\":\""  + String(tickerConfig.weather_city_name)  + "\","    
        "\"input_weather_city_id\":\""    + String(tickerConfig.weather_city_id)    + "\","    

        "\"input_timezone\":\""           + String(tickerConfig.clock_timezone)     + "\","    
        "\"input_timezone_2\":\""         + String(tickerConfig.clock_2_timezone)     + "\","                    
        "\"input_timezone_3\":\""         + String(tickerConfig.clock_3_timezone)     + "\","            

        "\"input_wakeup_time\":\""        + String(input_wakeup_time)               + "\","              
        "\"input_sleep_time\":\""         + String(input_sleep_time)                + "\","                       
        "\"input_crypto_1\":\""           + String(tickerConfig.crypto_1) + "\","                     
        "\"input_crypto_2\":\""           + String(tickerConfig.crypto_2) + "\","                     
        "\"input_crypto_3\":\""           + String(tickerConfig.crypto_3) + "\","                     
        "\"input_crypto_4\":\""           + String(tickerConfig.crypto_4) + "\","                     
        "\"input_crypto_5\":\""           + String(tickerConfig.crypto_5) + "\","                     
        "\"input_crypto_6\":\""           + String(tickerConfig.crypto_6) + "\","                             
        "\"input_stock_1\":\""            + String(tickerConfig.stock_1) + "\","                     
        "\"input_stock_2\":\""            + String(tickerConfig.stock_2) + "\","                     
        "\"input_stock_3\":\""            + String(tickerConfig.stock_3) + "\","                     
        "\"input_stock_4\":\""            + String(tickerConfig.stock_4) + "\","                     
        "\"input_stock_5\":\""            + String(tickerConfig.stock_5) + "\","                     
        "\"input_stock_6\":\""            + String(tickerConfig.stock_6) + "\","  
        "\"input_news_codes\":\""         + String(tickerConfig.news_codes)       + "\","                             
        "\"input_news_limit\":\""         + String(tickerConfig.news_limit)       + "\","                                     
        "\"input_scroll_speed\":\""       + String(tickerConfig.scroll_speed)     + "\","          
        "\"input_matrix_brightness_mode\":\"" + String(tickerConfig.matrix_brightness_mode) + "\","       
//        "\"input_rgb_mode\":\"" + String(tickerConfig.rgb_leds_mode) + "\","                         
        "\"input_wake_sleep_mode\":\""        + String(tickerConfig.wake_sleep_mode)        + "\","              
        "\"input_device_awake_weekends\":\""  + String(tickerConfig.device_awake_weekends)  + "\","              

        "\"input_freq_date\":\""              + String(tickerConfig.ticker_content_freq_date)       + "\","              
        "\"input_freq_crypto\":\""            + String(tickerConfig.ticker_content_freq_crypto)     + "\","              
        "\"input_freq_news\":\""              + String(tickerConfig.ticker_content_freq_news)       + "\","              
        "\"input_freq_weather\":\""           + String(tickerConfig.ticker_content_freq_weather)    + "\","              
        "\"input_freq_stock\":\""             + String(tickerConfig.ticker_content_freq_stock)      + "\","              
        "\"input_freq_countdown\":\""         + String(tickerConfig.ticker_content_freq_countdown)  + "\","              

        "\"input_countdown_name\":\""         + String(tickerConfig.countdown_name)       + "\","              
        "\"input_countdown_datetime\":"       + String(tickerConfig.countdown_datetime)   +","      

        "\"input_password\":\""               + String(tickerConfig.login_password)       + "\","
        "\"version\":1,"
        "\"awesomeness\":1"
        "}";

    // Send it.
    webServer.send(200, "text/json", json_output); // otherwise, respond with a 404 (Not Found) error

} // return a configuration string




void HTTPConfigSubmitHandler() // Handler
{ 

      // Blatently just iterate through post values, without doing any real validation
      TickerConfig newConfig;   

      newConfig = { 0 }; // Start Afresh

      newConfig.config_version = USER_CONFIG_VER;

      // Copy char array length MINUS ONE due to the terminating \0 required
      webServer.arg("input_owner_name")             .toCharArray(newConfig.owner_name,          sizeof(newConfig.owner_name));
      webServer.arg("input_weather_city_name")      .toCharArray(newConfig.weather_city_name,   sizeof(newConfig.weather_city_name));
      
      webServer.arg("input_timezone")               .toCharArray(newConfig.clock_timezone,     sizeof(newConfig.clock_timezone));
      webServer.arg("input_timezone_2")               .toCharArray(newConfig.clock_2_timezone,     sizeof(newConfig.clock_2_timezone));
      webServer.arg("input_timezone_3")               .toCharArray(newConfig.clock_3_timezone,     sizeof(newConfig.clock_3_timezone));

      //webServer.arg("input_ticker_currency")        .toCharArray(newConfig.ticker_currency,     7);
      //newConfig.ticker_mode = webServer.arg("input_ticker_mode").toInt();

/*      
    <?php
    
    for ($i=0; $i<=10;$i++)
    {
            echo "webServer.arg(\"input_ticker_$i\")            .toCharArray(newConfig.ticker$i,        8);\n";
            echo "newConfig.qty$i = webServer.arg(\"input_qty_$i\").toFloat();\n";     
    }

*/
      // Float Precision, same as an int - https://chortle.ccsu.edu/java5/Notes/chap11/ch11_2.html

      // Crypto
      webServer.arg("input_crypto_1").toCharArray(newConfig.crypto_1,        sizeof(newConfig.crypto_1));
      webServer.arg("input_crypto_2").toCharArray(newConfig.crypto_2,        sizeof(newConfig.crypto_1));
      webServer.arg("input_crypto_3").toCharArray(newConfig.crypto_3,        sizeof(newConfig.crypto_1));
      webServer.arg("input_crypto_4").toCharArray(newConfig.crypto_4,        sizeof(newConfig.crypto_1));
      webServer.arg("input_crypto_5").toCharArray(newConfig.crypto_5,        sizeof(newConfig.crypto_1));
                           
			// News		   
      webServer.arg("input_stock_1").toCharArray(newConfig.stock_1,        sizeof(newConfig.crypto_1));
      webServer.arg("input_stock_2").toCharArray(newConfig.stock_2,        sizeof(newConfig.crypto_1));
      webServer.arg("input_stock_3").toCharArray(newConfig.stock_3,        sizeof(newConfig.crypto_1));
      webServer.arg("input_stock_4").toCharArray(newConfig.stock_4,        sizeof(newConfig.crypto_1));
      webServer.arg("input_stock_5").toCharArray(newConfig.stock_5,        sizeof(newConfig.crypto_1));

      // News feed codes
      webServer.arg("input_news_codes").toCharArray(newConfig.news_codes,      sizeof(newConfig.news_codes));     
      newConfig.news_limit          =     webServer.arg("input_news_limit"     ).toInt();     


      // Content display frequency
      newConfig.ticker_content_freq_date          =     webServer.arg("input_freq_date"     ).toInt();
      newConfig.ticker_content_freq_crypto        =     webServer.arg("input_freq_crypto"   ).toInt();
      newConfig.ticker_content_freq_news          =     webServer.arg("input_freq_news"     ).toInt();
      newConfig.ticker_content_freq_weather       =     webServer.arg("input_freq_weather"  ).toInt();
      newConfig.ticker_content_freq_stock         =     webServer.arg("input_freq_stock" ).toInt();  
      newConfig.ticker_content_freq_countdown     =     webServer.arg("input_freq_countdown" ).toInt();            

      // Set config for LED / Matrix behaviour
      newConfig.matrix_brightness_mode  = webServer.arg("input_matrix_brightness_mode").toInt();
      newConfig.scroll_speed            = webServer.arg("input_scroll_speed").toInt();
//      newConfig.rgb_leds_mode           = webServer.arg("input_rgb_mode").toInt();
      newConfig.device_awake_weekends   = webServer.arg("input_device_awake_weekends").toInt();       
      newConfig.wake_sleep_mode         = webServer.arg("input_wake_sleep_mode").toInt(); 
 
      // Sleep and wake hour
      newConfig.wakeup_hour     =   webServer.arg("input_wakeup_time").substring(0,2).toInt(); // 20   (from '20_45')            f
      newConfig.wakeup_minute   =   webServer.arg("input_wakeup_time").substring(3,5).toInt(); // 45 (from '20_45')
      newConfig.sleep_hour      =   webServer.arg("input_sleep_time").substring(0,2).toInt(); // 20   (from '20_45')
      newConfig.sleep_minute    =   webServer.arg("input_sleep_time").substring(3,5).toInt(); // 45 (from '20_45')      

      // Password - clean crap from it
      String password =  webServer.arg("input_password");
      password.trim();
      password.toCharArray(newConfig.login_password,        9);

      // Password - clean crap from it
      String countdown_name =  webServer.arg("input_countdown_name");
      countdown_name.trim();
      countdown_name.toCharArray(newConfig.countdown_name,        63);

      newConfig.countdown_datetime = webServer.arg("input_countdown_datetime").toInt();
      
      Sprintln("Reserving EEPROM"); // per: http://esp8266.github.io/Arduino/versions/2.0.0/doc/libraries.html
      EEPROM.begin(EEPROM_BYTES_RESERVE); // reserve memory for EEPROM

      EEPROM.put(eeprom_addr_TickerConfig, newConfig);

      // https://gist.github.com/igrr/3b66292cf8708770f76b
      // https://techtutorialsx.com/2016/10/22/esp8266-webserver-getting-query-parameters/

      Sprintln("Comitting to EEPROM"); // per: http://esp8266.github.io/Arduino/versions/2.0.0/doc/libraries.html
      EEPROM.commit();
      EEPROM.end(); // very important

      // Send the success page.
      handleFileRead("/success.html");

      // Now refresh the current in-memory config
      tickerConfig = newConfig;

      // Need to reset the update timers, so the main loop gets the latest server data again!
      //reload_required = true;

      delay(500); 
      ESP.restart();   // clear, wait, restart!

      
} //HTTPsubmitHandler


void HTTPLoginSubmitHandler() 
{ //Handler

      String password =  webServer.arg("input_password");
      password.trim();

      Sprintln("Recieved Password: " + password);
      Sprintln("Comparing to password: " + String(tickerConfig.login_password));
    
    if (password == "")
    {
       handleFileRead("/login.html");
    }
    
    if (password.equals(tickerConfig.login_password))
    {
       handleFileRead("/config.html");
    }
    else
    {
       handleFileRead("/login.html");
    }
}


void HTTPConfigResetHandler() 
{ //Handler
    webServer.send(200, "text/html", F("<html><head><title>Reset</title></head><body><h2>Are you sure you want to reset all configuration?</h2><p>&nbsp;</p><p><form action=\"./reset_submit\" method=\"post\"><button type=\"submit\" value=\"Submit\">Submit</button></form></p><p>Clicking the button will cause the config to reset and the device to be rebooted.</p></body></html>"));       //Response to the HTTP request       
}


void HTTPConfigResetSubmitHandler() 
{ //Handler
    
    webServer.send(200, "text/html", F("<html><head><title>Reset</title><script>setTimeout(function () { window.location.href = \"./\"; }, 15000);</script></head><body><h2>Configuration has been reset!</h2><h3>Device Restarting - PLEASE WAIT for this page to refresh!</h3></body></html>"));       //Response to the HTTP request       

    //String nuke =  webServer.arg("nuke");
    EEPROM_clear_all(); // even wipe the system config with our hidden ID!
 
    delay(1000); 
    ESP.restart();   // clear, wait, restart!
             
}


// Advanced - Secret hidden stuff
void HTTPConfigAdvancedHandler() 
{ //Handler

     handleFileRead("/advanced.html");
     
}
/*
void HTTPConfigAdvancedSubmitHandler() 
{ //Handler

    String host = webServer.arg("input_iot_endpoint_host");
    String path = webServer.arg("input_iot_endpoint_path");   
    
    if (host == "" || path == "")
    {

      // Nothing occured
      webServer.send(200, "text/html", F("<html><head><title>Advanced</title></head><body><p>No update to advanced configuration was made.</p></body></html>"));       //Response to the HTTP request       
    
    }
	// Check the host or path for validness
	else if ( 	(host.indexOf("/") != -1) || // host shouldn't contain //'s, like http://
				(path.endsWith("/") == false)
	)
	{
      // Nothing occured
      webServer.send(200, "text/html", F("<html><head><title>Advanced</title></head><body><p>Invalid host or path. The hostname must only be in the format of www.domain.com, the path must end with a /. For example: /iot/ticker/.</p></body></html>"));       //Response to the HTTP request       
	
	}
    else
    {

      // blatently just iterate through post values, without doing any real validation
      // we just hope the browser sends these in the right order for our struct
      SystemConfig newConfig;      
      
      // Copy the values from the old struct :-)
      newConfig = systemConfig;
	  
	    path.trim(); host.trim();

      // Submitted values
      host.toCharArray(newConfig.endpoint_1_host,        64);
      path.toCharArray(newConfig.endpoint_1_path,        64);    

      Sprintln("Reserving EEPROM"); // per: http://esp8266.github.io/Arduino/versions/2.0.0/doc/libraries.html
      EEPROM.begin(EEPROM_BYTES_RESERVE); // reserve memory for EEPROM        

      EEPROM.put(eeprom_addr_SystemConfig, newConfig);

      // https://gist.github.com/igrr/3b66292cf8708770f76b
      // https://techtutorialsx.com/2016/10/22/esp8266-webserver-getting-query-parameters/

      Sprintln("Wrote: " + String(newConfig.endpoint_1_host));
      Sprintln("Wrote: " + String(newConfig.endpoint_1_path));
      
      Sprintln("Updated EEPROM advanced configuration.");

      EEPROM.commit();
      EEPROM.end(); // very important
  
      
      handleFileRead("/success.html");      
    }
}
*/

void HTTPMessageSubmitHandler() 
{ //Handler

    String message = webServer.arg("input_message");

    // blatently just iterate through post values, without doing any real validation
    // we just hope the browser sends these in the right order for our struct
    CustomMessage newMessage; 
    newMessage = { 0 }; // zero out the new struct    

    // restart - HIDDEN
    //if (message.equals(".restart")) ESP.reset(); // cuase the device to crash and watchdog rstart
          

    if ( message.length() < 2) // clear the message
    {
      // Nothing occured
      //webServer.send(200, "text/html", F("<html><head><title>Set Message</title></head><body><p>Invalid message. Please make it longer. Thanks :-)</p></body></html>"));       //Response to the HTTP request       
      newMessage.message_timestamp      = 0;   
      newMessage.message_last_displayed = 0;
      newMessage.message_length         = 0;      
    
    }
    else // populate the new message
    {
      newMessage.message_last_displayed = 0;
      newMessage.message_timestamp      = clockMain.getEpochSecond();
      newMessage.message_length         = message.length();
      message.toCharArray(newMessage.message, 251);       // 250 of user characters + 1 for the terminating \0

      newMessage.message_display_freq   = webServer.arg("input_custom_message_display_freq").toInt();
      newMessage.message_expiry = webServer.arg("input_custom_message_expiry").toInt();

      Sprintln("Setting message to: "           + String(newMessage.message));
      Sprintln("Setting message length to: "    + String(newMessage.message_length));            
      Sprintln("Setting message timestamp to: " + String(newMessage.message_timestamp));
      Sprintln("Setting display freq to: "      + String(newMessage.message_display_freq));            
      Sprintln("Setting display expiry to: "    + String(newMessage.message_expiry));
           
    }

      // HACK: We should probably just adjust the existing struct in-place, instead of copy?
      customMessage = newMessage; // copy to global variable

      if ( webServer.arg("ajax_baby").toInt() == 1 )
      {
          //Response to the HTTP JSON request       
          webServer.send(200, F("application/xml"), F("{\"success\": true}"));       
      }          
      else
      {  
         // Reponse to post request
         handleFileRead("/success.html");  
          // webServer.send(200, F("text/html"), F("<html><head><title>Set Message</title></head><body><p>Invalid message. Please make it longer. Thanks :-)</p></body></html>"));       
      }
      
      //handleFileRead("/success.html");  
          
} // message handle submit


/**** Forced Update Handeller */
void HTTPUpdateHandler()
{
  Serial.println(F("EEPROM_set_firmware_needs_update()"));
  EEPROM_set_firmware_needs_update();
  EEPROM_set_filesystem_needs_update(true);  
  delay(100);
  ESP.reset();

  /*
  systemConfig.last_firmware_check_time_t = 0;
  checkForFirmwareUpdate();

  // System should reboot.
  webServer.send(200, "text/html", F("Completed."));       //Response to the HTTP request      
  */
  
}


void EEPROM_SerialDebug()
{
 Serial.println();
  Serial.print( F("Heap: ") ); Serial.println(system_get_free_heap_size());
  Serial.print( F("Boot Vers: ") ); Serial.println(system_get_boot_version());
  Serial.print( F("CPU: ") ); Serial.println(system_get_cpu_freq());
  Serial.print( F("SDK: ") ); Serial.println(system_get_sdk_version());
  Serial.print( F("Chip ID: ") ); Serial.println(system_get_chip_id());
  Serial.print( F("Flash ID: ") ); Serial.println(spi_flash_get_id());
  Serial.print( F("Flash Size: ") ); Serial.println(ESP.getFlashChipRealSize());
  Serial.print( F("Sketch MD5: ") ); Serial.println(ESP.getSketchMD5());
  Serial.print( F("Vcc: ") ); Serial.println(ESP.getVcc());
  Serial.println();

  Serial.print( F("Config: Owner Name:") ); Serial.println(tickerConfig.owner_name);
  Serial.print( F("Config: Clock Timezone:") ); Serial.println(tickerConfig.clock_timezone);
  Serial.print( F("Config: Weather City ID:") ); Serial.println(tickerConfig.weather_city_id);
  Serial.print( F("Config: Weather Name:") ); Serial.println(tickerConfig.weather_city_name);
  Serial.print( F("Config: News Codes:") ); Serial.println(tickerConfig.news_codes);
  Serial.print( F("Config: News Limit:") ); Serial.println(tickerConfig.news_limit, DEC);  
  Serial.println( F("---------------") );
  //Serial.print( F("Config: Ticker Currency:") ); Serial.println(tickerConfig.ticker_currency     );
  Serial.print( F("Config: Crypto 1:") ); Serial.println(tickerConfig.crypto_1  );    
  Serial.print( F("Config: Stock 1:") ); Serial.println(tickerConfig.stock_1  );     
  Serial.println( F("---------------") );
  Serial.print( F("Config: Wakeup Hour:") );  Serial.println(tickerConfig.wakeup_hour, DEC   );  
  Serial.print( F("Config: Wakeup Min:") ); Serial.println(tickerConfig.wakeup_minute, DEC   );    
  Serial.print( F("Config: Sleep Hour:") );  Serial.println(tickerConfig.sleep_hour, DEC   );  
  Serial.print( F("Config: Sleep Min:") ); Serial.println(tickerConfig.sleep_minute, DEC   );    
  Serial.println( F("---------------") );
  Serial.print( F("Config: Matrix Brightness:") ); Serial.println(tickerConfig.matrix_brightness_mode, DEC   );      
  Serial.print( F("Config: Scroll Speed:") ); Serial.println(tickerConfig.scroll_speed , DEC  );        
//  Serial.print( F("Config: RGB Led Mode:") ); Serial.println(tickerConfig.rgb_leds_mode , DEC  );          
  Serial.print( F("Config: Wake Sleep Mode:") ); Serial.println(tickerConfig.wake_sleep_mode, DEC   );   
  Serial.println( F("---------------") );
  Serial.print( F("Config: Date Frequency:") );  Serial.println( tickerConfig.ticker_content_freq_date, DEC   );  
  Serial.print( F("Config: Weather Freq:") ); Serial.println(tickerConfig.ticker_content_freq_weather, DEC   );    
  Serial.print( F("Config: News Freq:") );  Serial.println(tickerConfig.ticker_content_freq_news, DEC   );  
  Serial.print( F("Config: Crypto Freq:") ); Serial.println(tickerConfig.ticker_content_freq_crypto, DEC   );    
  Serial.print( F("Config: Stock Freq:") ); Serial.println(tickerConfig.ticker_content_freq_stock, DEC   );    

/*
  Serial.println( F("---------------") );    
  Serial.print( F("Endpoint 1 Host:") ); Serial.println(systemConfig.endpoint_1_host);     
  Serial.print( F("Endpoint 1 Path:") ); Serial.println(systemConfig.endpoint_1_path);       

  Serial.print( F("Endpoint 2 Host:") ); Serial.println(systemConfig.endpoint_2_host);     
  Serial.print( F("Endpoint 2 Path:") ); Serial.println(systemConfig.endpoint_2_path); 
*/

}