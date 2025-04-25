#include "display.h"

Display* Display::instance = nullptr;

Display::Display() {
    // 配置TFT屏幕引脚
    tft.init();
    tft.setRotation(1); // 横屏显示
    tft.fillScreen(TFT_BLACK);
    
    // 配置文本显示
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);     // 基础字体大小
    tft.setTextFont(2);     // 使用Font2 (16像素高度)
    tft.setTextDatum(MC_DATUM); // 设置文本对齐方式为居中
    
    // 设置背光
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
}

Display& Display::getInstance() {
    if (!instance) {
        instance = new Display();
    }
    return *instance;
}

void Display::begin() {
    clear();
}

void Display::clear() {
    tft.fillScreen(TFT_BLACK);
}

void Display::drawText(const char* text, uint16_t x, uint16_t y, uint8_t size) {
    tft.setTextFont(TEXT_FONT);
    tft.setTextSize(size);
    tft.setTextDatum(TL_DATUM);  // 左对齐
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(text, x, y);
}

void Display::drawCenteredText(const char* text, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t size) {
    tft.setTextFont(TEXT_FONT);
    tft.setTextSize(size);
    tft.setTextDatum(MC_DATUM);  // 居中对齐
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(text, x + w/2, y + h/2);
}

void Display::drawFolder(uint16_t x, uint16_t y, bool isComic) {
    const uint16_t folderColor = isComic ? TFT_YELLOW : TFT_CYAN;
    
    // 绘制文件夹图标
    tft.fillRect(x, y, 40, 30, folderColor);
    tft.fillRect(x + 10, y - 5, 20, 5, folderColor);
    
    // 如果是漫画文件夹，添加特殊标记
    if (isComic) {
        tft.drawLine(x + 10, y + 15, x + 30, y + 15, TFT_BLACK);
        tft.drawLine(x + 15, y + 10, x + 15, y + 20, TFT_BLACK);
    }
}

void Display::drawButton(const char* text, uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool isActive) {
    // 绘制按钮背景
    uint16_t bgColor = isActive ? TFT_BLUE : TFT_DARKGREY;
    uint16_t textColor = TFT_WHITE;
    
    tft.fillRoundRect(x, y, w, h, 5, bgColor);
    tft.drawRoundRect(x, y, w, h, 5, textColor);
    
    // 绘制文本
    tft.setTextColor(textColor);
    drawCenteredText(text, x, y, w, h);
    tft.setTextColor(TFT_WHITE, TFT_BLACK); // 恢复默认颜色
}
