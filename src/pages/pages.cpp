#include <Arduino.h> // 包含 Arduino 核心库，用于 Serial 等
#include "pages.h"   // 包含页面类的声明

// 初始化 FileBrowserPage 的静态成员 comicViewer
// 这个指针用于缓存 ComicViewerPage 的实例，避免重复创建
// 并在不同页面间共享同一个漫画阅读器实例，可以保留阅读状态
ComicViewerPage *FileBrowserPage::comicViewer = nullptr;


// --- 页面类实现已移动到各自的 .cpp 文件 ---
// FileBrowserPage implementation -> file_browser_page.cpp
// ImageViewerPage implementation -> image_viewer_page.cpp
// ComicViewerPage implementation -> comic_viewer_page.cpp


// --- 页面工厂函数实现 ---
// 这些函数用于创建各种页面类的实例。
// 使用工厂模式可以将对象的创建逻辑与使用逻辑分离。

/**
 * @brief 创建文件浏览器页面的工厂函数。
 * @return 指向新创建的 FileBrowserPage 对象的指针。
 */
FileBrowserPage *createFileBrowserPage()
{
    // 直接使用 new 创建 FileBrowserPage 实例
    return new FileBrowserPage();
}

/**
 * @brief 创建图片查看器页面的工厂函数。
 * @return 指向新创建的 ImageViewerPage 对象的指针。
 */
ImageViewerPage *createImageViewerPage()
{
    // 直接使用 new 创建 ImageViewerPage 实例
    return new ImageViewerPage();
}

ComicViewerPage *createComicViewerPage()
{
    if (FileBrowserPage::comicViewer == nullptr)
    {
        FileBrowserPage::comicViewer = new ComicViewerPage();
    }
    return FileBrowserPage::comicViewer;
}
