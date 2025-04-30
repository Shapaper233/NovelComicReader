#ifndef DISPLAY_H
#define DISPLAY_H

#include <TFT_eSPI.h>         // Main graphics library
#include <SPI.h>              // Required by TFT_eSPI
#include "../config/config.h" // For screen dimensions and pins (via User_Setup.h indirectly)
#include "font.h"             // For custom font rendering support

/**
 * @brief Singleton class managing the TFT display.
 * Provides basic drawing functions and access to the underlying TFT_eSPI object.
 * UI-specific drawing logic (like buttons, icons) should be handled by Page classes.
 */
class Display {
private:
    static Display* instance; // Singleton instance pointer
    TFT_eSPI tft;             // The TFT_eSPI library object
    
    Display();

public:
    static Display& getInstance();
    
    // 初始化显示
    void begin();
    
    // 基本绘图功能
    void clear();
    void drawText(const char* text, uint16_t x, uint16_t y, uint8_t size = 2, bool useCustomFont = true);
    void drawCenteredText(const char* text, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t size = 2, bool useCustomFont = true);
    void drawCharacter(const char* character, uint16_t x, uint16_t y, uint8_t size, bool useCustomFont = true);
    void drawProgressBar(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t progress, uint16_t outlineColor = TFT_WHITE, uint16_t barColor = TFT_GREEN, uint16_t bgColor = TFT_BLACK);
    // Removed: void drawFolder(uint16_t x, uint16_t y, bool isComic = false);
    // Removed: void drawButton(const char* text, uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool isActive = false);

    // 获取底层 TFT_eSPI 对象，允许 Page 类进行更复杂的绘图操作
    TFT_eSPI* getTFT() { return &tft; }

    // 获取屏幕尺寸 (从 config.h 读取)
    uint16_t width() const { return SCREEN_WIDTH; }
    uint16_t height() const { return SCREEN_HEIGHT; }
};

#endif // DISPLAY_H
