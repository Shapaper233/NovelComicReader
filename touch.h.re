#ifndef TOUCH_H
#define TOUCH_H

#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <cstdint>
#include "config.h"

class TouchManager {
public:
    static void begin();
    static bool touched();
    static TS_Point getPoint();
    static uint16_t mapTouchX(uint16_t x);
    static uint16_t mapTouchY(uint16_t y);

private:
    static XPT2046_Touchscreen ts;
    static SPIClass touchSPI;
    static bool initialized;
};

#endif // TOUCH_H
