#include <Arduino.h> // 引入 Arduino 核心库
#include "display.h" // 引入显示头文件

// 静态成员初始化
Display *Display::instance = nullptr;

// 构造函数（私有，单例模式）
Display::Display()
{
    // 初始化 TFT 屏幕
    tft.init();
    tft.setRotation(1);        // 设置屏幕为横屏模式
    tft.fillScreen(TFT_BLACK); // 全屏填充黑色，清屏

    // 文本颜色配置：白色文字，黑色背景
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);         // 设置基础字体大小（缩放因子）
    tft.setTextFont(2);         // 设置字体为 Font2（16 像素高）
    tft.setTextDatum(MC_DATUM); // 设置文本居中对齐（用于居中绘制）

    // 设置背光引脚为输出，并打开背光
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH); // 打开背光
}

// 获取单例实例
Display &Display::getInstance()
{
    if (!instance)
    {
        instance = new Display();
    }
    return *instance;
}

// 初始化函数，可在外部调用
void Display::begin()
{
    clear(); // 启动时清屏
}

// 清除屏幕内容（填充黑色）
void Display::clear()
{
    tft.fillScreen(TFT_BLACK);
}

// 绘制单个字符（根据是否 ASCII 决定使用内建字体或自定义点阵）
void Display::drawCharacter(const char *character, uint16_t x, uint16_t y, uint8_t size, bool useCustomFont)
{
    if (!useCustomFont || Font::isAscii(character))
    {
        // ASCII 字符使用 TFT 内建字体绘制
        tft.setTextFont(TEXT_FONT);
        tft.setTextSize(size);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextDatum(TL_DATUM); // 设置为左上对齐，便于精确定位
        tft.drawString(character, x, y);
        return;
    }

    // 使用自定义字体渲染非 ASCII 字符（如中文）
    Font &font = Font::getInstance();
    uint8_t *bitmap = font.getCharacterBitmap(character, size * 16); // 获取字符点阵（单位像素）
    if (!bitmap)
    {
        return; // 获取失败则跳过绘制
    }

    // 字符宽高均为 size * 16（保证统一比例）
    uint16_t fontWidth = size * 16;
    uint16_t fontHeight = size * 16;
    uint16_t byteWidth = (fontWidth + 7) / 8; // 每行占用的字节数（按位对齐）

    // 创建颜色位图缓冲区（用于存储转换后的 RGB565 数据）
    uint16_t *colorBitmap = new uint16_t[fontWidth * fontHeight];
    if (!colorBitmap)
    {
        return; // 内存分配失败
    }

    // 将黑白点阵数据转换为彩色位图（RGB565）
    for (uint16_t py = 0; py < fontHeight; py++)
    {
        for (uint16_t px = 0; px < fontWidth; px++)
        {
            uint16_t byteIndex = (py * byteWidth) + (px / 8);                      // 获取对应字节
            uint8_t bitIndex = px % 8;                                             // 获取在该字节中的位位置
            bool isPixelSet = (bitmap[byteIndex] & (1 << bitIndex));               // 判断该像素是否为前景色
            colorBitmap[py * fontWidth + px] = isPixelSet ? TFT_WHITE : TFT_BLACK; // 白字黑底
        }
    }

    // 将整块字符图像一次性绘制到屏幕上（优化性能）
    tft.pushImage(x, y, fontWidth, fontHeight, colorBitmap);

    // 释放动态分配的内存
    delete[] colorBitmap;
}

// 绘制整段文字，支持自定义字体与 ASCII 判断
void Display::drawText(const char *text, uint16_t x, uint16_t y, uint8_t size, bool useCustomFont)
{
    size_t offset = 0;
    uint16_t curX = x;

    while (true)
    {
        String character = Font::getNextCharacter(text, offset); // 提取下一个字符（支持多字节）
        if (character.length() == 0)
            break;

        drawCharacter(character.c_str(), curX, y, size, useCustomFont); // 逐字符绘制

        // 根据字符类型和字体大小计算下一个字符的起始 X 坐标
        uint16_t charWidth;
        if (!useCustomFont || Font::isAscii(character.c_str()))
        {
            charWidth = 8 * size; // ASCII 字符宽度（Font2）
        }
        else
        {
            charWidth = size * 16; // 中文等非 ASCII 字符宽度
        }
        curX += charWidth - size / 4; // 减小间距避免过宽
    }
}

// 在指定区域内居中绘制一段文字
void Display::drawCenteredText(const char *text, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t size, bool useCustomFont)
{
    size_t offset = 0;
    uint16_t totalWidth = 0;
    String temp = text;

    // 首先计算整段文本的总宽度
    while (true)
    {
        String character = Font::getNextCharacter(text, offset);
        if (character.length() == 0)
            break;

        uint16_t charWidth;
        if (!useCustomFont || Font::isAscii(character.c_str()))
        {
            charWidth = 8 * size;
        }
        else
        {
            charWidth = size * 16;
        }
        totalWidth += charWidth - size / 4;
    }

    // 根据区域和总宽度计算起始坐标，实现居中效果
    uint16_t charHeight = size * 16;
    uint16_t startX = x + (w - totalWidth) / 2;
    uint16_t startY = y + (h - charHeight) / 2;

    drawText(text, startX, startY, size, useCustomFont); // 实际绘制
}

// 绘制进度条组件
void Display::drawProgressBar(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t progress, uint16_t outlineColor, uint16_t barColor, uint16_t bgColor)
{
    // 限制进度值在 0~100 范围
    if (progress > 100)
        progress = 100;

    // 绘制边框
    tft.drawRect(x, y, w, h, outlineColor);

    // 计算当前进度对应的填充宽度
    uint16_t barW = (w - 2) * progress / 100;

    // 绘制填充部分
    tft.fillRect(x + 1, y + 1, barW, h - 2, barColor);

    // 绘制剩余未填充部分的背景
    if (barW < (w - 2))
    {
        tft.fillRect(x + 1 + barW, y + 1, (w - 2) - barW, h - 2, bgColor);
    }
}
