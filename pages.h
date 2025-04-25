#ifndef PAGES_H
#define PAGES_H

#include "router.h"
#include "display.h"
#include "sdcard.h"

// 文件浏览页面
class FileBrowserPage : public Page {
private:
    Display& displayManager;
    SDCard& sdManager;
    
    // UI元素位置
    static constexpr uint16_t HEADER_HEIGHT = 40;
    static constexpr uint16_t FOOTER_HEIGHT = 40;
    static constexpr uint16_t CONTENT_Y = HEADER_HEIGHT;
    static constexpr uint16_t CONTENT_HEIGHT = SCREEN_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT;
    
    // 绘制UI元素
    void drawHeader();
    void drawContent();
    void drawFooter();
    void drawNavigationButtons();
    
    // 处理触摸事件
    bool handleItemTouch(uint16_t x, uint16_t y);
    bool handleNavigationTouch(uint16_t x, uint16_t y);

public:
    FileBrowserPage();
    virtual void display() override;
    virtual void handleTouch(uint16_t x, uint16_t y) override;
};

// 图片查看页面（用于显示漫画）
class ImageViewerPage : public Page {
private:
    Display& displayManager;
    SDCard& sdManager;
    
    String currentImagePath;
    
    void loadAndDisplayImage();
    bool handleSwipeGesture(uint16_t x, uint16_t y);

public:
    ImageViewerPage();
    virtual void display() override;
    virtual void handleTouch(uint16_t x, uint16_t y) override;
    
    void setImagePath(const String& path) { currentImagePath = path; }
};

// 创建页面实例的工厂函数
FileBrowserPage* createFileBrowserPage();
ImageViewerPage* createImageViewerPage();

#endif // PAGES_H
