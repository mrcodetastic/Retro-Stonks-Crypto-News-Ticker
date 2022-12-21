#pragma once

/********************************** DATE/TIME HELPER UTILS  *************************************/
// NOTE: Ensure destination buffer at least 12 characters.
void getFormattedTimeToCharBuffer(char * parolaBuffer, TimeLib2 &clockInstance)
{
  /*
    unsigned long rawTime = now();
    int hours             = (rawTime % 86400L) / 3600;
    int minutes           = (rawTime % 3600)   / 60;
  */
  char am_or_pm[3];

  // Convert to 12 hour time
  if (clockInstance.isAM() == false)
  {
    strcpy(am_or_pm, "PM");
  }
  else
  {
    strcpy(am_or_pm, "AM");
  }

  sprintf((char *)parolaBuffer, "%02d:%02d %s",  clockInstance.hourFormat12(), clockInstance.minute(), am_or_pm );

} // end getFormattedTimeToCharBuffer

// NOTE: Ensure destination buffer at least 12 characters.
void getFormattedDateToCharBuffer(char * parolaBuffer, TimeLib2 &clockInstance)
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
  strcpy(weekday_str, dayShortStr(clockInstance.weekday()));
  sprintf((char *)parolaBuffer, "%s %d %s",  weekday_str, clockInstance.day(),  monthShortStr(clockInstance.month()) );

} // end getFormattedDateToCharBuffer


/************************************* CRYPTO CODE CHECK ****************************************/
bool is_valid_symbol (char *symbol) {
  int symbol_str_length = strlen(symbol);

  if ( symbol_str_length < 3) {
    return false;
  }

  // https://www.geeksforgeeks.org/isalpha-isdigit-functions-c-example/
  int alphabet = 0, number = 0, i;
  for (i = 0; symbol[i] != '\0'; i++) {
    // check for alphabets
    if (isalpha(symbol[i]) != 0)
      alphabet++;

    // check for decimal digits
    else if (isdigit(symbol[i]) != 0)
      number++;
  }

  // If the number of digits or characters is less than the string length
  // then we must have some other rubbish in the character string
  if ((alphabet + number) < symbol_str_length)
    return false;

  return true;
}


/************************************* MATH UTILS  ****************************************/

// From: https://gist.github.com/bmccormack/d12f4bf0c96423d03f82
int movingAvg(int *ptrArrNumbers, long *ptrSum, int pos, int len, int nextNum)
{
  //Subtract the oldest number from the prev sum, add the new number
  *ptrSum = *ptrSum - ptrArrNumbers[pos] + nextNum;
  //Assign the nextNum to the position in the array
  ptrArrNumbers[pos] = nextNum;
  //return the average
  return *ptrSum / len;
}


/******************************** I2C Real Time Clock ***************************************/
bool check_if_exist_I2C(byte device_address) 
{
  byte error, address;

      PRINTX(F("Scanning I2C bus for device of address: 0x"), device_address);
  
  for (address = 1; address < 127; address++ ) 
  {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0)
    {
      //Sprint(F("I2C device found at address 0x"));
      PRINTX(F("I2C device found at address 0x"), address);

      if (address == device_address )
      {
        return true;
      }
    }
  }
    
     Sprintln(F("No I2C devices found of address"));

    return false;

}