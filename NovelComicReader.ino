#include "config.h"
#include "display.h"

#include "sdcard.h"
#include "router.h"
#include "pages.h"
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <TFT_eSPI.h>


SPIClass touchSPI = SPIClass(TOUCH_SPI);
SPIClass sdSPI = SPIClass(HSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);

Router& router = Router::getInstance();
Display& display = Display::getInstance();
SDCard &sd = SDCard::getInstance();

void setup() {
    Serial.begin(115200);
    Serial.println("Starting Novel/Comic Reader...");

    // 初始化触摸屏SPI
    touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    ts.begin(touchSPI);
    ts.setRotation(1);

    // 初始化SD卡SPI
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
    
    // 注册页面路由
    router.registerPage("browser", createFileBrowserPage);
    router.registerPage("viewer", createImageViewerPage);
    
    // 导航到文件浏览页面
    router.navigateTo("browser");
    
    Serial.println("Initialization complete!");
}

void loop() {
    
    

    // 检查触摸
    if (ts.touched()) {
        TS_Point touchp = ts.getPoint();

        // 映射触摸坐标到屏幕
        int touchx = map(touchp.x, TOUCH_MIN_X, TOUCH_MAX_X, 0, SCREEN_WIDTH);
        int touchy = map(touchp.y, TOUCH_MIN_Y, TOUCH_MAX_Y, 0, SCREEN_HEIGHT);
        Serial.println("Touch detected: (" + String(touchx) + ", " + String(touchy) + ")");
        Page *currentPage = router.getCurrentPage();
        if (currentPage)
        {
            currentPage->handleTouch(touchx, touchy);
        }
        // 防抖延迟
        //delay(200);

        // 等待触摸释放
        //while (touch.isTouched()) {
        //    touch.update();
        //    delay(10);
        //}
    }

    // 其他系统任务
    delay(10);  // 防止过度占用CPU
}
