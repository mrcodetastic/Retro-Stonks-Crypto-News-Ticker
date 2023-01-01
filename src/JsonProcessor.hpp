#pragma once

#include <ArduinoJson.h>
//#include <TickerData.hpp>
#include <list>
#include <string>
#include <TimeLib2.hpp>

//extern TimeLib2 clockMain;

struct TickerInstance
{
    char name[64];     
    char code[16];
    char  currency_code[4];
    float price_reporting_ccy;
    float percent_change_24h;
    float price_change_24h;
    char parola_currency_code[2];
    char parola_upordown_code[2];
};

// New - Weather instances (current and forecast)
struct WeatherInstance
{
    time_t datetime;
    time_t datetime_tzadjusted;  // Adjusted for current timezone
    char   datetime_desc[32];    // i.e. Tonight, Tomorrow, etc.
    char   city_id[16];
    char   city_name[16];    
    int8_t temp_now;
    int8_t humidity;
    char   summary[64];    
    char   description[64];
    char   icon[16];    
};

// Current Weather
WeatherInstance CurrentWeather;
std::string CurrentWeather_str;

// Weather Forecasts
std::list<std::string> WeatherForecasts_str; // X items
std::list<WeatherInstance> WeatherForecasts; // X items

// Crypto
std::list<std::string> CryptoTicker_str;
std::list<TickerInstance> CryptoTickers;

// Equities 
std::list<std::string> EquitiesTicker_str;
std::list<TickerInstance> EquitiesTickers;

// News Headlines
std::list<std::string> NewsHeadlines_str; // 10 items


class JsonProcessor {

    private:

    public:
        virtual bool process_json_document(DynamicJsonDocument &doc);

};

class CurrentWeatherProcessor : public JsonProcessor {

    private:
        WeatherInstance weather;    
        time_t gmt_offset;

    public:
        void set_gmt_offset(time_t offset)
        {
            gmt_offset = offset;
        }

        bool process_json_document(DynamicJsonDocument &doc)
        {

            /*
            {
            "data": [
                {
                "dt": 1671309847,
                "name": "London",
                "id": 2643743,
                "country": "GB",
                "temp": -0.1,
                "humidity": 87,
                "summary": "Clouds",
                "description": "Overcast Clouds",
                "cloud_percentage": 88,
                "icon_day": "clouds",
                "icon_night": "clouds",
                "dt_txt": ""
                }
            ],
            "api_version": 3,
            "cod": 200
            }
            */

            int cod = doc["cod"];
            if (!cod) return false;


            JsonArray array = doc["data"].as<JsonArray>();
            for (JsonObject item : array)
            {
                    weather.datetime                = item["dt"].as<long>(); 
                    weather.datetime_tzadjusted     = weather.datetime + gmt_offset;                           

                    snprintf_P(weather.city_id, sizeof(weather.city_id), "%ld", doc["id"].as<long>());                                             

                    const char* city_name = item["name"];
                    Sprintln(city_name);

                    if (city_name)
                        strncpy(weather.city_name, city_name, sizeof(weather.city_name));             
                        
                    weather.humidity    = item["humidity"].as<int>();
                    weather.temp_now    = item["temp"].as<int>();

                    const char* icon = item["icon_day"];
                    if (icon)
                        strncpy(weather.icon,       icon,          sizeof(weather.icon));

                    strncpy(weather.summary,        item["summary"]     .as<const char *>(),   sizeof(weather.summary));                                        
                    strncpy(weather.description,    item["description"] .as<const char *>(),   sizeof(weather.description));
                                        
                
                    // Process
                    char temp[128]= {0};  // hack
                    snprintf_P(temp, sizeof(temp), "Current Weather in %s \x10 %s and %d\xB0.", weather.city_name, weather.description, weather.temp_now);

                    // CurrentWeather_str.empty();
                    CurrentWeather_str = temp; // http://www.cplusplus.com/forum/general/48362/ 

                    // Empty it
                    CurrentWeather = weather;

                    break;
            }

            return true;

        } // processTickerData

}; // end CurrentWeatherProcessor


class ForecastWeatherProcessor : public JsonProcessor {

    private:
        WeatherInstance weather; 
        int     forecast_count  = 0;   // Apparently forward_list doesn't have a size function..
        int     previous_forecast_day_of_month  = -1;  
        time_t  gmt_offset;             

    public:
        void set_gmt_offset(time_t offset)
        {
            gmt_offset = offset;
        }

        bool process_json_document(DynamicJsonDocument &doc)
        {

            forecast_count = 0;
            WeatherForecasts_str.clear();
            WeatherForecasts.clear(); // get rid of struct 
                        
            /*
                {
                "data": [
                    {
                    "dt": 1671321600,
                    "name": "London",
                    "id": 2643743,
                    "country": "GB",
                    "temp": -0.4,
                    "humidity": 85,
                    "summary": "Clouds",
                    "description": "Broken Clouds",
                    "cloud_percentage": 74,
                    "icon_day": "clouds",
                    "icon_night": "clouds",
                    "dt_txt": "2022-12-18 00:00:00"
                    },
                    {
                    "dt": 1671332400,
                    "name": "London",
                    "id": 2643743,
                    "country": "GB",
                    "temp": -0.2,
                    "humidity": 81,
                    "summary": "Clouds",
                    "description": "Broken Clouds",
                    "cloud_percentage": 62,
                    "icon_day": "clouds",
                    "icon_night": "clouds",
                    "dt_txt": "2022-12-18 03:00:00"
                    },
            */

            int cod = doc["cod"];
            if (!cod) return false;


            JsonArray array = doc["data"].as<JsonArray>();
            for (JsonObject item : array)
            {
                    weather.datetime    = item["dt"].as<long>();         
                    weather.datetime_tzadjusted     = weather.datetime + gmt_offset;                                                 

                    snprintf_P(weather.city_id, sizeof(weather.city_id), "%ld", doc["id"].as<long>());                                             

                    const char* city_name = item["name"];
                   
                    if (city_name)
                        strncpy(weather.city_name, city_name, sizeof(weather.city_name));             
                        
                    weather.humidity    = item["humidity"].as<int>();
                    weather.temp_now    = item["temp"].as<int>();

                    const char* icon = item["icon_day"];
                    if (icon)
                        strncpy(weather.icon,       icon,          sizeof(weather.icon));

                    strncpy(weather.summary,        item["summary"]     .as<const char *>(),   sizeof(weather.summary));                                        
                    strncpy(weather.description,    item["description"] .as<const char *>(),   sizeof(weather.description));
                                        
                

                    //
                    // Now check to see if we want to process and store this weather forecast?
                    //
                    int forecast_day_of_week           = clockMain.weekday(weather.datetime_tzadjusted);   // the weekday (Sunday is day 1) 
                    int forecast_day_of_month          = clockMain.day(weather.datetime_tzadjusted);      // the day of month (first day of the month is 1, not 0 :-) )

                    // Get the first forecast of the afternoon each day, and not for the remainder of today.
                    
                    if ( clockMain.day() == forecast_day_of_month ) {
                        Sprintln (F("Skipping forecast: Current day"));    
                        continue;
                    } else if ( clockMain.isAM(weather.datetime_tzadjusted) ) {
                        Sprintln ( F("Skipping forecast: Morning time") ); 
                        continue;
                    } else if ( previous_forecast_day_of_month == forecast_day_of_month ) {
                        Sprintln ( F("Skipping forecast: Already covered this day.") );  
                        continue;
                    } else if (forecast_count++ > 5) {
                        Sprintln(F("Forecast beyond our horizon - ignoring..."));
                        continue;     
                    
                    }

                    previous_forecast_day_of_month = forecast_day_of_month;

                    char temp[128] = {0};  // hack   
                    std::string s;
                    if ( (clockMain.day() + 1) ==  forecast_day_of_month ) { // is the forecast for tomrrow?
                        snprintf_P(temp, sizeof(temp), "Tomorrow \x10 %s and %d\xB0.", weather.description, weather.temp_now);
                    } else {
                        snprintf_P(temp, sizeof(temp), "%s \x10 %s and %d\xB0.", dayStr(forecast_day_of_week), weather.description, weather.temp_now);    
                    }
                    s = temp;
                    Sprint("== "); Sprintln(temp);

                    WeatherForecasts_str.push_back(temp); // add compiled string to list
                    WeatherForecasts.push_back(weather); // add the raw forecast

            } // iterate through array item

            return true;

        } // processTickerData

}; 


class NewsProcessor : public JsonProcessor {

    private:         

    public:
        bool process_json_document(DynamicJsonDocument &doc)
        {
            NewsHeadlines_str.clear();

            /*
                {
                "data": [
                    {
                    "feed_timestamp": "2022-12-17 21:36:47",
                    "feed_title": null,
                    "feed_language": "en-gb",
                    "feed_code": "BT",
                    "datetime": 1671309916,
                    "title": "Eurovision 2023: Ukraine chooses Tvorchi from bomb shelter",
                    "title_length": 58
                    },
                    {
                    "feed_timestamp": "2022-12-17 21:36:47",
                    "feed_title": null,
                    "feed_language": "en-gb",
                    "feed_code": "BT",
                    "datetime": 1671305123,
                    "title": "Taraneh Alidoosti: Top Iran actress who supported protests arrested",
                    "title_length": 67
                    },
                    {
                    "feed_timestamp": "2022-12-17 21:36:47",
                    "feed_title": null,
                    "feed_language": "en-gb",
                    "feed_code": "BT",
                    "datetime": 1671304341,
                    "title": "Homes flooded after north London water mains burst",
                    "title_length": 50
                    },
            */

            int cod = doc["cod"];
            if (!cod) return false;


            JsonArray array = doc["data"].as<JsonArray>();
            for (JsonObject item : array)
            {
                const char* headline = item["title"];

                //const char* feed = item["feed_code"];

                if (headline)
                {
                    Sprint(feed); Sprint(": ");
                    Sprintln(headline);
                    std::string tmp = "\x7 " + (std::string) headline;
                    NewsHeadlines_str.push_front(tmp);                       
                }
            
            } // iterate through array item

            return true;            

        } // processTickerData
};  // end processor


class TickerProcessor : public JsonProcessor {

    private:
        bool crypto_mode = false;
        TickerInstance ticker;
        int count = 0;       

    public:
        void set_crypto_mode(bool mode) {
            crypto_mode = mode;
        }

        bool process_json_document(DynamicJsonDocument &doc)
        {
            count = 0;

            if (crypto_mode) {  
                CryptoTicker_str.clear(); // list of strings
                CryptoTickers.clear(); // list of objects
            } else {
                EquitiesTicker_str.clear(); // list of strings
                EquitiesTickers.clear(); // list of objects
            }
                        
            /*
                {
                "data": [
                    {
                    "feed_timestamp": "2022-12-17 21:47:31",
                    "feed_reporting_ccy": "USD",
                    "name": "Bitcoin",
                    "code": "BTC",
                    "price_reporting_ccy": 16724.74,
                    "percent_change_24h": -0.77,
                    "price_change_24h": -129
                    }
                ],
                "api_version": 3,
                "cod": 200
                }
            */

            int cod = doc["cod"];
            if (!cod) return false;


            JsonArray array = doc["data"].as<JsonArray>();
            for (JsonObject item : array)
            {
                    // name
                    const char* name = item["name"];
                    if (name)
                        strncpy(ticker.name, name, sizeof(ticker.name));

                    // symbol
                    const char* code = item["code"];
                    if (code)
                        strncpy(ticker.code, code, sizeof(ticker.code));

                    // price_reporting_ccy
                    float price_reporting_ccy = item["price_reporting_ccy"];
                    if (price_reporting_ccy)
                        ticker.price_reporting_ccy = price_reporting_ccy;

                    // percent_change_24h
                    float percent_change_24h = item["percent_change_24h"];
                    if (percent_change_24h)
                        ticker.percent_change_24h = percent_change_24h;

                    // price_change_24h
                    float price_change_24h = item["price_change_24h"];
                    if (price_change_24h)
                        ticker.price_change_24h = price_change_24h;


                    const char* feed_reporting_ccy = item["feed_reporting_ccy"];
                    if (feed_reporting_ccy)
                        strncpy(ticker.currency_code, feed_reporting_ccy, sizeof(ticker.currency_code));


                    // 
                    // Now process item
                    //            
                    Sprintln(F("Processing ticker."));

                    memset(ticker.parola_upordown_code, '\0', sizeof(ticker.parola_upordown_code));
                    memset(ticker.parola_currency_code, '\0', sizeof(ticker.parola_currency_code));

                    ticker.parola_upordown_code[0] = (ticker.percent_change_24h > 0) ? 0x18:0x19;

                    if ( strcmp(ticker.currency_code, "EUR") == 0 ) {
                    ticker.parola_currency_code[0]   = (char)128;  //   5, 62, 85, 85, 85, 65,		// 128 - 'Euro symbol'   
                    } else if ( strcmp(ticker.currency_code, "GBP") == 0 ) {
                        ticker.parola_currency_code[0] = (char)163;    // 4, 68, 126, 69, 65,		// 163 - '£ Pound sign'
                    } else
                    {
                        ticker.parola_currency_code[0] = (char)'$';    
                    }


                    char temp[128];  // hack  
                    // Refer to https://github.com/MajicDesigns/MD_MAX72XX/blob/main/src/MD_MAX72xx_font.cpp for the hex values
                    // Currently use bullet point then right arrow.
                    snprintf_P(temp, sizeof(temp), "\x7 %s \x16 %s%0.2lf%s  %s  %0.2lf%%", ticker.name, ticker.parola_currency_code, ticker.price_reporting_ccy, "", ticker.parola_upordown_code, ticker.percent_change_24h); 
                    Sprintln(temp); 

                    if (crypto_mode) {
                        CryptoTicker_str.push_back(temp);   // http://www.cplusplus.com/forum/general/48362/ 
                        CryptoTickers.push_back(ticker);  // add struct to list
                    } else {
                        EquitiesTicker_str.push_back(temp);   // http://www.cplusplus.com/forum/general/48362/ 
                        EquitiesTickers.push_back(ticker);     // add struct to list
                    }

                    count++;

            } // iterate through array item

            return true;            

        } // processTickerData

}; 



class TimeProcessor : public JsonProcessor {

    private:
        time_t _offset    = 0;
        time_t _timestamp = 0;   
       // const char * _timezone_name[64];

    public:

        void update(time_t &timestamp, time_t &offset)
        {
            timestamp = _timestamp; offset = _offset;
        }

        bool process_json_document(DynamicJsonDocument &doc)
        {

/*
            {
            "data": {
                "time": "2022-12-17 22:20:44",
                "timestamp": 1671315644,
                "timestamp_offset": 0
            },
            "api_version": 3,
            "cod": 200
            }
*/
            int cod = doc["cod"];
            if (!cod) return false;

            _timestamp          = doc["data"]["timestamp"];
            _offset             = doc["data"]["timestamp_offset"];

            // The PHP should return a timezone name as well. Europe/London should be 'London'.
           // _timezone_name      = doc["data"]["timezone_name"];

            return true;            


        } // processTickerData
};  // end processor
