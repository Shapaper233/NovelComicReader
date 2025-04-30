#include <Arduino.h>       // 包含标准的 Arduino 库
#include <WString.h>       // 显式包含 WString.h 以确保 String 类的定义可用
#include "router.h"        // 包含 Router 类的头文件
#include "../pages/pages.h" // 包含完整的 Page 类定义 (包括子类的前向声明可能不够)

// 初始化静态单例实例指针
Router* Router::instance = nullptr;

// 获取 Router 单例实例的静态方法
Router& Router::getInstance()
{
    // 如果实例尚未创建
    if (!instance)
    {
        // 创建一个新的 Router 实例
        instance = new Router();
    }
    // 返回实例的引用
    return *instance;
}

// 注册一个页面创建函数，并关联一个名称
void Router::registerPage(const std::string &name, std::function<Page *()> creator)
{
    // 在 routes map 中存储名称和对应的创建函数
    routes[name] = creator;
}

// 导航到指定名称的注册页面，可选择传递参数
void Router::navigateTo(const std::string &name, void *params)
{
    // 在 routes map 中查找页面名称
    auto it = routes.find(name);
    // 如果找到了对应的路由
    if (it != routes.end())
    {
        // 调用创建函数创建新页面的实例
        Page *newPage = it->second();
        // 如果提供了参数，则调用新页面的 setParams 方法传递参数
        if (params) {
            newPage->setParams(params);
        }

        // 在删除旧页面之前，将旧页面的详细信息保存到历史记录中
        if (currentPage) // 检查当前页面是否存在
        {
            // 将我们即将离开的页面的名称和参数压入历史记录栈
            history.push_back({currentPageName, currentPageParams});
        }

        // 现在删除旧页面对象并切换到新页面
        delete currentPage; // 对 nullptr 调用 delete 是安全的
        currentPage = newPage; // 更新当前页面指针
        currentPageName = name; // 存储新页面的名称
        currentPageParams = params; // 存储用于新页面的参数
        // 调用新页面的 display 方法来显示它
        currentPage->display();
    }
    else {
        // 如果找不到路由，可以考虑添加错误处理逻辑
        Serial.printf("错误：未找到路由 '%s'\n", name.c_str());
    }
}

// 导航回历史记录中的上一个页面
bool Router::goBack()
{
    // 如果历史记录为空
    if (history.empty())
    {
        return false; // 无法再后退
    }

    // 获取要返回到的页面的信息 (历史记录中的最后一项)
    RouteHistoryItem lastPageInfo = history.back();
    history.pop_back(); // 从历史记录中移除该项

    // 查找该页面的创建函数
    auto it = routes.find(lastPageInfo.name);
    // 如果找到了对应的路由
    if (it != routes.end())
    {
        // 创建上一页面的实例
        Page* previousPage = it->second();
        // 临时存储要使用的参数 (来自历史记录)
        void* paramsToUse = lastPageInfo.params;

        // 如果历史记录中有参数，则设置给上一页面
        if (paramsToUse) {
             previousPage->setParams(paramsToUse);
        }

        // 在删除当前页面对象之前，调用其 cleanup 方法
        if (currentPage) {
            currentPage->cleanup();
        }
        // 现在删除当前页面对象
        delete currentPage;

        // 重要：在删除页面之后、覆盖 currentPageParams 之前，删除动态分配的参数。
        // 仅当它是已知的动态类型时才删除。
        // 我们假设 "text" 和 "comic" 路由使用动态分配的 String* 参数。
        // 注意：这假设传递给 navigateTo 的这些路由的参数是用 `new String(...)` 分配的。
        if (currentPageParams && (currentPageName == "text" || currentPageName == "comic")) {
             // Serial.printf("正在删除 'goBack' 中的动态参数 (String*): %s\n", static_cast<String*>(currentPageParams)->c_str()); // 调试信息
             delete static_cast<String*>(currentPageParams);
        }

        // 切换到上一页面
        currentPage = previousPage; // 更新当前页面指针
        currentPageName = lastPageInfo.name; // 恢复页面名称
        currentPageParams = paramsToUse;     // 从历史记录项恢复参数
        // 显示恢复的页面
        currentPage->display();
        return true; // 成功返回
    }
    else {
        // 如果找不到路由，理想情况下应处理错误
        Serial.printf("错误：在 'goBack' 中未找到路由 '%s'\n", lastPageInfo.name.c_str());
        // 也许应该将历史记录项重新加回去？
        // history.push_back(lastPageInfo); // 如果路由失败则重新添加？
        return false; // 返回失败
    }
}

// 获取指向当前活动页面的指针
Page* Router::getCurrentPage()
{
    return currentPage;
}

// Router 类的析构函数
Router::~Router()
{
    // 清理当前页面（如果存在）
    if (currentPage) {
        currentPage->cleanup(); // 删除前调用 cleanup
        delete currentPage;
        currentPage = nullptr;
    }
    // 是否需要清理历史记录中剩余的动态分配参数？
    // 这可能很复杂，因为可能使用了不同的类型。
    // 目前，我们只在 goBack 中处理当前页面的参数。

    // 注意：单例实例本身不在这里删除。
    // 合适的单例清理通常以不同方式处理（例如，静态析构函数、显式清理函数）。
    // 对于嵌入式系统，如果路由器永远存在，让它泄漏可能是可接受的。
    // delete instance; // 避免在此处删除静态实例指针
}
