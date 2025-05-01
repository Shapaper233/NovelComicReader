#ifndef MENU_PAGE_H
#define MENU_PAGE_H

#include <Arduino.h> // Add Arduino core explicitly for linter
#include "pages.h" // Includes Page base class, Router, Display etc. via transitive includes
#include <vector>
#include <string> // Use std::string for menu item text and page names

class MenuPage : public Page {
private:
    Display& displayManager;
    Router& router; // Need router to navigate

    // Structure to hold menu item information
    struct MenuItem {
        std::string label;      // Text displayed for the item
        std::string targetPage; // The page NAME (string) to navigate to when clicked
        void* params;           // Optional parameters to pass to the target page
        // Add icon data/path later if needed: const unsigned char* iconData;
    };

    std::vector<MenuItem> menuItems; // List of all available menu items
    int currentPageIndex;            // Index of the currently displayed page (0-based)
    int itemsPerPage;                // Maximum number of items displayable on one screen (will be COLS * ROWS)

    // --- UI Layout Constants (Grid Layout 3x4) ---
    static constexpr uint8_t MENU_COLS = 3;       // Number of columns in the grid
    static constexpr uint8_t MENU_ROWS = 4;       // Number of rows in the grid
    static constexpr uint16_t HEADER_HEIGHT = 30; // Height for the header (e.g., "Menu")
    static constexpr uint16_t FOOTER_HEIGHT = 35; // Height for the footer (pagination controls)
    static constexpr uint16_t ITEM_PADDING_X = 10; // Horizontal padding between items and screen edges
    static constexpr uint16_t ITEM_PADDING_Y = 10; // Vertical padding between items and header/footer
    // Button width/height will be calculated dynamically based on screen size, padding, and grid dimensions
    // static constexpr uint16_t BUTTON_WIDTH = ...; // Calculated in cpp
    // static constexpr uint16_t BUTTON_HEIGHT = ...; // Calculated in cpp
    static constexpr uint16_t NAV_BUTTON_WIDTH = 70;  // Width for Prev/Next buttons in footer
    static constexpr uint16_t NAV_BUTTON_HEIGHT = 28; // Height for Prev/Next buttons in footer
    static constexpr uint16_t CONTENT_Y = HEADER_HEIGHT; // Y position where grid starts
    // Calculated content height available for grid items
    static constexpr uint16_t CONTENT_HEIGHT = SCREEN_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT;

    // Calculated values (will be set in constructor or relevant methods)
    uint16_t gridButtonWidth;
    uint16_t gridButtonHeight;

    // --- Private Helper Functions ---
    void drawHeader();          // Draws the "Menu" title bar
    void drawMenuItems();       // Draws the grid of items for the current page
    void drawFooter();          // Draws pagination controls (Prev/Next buttons, page number)
    void calculateGridDimensions(); // Calculates button sizes and itemsPerPage
    int getTotalPages() const;  // Calculates the total number of menu pages
    void populateMenuItems();   // Fills the menuItems vector

public:
    /**
     * @brief Constructor for MenuPage.
     */
    MenuPage();

    /**
     * @brief Displays the menu page content (overrides Page).
     */
    virtual void display() override;

    /**
     * @brief Handles touch input on the menu page (overrides Page).
     * @param x Touch X coordinate.
     * @param y Touch Y coordinate.
     */
    virtual void handleTouch(uint16_t x, uint16_t y) override;

    /**
     * @brief Handles periodic tasks in the main loop (overrides Page).
     */
    virtual void handleLoop() override; // Keep consistent with other pages

    /**
     * @brief Destructor for MenuPage.
     */
    virtual ~MenuPage() = default;
};

#endif // MENU_PAGE_H
