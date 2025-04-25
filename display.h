#ifndef DISPLAY_H
#define DISPLAY_H

#include <TFT_eSPI.h>
#include <SPI.h>
#include "config.h"

class Display {
private:
    static Display* instance;
    TFT_eSPI tft;
    
    Display();

public:
    static Display& getInstance();
    
    // 初始化显示
    void begin();
    
    // 基本绘图功能
    void clear();
    void drawText(const char* text, uint16_t x, uint16_t y, uint8_t size = 2);
    void drawCenteredText(const char* text, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t size = 2);
    void drawFolder(uint16_t x, uint16_t y, bool isComic = false);
    void drawButton(const char* text, uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool isActive = false);
    
    // 获取TFT对象
    TFT_eSPI* getTFT() { return &tft; }
    
    // 获取屏幕尺寸
    uint16_t width() const { return SCREEN_WIDTH; }
    uint16_t height() const { return SCREEN_HEIGHT; }
};

#endif // DISPLAY_H
