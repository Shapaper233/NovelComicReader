#include "pages.h"

// FileBrowserPage implementation
FileBrowserPage::FileBrowserPage()
    : displayManager(Display::getInstance())
    , sdManager(SDCard::getInstance()) {}

void FileBrowserPage::display() {
    displayManager.clear();
    drawHeader();
    drawContent();
    drawFooter();
    drawNavigationButtons();
}

void FileBrowserPage::drawHeader() {
    // 绘制标题栏
    displayManager.drawCenteredText(sdManager.getCurrentPath().c_str(), 0, 0, SCREEN_WIDTH, HEADER_HEIGHT);
    displayManager.getTFT()->drawFastHLine(0, HEADER_HEIGHT - 1, SCREEN_WIDTH, TFT_DARKGREY);
}

void FileBrowserPage::drawContent() {
    const auto& items = sdManager.getCurrentItems();
    size_t startIdx = sdManager.getCurrentPage() * MAX_ITEMS_PER_PAGE;
    size_t endIdx = min(startIdx + MAX_ITEMS_PER_PAGE, items.size());
    
    for (size_t i = startIdx; i < endIdx; i++) {
        const auto& item = items[i];
        uint16_t y = CONTENT_Y + (i - startIdx) * ITEM_HEIGHT;
        
        // 绘制文件夹图标
        if (item.isDirectory) {
            displayManager.drawFolder(5, y + 5, item.isComic);
        }
        
        // 绘制文件名
        displayManager.drawText(item.name.c_str(), 50, y + 10);
    }
}

void FileBrowserPage::drawFooter() {
    // 绘制分页信息
    char pageInfo[32];
    snprintf(pageInfo, sizeof(pageInfo), "Page %d/%d", 
             sdManager.getCurrentPage() + 1, sdManager.getTotalPages());
    
    displayManager.drawCenteredText(pageInfo, 0, 
                            SCREEN_HEIGHT - FOOTER_HEIGHT,
                            SCREEN_WIDTH, FOOTER_HEIGHT);
    
    displayManager.getTFT()->drawFastHLine(0, 
                                   SCREEN_HEIGHT - FOOTER_HEIGHT,
                                   SCREEN_WIDTH, TFT_DARKGREY);
}

void FileBrowserPage::drawNavigationButtons() {
    // 返回按钮
    if (sdManager.getCurrentPath() != "/") {
        displayManager.drawButton("Back", 5, 5, 60, 30);
    }
    
    // 分页按钮
    if (sdManager.getCurrentPage() > 0) {
        displayManager.drawButton("<", 5, 
                          SCREEN_HEIGHT - FOOTER_HEIGHT + 5,
                          30, 30);
    }
    
    if (sdManager.getCurrentPage() < sdManager.getTotalPages() - 1) {
        displayManager.drawButton(">", 
                          SCREEN_WIDTH - 35,
                          SCREEN_HEIGHT - FOOTER_HEIGHT + 5,
                          30, 30);
    }
}

bool FileBrowserPage::handleItemTouch(uint16_t x, uint16_t y) {
    // 计算点击的项目索引
    if (y < CONTENT_Y || y >= SCREEN_HEIGHT - FOOTER_HEIGHT) {
        return false;
    }
    
    size_t itemIdx = (y - CONTENT_Y) / ITEM_HEIGHT;
    size_t actualIdx = sdManager.getCurrentPage() * MAX_ITEMS_PER_PAGE + itemIdx;
    
    const auto& items = sdManager.getCurrentItems();
    if (actualIdx >= items.size()) {
        return false;
    }
    
    const auto& item = items[actualIdx];
    if (item.isDirectory) {
        // 进入文件夹
        sdManager.enterDirectory(item.name);
        display();
        return true;
    }
    
    return false;
}

bool FileBrowserPage::handleNavigationTouch(uint16_t x, uint16_t y) {
    // 返回按钮
    if (y < HEADER_HEIGHT && x < 65 && sdManager.getCurrentPath() != "/") {
        sdManager.goBack();
        display();
        return true;
    }
    
    // 分页按钮
    if (y >= SCREEN_HEIGHT - FOOTER_HEIGHT) {
        if (x < 35 && sdManager.getCurrentPage() > 0) {
            sdManager.prevPage();
            display();
            return true;
        }
        
        if (x >= SCREEN_WIDTH - 35 && 
            sdManager.getCurrentPage() < sdManager.getTotalPages() - 1) {
            sdManager.nextPage();
            display();
            return true;
        }
    }
    
    return false;
}

void FileBrowserPage::handleTouch(uint16_t x, uint16_t y) {
    if (!handleNavigationTouch(x, y)) {
        handleItemTouch(x, y);
    }
}

// ImageViewerPage implementation
ImageViewerPage::ImageViewerPage()
    : displayManager(Display::getInstance())
    , sdManager(SDCard::getInstance()) {}

void ImageViewerPage::display() {
    displayManager.clear();
    loadAndDisplayImage();
}

void ImageViewerPage::loadAndDisplayImage() {
    if (currentImagePath.length() == 0) return;
    
    // 在实际应用中，这里需要实现图片加载和显示逻辑
    // 可以使用JPEGDEC库来解码JPEG图片
    // 具体实现取决于图片格式和显示需求
}

bool ImageViewerPage::handleSwipeGesture(uint16_t x, uint16_t y) {
    // 实现左右滑动切换图片的逻辑
    return false;
}

void ImageViewerPage::handleTouch(uint16_t x, uint16_t y) {
    if (!handleSwipeGesture(x, y)) {
        // 点击屏幕中间区域返回文件浏览页面
        Router::getInstance().goBack();
    }
}

// 工厂函数实现
FileBrowserPage* createFileBrowserPage() {
    return new FileBrowserPage();
}

ImageViewerPage* createImageViewerPage() {
    return new ImageViewerPage();
}
