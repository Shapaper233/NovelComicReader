# 如何向 NovelComicReader 添加新页面

本文档介绍向 NovelComicReader 项目添加新视图页面的步骤。

## 步骤概览

1.  **创建页面文件**: 为新页面创建头文件 (`.h`) 和实现文件 (`.cpp`)。
2.  **定义页面类**: 在头文件中声明继承自 `Page` 基类的新页面类。
3.  **实现页面逻辑**: 在实现文件中编写页面的构造函数、显示逻辑和触摸处理逻辑。
4.  **创建工厂函数**: 定义并实现一个用于创建新页面实例的工厂函数。
5.  **集成页面**: 在 `src/pages/pages.h` 中声明工厂函数，并在 `src/pages/pages.cpp` 中实现它（如果需要），最后在 `NovelComicReader.ino` 中注册页面路由。

## 详细步骤

### 1. 创建页面文件

在 `src/pages/` 目录下创建两个新文件：

*   `your_new_page.h` (头文件)
*   `your_new_page.cpp` (实现文件)

将 `your_new_page` 替换为你页面的实际名称 (例如 `settings_page`)。

### 2. 定义页面类 (`your_new_page.h`)

在头文件中，你需要：

*   使用包含守卫 (`#ifndef`, `#define`, `#endif`) 防止重复包含。
*   包含必要的头文件，至少需要 `src/pages/pages.h` 来获取 `Page` 基类定义。
*   声明你的新页面类，使其公有继承自 `Page`。
*   声明必要的虚函数：
    *   `virtual void display() override;` - 负责绘制页面的所有内容。
    *   `virtual void handleTouch(uint16_t x, uint16_t y) override;` - 处理该页面的触摸输入。
    *   `virtual void handleLoop() override;` - (可选) 处理在主 `loop()` 中调用的周期性任务。即使页面不需要周期性任务，也必须覆盖此方法（可以为空实现）。
    *   `virtual void setParams(void* params) override;` (可选) - 如果页面需要从导航中接收参数。
    *   `virtual void cleanup() override;` (可选) - 如果页面在销毁前需要释放资源（例如动态分配的内存）。
*   声明构造函数 `YourNewPage();`。
*   声明私有成员变量，例如对 `Display`, `SDCard`, `Touch` 管理器的引用，以及页面状态变量。
*   在文件末尾声明该页面的工厂函数。

**示例 (`your_new_page.h`):**

```cpp
#ifndef YOUR_NEW_PAGE_H
#define YOUR_NEW_PAGE_H

#include "pages.h" // 包含 Page 基类和核心管理器引用

class YourNewPage : public Page {
private:
    Display& displayManager; // 显示管理器引用
    // 其他私有成员...
    int someState;

    // 私有辅助函数 (例如绘制 UI 元素)
    void drawUI();

public:
    YourNewPage(); // 构造函数
    virtual ~YourNewPage() = default; // 虚析构函数

    virtual void display() override;
    virtual void handleTouch(uint16_t x, uint16_t y) override;
    // virtual void setParams(void* params) override; // 如果需要
    // virtual void cleanup() override; // 如果需要
};

// 工厂函数声明
YourNewPage* createYourNewPage();

#endif // YOUR_NEW_PAGE_H
```

### 3. 实现页面逻辑 (`your_new_page.cpp`)

在实现文件中，你需要：

*   包含对应的头文件 (`your_new_page.h`)。
*   实现构造函数：初始化成员变量，特别是对 `Display`, `SDCard`, `Touch` 等单例管理器的引用。
*   实现 `display()` 方法：使用 `displayManager` 的绘图函数来绘制页面的用户界面。
*   实现 `handleTouch()` 方法：根据触摸坐标 `x`, `y` 判断用户交互，更新页面状态或导航到其他页面。
*   实现 `setParams()` 和 `cleanup()` (如果声明了)。
*   实现工厂函数：简单地返回一个新创建的页面实例 (`return new YourNewPage();`)。

**示例 (`your_new_page.cpp`):**

```cpp
#include "your_new_page.h"
#include "../core/router.h" // 如果需要导航

YourNewPage::YourNewPage() :
    displayManager(Display::getInstance()), // 获取 Display 单例
    someState(0) // 初始化状态
{
    // 构造函数体 (如果需要其他初始化)
}

void YourNewPage::display() {
    displayManager.clear(); // 清屏
    drawUI(); // 绘制界面
    // ... 其他显示逻辑 ...
}

void YourNewPage::handleTouch(uint16_t x, uint16_t y) {
    // ... 触摸处理逻辑 ...
    // 例如：检查是否点击了某个按钮
    // if (x > 100 && x < 200 && y > 100 && y < 150) {
    //     Router::getInstance().navigateTo("other_page");
    // }
}

void YourNewPage::drawUI() {
    // 使用 displayManager 绘制文本、形状等
    displayManager.drawText("Your New Page", 10, 10);
    // ...
}

// 工厂函数实现
YourNewPage* createYourNewPage() {
    return new YourNewPage();
}
```

### 4. 集成页面

#### a. 声明工厂函数 (`src/pages/pages.h`)

在 `src/pages/pages.h` 文件底部的 “页面工厂函数声明” 部分，添加你的新页面的工厂函数声明：

```cpp
// --- 页面工厂函数声明 ---
// ... 其他页面的声明 ...

/**
 * @brief 创建你的新页面实例。
 * @return YourNewPage* 指向新实例的指针。
 */
YourNewPage* createYourNewPage(); // 添加这一行
```

#### b. (可选) 实现工厂函数 (`src/pages/pages.cpp`)

如果你的工厂函数不仅仅是 `return new YourNewPage();`（例如，如果它管理一个单例实例），你可能需要在 `src/pages/pages.cpp` 中提供完整的实现。但对于简单页面，直接在 `.cpp` 文件中实现通常就足够了。

#### c. 注册页面路由 (`NovelComicReader.ino`)

在 `NovelComicReader.ino` 文件的 `setup()` 函数中，找到 `router.registerPage(...)` 的调用位置，并为你的新页面添加一行注册代码，直接传递工厂函数的名称（函数指针）：

```cpp
void setup() {
    // ... 其他初始化代码 ...

    // 注册页面路由 (使用函数指针)
    router.registerPage("browser", createFileBrowserPage);
    router.registerPage("viewer", createImageViewerPage);
    router.registerPage("comic", createComicViewerPage);
    router.registerPage("text", createTextViewerPage);
    router.registerPage("your_page_key", createYourNewPage); // 添加这一行，直接传递函数名

    // ... 导航到初始页面 ...
    router.navigateTo("browser");

    Serial.println("Initialization complete!");
}
```

将 `"your_page_key"` 替换为你希望用来导航到此页面的唯一字符串标识符。

**重要**: 确保你的工厂函数（例如 `createYourNewPage`）在 `src/pages/pages.h` 中声明，并且其返回类型为 `Page*`（而不是具体的页面类型如 `YourNewPage*`）。

完成这些步骤后，你的新页面就成功集成到项目中了。这种方法避免了在 `NovelComicReader.ino` 中包含具体页面头文件的需要，从而改善了代码的解耦。你可以通过调用 `Router::getInstance().navigateTo("your_page_key");` 来导航到这个新页面。
