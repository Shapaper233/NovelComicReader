#define USER_SETUP_INFO "User_Setup"

// 定义显示驱动型号
#define ILI9341_DRIVER

// SPI设置
#define SPI_FREQUENCY       40000000  // 40MHz SPI总线速度
#define SPI_READ_FREQUENCY  20000000  // 20MHz 读取速度
#define SPI_TOUCH_FREQUENCY 2500000   // 2.5MHz 触摸读取速度
#define USE_HSPI_PORT             // 使用HSPI端口而不是默认的VSPI

// 定义TFT屏幕引脚和配置
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1  // 连接到ESP32的RST

// 定义触摸屏引脚
#define TOUCH_CS 33

// 背光控制引脚
#define TFT_BL   21
#define TFT_BACKLIGHT_ON HIGH

// 显示方向设置
#define TFT_ROTATION 1  // 0-Portrait, 1-Landscape

// 字体加载设置
#define LOAD_GLCD   // 字体 1. 原版 Adafruit 8 像素字体
#define LOAD_FONT2  // 字体 2. Small 16 像素高 & 字符
#define LOAD_FONT4  // 字体 4. Medium 26 像素高 & 字符
#define LOAD_FONT6  // 字体 6. Large 48 像素字体
#define LOAD_FONT7  // 字体 7. 7 段 48 像素字体
#define LOAD_FONT8  // 字体 8. Large 75 像素字体
#define LOAD_GFXFF  // FreeFonts. 包含在 SPIFFS 中

// 字体渲染设置
#define SMOOTH_FONT
#define TEXT_FONT 2     // 使用Font2作为默认字体
#define TEXT_SIZE 1     // 默认字体大小
#define TEXT_HEIGHT 16  // Font2的字体高度

// 显示优化
#define DMA_ENABLE    // 启用SPI DMA传输
#define DISPLAY_POWER_SAVE // 启用显示省电模式
#define CGRAM_OFFSET      // 使用字符发生器RAM偏移
#define SUPPORT_TRANSACTIONS  // 支持SPI事务

// 颜色定义
#define TFT_TRANSPARENT 0x0120

// 内存优化
#define SPRITE_ROTATION_SUPPORT    // 支持Sprite旋转
#define SUPPORT_SPRITES           // 启用Sprite支持
#define SPRITE_MAX_SIZE    256   // 最大Sprite尺寸
