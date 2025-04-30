#include "touch.h"    // 包含 Touch 类的头文件
#include <Arduino.h> // 包含 Arduino 核心库，用于 map(), Serial, String, pinMode, digitalWrite 等函数

// 初始化静态单例实例指针为空
Touch* Touch::instance = nullptr;

/**
 * @brief 私有构造函数 (用于单例模式)。
 * 初始化成员变量，包括 XPT2046_Touchscreen 对象 (传入 CS 和 IRQ 引脚)
 * 以及专用的 SPIClass 实例。
 */
Touch::Touch() :
    ts(XPT2046_CS, XPT2046_IRQ), // 使用 CS 和 IRQ 引脚初始化 XPT2046_Touchscreen 对象
    touchSPI(TOUCH_SPI),        // 使用正确的 SPI 总线 (VSPI/HSPI) 初始化 SPIClass 实例
    initialized(false),         // 初始状态设置为未初始化
    lastX(0), lastY(0)          // 初始化最后记录的坐标为 (0, 0)
{}

/**
 * @brief 获取 Touch 类的单例实例。
 * 如果实例尚不存在，则创建它。
 * @return Touch& 对单例实例的引用。
 */
Touch& Touch::getInstance() {
    // 如果实例指针为空
    if (!instance) {
        // 创建一个新的 Touch 实例
        instance = new Touch();
    }
    // 返回实例的引用
    return *instance;
}

/**
 * @brief 初始化触摸屏的 SPI 总线和 XPT2046 控制器。
 * 在成功完成后设置 initialized 标志。
 */
void Touch::begin() {
    // 防止重复初始化
    if (initialized) return;

    Serial.println("正在初始化触摸屏 SPI 和控制器..."); // 打印初始化信息到串口
    // 初始化触摸专用的 SPI 总线
    // 引脚在 config.h 中定义 (XPT2046_CLK, _MISO, _MOSI, _CS)
    touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);

    // 初始化 XPT2046 触摸屏控制器，传入 SPI 总线实例
    ts.begin(touchSPI);
    // 根据显示方向设置旋转。
    // 这确保触摸坐标与显示坐标匹配。
    // 值 '1' 通常对应 ILI9341 的横屏模式。
    ts.setRotation(1);

    initialized = true; // 标记为已初始化
    Serial.println("触摸屏已初始化。"); // 打印初始化完成信息
}

/**
 * @brief 检查屏幕当前是否被触摸。
 * 依赖于底层 XPT2046 库的 touched() 方法。
 * @return 如果被触摸返回 true，否则返回 false。如果未初始化，则返回 false。
 */
bool Touch::isTouched() {
    // 如果尚未初始化
    if (!initialized) {
        Serial.println("警告：在 begin() 之前调用了 Touch::isTouched()"); // 打印警告信息
        return false; // 返回 false
    }
    // 调用库函数检查触摸状态
    return ts.touched();
}

/**
 * @brief 读取原始触摸点，将其映射到屏幕坐标，并存储它们。
 * 使用 map() 函数和 config.h 中的校准值。
 * @param x 用于存储映射后 X 坐标的引用。
 * @param y 用于存储映射后 Y 坐标的引用。
 * @return 如果检测到触摸并且成功读取/映射了坐标，则返回 true。
 * @return 如果未初始化、未触摸或读取失败，则返回 false。
 */
bool Touch::getPoint(uint16_t& x, uint16_t& y) {
    // 如果尚未初始化
    if (!initialized) {
        // Serial.println("警告：在 begin() 之前调用了 Touch::getPoint()"); // 可以取消注释以进行调试
        return false; // 如果未初始化，无法获取点
    }
    // 如果当前未触摸屏幕
    if (!ts.touched()) {
        return false; // 没有检测到触摸
    }

    // 从控制器获取原始触摸点数据
    TS_Point touchp = ts.getPoint();

    // 可选：检查潜在的无效读数 (取决于库/硬件)
    // if (touchp.z < ts.pressureThreshhold) return false; // 示例：检查压力阈值

    // 使用 config.h 中定义的校准值，将原始 X 和 Y 坐标映射到屏幕尺寸
    // map(value, fromLow, fromHigh, toLow, toHigh)
    x = map(touchp.x, TOUCH_MIN_X, TOUCH_MAX_X, 0, SCREEN_WIDTH);
    y = map(touchp.y, TOUCH_MIN_Y, TOUCH_MAX_Y, 0, SCREEN_HEIGHT);

    // 可选：将坐标严格限制在屏幕边界内
    // x = constrain(x, 0, SCREEN_WIDTH - 1);
    // y = constrain(y, 0, SCREEN_HEIGHT - 1);

    // 存储最新的有效映射坐标
    lastX = x;
    lastY = y;

    return true; // 成功读取并映射了一个触摸点
}
