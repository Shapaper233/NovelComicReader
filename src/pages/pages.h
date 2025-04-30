#ifndef PAGES_H
#define PAGES_H

#include <Arduino.h> // 包含 Arduino 核心库，提供 String, Serial 等基础功能
#include <vector>    // 包含 std::vector 模板类

// 前向声明 (Forward Declarations)
class Page; // Forward declare Page for Router
// Forward declaration for TextViewerPage (defined in text_viewer_page.h)
class TextViewerPage; // Keep this if needed elsewhere, or move if possible

// 包含项目内其他模块的头文件
#include "../core/router.h"    // 页面路由系统
#include "../core/display.h"   // 显示管理 (封装 TFT_eSPI)
#include "../core/sdcard.h"    // SD 卡文件系统管理
#include "../core/touch.h"     // 触摸管理

// 页面基类 (Moved definition before FileBrowserPage)
class Page
{
public:
    virtual void display() = 0;
    virtual void handleTouch(uint16_t x, uint16_t y) = 0;
    // Add a virtual method to receive parameters after creation
    virtual void setParams(void* params) {}
    // Add a virtual method for explicit resource cleanup before destruction
    virtual void cleanup() {}
    virtual ~Page() = default;
};


/**
 * @brief 文件浏览器页面类
 * 负责显示 SD 卡中的文件和目录列表，处理导航和文件/目录选择。
 */
class FileBrowserPage : public Page {
private:
    Display& displayManager; // 显示管理器引用
    SDCard& sdManager;       // SD 卡管理器引用
    // 静态成员，用于缓存 ComicViewerPage 实例 (实现单例模式)
    // static ComicViewerPage* comicViewer; // Removed static instance

    // --- UI 布局常量 ---
    static constexpr uint16_t HEADER_HEIGHT = 40; // 顶部标题栏高度
    static constexpr uint16_t FOOTER_HEIGHT = 40; // 底部状态栏高度
    static constexpr uint16_t CONTENT_Y = HEADER_HEIGHT; // 内容区域起始 Y 坐标
    // 内容区域高度 (屏幕高度 - 头部 - 底部)
    static constexpr uint16_t CONTENT_HEIGHT = SCREEN_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT;
    // 列表项高度和每页最大项数在 sdcard.h 中定义 (ITEM_HEIGHT, MAX_ITEMS_PER_PAGE)

    // --- 私有辅助函数 ---
    void drawHeader();          // 绘制标题栏 (当前路径)
    void drawContent();         // 绘制文件/目录列表
    void drawFooter();          // 绘制状态栏 (分页信息)
    void drawNavigationButtons(); // 绘制导航按钮 (返回, 上/下一页)
    // Added declarations for helper drawing functions:
    void _drawFolder(uint16_t x, uint16_t y, bool isComic);
    void _drawTextFile(uint16_t x, uint16_t y); // Declaration for text file icon helper
    void _drawButton(const char *text, uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool isActive = false);

    // 处理特定区域的触摸事件
    bool handleItemTouch(uint16_t x, uint16_t y);     // 处理文件/目录项点击
    bool handleNavigationTouch(uint16_t x, uint16_t y); // 处理导航按钮点击

public:
    /**
     * @brief FileBrowserPage 构造函数
     */
    FileBrowserPage();

    /**
     * @brief 显示页面内容 (重写 Page 基类方法)
     */
    virtual void display() override;

    /**
     * @brief 处理触摸事件 (重写 Page 基类方法)
     * @param x 触摸点 X 坐标
     * @param y 触摸点 Y 坐标
     */
    virtual void handleTouch(uint16_t x, uint16_t y) override;

    /**
     * @brief 获取 SD 卡管理器实例 (可能用于其他类访问)
     * @return SDCard& SD 卡管理器引用
     */
    SDCard& getSDCard() { return sdManager; }

    // 友元声明，允许工厂函数访问私有/保护成员 (特别是 comicViewer)
    // friend ComicViewerPage* createComicViewerPage(); // Removed friend declaration
};


/**
 * @brief 图片查看器页面类 (当前为占位符)
 * 设计用于显示单张图片，例如从文件浏览器打开的非漫画图片。
 */
class ImageViewerPage : public Page {
private:
    Display& displayManager; // 显示管理器引用
    SDCard& sdManager;       // SD 卡管理器引用

    String currentImagePath; // 当前要显示的图片路径

    // --- 私有辅助函数 ---
    void loadAndDisplayImage(); // 加载并显示图片 (待实现)
    bool handleSwipeGesture(uint16_t x, uint16_t y); // 处理滑动切换图片 (待实现)

public:
    /**
     * @brief ImageViewerPage 构造函数
     */
    ImageViewerPage();

    /**
     * @brief 显示页面内容 (重写 Page 基类方法)
     */
    virtual void display() override;

    /**
     * @brief 处理触摸事件 (重写 Page 基类方法)
     * @param x 触摸点 X 坐标
     * @param y 触摸点 Y 坐标
     */
    virtual void handleTouch(uint16_t x, uint16_t y) override;

    /**
     * @brief 设置要显示的图片路径
     * @param path 图片文件的完整路径
     */
    void setImagePath(const String& path);
};


/**
 * @brief 文本阅读器页面类 (当前未分离实现)
 * 设计用于显示文本文件内容，支持分页和章节导航。
// Forward declaration for TextViewerPage (defined in text_viewer_page.h)
class TextViewerPage;

/**
 * @brief 漫画阅读器页面类
 * 负责加载、显示和滚动浏览漫画图片序列。
 */
class ComicViewerPage : public Page {
private:
    Display& displayManager; // 显示管理器引用
    SDCard& sdManager;       // SD 卡管理器引用
    Touch& touchManager;     // 触摸管理器引用
    String currentPath;      // 当前打开的漫画目录路径
    int scrollOffset;        // 当前垂直滚动偏移量 (从漫画顶部开始的像素)
    std::vector<String> imageFiles; // 漫画图片文件路径列表 (e.g., "1.bmp", "2.bmp")
    std::vector<int> imageHeights;  // 缓存的每张图片的高度
    int totalComicHeight;           // 缓存的所有图片的总高度

    // --- 私有辅助函数 ---
    /**
     * @brief 加载漫画目录下的所有图片，并计算缓存高度。
     */
    void loadImages();

    /**
     * @brief 绘制当前视口的漫画内容。
     * @return true if drawing was interrupted by touch, false otherwise.
     */
    bool drawContent();

    /**
     * @brief 处理滚动触摸手势和双击返回。
     * @param x 触摸点 X 坐标
     * @param y 触摸点 Y 坐标
     * @return true 如果事件被处理 (滚动或返回)
     * @return false 如果事件未被处理
     */
    bool handleScrollGesture(uint16_t x, uint16_t y);

    /**
     * @brief 执行屏幕滚动（优化版）。
     * @param scrollDelta 滚动距离 (+向下, -向上)
     */
    void scrollDisplay(int scrollDelta);

    /**
     * @brief 绘制屏幕上新暴露的区域。
     * @param y 新区域的屏幕起始 Y 坐标
     * @param h 新区域的高度
     * @return true if drawing was interrupted by touch, false otherwise.
     */
    bool drawNewArea(int y, int h);

    /**
     * @brief Helper function to process a touch event that interrupted drawing.
     */
    void handleTouchInterrupt();

public:
    /**
     * @brief ComicViewerPage 构造函数
     */
    ComicViewerPage();

    /**
     * @brief 显示页面内容 (重写 Page 基类方法)
     */
    virtual void display() override;

    /**
     * @brief 处理触摸事件 (重写 Page 基类方法)
     * @param x 触摸点 X 坐标
     * @param y 触摸点 Y 坐标
     */
    virtual void handleTouch(uint16_t x, uint16_t y) override;

    /**
     * @brief 设置要显示的漫画目录路径。
     * @param path 漫画目录的完整路径
     */
    void setComicPath(const String& path);

    /**
     * @brief 设置页面参数 (重写 Page 基类方法)
     * @param params 指向参数的 void 指针 (预期为 String*)
     */
    virtual void setParams(void* params) override;

    /**
     * @brief 清理页面资源 (重写 Page 基类方法)
     */
    virtual void cleanup() override; // Added cleanup declaration
};


// --- 页面工厂函数声明 ---
// 提供创建不同页面实例的标准接口。

/**
 * @brief 创建文件浏览器页面实例。
 * @return FileBrowserPage* 指向新实例的指针。
 */
FileBrowserPage* createFileBrowserPage();

/**
 * @brief 创建图片查看器页面实例。
 * @return ImageViewerPage* 指向新实例的指针。
 */
ImageViewerPage* createImageViewerPage();

/**
 * @brief 创建文本阅读器页面实例。
 * @return TextViewerPage* 指向新实例的指针。
 */
TextViewerPage* createTextViewerPage(); // Declaration added for consistency

/**
 * @brief 创建或获取漫画阅读器页面实例 (单例)。
 * @return ComicViewerPage* 指向实例的指针。
 */
ComicViewerPage* createComicViewerPage();


#endif // PAGES_H
