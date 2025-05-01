#ifndef TEXT_VIEWER_PAGE_H
#define TEXT_VIEWER_PAGE_H

#include <Arduino.h> // Core Arduino library
#include <FS.h>      // File system library
#include <vector>    // For storing lines of text

#include "pages.h"            // Base Page class
#include "../core/display.h"  // Display manager
#include "../core/font.h"     // Font manager
#include "../config/config.h" // Screen dimensions etc.

class TextViewerPage : public Page
{
private:
    Display &displayManager;
    Font &fontManager;
    String filePath;           // Path to the text file
    std::vector<String> lines; // Stores the wrapped lines of the file
    int currentScrollLine;     // Index of the top visible line
    int totalLines;            // Total number of wrapped lines
    int linesPerPage;          // How many lines fit on the screen
    int lineHeight;            // Height of a single line in pixels
    bool fileLoaded;           // Flag indicating if the file content is loaded

    // Private helper methods
    void loadContent();            // Loads and wraps text from the file
    void drawContent();            // Draws the visible text lines
    void drawScrollbar();          // Draws the scrollbar
    void handleScroll(int touchY); // Handles scrolling based on touch Y
    void calculateLayout();        // Calculates linesPerPage and lineHeight

public:
    TextViewerPage();
    ~TextViewerPage() override = default; // Use default destructor

    void display() override;
    void handleTouch(uint16_t x, uint16_t y) override;
    // Override to receive parameters from the router
    void setParams(void *params) override;

    // Method to set the file path before displaying the page
    void setFilePath(const String &path);
};

#endif // TEXT_VIEWER_PAGE_H
