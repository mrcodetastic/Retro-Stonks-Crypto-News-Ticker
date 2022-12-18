#ifndef TICKER_DEBUG
#define TICKER_DEBUG

  /*----------------------------------------------------------------------------*/

/*
  void debuginfo() 
  {
      Sprintln("---");
      PRINTD("Current ADC:",        adc_readings[adc_current_position] );
      PRINTD("Moving Avg:",         adc_maverage );
      Sprintln("-");
      PRINTD("getFreeHeap:",            ESP.getFreeHeap() );
      PRINTD("getHeapFragmentation:",   ESP.getHeapFragmentation() );
      PRINTD("getMaxFreeBlockSize:",    ESP.getMaxFreeBlockSize() );    
      Sprintln("---");
  }  
*/

 //#define DEBUG_MODE 1

  #if DEBUG_MODE
  
      #define Sprintln(a) (Serial.println(a)) 
      #define SprintlnDEC(a, x) (Serial.println(a, x))
      
      #define Sprint(a) (Serial.print(a))
      #define SprintDEC(a, x) (Serial.print(a, x))

      #define PRINTD(s, v)   { Serial.print(F(s)); Serial.println(v); }      // Print a string followed by a value (decimal)
      #define PRINTX(s, v)  { Serial.print(s); Serial.println(v, HEX); } // Print a string followed by a value (hex)
      #define PRINTB(s, v)  { Serial.print(s); Serial.println(v, BIN); } // Print a string followed by a value (binary)
      #define PRINTC(s, v)  { Serial.print(s); Serial.println((char)v); }  // Print a string followed by a value (char)

  
  #else
  
      #pragma message "COMPILING WITHOUT SERIAL DEBUGGING"
      #define Sprintln(a)
      #define SprintlnDEC(a, x)
      
      #define Sprint(a)
      #define SprintDEC(a, x)

      #define PRINTD(s, v)   // Print a string followed by a value (decimal)
      #define PRINTX(s, v)  // Print a string followed by a value (hex)
      #define PRINTB(s, v)  // Print a string followed by a value (binary)
      #define PRINTC(s, v)  // Print a string followed by a value (char)      
      
  #endif




#endif

