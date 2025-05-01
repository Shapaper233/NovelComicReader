#ifndef TEXT_VIEWER_PAGE_H
#define TEXT_VIEWER_PAGE_H

#include <Arduino.h> // Core Arduino library
#include <FS.h>      // File system library
#include <vector>    // For storing lines of text
#include <map>       // For partial line index

#include "pages.h"            // Base Page class
#include "../core/display.h"  // Display manager
#include "../core/font.h"     // Font manager
#include "../core/touch.h"    // Touch manager
#include "../config/config.h" // Screen dimensions etc.

class TextViewerPage : public Page
{
private:
    Display &displayManager;
    Font &fontManager;
    Touch &touchManager;       // Reference to Touch singleton
    String filePath;           // Path to the text file
    // std::vector<String> lines; // No longer storing all lines
    // std::vector<size_t> lineStartPositions; // REMOVED: To save memory, avoid storing all positions
    std::map<int, size_t> lineIndex; // Partial index: line number -> file position
    const int INDEX_INTERVAL = 100;  // Store position every 100 lines

    int currentScrollLine;     // Index of the top visible line
    int totalLines;            // Total number of wrapped lines (calculated once)
    int linesPerPage;          // How many lines fit on the screen
    int lineHeight;            // Height of a single line in pixels
    bool fileLoaded;           // Flag indicating if file metadata (size, total lines) is calculated
    unsigned long startTimeMillis; // For ETC calculation
    String errorMessage;       // Stores error message from metadata calculation

    // Private helper methods
    void calculateFileMetadata();  // Calculates total lines, size, and populates partial index
    void drawContent();            // Draws the visible text lines (uses partial index + sequential read)
    void drawScrollbar();          // Draws the scrollbar
    void handleScroll(int touchY); // Handles scrolling based on touch Y
    void calculateLayout();        // Calculates linesPerPage and lineHeight
    // Updated to show size, line count, and ETC during loading
    void updateLoadingProgress(size_t currentBytes, size_t totalBytes, int currentLineCount, unsigned long elapsedMillis);

public:
    TextViewerPage();
    ~TextViewerPage() override = default; // Use default destructor

    void display() override;
    void handleTouch(uint16_t x, uint16_t y) override;
    // Override to receive parameters from the router
    void setParams(void *params) override;

    // Method to set the file path before displaying the page
    void setFilePath(const String &path);

    // Method to clean up resources (e.g., when navigating away)
    void cleanup();
};

#endif // TEXT_VIEWER_PAGE_H
