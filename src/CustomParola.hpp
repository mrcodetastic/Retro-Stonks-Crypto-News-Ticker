#ifndef CUSTOM_PAROLA
#define CUSTOM_PAROLA

/**********************************************************************************************
 * Parola Sprites
 */

// Sprite Definitions
const uint8_t F_PMAN1 = 6;
const uint8_t W_PMAN1 = 8;
uint8_t PROGMEM pacman1[F_PMAN1 * W_PMAN1] =  // gobbling pacman animation
{
  0x00, 0x81, 0xc3, 0xe7, 0xff, 0x7e, 0x7e, 0x3c,
  0x00, 0x42, 0xe7, 0xe7, 0xff, 0xff, 0x7e, 0x3c,
  0x24, 0x66, 0xe7, 0xff, 0xff, 0xff, 0x7e, 0x3c,
  0x3c, 0x7e, 0xff, 0xff, 0xff, 0xff, 0x7e, 0x3c,
  0x24, 0x66, 0xe7, 0xff, 0xff, 0xff, 0x7e, 0x3c,
  0x00, 0x42, 0xe7, 0xe7, 0xff, 0xff, 0x7e, 0x3c,
};

// Sprite Definition
const uint8_t F_ROCKET = 2;
const uint8_t W_ROCKET = 11;
uint8_t PROGMEM rocket[F_ROCKET * W_ROCKET] =  // rocket
{
  0x18, 0x24, 0x42, 0x81, 0x99, 0x18, 0x99, 0x18, 0xa5, 0x5a, 0x81,
  0x18, 0x24, 0x42, 0x81, 0x18, 0x99, 0x18, 0x99, 0x24, 0x42, 0x99,
};




/**********************************************************************************************
 * Custom MD_PAROLA wapper functions to make things easier with multiple matrix display zones.
 */


void resetZoneSizes();
bool allZoneAnimationsCompleted();
void waitForZoneAnimationComplete();
void showStartBranding();
void printTextOrScrollLeft();
void printPleaseWait();


void setDefaultZoneSizes()
{
  Parola.setZone(ZONE_RIGHT, 0, 0);               // Zone 0 we don't use just yet...
  Parola.setZone(ZONE_LEFT, 0, MAX_DEVICES-1); 
}

bool allZoneAnimationsCompleted() // Returns true when all zones have completed animation.
{
    boolean bAllDone = true;

    for (uint8_t i=0; i<NUM_ZONES && bAllDone; i++)
      bAllDone = bAllDone && Parola.getZoneStatus(i);
 
    return bAllDone;

}

void waitForZoneAnimationComplete()
{
    while ( !allZoneAnimationsCompleted() )
    {
          Parola.displayAnimate();

#if defined(ESP8266)          
          yield(); // keep the watchdog fed. Removed: NOT relevant to ESP32
#endif   
    }
 

}


void printTextOrScrollLeft(const char *text, bool force_print = false, int speed = SCROLL_SPEED_MS_DELAY_SLOW)
{
    Parola.displayClear();

    //Sprint("String length is: ");
    //SprintlnDEC( strlen(text), DEC);

    // message is grater than about 10 characters... will need to scroll
    if ( strlen(text) < 14 || force_print) {
      Parola.displayZoneText(ZONE_LEFT, text, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);        
    } else {
      Parola.displayZoneText(ZONE_LEFT, text, PA_LEFT, speed, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);                    
    }

    waitForZoneAnimationComplete();

}

void showStartBranding()
{
  
  Parola.setZone(ZONE_RIGHT, 0, 0);               // ONLY THE right most MAX7219 module is for ZONE_RIGHT
  Parola.setZone(ZONE_LEFT, 1, MAX_DEVICES-1);    

  // Be friendly
  // \x3 is a love heart
  // Use HEX reference to print custom character per MAXX7219 font definition - Refer to MD_MAX72xx_font.cpp
  Parola.displayZoneText(ZONE_LEFT, "RetroTicker", PA_CENTER, SCROLL_SPEED_MS_DELAY_SLOW, 0, PA_OPENING_CURSOR);    

  waitForZoneAnimationComplete();

  Parola.displayZoneText(ZONE_RIGHT, "\x3", PA_CENTER, SCROLL_SPEED_MS_DELAY_SLOW, 0, PA_SCROLL_UP);    

  waitForZoneAnimationComplete();

  setDefaultZoneSizes(); // Go back to how it should be

} // end start branding


void show_starting_greeting()
{
    Parola.displayClear();

    char greeting[128] { '\0' }; // bigger than "Hello " + 64 characters 

    if (strlen(tickerConfig.owner_name) < 2) {
      strcpy(greeting, "Hello!");
    } else {
      sprintf(greeting, "Hi %s", tickerConfig.owner_name);
    }

    if (strlen(greeting) > 10) {
      // To big to fit on one view
      Parola.displayZoneText(ZONE_LEFT, greeting, PA_LEFT, TICKER_SCROLL_SPEED_NORMAL, 2000, PA_SCROLL_LEFT);           
    } else {
          Parola.displayZoneText(ZONE_LEFT, greeting, PA_CENTER, TICKER_SCROLL_SPEED_NORMAL, 2000, PA_RANDOM, PA_NO_EFFECT);  // Name          
    }

    // Do animation
    waitForZoneAnimationComplete();  
}

void printPleaseWait()
{
  printTextOrScrollLeft("Please Wait!");
}

#endif