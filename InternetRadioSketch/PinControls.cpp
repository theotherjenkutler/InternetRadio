//
// PinControls.cpp

#include "PinControls.h"

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

void PinControls::updateVolume()
{
    static double old_v = -1.0;
    double v = analogRead(GPIO_VOLUME);
    if (v > 4086) { v = 4086 ;}
    if (v < 6) { v = 6; }
    v = v - 6.0;
    v = v / 40.80;  // 0.0 to 100.0
    int vi = v;
    v = vi;
    v = v / 100.0;
    if (v != old_v) {
        Serial.printf("Hardware volume =  %2.3f\n", v);
        old_v = v;
        // send it to listen_volume, update VLSI, update WS clients
        // send it as a WS message so that it propogates everywhere
    }
}

boolean PinControls::getSwitchState()
{
    static int old_s = -1;
    int s = digitalRead(GPIO_SWITCH);
    if (s != old_s) {
        Serial.printf("Hardware switch = %d\n", s);
        old_s = s;
        // TODO: what do want to use the switch for ?? kiosk mode maybe ??
    }
}

//
// END OF PinControls.cpp
