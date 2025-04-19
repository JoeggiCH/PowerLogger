#include <Wire.h> // must be included here so that Arduino library object file references work
#include <RtcDS1307.h>
RtcDS1307<TwoWire> Rtc(Wire);

void printDateTime(const RtcDateTime& dt);


// handy routine to return true if there was an error
// but it will also print out an error message with the given topic
bool wasError(const char* errorTopic = "")
{
    uint8_t error = Rtc.LastError();
    if (error != 0)
    {
        // we have a communications error
        // see https://www.arduino.cc/reference/en/language/functions/communication/wire/endtransmission/
        // for what the number means
        Serial.print("[");
        Serial.print(errorTopic);
        Serial.print(F("] WIRE communications error ("));
        Serial.print(error);
        Serial.print(") : ");

        switch (error)
        {
        case Rtc_Wire_Error_None:
            Serial.println(F("(none?!)"));
            break;
        case Rtc_Wire_Error_TxBufferOverflow:
            Serial.println(F("transmit buffer overflow"));
            break;
        case Rtc_Wire_Error_NoAddressableDevice:
            Serial.println(F("no device responded"));
            break;
        case Rtc_Wire_Error_UnsupportedRequest:
            Serial.println(F("device doesn't support request"));
            break;
        case Rtc_Wire_Error_Unspecific:
            Serial.println(F("unspecified error"));
            break;
        case Rtc_Wire_Error_CommunicationTimeout:
            Serial.println(F("communications timed out"));
            break;
        }
        return true;
    }
    return false;
}


void rtcsetup (char const *compile_date, char const *compile_time) 
{

    //Serial.print(F("Main.cpp was compiled on: here "));
    //Serial.print(compile_date);
    //Serial.print(" ");
    //Serial.println(compile_time);
    RtcDateTime compiled = RtcDateTime(compile_date, compile_time);

    Rtc.Begin();
    #if defined(WIRE_HAS_TIMEOUT)
        Wire.setWireTimeout(3000 /* us */, true /* reset_on_timeout */);
    #endif

    if (!Rtc.IsDateTimeValid()) 
    {
        if (!wasError("setup IsDateTimeValid"))
        {
            // Common Causes:
            //    1) first time you ran and the device wasn't running yet
            //    2) the battery on the device is low or even missing

            Serial.println("RTC lost confidence in the DateTime!");
            // following line sets the RTC to the date & time this sketch was compiled
            // it will also reset the valid flag internally unless the Rtc device is
            // having an issue

            Rtc.SetDateTime(compiled);
        }
    }

    if (!Rtc.GetIsRunning())
    {
        if (!wasError("GetIsRunning in rtcsetup"))
        {
            Serial.println(F("RTC was not actively running, starting now"));
            Rtc.SetIsRunning(true);
        }
    }

    RtcDateTime now = Rtc.GetDateTime();
    if (!wasError("setup GetDateTime"))
    {

        Serial.print(F("Current date on RTC:"));
        printDateTime(now);

        if (now < compiled)
        {
            Serial.println(F("RTC datetime older than compile datetime, updating RTC"));
            Rtc.SetDateTime(compiled);
        }
        else if (now >= compiled)
        {
            Serial.println(F("; RTC > compile datetime, as expected; init ok"));
        }
    }

    // never assume the Rtc was last configured by you, so
    // just clear them to your needed state
    Rtc.SetSquareWavePin(DS1307SquareWaveOut_Low); 
    wasError("setup SetSquareWavePin");

}

#define countof(a) (sizeof(a) / sizeof(a[0]))

void printDateTime(const RtcDateTime& dt)
{
    char datestring[26];

    snprintf_P(datestring, 
            countof(datestring),
            PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
            dt.Month(),
            dt.Day(),
            dt.Year(),
            dt.Hour(),
            dt.Minute(),
            dt.Second() );
    Serial.print(datestring);
}