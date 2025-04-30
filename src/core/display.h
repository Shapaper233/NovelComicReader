#ifndef DISPLAY_H
#define DISPLAY_H

#include <TFT_eSPI.h>         // 主图形库，用于驱动 TFT 屏幕
#include <SPI.h>              // TFT_eSPI 依赖的 SPI 通信库
#include "../config/config.h" // 包含屏幕尺寸和引脚定义（通过 User_Setup.h 间接配置）
#include "font.h"             // 自定义字体渲染支持

/**
 * @brief 单例类，用于管理 TFT 显示屏。
 * 提供基础的绘图功能和对底层 TFT_eSPI 对象的访问。
 * UI 相关的绘图（如按钮、图标）应由 Page 类处理。
 */
class Display
{
private:
    static Display *instance; // 单例指针实例
    TFT_eSPI tft;             // TFT_eSPI 显示对象，用于所有图形操作

    Display(); // 构造函数私有化，确保单例模式

public:
    // 获取 Display 单例引用
    static Display &getInstance();

    // 初始化显示屏
    void begin();

    // 清屏（填充为黑色）
    void clear();

    // 绘制文本（支持 ASCII 和自定义字体）
    void drawText(const char *text, uint16_t x, uint16_t y, uint8_t size = 2, bool useCustomFont = true);

    // 居中绘制一段文本，指定区域
    void drawCenteredText(const char *text, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t size = 2, bool useCustomFont = true);

    // 绘制单个字符，自动判断是否使用自定义字体
    void drawCharacter(const char *character, uint16_t x, uint16_t y, uint8_t size, bool useCustomFont = true);

    // 绘制进度条（含边框、填充和背景）
    void drawProgressBar(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t progress, uint16_t outlineColor = TFT_WHITE, uint16_t barColor = TFT_GREEN, uint16_t bgColor = TFT_BLACK);

    // 获取底层的 TFT_eSPI 对象，用于更复杂的绘图操作
    TFT_eSPI *getTFT() { return &tft; }

    // 获取屏幕宽度
    uint16_t width() const { return SCREEN_WIDTH; }

    // 获取屏幕高度
    uint16_t height() const { return SCREEN_HEIGHT; }
};

#endif // DISPLAY_H
