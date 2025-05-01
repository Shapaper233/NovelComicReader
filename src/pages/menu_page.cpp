#include "menu_page.h"
#include "../core/font.h" // For drawing text and selectFont
#include <string>         // For std::to_string
#include <algorithm>      // For std::min

// Constructor
MenuPage::MenuPage() :
    displayManager(Display::getInstance()),
    router(Router::getInstance()),
    currentPageIndex(0),
    itemsPerPage(0), // Will be calculated
    gridButtonWidth(0),
    gridButtonHeight(0)
{
    calculateGridDimensions(); // Calculate grid button sizes and items per page
    populateMenuItems();
    Serial.println("MenuPage created."); // Debug message
}

// Calculate grid button dimensions and items per page
void MenuPage::calculateGridDimensions() {
    // Calculate total horizontal space available for buttons (Screen width - padding on both sides - padding between columns)
    uint16_t totalButtonWidth = SCREEN_WIDTH - (2 * ITEM_PADDING_X) - ((MENU_COLS - 1) * ITEM_PADDING_X);
    // Calculate total vertical space available for buttons (Content height - padding top/bottom - padding between rows)
    uint16_t totalButtonHeight = CONTENT_HEIGHT - (2 * ITEM_PADDING_Y) - ((MENU_ROWS - 1) * ITEM_PADDING_Y);

    if (MENU_COLS > 0) {
        gridButtonWidth = totalButtonWidth / MENU_COLS;
    } else {
        gridButtonWidth = 0;
    }

    if (MENU_ROWS > 0) {
        gridButtonHeight = totalButtonHeight / MENU_ROWS;
    } else {
        gridButtonHeight = 0;
    }

    itemsPerPage = MENU_COLS * MENU_ROWS;

    Serial.printf("Grid Dimensions: BtnW=%d, BtnH=%d, ItemsPerPage=%d\n", gridButtonWidth, gridButtonHeight, itemsPerPage); // Debug
}

// Populate the list of menu items
void MenuPage::populateMenuItems() {
    menuItems.clear(); // Clear existing items first

    // Add "File Management" item using its registered string name
    menuItems.push_back({"文件管理器", "browser", nullptr});

    // --- Add more menu items here in the future ---
    // Example: menuItems.push_back({"Settings", "settings", nullptr}); // Assuming "settings" is registered
    // Example: menuItems.push_back({"About", PageName::ABOUT, nullptr});

    Serial.printf("Populated %d menu items.\n", menuItems.size()); // Debug
}

// Calculate total number of pages needed based on fixed itemsPerPage
int MenuPage::getTotalPages() const {
    if (itemsPerPage <= 0 || menuItems.empty()) {
        return 1; // Always at least one page, even if empty
    }
    // Ceiling division: (total items + items per page - 1) / items per page
    return (menuItems.size() + itemsPerPage - 1) / itemsPerPage;
}

// Draw the header bar
void MenuPage::drawHeader() {
    TFT_eSPI* tft = displayManager.getTFT(); // Get the underlying TFT object
    tft->fillRect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, TFT_DARKGREY);
    // Use Display manager's centered text function for the header
    // Set text color via TFT object before calling drawCenteredText
    tft->setTextColor(TFT_WHITE, TFT_DARKGREY); // Set text and background color
    // Use smaller font size (size 2) for header
    displayManager.drawCenteredText("菜单", 0, 0, SCREEN_WIDTH, HEADER_HEIGHT, 1);
}

// Draw the menu items for the current page in a grid
void MenuPage::drawMenuItems() {
    TFT_eSPI* tft = displayManager.getTFT();
    tft->fillRect(0, CONTENT_Y, SCREEN_WIDTH, CONTENT_HEIGHT, TFT_BLACK); // Clear content area

    if (itemsPerPage <= 0 || gridButtonWidth <= 0 || gridButtonHeight <= 0) return; // Cannot display items if grid is invalid

    int startIndex = currentPageIndex * itemsPerPage;
    int endIndex = std::min((int)menuItems.size(), startIndex + itemsPerPage);

    for (int i = startIndex; i < endIndex; ++i) {
        int itemIndexOnPage = i - startIndex;
        int row = itemIndexOnPage / MENU_COLS;
        int col = itemIndexOnPage % MENU_COLS;

        // Calculate top-left corner of the button
        uint16_t itemX = ITEM_PADDING_X + col * (gridButtonWidth + ITEM_PADDING_X);
        uint16_t itemY = CONTENT_Y + ITEM_PADDING_Y + row * (gridButtonHeight + ITEM_PADDING_Y);

        // Draw the button background with standard cyan
        tft->fillRoundRect(itemX, itemY, gridButtonWidth, gridButtonHeight, 5, TFT_CYAN);

        // Draw the item label centered within the button
        // Temporarily set background color for drawString to match button, then reset
        // uint16_t currentBgColor = tft->textbgcolor; // Store current background color - Not needed with drawCenteredText
        // tft->setTextColor(TFT_WHITE, TFT_BLUE);     // Set text color with button background - Handled by drawCenteredText
        // tft->drawString(menuItems[i].label.c_str(), itemX + itemWidth / 2, currentY + MENU_ITEM_HEIGHT / 2); // Use renamed constant
        // tft->setTextColor(TFT_WHITE, currentBgColor); // Restore background color - Not needed

        // Use Display manager's centered text function with smaller font size (size 2)
        tft->setTextColor(TFT_BLACK, TFT_CYAN); // Set text color (BLACK for contrast) and background for the label
        displayManager.drawCenteredText(menuItems[i].label.c_str(), itemX, itemY, gridButtonWidth, gridButtonHeight, 1);
    }
}

// Draw the footer with pagination controls
void MenuPage::drawFooter() {
    TFT_eSPI* tft = displayManager.getTFT();
    tft->fillRect(0, SCREEN_HEIGHT - FOOTER_HEIGHT, SCREEN_WIDTH, FOOTER_HEIGHT, TFT_DARKGREY);

    int totalPages = getTotalPages();
    if (totalPages <= 1) return; // No need for pagination if only one page

    // selectFont(FONT_SIZE_MEDIUM, FONT_TYPE_DEFAULT); // REMOVED - Use Display methods
    // tft->setTextDatum(MC_DATUM); // Handled by drawCenteredText
    // tft->setTextColor(TFT_WHITE); // Set before drawing text

    // Use the NAV_BUTTON constants defined in the header
    uint16_t buttonY = SCREEN_HEIGHT - FOOTER_HEIGHT + (FOOTER_HEIGHT - NAV_BUTTON_HEIGHT) / 2;

    // Draw "Prev" button if not on the first page
    if (currentPageIndex > 0) {
        tft->fillRoundRect(ITEM_PADDING_X, buttonY, NAV_BUTTON_WIDTH, NAV_BUTTON_HEIGHT, 5, TFT_BLUE);
        tft->setTextColor(TFT_WHITE, TFT_BLUE); // Set text and background for button text
        // Use drawCenteredText within the button bounds with smaller font size (size 2)
        displayManager.drawCenteredText("Prev", ITEM_PADDING_X, buttonY, NAV_BUTTON_WIDTH, NAV_BUTTON_HEIGHT, 2);
    }

    // Draw "Next" button if not on the last page
    if (currentPageIndex < totalPages - 1) {
        uint16_t nextButtonX = SCREEN_WIDTH - ITEM_PADDING_X - NAV_BUTTON_WIDTH;
        tft->fillRoundRect(nextButtonX, buttonY, NAV_BUTTON_WIDTH, NAV_BUTTON_HEIGHT, 5, TFT_BLUE);
        tft->setTextColor(TFT_WHITE, TFT_BLUE); // Set text and background for button text
        // Use drawCenteredText within the button bounds with smaller font size (size 2)
        displayManager.drawCenteredText("Next", nextButtonX, buttonY, NAV_BUTTON_WIDTH, NAV_BUTTON_HEIGHT, 2);
    }

    // Draw Page Number (e.g., "1 / 3")
    tft->setTextColor(TFT_WHITE, TFT_DARKGREY); // Set text color with footer background
    std::string pageNumStr = std::to_string(currentPageIndex + 1) + " / " + std::to_string(totalPages);
    // Use drawCenteredText for the page number in the middle with smaller font size (size 2)
    displayManager.drawCenteredText(pageNumStr.c_str(), 0, SCREEN_HEIGHT - FOOTER_HEIGHT, SCREEN_WIDTH, FOOTER_HEIGHT, 2);

    // Reset text color if needed
    // tft->setTextColor(TFT_WHITE, TFT_BLACK); // Example reset
}

// Main display function, called by the router
void MenuPage::display() {
    Serial.println("MenuPage::display() called");
    displayManager.getTFT()->fillScreen(TFT_BLACK); // Clear screen using TFT object
    drawHeader();
    drawMenuItems();
    drawFooter();
}

// Handle touch events for grid layout
void MenuPage::handleTouch(uint16_t x, uint16_t y) {
    Serial.printf("MenuPage::handleTouch(%d, %d)\n", x, y);

    // 1. Check for touch on Menu Items (Grid)
    if (y >= CONTENT_Y && y < (SCREEN_HEIGHT - FOOTER_HEIGHT) && itemsPerPage > 0 && gridButtonWidth > 0 && gridButtonHeight > 0) {
        int startIndex = currentPageIndex * itemsPerPage;
        int endIndex = std::min((int)menuItems.size(), startIndex + itemsPerPage);

        for (int i = startIndex; i < endIndex; ++i) {
            int itemIndexOnPage = i - startIndex;
            int row = itemIndexOnPage / MENU_COLS;
            int col = itemIndexOnPage % MENU_COLS;

            // Calculate button bounds
            uint16_t itemX = ITEM_PADDING_X + col * (gridButtonWidth + ITEM_PADDING_X);
            uint16_t itemY = CONTENT_Y + ITEM_PADDING_Y + row * (gridButtonHeight + ITEM_PADDING_Y);
            uint16_t itemEndX = itemX + gridButtonWidth;
            uint16_t itemEndY = itemY + gridButtonHeight;

            // Check if touch is within the bounds of this item
            if (x >= itemX && x < itemEndX && y >= itemY && y < itemEndY) {
                Serial.printf("Touched grid item: %s (index %d)\n", menuItems[i].label.c_str(), i);
                // Navigate to the target page
                router.navigateTo(menuItems[i].targetPage, menuItems[i].params);
                return; // Touch handled
            }
        }
    }

    // 2. Check for touch on Footer Pagination Buttons
    if (y >= (SCREEN_HEIGHT - FOOTER_HEIGHT)) {
        int totalPages = getTotalPages();
        uint16_t buttonY = SCREEN_HEIGHT - FOOTER_HEIGHT + (FOOTER_HEIGHT - NAV_BUTTON_HEIGHT) / 2;
        uint16_t buttonEndY = buttonY + NAV_BUTTON_HEIGHT;

        // Check "Prev" button
        if (currentPageIndex > 0 &&
            x >= ITEM_PADDING_X && x < (ITEM_PADDING_X + NAV_BUTTON_WIDTH) &&
            y >= buttonY && y < buttonEndY)
        {
            Serial.println("Touched Prev button");
            currentPageIndex--;
            display(); // Redraw the page
            return; // Touch handled
        }

        // Check "Next" button
        uint16_t nextButtonX = SCREEN_WIDTH - ITEM_PADDING_X - NAV_BUTTON_WIDTH;
        if (currentPageIndex < totalPages - 1 &&
            x >= nextButtonX && x < (nextButtonX + NAV_BUTTON_WIDTH) &&
            y >= buttonY && y < buttonEndY)
        {
            Serial.println("Touched Next button");
            currentPageIndex++;
            display(); // Redraw the page
            return; // Touch handled
        }
    }
}

// Handle periodic tasks (can be empty for now)
void MenuPage::handleLoop() {
    // Nothing needed here for a static menu page yet
}
