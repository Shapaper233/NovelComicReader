#include <Arduino.h>       // Standard Arduino include
#include <WString.h>       // Explicitly include for String class definition
#include "router.h"
#include "../pages/pages.h" // Include the full Page definition

// 初始化静态成员
Router* Router::instance = nullptr;

// Static method to get the singleton instance
Router& Router::getInstance()
{
    if (!instance)
    {
        instance = new Router();
    }
    return *instance;
}

// Register a page creator function with a name
void Router::registerPage(const std::string &name, std::function<Page *()> creator)
{
    routes[name] = creator;
}

// Navigate to a registered page by name, optionally passing parameters
void Router::navigateTo(const std::string &name, void *params)
{
    auto it = routes.find(name);
    if (it != routes.end())
    {
        // 创建新页面
        Page *newPage = it->second();
        if (params) { // Call setParams only if params are provided
            newPage->setParams(params);
        }

        // Save OLD page details to history BEFORE deleting
        if (currentPage)
        {
            // Push the name and params of the page we are LEAVING
            history.push_back({currentPageName, currentPageParams});
        }

        // Now delete the old page and switch
        delete currentPage; // delete is safe on nullptr
        currentPage = newPage;
        currentPageName = name; // Store the name of the NEW page
        currentPageParams = params; // Store the params used for the NEW page
        currentPage->display();
    }
    // Consider adding error handling if route not found
}

// Navigate back to the previous page in history
bool Router::goBack()
{
    if (history.empty())
    {
        return false; // Cannot go back further
    }

    // Get the details of the page to return TO
    RouteHistoryItem lastPageInfo = history.back();
    history.pop_back();

    // Find the factory for that page
    auto it = routes.find(lastPageInfo.name);
    if (it != routes.end())
    {
        // Create the previous page instance
        Page* previousPage = it->second();
        void* paramsToUse = lastPageInfo.params; // Store params temporarily

        if (paramsToUse) { // Use the PARAMS saved in history
             previousPage->setParams(paramsToUse);
        }

        // Call cleanup on the current page BEFORE deleting it
        if (currentPage) {
            currentPage->cleanup();
        }
        // Now delete the current page object
        delete currentPage;

        // IMPORTANT: Delete the dynamically allocated parameter *after* the page is deleted
        // and *before* overwriting currentPageParams. Only delete if it's a known dynamic type.
        // We assume "text" and "comic" routes use dynamic String* params.
        // Note: This assumes the params passed to navigateTo for these routes were allocated with `new String(...)`
        if (currentPageParams && (currentPageName == "text" || currentPageName == "comic")) {
             delete static_cast<String*>(currentPageParams);
        }

        // Switch to the previous page
        currentPage = previousPage;
        currentPageName = lastPageInfo.name; // Restore name
        currentPageParams = paramsToUse;     // Restore params from history item
        currentPage->display();
        return true;
    }
    // Should ideally handle error if route not found
    // Maybe push the history item back?
    // history.push_back(lastPageInfo); // Re-add if route failed?
    return false;
}

// Get a pointer to the currently active page
Page* Router::getCurrentPage()
{
    return currentPage;
}

// Destructor
Router::~Router()
{
    // Clean up the current page if it exists
    if (currentPage) {
        currentPage->cleanup(); // Call cleanup before deleting
        delete currentPage;
        currentPage = nullptr;
    }
    // Clean up any dynamically allocated params left in history?
    // This might be complex if different types were used.
    // For now, we only handle the current page's params in goBack.

    // Note: The singleton instance itself is not deleted here.
    // Proper singleton cleanup is often handled differently (e.g., static destructor, explicit cleanup function).
    // For an embedded system, letting it leak might be acceptable if the router lives forever.
    // delete instance; // Avoid deleting the static instance pointer here
}
