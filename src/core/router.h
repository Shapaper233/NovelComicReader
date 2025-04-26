#ifndef ROUTER_H
#define ROUTER_H

#include <Arduino.h> // Added to define String type
#include <functional>
#include <vector>
#include <string> // std::string
#include <unordered_map>
#include <cstdint>
#include "../config/config.h" // Adjusted path
// #include "pages.h" // Remove this include

// Forward declarations to break circular dependency
class Page;
class FileBrowserPage;
class ImageViewerPage;
class TextViewerPage;
class ComicViewerPage;

// 路由历史记录项
struct RouteHistoryItem
{
    std::string name;
    void *params;
};

// 路由处理类
class Router
{
private:
    Router() = default;
    std::vector<RouteHistoryItem> history;
    std::unordered_map<std::string, std::function<Page *()>> routes;
    Page *currentPage = nullptr;
    std::string currentPageName; // Added: Track the name of the current page
    void* currentPageParams = nullptr; // Added: Track the params used for the current page

    static Router *instance;

public:
    // Prevent copying and assignment
    Router(const Router&) = delete;
    Router& operator=(const Router&) = delete;

    // Static method to get the singleton instance
    static Router &getInstance();

    // Register a page creator function with a name
    void registerPage(const std::string &name, std::function<Page *()> creator);

    // Navigate to a registered page by name, optionally passing parameters
    void navigateTo(const std::string &name, void *params = nullptr);

    // Navigate back to the previous page in history
    bool goBack();

    // Get a pointer to the currently active page
    Page *getCurrentPage();

    // Destructor
    ~Router();
};

#endif // ROUTER_H
