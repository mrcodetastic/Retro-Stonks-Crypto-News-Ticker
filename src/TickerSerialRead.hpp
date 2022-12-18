#ifndef TICKER_SERIAL_H
#define TICKER_SERIAL_H

extern void performFilesystemUpdate();
extern void EEPROM_set_firmware_needs_update();
extern bool reload_required;

// For serial read
int     incomingByte    = 0;
bool    command_start   = false;

void handleSerialRead()
{
    if (Serial.available() > 0) {

        // read the incoming byte:
        incomingByte = Serial.read();

        if (incomingByte == 'x') {
            Serial.println("OK");
            command_start = true;
        }
        
        if ( !command_start) return; // don't go any further unless x was sent before

        switch(incomingByte) {        
            case 'r':
                ESP.reset();
                break;
            case 'f':
                EEPROM_clear_all();
                ESP.reset();
                break;
            case 'd':
                EEPROM_SerialDebug();
                break;
            case 'a':
                sprint_adc = !sprint_adc;                
            case 't':
                Serial.println("Input recieved...");
                break;

            case 'u':
                //performFilesystemUpdate();
                EEPROM_set_firmware_needs_update();
                EEPROM_set_filesystem_needs_update(true);
                ESP.reset();
                break;

            case 'm':
                Serial.print("Heap free:");
                Serial.println(String(ESP.getFreeHeap(), DEC));
                Serial.print("Stack free:");
                Serial.println(String(ESP.getFreeContStack(), DEC));   
                break;

            case 'l':
                Serial.print(F("Forcing feed reload next cycle."));
                reload_required = true;
                break;                

        } // end switch

    } // end data received

} // end handleSerialRead

#endif