#ifndef TOUCH_H
#define TOUCH_H

#include <XPT2046_Touchscreen.h> // Touchscreen library
#include <SPI.h>                 // Required by XPT2046 library
#include "../config/config.h"    // For pin definitions and calibration values

/**
 * @brief Singleton class for managing the XPT2046 touchscreen.
 * Handles initialization, reading touch status, and getting mapped coordinates.
 */
class Touch {
private:
    XPT2046_Touchscreen ts;     // Instance of the touchscreen library
    SPIClass touchSPI;          // Dedicated SPI bus instance for touch (if needed, e.g., VSPI)
    bool initialized;           // Flag to track if begin() has been called
    uint16_t lastX, lastY;      // Stores the last successfully read and mapped coordinates

    // Private constructor for Singleton pattern
    Touch();
    static Touch* instance;     // Singleton instance pointer

public:
    // Delete copy constructor and assignment operator
    Touch(const Touch&) = delete;
    Touch& operator=(const Touch&) = delete;

    // Get the singleton instance
    static Touch& getInstance();

    /**
     * @brief Initializes the SPI bus and the touchscreen controller.
     * Must be called once in setup().
     */
    void begin();

    /**
     * @brief Checks if the touchscreen is currently being pressed.
     * @return true if touched, false otherwise.
     */
    bool isTouched();

    /**
     * @brief Reads the current touch coordinates and maps them to screen coordinates.
     * Uses calibration values defined in config.h.
     * @param x Reference to store the mapped X coordinate.
     * @param y Reference to store the mapped Y coordinate.
     * @return true if a touch was detected and coordinates were successfully read and mapped.
     * @return false if not touched or an error occurred.
     */
    bool getPoint(uint16_t& x, uint16_t& y);
};

#endif // TOUCH_H
