#include <Arduino.h>
#include <FS.h>        // 用于文件系统操作，例如 File 类型
#include <algorithm>   // 用于 std::min 和 std::max

#include "pages.h"     // 包含页面基类和相关定义 (Correct path)
#include "../core/display.h"   // 包含显示管理类 (Adjusted path)
#include "../core/sdcard.h"    // 包含 SD 卡管理类 (Adjusted path)
#include "../core/router.h"    // 包含页面路由类 (Adjusted path)
#include "../config/config.h"    // 包含配置常量 (Adjusted path)

// FileBrowserPage 类实现

// 构造函数：初始化显示管理器和 SD 卡管理器的实例引用
FileBrowserPage::FileBrowserPage()
    : displayManager(Display::getInstance()), sdManager(SDCard::getInstance()) {}

// 显示文件浏览器页面的主函数
void FileBrowserPage::display()
{
    displayManager.clear(); // 清屏
    drawHeader();           // 绘制顶部标题栏
    drawContent();          // 绘制文件/目录列表
    drawFooter();           // 绘制底部状态栏
    drawNavigationButtons(); // 绘制导航按钮（返回、上一页、下一页）
}

// 绘制页面顶部标题栏
void FileBrowserPage::drawHeader()
{
    // 在屏幕顶部居中显示当前路径
    displayManager.drawCenteredText(sdManager.getCurrentPath().c_str(), 0, 0, SCREEN_WIDTH, HEADER_HEIGHT, 1);
    // 在标题栏下方绘制一条分隔线
    displayManager.getTFT()->drawFastHLine(0, HEADER_HEIGHT - 1, SCREEN_WIDTH, TFT_DARKGREY);
}

// --- Private Helper Functions for Drawing ---

// Helper function to draw a folder icon
void FileBrowserPage::_drawFolder(uint16_t x, uint16_t y, bool isComic)
{
    TFT_eSPI* tft = displayManager.getTFT(); // Get TFT object
    const uint16_t folderColor = isComic ? TFT_YELLOW : TFT_CYAN;

    // 绘制文件夹图标
    tft->fillRect(x, y, 40, 30, folderColor);
    tft->fillRect(x + 10, y - 5, 20, 5, folderColor);

    // 如果是漫画文件夹，添加特殊标记
    if (isComic)
    {
        tft->drawLine(x + 10, y + 15, x + 30, y + 15, TFT_BLACK);
        tft->drawLine(x + 15, y + 10, x + 15, y + 20, TFT_BLACK);
    }
}

// Helper function to draw a button
void FileBrowserPage::_drawButton(const char *text, uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool isActive)
{
    TFT_eSPI* tft = displayManager.getTFT(); // Get TFT object
    // 绘制按钮背景
    uint16_t bgColor = isActive ? TFT_BLUE : TFT_DARKGREY;
    uint16_t textColor = TFT_WHITE;

    tft->fillRoundRect(x, y, w, h, 5, bgColor);
    tft->drawRoundRect(x, y, w, h, 5, textColor);

    // 绘制文本 (using displayManager's text functions)
    // Note: We need to manage text color/datum within this scope if needed,
    // or rely on displayManager's state if appropriate.
    // For simplicity, using drawCenteredText which handles its own state.
    uint16_t textSize = 1; // Adjust size as needed for buttons
    // Calculate vertical center for text based on button height and assumed text height (Font 2 = 16px)
    uint16_t textY = y + (h - 16 * textSize) / 2;
    // Use displayManager's text drawing function
    displayManager.drawCenteredText(text, x, y, w, h, textSize, true);
    // Restore default text color if necessary (drawCenteredText might change it)
    // tft->setTextColor(TFT_WHITE, TFT_BLACK); // Example if needed
}


// --- Public Methods ---

// 绘制页面中间的文件/目录列表区域
void FileBrowserPage::drawContent()
{
    const auto &items = sdManager.getCurrentItems(); // 获取当前目录下的所有项目
    // 计算当前页需要显示的项目起始和结束索引
    size_t startIdx = sdManager.getCurrentPage() * MAX_ITEMS_PER_PAGE;
    size_t endIdx = std::min(startIdx + MAX_ITEMS_PER_PAGE, items.size()); // 使用 std::min 确保不越界

    // 遍历并绘制当前页的项目
    for (size_t i = startIdx; i < endIdx; i++)
    {
        const auto &item = items[i];
        // 计算当前项目绘制的 Y 坐标
        uint16_t y = CONTENT_Y + (i - startIdx) * ITEM_HEIGHT;

        // 如果是目录，绘制文件夹图标（区分普通目录和漫画目录）
        if (item.isDirectory)
        {
            _drawFolder(5, y + 5, item.isComic); // Use helper function
        }

        // 绘制文件名/目录名
        displayManager.drawText(item.name.c_str(), 50, y + 10, 1);
    }
}

// 绘制页面底部状态栏（分页信息）
void FileBrowserPage::drawFooter()
{
    // 格式化分页信息字符串 "Page X/Y"
    char pageInfo[32];
    snprintf(pageInfo, sizeof(pageInfo), "Page %d/%d",
             sdManager.getCurrentPage() + 1, sdManager.getTotalPages());

    // 在屏幕底部居中显示分页信息
    displayManager.drawCenteredText(pageInfo, 0,
                                    SCREEN_HEIGHT - FOOTER_HEIGHT,
                                    SCREEN_WIDTH, FOOTER_HEIGHT);

    // 在状态栏上方绘制一条分隔线
    displayManager.getTFT()->drawFastHLine(0,
                                           SCREEN_HEIGHT - FOOTER_HEIGHT,
                                           SCREEN_WIDTH, TFT_DARKGREY);
}

// 绘制导航按钮（返回、上一页、下一页）
void FileBrowserPage::drawNavigationButtons()
{
    // 如果当前不在根目录，绘制“返回”按钮
    if (sdManager.getCurrentPath() != "/")
    {
        _drawButton("Back", 5, 5, 60, 30); // Use helper function
    }

    // 如果当前不是第一页，绘制“上一页”按钮
    if (sdManager.getCurrentPage() > 0)
    {
        _drawButton("<", 5,
                                  SCREEN_HEIGHT - FOOTER_HEIGHT + 5,
                                  30, 30); // Use helper function
    }

    // 如果当前不是最后一页，绘制“下一页”按钮
    if (sdManager.getCurrentPage() < sdManager.getTotalPages() - 1)
    {
        _drawButton(">",
                                  SCREEN_WIDTH - 35,
                                  SCREEN_HEIGHT - FOOTER_HEIGHT + 5,
                                  30, 30); // Use helper function
    }
}

// 处理文件/目录项目的触摸事件
bool FileBrowserPage::handleItemTouch(uint16_t x, uint16_t y)
{
    // 检查触摸点是否在内容区域内
    if (y < CONTENT_Y || y >= SCREEN_HEIGHT - FOOTER_HEIGHT)
    {
        return false; // 不在内容区，未处理
    }

    // 根据 Y 坐标计算点击了哪个项目
    size_t itemIdx = (y - CONTENT_Y) / ITEM_HEIGHT;
    // 计算项目在整个列表中的实际索引
    size_t actualIdx = sdManager.getCurrentPage() * MAX_ITEMS_PER_PAGE + itemIdx;

    const auto &items = sdManager.getCurrentItems();
    // 检查计算出的索引是否有效
    if (actualIdx >= items.size())
    {
        return false; // 无效索引，未处理
    }

    const auto &item = items[actualIdx]; // 获取被点击的项目
    Serial.print("Clicked item: ");
    Serial.println(item.name);

    // 判断点击的是目录还是文件
    if (item.isDirectory)
    {
        Serial.print("Is directory, isComic: ");
        Serial.println(item.isComic ? "yes" : "no");
        Serial.print("Item name: ");
        Serial.println(item.name);

        // 如果是标记为漫画的目录
        if (item.isComic)
        {
            // 构建漫画目录的完整路径
            String fullPath = sdManager.getCurrentPath();
            if (fullPath != "/")
            {
                fullPath += "/";
            }
            fullPath += item.name;

            Serial.print("Opening comic at path: ");
            Serial.println(fullPath);

            // 获取或创建 ComicViewerPage 实例
            // 注意：comicViewer 是 FileBrowserPage 的静态成员，用于缓存 ComicViewerPage 实例
            // 这样可以避免每次打开漫画都重新创建页面对象，并保留阅读进度等状态
            if (comicViewer == nullptr)
            {
                Serial.println("Creating new ComicViewerPage");
                comicViewer = createComicViewerPage(); // 调用工厂函数创建
            }
            else
            {
                Serial.println("Reusing existing ComicViewerPage");
            }
            // 设置漫画阅读器要加载的路径
            comicViewer->setComicPath(fullPath);

            // 使用 Router 导航到漫画阅读器页面
            Router::getInstance().navigateTo("comic", comicViewer);
            Serial.println("Comic viewer opened");
        }
        else // 如果是普通目录
        {
            // 进入该目录
            Serial.print("Entering directory: ");
            Serial.println(item.name);
            sdManager.enterDirectory(item.name);
            display(); // 重新绘制文件浏览器页面以显示新目录内容
        }
        return true; // 触摸事件已处理
    }
    // 如果点击的是文件（当前逻辑下文件不可点击，直接返回 false）
    // 未来可以扩展为打开文件，例如图片文件使用 ImageViewerPage
    return false; // 文件点击事件未处理
}

// 处理导航按钮（返回、上一页、下一页）的触摸事件
bool FileBrowserPage::handleNavigationTouch(uint16_t x, uint16_t y)
{
    // 检查是否点击了“返回”按钮区域（左上角）
    if (y < HEADER_HEIGHT && x < 65 && sdManager.getCurrentPath() != "/")
    {
        sdManager.goBack(); // 返回上一级目录
        display();          // 重新绘制页面
        return true;        // 触摸事件已处理
    }

    // 检查是否点击了分页按钮区域（底部）
    if (y >= SCREEN_HEIGHT - FOOTER_HEIGHT)
    {
        // 检查是否点击了“上一页”按钮（左下角）
        if (x < 35 && sdManager.getCurrentPage() > 0)
        {
            sdManager.prevPage(); // 切换到上一页
            display();            // 重新绘制页面
            return true;          // 触摸事件已处理
        }

        // 检查是否点击了“下一页”按钮（右下角）
        if (x >= SCREEN_WIDTH - 35 &&
            sdManager.getCurrentPage() < sdManager.getTotalPages() - 1)
        {
            sdManager.nextPage(); // 切换到下一页
            display();            // 重新绘制页面
            return true;          // 触摸事件已处理
        }
    }

    return false; // 触摸点不在任何导航按钮区域，未处理
}

// 处理页面上的所有触摸事件
void FileBrowserPage::handleTouch(uint16_t x, uint16_t y)
{
    // 优先处理导航按钮的触摸
    if (!handleNavigationTouch(x, y))
    {
        // 如果不是导航按钮，再处理文件/目录项目的触摸
        handleItemTouch(x, y);
    }
}
