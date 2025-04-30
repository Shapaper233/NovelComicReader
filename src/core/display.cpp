#include <Arduino.h> // Include Arduino core headers
#include "display.h"

Display *Display::instance = nullptr;

Display::Display()
{
    // 配置TFT屏幕引脚
    tft.init();
    tft.setRotation(1); // 横屏显示
    tft.fillScreen(TFT_BLACK);

    // 配置文本显示
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);         // 基础字体大小
    tft.setTextFont(2);         // 使用Font2 (16像素高度)
    tft.setTextDatum(MC_DATUM); // 设置文本对齐方式为居中

    // 设置背光
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
}

Display &Display::getInstance()
{
    if (!instance)
    {
        instance = new Display();
    }
    return *instance;
}

void Display::begin()
{
    clear();
}

void Display::clear()
{
    tft.fillScreen(TFT_BLACK);
}

void Display::drawCharacter(const char *character, uint16_t x, uint16_t y, uint8_t size, bool useCustomFont)
{
    if (!useCustomFont || Font::isAscii(character))
    {
        // ASCII字符使用内置字体
        tft.setTextFont(TEXT_FONT);
        tft.setTextSize(size);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextDatum(TL_DATUM); // 设置为左对齐以确保位置准确
        tft.drawString(character, x, y);
        return;
    }

    // 非ASCII字符使用自定义点阵字体
    Font &font = Font::getInstance();
    uint8_t *bitmap = font.getCharacterBitmap(character, size * 16); // 将size转换为实际像素大小
    if (!bitmap)
    {
        return; // 字体数据加载失败
    }

    // 计算字体大小
    uint16_t fontWidth = size * 16; // 保持一致的字符宽度
    uint16_t fontHeight = size * 16;
    uint16_t byteWidth = (fontWidth + 7) / 8;

    // --- Optimized drawing using pushImage ---
    // Create a buffer for 16-bit color data (RGB565)
    uint16_t *colorBitmap = new uint16_t[fontWidth * fontHeight];
    if (!colorBitmap) {
        // Failed to allocate memory
        return;
    }

    // Convert monochrome bitmap to 16-bit color bitmap
    for (uint16_t py = 0; py < fontHeight; py++)
    {
        for (uint16_t px = 0; px < fontWidth; px++)
        {
            uint16_t byteIndex = (py * byteWidth) + (px / 8);
            uint8_t bitIndex = px % 8;
            bool isPixelSet = (bitmap[byteIndex] & (1 << bitIndex));
            colorBitmap[py * fontWidth + px] = isPixelSet ? TFT_WHITE : TFT_BLACK; // Use background color for '0' bits
        }
    }

    // Push the entire character bitmap at once
    tft.pushImage(x, y, fontWidth, fontHeight, colorBitmap);

    // Clean up the allocated buffer
    delete[] colorBitmap;
    // --- End of optimized drawing ---
}

void Display::drawText(const char *text, uint16_t x, uint16_t y, uint8_t size, bool useCustomFont)
{
    size_t offset = 0;
    uint16_t curX = x;

    while (true)
    {
        String character = Font::getNextCharacter(text, offset);
        if (character.length() == 0)
            break;

        drawCharacter(character.c_str(), curX, y, size, useCustomFont);

        // 计算下一个字符的位置
        uint16_t charWidth;
        if (!useCustomFont || Font::isAscii(character.c_str()))
        {
            charWidth = 8 * size; // ASCII字符宽度（Font2的标准宽度）
        }
        else
        {
            charWidth = size * 16; // 中文字符宽度与高度相同
        }
        curX += charWidth - size / 4; // 减小字符间距
    }
}

void Display::drawCenteredText(const char *text, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t size, bool useCustomFont)
{
    // 计算文本总宽度
    size_t offset = 0;
    uint16_t totalWidth = 0;
    String temp = text;

    while (true)
    {
        String character = Font::getNextCharacter(text, offset);
        if (character.length() == 0)
            break;

        uint16_t charWidth;
        if (!useCustomFont || Font::isAscii(character.c_str()))
        {
            charWidth = 8 * size; // ASCII字符宽度
        }
        else
        {
            charWidth = size * 16; // 中文字符宽度
        }
        totalWidth += charWidth - size / 4; // 减小字符间距
    }

    // 居中绘制
    uint16_t charHeight = size * 16; // 统一使用16作为基准高度
    uint16_t startX = x + (w - totalWidth) / 2;
    uint16_t startY = y + (h - charHeight) / 2; // 使用标准居中计算
    drawText(text, startX, startY, size, useCustomFont);
}

// Removed drawFolder implementation
// Removed drawButton implementation

void Display::drawProgressBar(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t progress, uint16_t outlineColor, uint16_t barColor, uint16_t bgColor) {
    // Clamp progress to 0-100
    if (progress > 100) progress = 100;

    // Draw outline
    tft.drawRect(x, y, w, h, outlineColor);

    // Calculate bar width
    uint16_t barW = (w - 2) * progress / 100; // Subtract 2 for outline thickness

    // Draw the progress bar fill
    tft.fillRect(x + 1, y + 1, barW, h - 2, barColor);

    // Draw the background fill for the remaining part
    if (barW < (w - 2)) {
        tft.fillRect(x + 1 + barW, y + 1, (w - 2) - barW, h - 2, bgColor);
    }
}
