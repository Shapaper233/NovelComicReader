#include "touch.h"
#include <Arduino.h> // For map(), Serial, String, pinMode, digitalWrite etc.

// Initialize static instance pointer for Singleton
Touch* Touch::instance = nullptr;

/**
 * @brief Private constructor for Singleton pattern.
 * Initializes the member variables, including the XPT2046_Touchscreen object
 * with its CS and IRQ pins, and the dedicated SPIClass instance.
 */
Touch::Touch() :
    ts(XPT2046_CS, XPT2046_IRQ), // Initialize XPT2046_Touchscreen with CS and IRQ pins
    touchSPI(TOUCH_SPI),        // Initialize SPIClass instance using the correct SPI bus (VSPI/HSPI)
    initialized(false),         // Start as not initialized
    lastX(0), lastY(0)          // Initialize last coordinates
{}

/**
 * @brief Gets the singleton instance of the Touch class.
 * Creates the instance if it doesn't exist yet.
 * @return Touch& Reference to the singleton instance.
 */
Touch& Touch::getInstance() {
    if (!instance) {
        instance = new Touch();
    }
    return *instance;
}

/**
 * @brief Initializes the SPI bus for the touchscreen and the XPT2046 controller.
 * Sets the initialized flag upon successful completion.
 */
void Touch::begin() {
    // Prevent multiple initializations
    if (initialized) return;

    Serial.println("Initializing Touchscreen SPI and Controller...");
    // Initialize the dedicated SPI bus for touch
    // Pins are defined in config.h (XPT2046_CLK, _MISO, _MOSI, _CS)
    touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);

    // Initialize the XPT2046 touchscreen controller, passing the SPI bus instance
    ts.begin(touchSPI);
    // Set the rotation according to the display orientation.
    // This ensures touch coordinates match the display coordinates.
    // Value '1' typically corresponds to landscape mode for ILI9341.
    ts.setRotation(1);

    initialized = true; // Mark as initialized
    Serial.println("Touchscreen initialized.");
}

/**
 * @brief Checks if the screen is currently being touched.
 * Relies on the underlying XPT2046 library's touched() method.
 * @return true if touched, false otherwise. Returns false if not initialized.
 */
bool Touch::isTouched() {
    if (!initialized) {
        Serial.println("Warning: Touch::isTouched() called before begin()");
        return false;
    }
    return ts.touched();
}

/**
 * @brief Reads the raw touch point, maps it to screen coordinates, and stores them.
 * Uses the map() function and calibration values from config.h.
 * @param x Reference to store the mapped X coordinate.
 * @param y Reference to store the mapped Y coordinate.
 * @return true if a touch was detected and coordinates were read/mapped successfully.
 * @return false if not initialized, not touched, or if reading failed.
 */
bool Touch::getPoint(uint16_t& x, uint16_t& y) {
    if (!initialized) {
        // Serial.println("Warning: Touch::getPoint() called before begin()");
        return false; // Cannot get point if not initialized
    }
    if (!ts.touched()) {
        return false; // No touch detected
    }

    // Get the raw touch point data from the controller
    TS_Point touchp = ts.getPoint();

    // Check for potentially invalid readings (optional, depends on library/hardware)
    // if (touchp.z < ts.pressureThreshhold) return false; // Example pressure check

    // Map the raw X and Y coordinates to the screen dimensions
    // using the calibration values defined in config.h
    x = map(touchp.x, TOUCH_MIN_X, TOUCH_MAX_X, 0, SCREEN_WIDTH);
    y = map(touchp.y, TOUCH_MIN_Y, TOUCH_MAX_Y, 0, SCREEN_HEIGHT);

    // Optional: Constrain coordinates to be strictly within screen bounds
    // x = constrain(x, 0, SCREEN_WIDTH - 1);
    // y = constrain(y, 0, SCREEN_HEIGHT - 1);

    // Store the latest valid mapped coordinates
    lastX = x;
    lastY = y;

    return true; // Successfully read and mapped a touch point
}
