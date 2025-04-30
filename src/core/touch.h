#ifndef TOUCH_H // 防止头文件被重复包含
#define TOUCH_H

#include <XPT2046_Touchscreen.h> // 包含 XPT2046 触摸屏库
#include <SPI.h>                 // 包含 SPI 库 (XPT2046 库需要)
#include "../config/config.h"    // 包含项目配置文件，用于引脚定义和校准值

/**
 * @brief 管理 XPT2046 触摸屏的单例类。
 * 处理初始化、读取触摸状态以及获取映射后的坐标。
 */
class Touch {
private:
    XPT2046_Touchscreen ts;     // 触摸屏库的实例对象
    SPIClass touchSPI;          // 触摸专用的 SPI 总线实例 (如果需要，例如 VSPI)
    bool initialized;           // 标记 begin() 是否已被调用并成功初始化
    uint16_t lastX, lastY;      // 存储最后一次成功读取并映射的坐标

    /**
     * @brief 私有构造函数 (用于单例模式)。
     * 初始化触摸屏库实例，传入 CS 引脚和 SPI 总线实例。
     */
    Touch();
    static Touch* instance;     // 指向 Touch 类单例实例的指针

public:
    // 删除拷贝构造函数和赋值操作符，防止复制单例实例
    Touch(const Touch&) = delete;
    Touch& operator=(const Touch&) = delete;

    /**
     * @brief 获取 Touch 类的单例实例。
     * @return Touch 类的引用。
     */
    static Touch& getInstance();

    /**
     * @brief 初始化 SPI 总线和触摸屏控制器。
     * 必须在 setup() 函数中调用一次。
     */
    void begin();

    /**
     * @brief 检查触摸屏当前是否被按下。
     * @return 如果被触摸返回 true，否则返回 false。
     */
    bool isTouched();

    /**
     * @brief 读取当前的触摸坐标，并将其映射到屏幕坐标。
     * 使用在 config.h 中定义的校准值。
     * @param x 用于存储映射后 X 坐标的引用。
     * @param y 用于存储映射后 Y 坐标的引用。
     * @return 如果检测到触摸并且成功读取和映射了坐标，则返回 true。
     * @return 如果未触摸或发生错误，则返回 false。
     */
    bool getPoint(uint16_t& x, uint16_t& y);
};

#endif // TOUCH_H
