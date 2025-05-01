#ifndef TEXT_VIEWER_PAGE_H
#define TEXT_VIEWER_PAGE_H

#include <Arduino.h>    // Core Arduino library
#include <FS.h>         // File system library (includes File object)
#include <vector>       // For storing line index

#include "pages.h"      // Base Page class
#include "../core/display.h" // Display manager
#include "../core/font.h"    // Font manager
#include "../config/config.h" // Screen dimensions etc.

class TextViewerPage : public Page {
private:
    Display& displayManager;
    Font& fontManager;
    String filePath;            // Path to the text file
    File currentFile;           // Currently open file object
    std::vector<size_t> lineIndex; // Stores byte offset for the start of each line
    int currentScrollLine;      // Index of the top visible line in the lineIndex
    int totalLines;             // Total number of lines found (size of lineIndex)
    int linesPerPage;           // How many lines fit on the screen
    int lineHeight;             // Height of a single line in pixels
    bool fileLoaded;            // Flag indicating if the file content is loaded
    size_t fileSize;            // Size of the file in bytes
    int originalLineCount;      // Number of lines based on newline characters

    // Private helper methods
    void loadContent();         // Scans file, builds index, shows loading UI
    void drawContent();         // Reads lines from file using index and draws them
    void drawScrollbar();       // Draws the scrollbar
    void closeFile();           // Helper to close the current file safely
    void handleScroll(int touchY); // Handles scrolling based on touch Y
    void calculateLayout();     // Calculates linesPerPage and lineHeight

public:
    TextViewerPage(); // Constructor declaration
    ~TextViewerPage() override = default; // Use default destructor

    void display() override;
    void handleTouch(uint16_t x, uint16_t y) override;
    // Override to receive parameters from the router
    void setParams(void* params) override;

    // Method to set the file path before displaying the page
    void setFilePath(const String& path);
};

#endif // TEXT_VIEWER_PAGE_H
