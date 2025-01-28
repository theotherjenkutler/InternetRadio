//
// PinControls.h

#ifndef _PINCONTROLS_H_
#define _PINCONTROLS_H_

#include <Arduino.h>

#include <driver/adc.h>

#define GPIO_VOLUME 35
#define GPIO_SWITCH 15

class PinControls
{
    public:
        PinControls();
        ~PinControls();

        void begin();

        void updateVolume();
        boolean getSwitchState();
};

#endif

//
// END OF PinControls.h
