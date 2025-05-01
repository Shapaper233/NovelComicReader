#include <Arduino.h> // 包含 Arduino 核心库，用于 Serial 等
#include "pages.h"   // 包含页面类的声明
#include "text_viewer_page.h" // Include TextViewerPage header here
#include "menu_page.h" // Include MenuPage header here

// Static member definition removed as the member itself was removed from FileBrowserPage


// --- 页面类实现已移动到各自的 .cpp 文件 ---
// FileBrowserPage implementation -> file_browser_page.cpp
// ImageViewerPage implementation -> image_viewer_page.cpp
// ComicViewerPage implementation -> comic_viewer_page.cpp


// --- 页面工厂函数实现 ---
// 这些函数用于创建各种页面类的实例。
// 使用工厂模式可以将对象的创建逻辑与使用逻辑分离。

/**
 * @brief 创建文件浏览器页面的工厂函数。
 * @return 指向新创建的 FileBrowserPage 对象的指针 (作为 Page*)。
 */
Page *createFileBrowserPage() // Return Page*
{
    // 直接使用 new 创建 FileBrowserPage 实例，隐式转换为 Page*
    return new FileBrowserPage();
}

/**
 * @brief 创建图片查看器页面的工厂函数。
 * @return 指向新创建的 ImageViewerPage 对象的指针 (作为 Page*)。
 */
Page *createImageViewerPage() // Return Page*
{
    // 直接使用 new 创建 ImageViewerPage 实例，隐式转换为 Page*
    return new ImageViewerPage();
}

/**
 * @brief 创建文本阅读器页面的工厂函数。
 * @return 指向新创建的 TextViewerPage 对象的指针 (作为 Page*)。
 */
Page *createTextViewerPage() // Return Page*
{
    // 直接使用 new 创建 TextViewerPage 实例，隐式转换为 Page*
    return new TextViewerPage();
}


/**
 * @brief 创建漫画阅读器页面的工厂函数。
 * @return 指向新创建的 ComicViewerPage 对象的指针 (作为 Page*)。
 */
Page *createComicViewerPage() // Return Page*
{
    // Remove singleton logic, always create a new instance，隐式转换为 Page*
    return new ComicViewerPage();
}

/**
 * @brief 创建菜单页面的工厂函数。
 * @return 指向新创建的 MenuPage 对象的指针 (作为 Page*)。
 */
Page *createMenuPage() // Return Page*
{
    // 直接使用 new 创建 MenuPage 实例，隐式转换为 Page*
    return new MenuPage();
}
