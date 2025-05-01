#include <Arduino.h>

#include "pages.h"     // 包含页面基类和相关定义 (Correct path)
#include "../core/display.h"   // 包含显示管理类 (Adjusted path)
#include "../core/sdcard.h"    // 包含 SD 卡管理类 (Adjusted path)
#include "../core/router.h"    // 包含页面路由类 (Adjusted path)
#include "../config/config.h"    // 包含配置常量 (Adjusted path)

// ImageViewerPage 类实现 (当前为占位符，未实现具体功能)

// 构造函数：初始化显示管理器和 SD 卡管理器的实例引用
ImageViewerPage::ImageViewerPage()
    : displayManager(Display::getInstance()), sdManager(SDCard::getInstance()) {}

// 显示图片查看器页面的主函数
void ImageViewerPage::display()
{
    displayManager.clear(); // 清屏
    loadAndDisplayImage();  // 加载并显示图片（当前未实现）
}

// 加载并显示图片（占位符函数）
void ImageViewerPage::loadAndDisplayImage()
{
    // 检查是否有图片路径被设置
    if (currentImagePath.length() == 0)
        return; // 没有路径，直接返回

    // TODO: 实现图片加载和显示逻辑
    // - 需要根据图片格式（如 BMP, JPG）选择合适的库（如 TFT_eSPI 自带的 BMP 绘制，或 JPEGDEC 库）
    // - 从 SD 卡读取图片文件
    // - 将图片绘制到屏幕上
    displayManager.drawCenteredText("Image Loading Not Implemented", 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
}

// 处理滑动操作（占位符函数）
bool ImageViewerPage::handleSwipeGesture(uint16_t x, uint16_t y)
{
    // TODO: 实现左右滑动切换图片的逻辑
    // - 检测触摸轨迹判断是左滑还是右滑
    // - 更新 currentImagePath 指向上一张或下一张图片
    // - 调用 display() 重新显示
    return false; // 默认未处理
}

// 处理页面上的所有触摸事件
void ImageViewerPage::handleTouch(uint16_t x, uint16_t y)
{
    // 尝试处理滑动事件
    if (!handleSwipeGesture(x, y))
    {
        // 如果不是滑动事件（或者滑动未处理），则认为是在屏幕中间点击
        // 点击屏幕中间区域返回文件浏览页面
        Router::getInstance().goBack();
    }
}

// 设置要显示的图片路径
void ImageViewerPage::setImagePath(const String &path)
{
    currentImagePath = path;
}

void ImageViewerPage::handleLoop() {
    // Currently no periodic tasks needed for image viewer
    return;
}
