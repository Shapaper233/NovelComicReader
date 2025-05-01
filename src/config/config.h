#ifndef CONFIG_H
#define CONFIG_H

// 字体设置
#define TEXT_FONT 2     // 使用Font2作为默认字体

// 颜色定义
#ifndef TFT_BLACK
#define TFT_BLACK       0x0000
#define TFT_WHITE       0xFFFF
#define TFT_BLUE        0x001F
#define TFT_DARKGREY    0x7BEF
#define TFT_YELLOW      0xFFE0
#define TFT_CYAN        0x07FF
#endif

// 文本对齐方式
#ifndef MC_DATUM
#define MC_DATUM        4  // 中心对齐
#define TL_DATUM        0  // 左对齐
#endif

// 屏幕引脚定义
#define TFT_CS   15    // 片选控制信号
#define TFT_DC   2     // 数据/命令选择
#define TFT_MOSI 13    // SPI写数据
#define TFT_SCLK 14    // SPI时钟
#define TFT_RST  -1    // 复位引脚(使用ESP32的EN)
#define TFT_MISO 12    // SPI读数据
#define TFT_BL   21    // 背光控制

// SPI总线配置
#define TOUCH_SPI VSPI // 触摸屏使用VSPI总线
#define TFT_SPI   HSPI // 显示屏使用HSPI总线

// 触摸屏引脚
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

// 按钮引脚定义
#define BUTTON_IO0 0 // GPIO0 按钮

// SD卡引脚定义
#define SD_CS   5     // SD卡片选
#define SD_MOSI 23    // SD卡SPI写数据
#define SD_MISO 19    // SD卡SPI读数据
#define SD_SCK  18    // SD卡SPI时钟

// 屏幕尺寸
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

// 触摸屏校准参数
#define TOUCH_MIN_X 200
#define TOUCH_MAX_X 3700
#define TOUCH_MIN_Y 300
#define TOUCH_MAX_Y 3800

// 界面常量
#define MAX_ITEMS_PER_PAGE 4    // 每页显示的最大项目数
#define ITEM_HEIGHT 40          // 每个项目的高度
#define ITEM_PADDING 5          // 项目间距

// 文件系统常量
#define INFO_FILE ".info"       // 漫画目录标识文件

#endif // CONFIG_H
