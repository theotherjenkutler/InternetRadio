//
// PinControls.cpp

#include "PinControls.h"
#include "src/ToolkitFiles/ToolkitFiles.h"
#include "src/ToolkitWiFi/websocket.h"
#include "src/ToolkitWiFi/http_file.h"

PinControls::PinControls()
{
}

PinControls::~PinControls()
{
    // nada
}

void PinControls::begin()
{
    pinMode(GPIO_SWITCH, INPUT_PULLUP);
//    pinMode(GPIO_VOLUME, INPUT);
//    adcAttachPin(GPIO_VOLUME);
//    adc1_config_width(ADC_WIDTH_12Bit);
//    adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_11);
}

static double readVolumeKnob()
{
    static double old_v = -1.0;
    double v = analogRead(GPIO_VOLUME);
    if (v > 4086) { v = 4086 ;}
    if (v < 6) { v = 6; }
    v = v - 6.0;
    v = v / 40.80;  // 0.0 to 100.0

    // The analog read has a lot of noise on it.
    // To filter this out, change the volume to an integer with a 
    // range of approx. 20
    v = v / 5.0;    // 0.0 to 20.0
    int vi = v;     // this gets rid of noise on the line
    v = vi;
    v = v / 20.0;
    return v;
}

void PinControls::updateVolume()
{
    static double old_v = -1.0;
    double v = readVolumeKnob();
    if (v != old_v) {
        if (-1.0 != old_v) {
            // send it to listen_volume, update VLSI, update WS clients
            // send it as a WS message so that it propogates everywhere
            const char *name = "listen_volume";
            static char value[32];
            sprintf(value, "%1.2f", v);
            SettingItem::updateOrAdd(name, value);
            ToolkitFiles::saveSettings();
            websocket_broadcast(name, (const char *) value);
            ToolkitWiFi_Server::handleWSLiveChanges(name, value);
            Serial.printf("Hardware volume =  %1.2f\n", v);
        }
        old_v = v;
    }  
}

boolean PinControls::getSwitchState()
{
    static int old_s = -1;
    int s = digitalRead(GPIO_SWITCH);
    if (s != old_s) {
        Serial.printf("Hardware switch = %d\n", s);
        old_s = s;
        // the switch will turn on kiosk mode
        http_turnOnKioskMode(s);
    }
}

//
// END OF PinControls.cpp
