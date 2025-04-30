#include "src/config/config.h"
#include "src/core/display.h"
#include "src/core/sdcard.h"
#include "src/core/router.h"
#include "src/pages/pages.h" // Includes base Page and factory declarations
#include "src/pages/text_viewer_page.h" // Include full definition for TextViewerPage
#include "src/core/touch.h"   // Include the new Touch class header
#include "src/core/font.h"    // Include Font class header
#include <SPI.h>              // Re-add for direct access
#include <XPT2046_Touchscreen.h> // Re-add for direct access (needed by Touch class indirectly)
#include <TFT_eSPI.h>         // Re-add for direct access


// SPIClass touchSPI = SPIClass(TOUCH_SPI); // Removed, handled by Touch class
SPIClass sdSPI = SPIClass(HSPI);
// XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ); // Removed, handled by Touch class

// Get Singleton instances
Router& router = Router::getInstance();
Display& display = Display::getInstance();
SDCard &sd = SDCard::getInstance();
Touch& touch = Touch::getInstance(); // Get Touch instance

void setup() {
    Serial.begin(115200);
    Serial.println("Starting Novel/Comic Reader...");

    // Initialize Touch Screen (now uses Touch class)
    touch.begin();

    // Initialize SD Card SPI
    sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    
    // 初始化显示屏
    display.begin();
    display.clear();
    Serial.println("Initializing...");
    display.drawCenteredText("Initializing...", 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    
    
    // 初始化SD卡
    if (!sd.begin()) {
        display.clear();
        Serial.println("SD Card Error!");
        display.drawCenteredText("SD Card Error!", 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
        while (1) delay(100);  // 停止执行
    }

    // Display loading message and load fast font cache
    display.clear();
    display.drawCenteredText("Loading Font Cache...", 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,1); // Changed text
    bool cacheLoaded = Font::getInstance().loadFastFontCache();
    if (cacheLoaded) {
        Serial.println("Fast cache loaded.");
    } else {
         Serial.println("Fast cache not loaded (or failed).");
         // Optional: Display a message if loading failed or took time
         // display.drawCenteredText("Cache Load Complete", 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
         // delay(500); // Show message briefly
    }
    display.clear(); // Clear loading message/progress bar
    
    // 注册页面路由
    router.registerPage("browser", []() -> Page* { return createFileBrowserPage(); });
    router.registerPage("viewer", []() -> Page* { return createImageViewerPage(); });
    router.registerPage("comic", []() -> Page* { return createComicViewerPage(); });
    router.registerPage("text", []() -> Page* { return createTextViewerPage(); }); // Register TextViewerPage
    
    // 导航到文件浏览页面
    router.navigateTo("browser");
    
    Serial.println("Initialization complete!");
}

void loop() {
    
    

    // Check for touch input using the Touch class
    uint16_t touchX, touchY;
    if (touch.getPoint(touchX, touchY)) { // getPoint now returns true if touched and provides mapped coordinates
        Serial.println("Touch detected: (" + String(touchX) + ", " + String(touchY) + ")");
        Page *currentPage = router.getCurrentPage();
        if (currentPage)
        {
            currentPage->handleTouch(touchX, touchY);
        }
        // Optional: Add debounce or touch release logic here if needed
        // delay(50); // Simple debounce

        // 等待触摸释放
        //while (touch.isTouched()) {
        //    touch.update();
        //    delay(10);
        //}
    }

    // 其他系统任务
    delay(10);  // 防止过度占用CPU
}
