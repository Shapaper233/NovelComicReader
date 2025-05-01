#include "src/config/config.h"
#include "src/core/display.h"
#include "src/core/sdcard.h"
#include "src/core/router.h"
#include "src/pages/pages.h" // Includes base Page and factory declarations
#include "src/core/touch.h"   // Include the new Touch class header
#include "src/core/font.h"    // Include Font class header
#include <SPI.h>              // Re-add for direct access
#include <XPT2046_Touchscreen.h> // Re-add for direct access (needed by Touch class indirectly)
#include <TFT_eSPI.h>         // Re-add for direct access

// 布尔类型存储是否息屏 true 是开
bool isScreenOn = true;

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
    
    // 注册页面路由 (使用函数指针)
    router.registerPage("browser", createFileBrowserPage);
    router.registerPage("viewer", createImageViewerPage);
    router.registerPage("comic", createComicViewerPage);
    router.registerPage("text", createTextViewerPage); // Register TextViewerPage using function pointer
    router.registerPage("menu", createMenuPage);       // Register MenuPage using function pointer
    
    // 导航到菜单页面 (设置为默认启动页面)
    router.navigateTo("menu");
    
    Serial.println("Initialization complete!");
}

void loop() {
    static unsigned long pressStartTime = 0; // 记录按下时间

    if (digitalRead(BUTTON_IO0) == LOW)
    { // 检测按键按下
        if (pressStartTime == 0)
        {
            pressStartTime = millis(); // 记录按下的时间
        }

        if (millis() - pressStartTime >= 2000)
        { // 长按超过 2 秒
            Serial.println("Entering deep sleep mode...");
            esp_deep_sleep_start();
        }
    }
    else
    { // 按键释放
        if (pressStartTime > 0 && millis() - pressStartTime < 2000)
        {
            Serial.println("short click buttom");
            // 在这里写你的普通功能
            if (isScreenOn)
            {
                Serial.println("close screen");
                digitalWrite(TFT_BL, LOW); // 关闭屏幕
                isScreenOn = false;
            }
            else
            {
                Serial.println("open screen");
                digitalWrite(TFT_BL, HIGH); // 开启屏幕
                isScreenOn = true;
            }
        }
        pressStartTime = 0; // 复位计时
    }
    if (!isScreenOn)
    {
        delay(100); // 如果屏幕关闭，延时避免过度占用CPU
        return;
    }

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

    // Handle periodic tasks for the current page
    Page* currentPage = router.getCurrentPage();
    if (currentPage) {
        currentPage->handleLoop();
    }

    // 其他系统任务
    delay(10);  // 防止过度占用CPU
}
