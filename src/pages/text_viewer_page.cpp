#include <Arduino.h>
#include <FS.h>
#include <algorithm>  // For std::min, std::max
#include <TFT_eSPI.h> // Include for color constants like TFT_LIGHTGREY
#include <SD.h>       // Include the SD library for SD.remove()
#include <ArduinoJson.h> // Include ArduinoJson library

#include "text_viewer_page.h"
#include "../core/router.h"
#include "../core/sdcard.h" // Needed for file operations

// --- Constants ---
const int TEXT_FONT_SIZE = 1;   // Use font size 1 (e.g., 16x16)
const int TEXT_MARGIN_X = 5;    // Left/right margin for text
const int TEXT_MARGIN_Y = 5;    // Top/bottom margin for text
const int SCROLLBAR_WIDTH = 10; // Width of the scrollbar
const int SCROLLBAR_MARGIN = 2; // Margin between text and scrollbar
const int BACK_BUTTON_WIDTH = 60;
const int BACK_BUTTON_HEIGHT = 30;
const int BACK_BUTTON_X = 5;
const int BACK_BUTTON_Y = 5;
// Add Top Button constants (placed next to Back button)
const int TOP_BUTTON_WIDTH = 50; // Slightly smaller buttons to fit more
const int TOP_BUTTON_HEIGHT = 30;
const int TOP_BUTTON_X = BACK_BUTTON_X + BACK_BUTTON_WIDTH + 5;
const int TOP_BUTTON_Y = 5;
// Bookmark Buttons (Prev, Add/Del, Next)
const int PREV_BM_BUTTON_WIDTH = 40;
const int PREV_BM_BUTTON_HEIGHT = 30;
const int PREV_BM_BUTTON_X = TOP_BUTTON_X + TOP_BUTTON_WIDTH + 5;
const int PREV_BM_BUTTON_Y = 5;
const int BM_BUTTON_WIDTH = 50; // Bookmark Add/Del
const int BM_BUTTON_HEIGHT = 30;
const int BM_BUTTON_X = PREV_BM_BUTTON_X + PREV_BM_BUTTON_WIDTH + 5;
const int BM_BUTTON_Y = 5;
const int NEXT_BM_BUTTON_WIDTH = 40;
const int NEXT_BM_BUTTON_HEIGHT = 30;
const int NEXT_BM_BUTTON_X = BM_BUTTON_X + BM_BUTTON_WIDTH + 5;
const int NEXT_BM_BUTTON_Y = 5;


// Define content area based on button/header (Y position remains the same)
const int CONTENT_Y = BACK_BUTTON_Y + BACK_BUTTON_HEIGHT + TEXT_MARGIN_Y * 2;
const int CONTENT_HEIGHT = SCREEN_HEIGHT - CONTENT_Y - TEXT_MARGIN_Y;

// --- Constructor ---
TextViewerPage::TextViewerPage()
    : displayManager(Display::getInstance()),
      fontManager(Font::getInstance()),
      touchManager(Touch::getInstance()), // Initialize touchManager reference
      currentScrollLine(0),
      totalLines(0),
      linesPerPage(0),
      lineHeight(0),
      fileLoaded(false),
      useCache(false) // Initialize useCache to false
      // No comma needed after the last initializer
      {
          // Constructor body can be empty or used for other initializations if needed
      }

// --- Private Member Variables --- (Adding detectedBookmarks here for clarity)
    std::vector<int> detectedBookmarks; // Stores line numbers of automatically detected bookmarks

// --- Public Methods ---

void TextViewerPage::setFilePath(const String &path)
{
    Serial.print("TextViewerPage::setFilePath received path: ");
    Serial.println(path); // Log the path as soon as it's received
    filePath = path;
    Serial.print("TextViewerPage::filePath assigned: ");
    Serial.println(filePath); // Log the path after assignment
    fileLoaded = false;       // Reset loaded flag when path changes
    useCache = false;         // Reset cache flag
    cacheFilePath = "";       // Clear cache path
    currentScrollLine = 0;    // Reset scroll position
    errorMessage = "";        // Clear any previous error message
    lineIndex.clear();        // Clear the index map
    bookmarks.clear();        // Clear bookmarks when file changes
}

// Implementation of the virtual setParams method
void TextViewerPage::setParams(void *params)
{
    if (params)
    {
        // Cast the void pointer back to a String pointer
        String *pathPtr = static_cast<String *>(params);
        // Call the existing setFilePath method with the actual path String
        setFilePath(*pathPtr);
    }
    else
    {
        // Handle case where no parameters were passed (optional)
        Serial.println("TextViewerPage::setParams received null params.");
        // Maybe set a default state or show an error?
        setFilePath(""); // Set empty path to indicate an issue
    }
}

void TextViewerPage::display()
{
    if (!fileLoaded) // fileLoaded now means "metadata calculated or loaded from cache"
    {
        calculateLayout(); // Calculate layout based on font size

        // --- Unified JSON Cache Logic ---
        bool loadedFromCache = false;
        if (filePath.length() > 0) {
            cacheFilePath = filePath + ".cacheinfo"; // Construct JSON cache file path
            const char* originalPathCStr = filePath.c_str();
            const char* cachePathCStr = cacheFilePath.c_str();
            Serial.printf("DEBUG: Checking for JSON cache file: %s\n", cachePathCStr);

            // Check if original file and JSON cache file exist
            if (SDCard::getInstance().exists(originalPathCStr) && SDCard::getInstance().exists(cachePathCStr)) {
                Serial.println("DEBUG: Original file and JSON cache file exist.");
                File originalFile = SDCard::getInstance().openFile(originalPathCStr);
                if (originalFile) {
                    size_t originalSize = originalFile.size();
                    originalFile.close(); // Close original file after getting size

                    // Attempt to load from JSON cache, which includes size check internally
                    if (loadMetadataFromCache(originalSize)) { // Pass original size for validation
                        Serial.println("DEBUG: Successfully loaded metadata from JSON cache.");
                        fileLoaded = true; // Mark as loaded
                        loadedFromCache = true;
                        startTimeMillis = millis(); // Set start time for potential redraws
                    } else {
                        Serial.println("DEBUG: Failed to load metadata from JSON cache (invalid size or corrupted?). Removing cache and recalculating.");
                        SD.remove(cachePathCStr); // Use standard SD.remove()
                    }
                } else {
                     Serial.println("DEBUG: Error opening original file to get size for cache validation.");
                }
            } else {
                 Serial.printf("DEBUG: Original file (%s) or JSON cache file (%s) does not exist. Will calculate metadata.\n",
                               originalPathCStr, cachePathCStr);
            }
        }
        // --- End Unified JSON Cache Logic ---

        // If not loaded from cache, calculate metadata
        if (!loadedFromCache) {
            calculateFileMetadata(); // Calculate size, total lines, and index
        }
    }

    // If metadata calculation/loading failed, the respective functions would have set fileLoaded=true
    // and potentially stored an error message in lines[0].
    // So, we clear *after* loadContent potentially displayed progress.
    displayManager.clear();

    // Draw Back Button (similar to FileBrowserPage)
    displayManager.getTFT()->fillRoundRect(BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT, 5, TFT_BLUE);
    displayManager.getTFT()->drawRoundRect(BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT, 5, TFT_WHITE);
    displayManager.drawCenteredText("Back", BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT, 1, false);

    // Draw Top Button
    displayManager.getTFT()->fillRoundRect(TOP_BUTTON_X, TOP_BUTTON_Y, TOP_BUTTON_WIDTH, TOP_BUTTON_HEIGHT, 5, TFT_CYAN); // Different color
    displayManager.getTFT()->drawRoundRect(TOP_BUTTON_X, TOP_BUTTON_Y, TOP_BUTTON_WIDTH, TOP_BUTTON_HEIGHT, 5, TFT_WHITE);
    displayManager.drawCenteredText("Top", TOP_BUTTON_X, TOP_BUTTON_Y, TOP_BUTTON_WIDTH, TOP_BUTTON_HEIGHT, 1, false);

    // Draw Prev Bookmark Button
    displayManager.getTFT()->fillRoundRect(PREV_BM_BUTTON_X, PREV_BM_BUTTON_Y, PREV_BM_BUTTON_WIDTH, PREV_BM_BUTTON_HEIGHT, 5, TFT_ORANGE);
    displayManager.getTFT()->drawRoundRect(PREV_BM_BUTTON_X, PREV_BM_BUTTON_Y, PREV_BM_BUTTON_WIDTH, PREV_BM_BUTTON_HEIGHT, 5, TFT_WHITE);
    displayManager.drawCenteredText("<", PREV_BM_BUTTON_X, PREV_BM_BUTTON_Y, PREV_BM_BUTTON_WIDTH, PREV_BM_BUTTON_HEIGHT, 2, false); // Larger font for symbol

    // Draw Bookmark Add/Remove Button
    displayManager.getTFT()->fillRoundRect(BM_BUTTON_X, BM_BUTTON_Y, BM_BUTTON_WIDTH, BM_BUTTON_HEIGHT, 5, TFT_MAGENTA);
    displayManager.getTFT()->drawRoundRect(BM_BUTTON_X, BM_BUTTON_Y, BM_BUTTON_WIDTH, BM_BUTTON_HEIGHT, 5, TFT_WHITE);
    displayManager.drawCenteredText("Bmk", BM_BUTTON_X, BM_BUTTON_Y, BM_BUTTON_WIDTH, BM_BUTTON_HEIGHT, 1, false);

    // Draw Next Bookmark Button
    displayManager.getTFT()->fillRoundRect(NEXT_BM_BUTTON_X, NEXT_BM_BUTTON_Y, NEXT_BM_BUTTON_WIDTH, NEXT_BM_BUTTON_HEIGHT, 5, TFT_ORANGE);
    displayManager.getTFT()->drawRoundRect(NEXT_BM_BUTTON_X, NEXT_BM_BUTTON_Y, NEXT_BM_BUTTON_WIDTH, NEXT_BM_BUTTON_HEIGHT, 5, TFT_WHITE);
    displayManager.drawCenteredText(">", NEXT_BM_BUTTON_X, NEXT_BM_BUTTON_Y, NEXT_BM_BUTTON_WIDTH, NEXT_BM_BUTTON_HEIGHT, 2, false); // Larger font for symbol


    // Draw Content Area Separator (remains the same Y position)
    displayManager.getTFT()->drawFastHLine(0, CONTENT_Y - TEXT_MARGIN_Y - 1, SCREEN_WIDTH, TFT_DARKGREY); // Adjusted Y slightly for clarity

    if (fileLoaded) // Check if metadata calculation is done
    {
        // Check if an error occurred during metadata calculation
        if (this->errorMessage.length() > 0) { // Use this->errorMessage
             displayManager.drawCenteredText(this->errorMessage.c_str(), 0, CONTENT_Y, SCREEN_WIDTH, CONTENT_HEIGHT, 2);
        } else if (totalLines > 0) { // Check if metadata seems valid (just check lines now)
            // Only draw content and scrollbar if no error and metadata loaded

            // --- Draw scrollbar FIRST ---
            drawScrollbar();
            // --- Then draw content ---
            drawContent();

        } else {
            // Handle case where fileLoaded is true but no lines/positions (e.g., empty file)
             displayManager.drawCenteredText("File is empty.", 0, CONTENT_Y, SCREEN_WIDTH, CONTENT_HEIGHT, 2);
        }
    }
    // No explicit else needed here, as loadContent handles the initial "Loading..." or error display.
}

void TextViewerPage::handleTouch(uint16_t x, uint16_t y)
{
    // Handle Back Button Touch
    if (x >= BACK_BUTTON_X && x < BACK_BUTTON_X + BACK_BUTTON_WIDTH &&
        y >= BACK_BUTTON_Y && y < BACK_BUTTON_Y + BACK_BUTTON_HEIGHT)
    {
        Router::getInstance().goBack(); // Navigate back
        return;
    }

    // Handle Top Button Touch
    if (x >= TOP_BUTTON_X && x < TOP_BUTTON_X + TOP_BUTTON_WIDTH &&
        y >= TOP_BUTTON_Y && y < TOP_BUTTON_Y + TOP_BUTTON_HEIGHT)
    {
        if (currentScrollLine != 0) {
            Serial.println("Top button touched. Scrolling to top.");
            currentScrollLine = 0;
            // Redraw content and scrollbar
            int scrollbarX = SCREEN_WIDTH - SCROLLBAR_WIDTH - SCROLLBAR_MARGIN;
            int contentClearX = TEXT_MARGIN_X;
            int contentClearY = CONTENT_Y;
            int contentClearWidth = SCREEN_WIDTH - TEXT_MARGIN_X * 2 - SCROLLBAR_WIDTH - SCROLLBAR_MARGIN;
            int contentClearHeight = CONTENT_HEIGHT;
            displayManager.getTFT()->fillRect(contentClearX, contentClearY, contentClearWidth, contentClearHeight, TFT_BLACK);
            displayManager.getTFT()->fillRect(scrollbarX, contentClearY, SCROLLBAR_WIDTH, contentClearHeight, TFT_BLACK); // Clear scrollbar track area
            drawContent();
            drawScrollbar();
        }
        return; // Don't process other touches if Top button was hit
    }

    // Handle Prev Bookmark Button Touch
    if (x >= PREV_BM_BUTTON_X && x < PREV_BM_BUTTON_X + PREV_BM_BUTTON_WIDTH &&
        y >= PREV_BM_BUTTON_Y && y < PREV_BM_BUTTON_Y + PREV_BM_BUTTON_HEIGHT)
    {
        goToPrevBookmark();
        return;
    }

    // Handle Bookmark Add/Remove Button Touch
    if (x >= BM_BUTTON_X && x < BM_BUTTON_X + BM_BUTTON_WIDTH &&
        y >= BM_BUTTON_Y && y < BM_BUTTON_Y + BM_BUTTON_HEIGHT)
    {
        toggleBookmark();
        return;
    }

    // Handle Next Bookmark Button Touch
    if (x >= NEXT_BM_BUTTON_X && x < NEXT_BM_BUTTON_X + NEXT_BM_BUTTON_WIDTH &&
        y >= NEXT_BM_BUTTON_Y && y < NEXT_BM_BUTTON_Y + NEXT_BM_BUTTON_HEIGHT)
    {
        goToNextBookmark();
        return;
    }


    // Handle Scrolling Touch (only if content is scrollable)
    if (fileLoaded && totalLines > linesPerPage) // Ensure file is loaded and scrollable
    {
        int scrollbarX = SCREEN_WIDTH - SCROLLBAR_WIDTH - SCROLLBAR_MARGIN;
        int scrollbarY = CONTENT_Y;
        int scrollbarHeight = CONTENT_HEIGHT;

        // Check if touch is within the scrollbar area
        if (x >= scrollbarX && x < scrollbarX + SCROLLBAR_WIDTH &&
            y >= scrollbarY && y < scrollbarY + scrollbarHeight)
        {
            // --- Scrollbar Touch Logic ---
            int relativeY = y - scrollbarY; // Y position relative to scrollbar top
            // Calculate target scroll line based on relative Y position
            // Ensure division by non-zero height
            float touchRatio = (scrollbarHeight > 0) ? (float)relativeY / scrollbarHeight : 0.0f;

            // Calculate the target top line, considering the range of possible top lines (0 to totalLines - linesPerPage)
            int targetLine = (int)(touchRatio * (totalLines - linesPerPage));

            // Clamp the target line
            targetLine = std::max(0, targetLine);
            targetLine = std::min(targetLine, totalLines - linesPerPage);

            // Only redraw if the line actually changes
            if (targetLine != currentScrollLine) {
                currentScrollLine = targetLine;
                Serial.printf("Scrollbar touched. Jumping to line: %d\n", currentScrollLine);

                // Redraw content and scrollbar (similar to handleScroll redraw logic)
                int contentClearX = TEXT_MARGIN_X;
                int contentClearY = CONTENT_Y;
                int contentClearWidth = SCREEN_WIDTH - TEXT_MARGIN_X * 2 - SCROLLBAR_WIDTH - SCROLLBAR_MARGIN;
                int contentClearHeight = CONTENT_HEIGHT;
                displayManager.getTFT()->fillRect(contentClearX, contentClearY, contentClearWidth, contentClearHeight, TFT_BLACK);
                displayManager.getTFT()->fillRect(scrollbarX, contentClearY, SCROLLBAR_WIDTH, contentClearHeight, TFT_BLACK); // Clear scrollbar track area

                drawContent();
                drawScrollbar();
            }
        }
        // Check if touch is within the main content area (excluding header/button AND scrollbar)
        else if (y > CONTENT_Y && x < scrollbarX) // Touched in content area, left of scrollbar
        {
            // --- Existing Half-Page Scroll Logic ---
            handleScroll(y); // Use the existing function for taps in the content area
        }
    }
}

// --- Private Helper Methods ---

// Helper function to format bytes into KB/MB
String formatBytes(size_t bytes) {
    if (bytes < 1024) {
        return String(bytes) + " B";
    } else if (bytes < 1024 * 1024) {
        return String(bytes / 1024.0, 1) + " KB";
    } else {
        return String(bytes / (1024.0 * 1024.0), 1) + " MB";
    }
}

// Helper function to format seconds into MM:SS
String formatETC(unsigned long seconds) {
    if (seconds == 0) return "--:--";
    unsigned long minutes = seconds / 60;
    seconds %= 60;
    char buffer[6]; // MM:SS + null terminator
    snprintf(buffer, sizeof(buffer), "%02lu:%02lu", minutes, seconds);
    return String(buffer);
}


void TextViewerPage::updateLoadingProgress(size_t currentBytes, size_t totalBytes, int currentLineCount, unsigned long elapsedMillis)
{
    // Define progress bar dimensions and position (centered)
    const int barWidth = SCREEN_WIDTH * 0.8;
    const int barHeight = 20;
    const int barX = (SCREEN_WIDTH - barWidth) / 2;
    const int barY = (SCREEN_HEIGHT - barHeight) / 2; // Center vertically

    // Calculate percentage
    int percentage = (totalBytes > 0) ? (int)(((float)currentBytes / totalBytes) * 100) : 0;
    percentage = std::max(0, std::min(100, percentage)); // Clamp between 0 and 100

    // --- Clear relevant areas ---
    // Clear area for top-left info text
    displayManager.getTFT()->fillRect(0, 0, SCREEN_WIDTH, 20, TFT_BLACK); // Clear top area for info text
    // Clear area for progress bar and percentage text below it
    int clearBarAreaY = barY; // Start clearing at the bar's top
    int clearBarAreaHeight = barHeight + 25; // Height to clear (bar + percentage text area below)
    displayManager.getTFT()->fillRect(barX - 5, clearBarAreaY, barWidth + 10, clearBarAreaHeight, TFT_BLACK);

    // --- Calculate ETC ---
    unsigned long etcSeconds = 0;
    if (currentBytes > 0 && elapsedMillis > 100) { // Avoid division by zero and unstable early values
        float bytesPerSecond = (float)currentBytes / (elapsedMillis / 1000.0f);
        if (bytesPerSecond > 0) {
            size_t remainingBytes = totalBytes - currentBytes;
            etcSeconds = (unsigned long)(remainingBytes / bytesPerSecond);
        }
    }
    String etcText = "ETC: " + formatETC(etcSeconds);


    // --- Draw Top-Left Info Text (Size, Lines, ETC) ---
    String sizeText = "Size: " + formatBytes(totalBytes);
    String lineText = "Lines: " + String(currentLineCount);
    // Combine info text, potentially wrapping if too long (simple concatenation for now)
    String infoText = sizeText + "  " + lineText + "  " + etcText;
    // Use displayManager.drawText for custom font rendering at top-left
    displayManager.drawText(infoText.c_str(), 5, 5, TEXT_FONT_SIZE, true); // Use font size 1 at (5, 5)

    // --- Draw Progress Bar ---
    // Draw progress bar outline
    displayManager.getTFT()->drawRect(barX, barY, barWidth, barHeight, TFT_WHITE);

    // Draw filled portion of the progress bar
    int fillWidth = (int)(((float)percentage / 100) * (barWidth - 4)); // Inner width, leave border
    if (fillWidth > 0) {
        displayManager.getTFT()->fillRect(barX + 2, barY + 2, fillWidth, barHeight - 4, TFT_BLUE);
    }

    // Draw percentage text further below the bar
    String progressText = String(percentage) + "%";
    int textY = barY + barHeight + 8; // Increased spacing below bar (was 5)
    displayManager.drawCenteredText(progressText.c_str(), 0, textY, SCREEN_WIDTH, 20, 2, false); // Use font size 2, built-in font

    // Optional: Force display update if needed, though TFT_eSPI usually updates automatically
    // displayManager.getTFT()->display(); // Or similar command if available/needed
}

void TextViewerPage::calculateLayout()
{
    // Use font size 1 (16x16) for calculations
    lineHeight = fontManager.getCharacterHeight(TEXT_FONT_SIZE * 16); // Assuming size 1 maps to 16px font
    if (lineHeight <= 0) // Check if lineHeight is valid
        lineHeight = 16; // Fallback if font not loaded yet or invalid size

    int availableHeight = CONTENT_HEIGHT; // Use defined content height
    if (availableHeight <= 0) availableHeight = 1; // Prevent division by zero

    linesPerPage = availableHeight / lineHeight;
    if (linesPerPage < 1)
        linesPerPage = 1; // Ensure at least one line fits
}

// Renamed from loadContent. Calculates metadata (size, total wrapped lines)
// and populates the partial line index. Updates progress display.
void TextViewerPage::calculateFileMetadata()
{
    lineIndex.clear(); // Clear previous index
    currentScrollLine = 0;
    totalLines = 0; // Reset total lines count
    fileLoaded = false; // Set to false initially
    startTimeMillis = millis(); // Record start time for ETC
    detectedBookmarks.clear(); // Clear previously detected bookmarks

    const char *pathCStr = filePath.c_str();

    // Don't log path here, might be sensitive or long. Log errors instead.

    // Clear previous error message
    this->errorMessage = ""; // Ensure member variable is cleared

    if (!SDCard::getInstance().exists(pathCStr))
    {
        Serial.print("Error: File not found: "); Serial.println(pathCStr);
        this->errorMessage = "Error: File not found."; // Store in member variable
        fileLoaded = true; // Mark as loaded (with error)
        // lineStartPositions.clear(); // No longer needed
        totalLines = 0; // No lines calculated
        return;
    }

    File file = SDCard::getInstance().openFile(pathCStr);
    if (!file)
    {
        Serial.print("Error: Could not open file: "); Serial.println(pathCStr);
        this->errorMessage = "Error: Could not open file."; // Store in member variable
        fileLoaded = true; // Mark as loaded (with error)
        // lineStartPositions.clear(); // No longer needed
        totalLines = 0;
        return;
    }

    // --- Get total file size for progress ---
    size_t totalSize = file.size();
    // int lastPercentage = -1; // No longer using percentage for update trigger
    unsigned long lastProgressUpdateMillis = 0; // Track time of last progress update

    // --- Local line counter for this pass ---
    int calculatedLines = 0;
    lineIndex[0] = 0; // First line always starts at position 0

    // --- Initial progress display ---
    displayManager.clear(); // Clear screen for loading progress
    updateLoadingProgress(0, totalSize, calculatedLines, 0); // Show 0 lines, 0 elapsed initially
    yield(); // Allow display to update

    Serial.print("Loading file: "); Serial.println(pathCStr); // Log start of loading

    int availableWidth = SCREEN_WIDTH - TEXT_MARGIN_X * 2 - SCROLLBAR_WIDTH - SCROLLBAR_MARGIN;
    if (availableWidth <= 0)
    {
        Serial.println("Error: Screen too narrow for text.");
        this->errorMessage = "Error: Screen too narrow."; // Store in member variable
        fileLoaded = true; // Mark as loaded (with error)
        // lineStartPositions.clear(); // No longer needed
        totalLines = 0;
        file.close();
        return;
    }

    String currentLine = "";
    String wordBuffer = "";

    // Define the width calculation lambda function *before* the loop
    auto calculateStringWidth = [&](const String& s) -> int {
        int totalWidth = 0;
        size_t offset = 0;
        uint16_t baseSize = TEXT_FONT_SIZE * 16; // Base size (e.g., 16)
        while (offset < s.length()) {
            String nextCharStr = fontManager.getNextCharacter(s.c_str(), offset);
            if (nextCharStr.length() > 0) {
                // Estimate width: ASCII ~ half, Non-ASCII ~ full
                if (fontManager.isAscii(nextCharStr.c_str())) {
                    totalWidth += baseSize / 2; // Estimate ASCII width as half
                } else {
                    totalWidth += baseSize;     // Estimate Non-ASCII width as full
                }
            }
        }
        return totalWidth;
    };

    while (file.available())
    {
        char c = file.read();

        // --- Periodic Progress Update (Time-based) ---
        unsigned long currentMillis = millis();
        if (currentMillis - lastProgressUpdateMillis >= 100) { // Update every 20ms
            updateLoadingProgress(file.position(), totalSize, calculatedLines, currentMillis - startTimeMillis);
            lastProgressUpdateMillis = currentMillis; // Record time of this update
            yield(); // Allow other tasks (like display updates) to run
        }
        // --- End Progress Update ---


        // Handle line breaks first (CRLF, LF, CR)
        if (c == '\n' || c == '\r')
        {
            if (c == '\r' && file.peek() == '\n') {
                file.read(); // Consume the '\n' for CRLF
            }

            // Process the completed line (including the last word buffer content)
            // size_t lineStartPosition = file.position(); // No longer storing positions

            if (wordBuffer.length() > 0) {
                if (calculateStringWidth(currentLine) + calculateStringWidth(wordBuffer) <= availableWidth) {
                    currentLine += wordBuffer;
                } else {
                    // Final word doesn't fit, count the line without it
                    if (currentLine.length() > 0) {
                        calculatedLines++;
                        // Position for the *next* line (the wordBuffer) is tricky, estimate based on current char pos?
                        // For simplicity, we'll store the position *after* the newline for now.
                        // The seek in drawContent will need refinement if exact word start is needed.
                    }
                    currentLine = wordBuffer; // Word becomes the new line
                }
                wordBuffer = ""; // Clear buffer after processing
            }
            calculatedLines++; // Count the completed line
            // --- Bookmark Detection ---
            String fullLineContent = currentLine; // Check the line content before resetting
            int bookmarkIndex = fullLineContent.indexOf("%书签标志%");
            if (bookmarkIndex != -1) {
                 Serial.printf("DEBUG: Found '%%书签标志%%' in line content: '%s' at index %d\n", fullLineContent.c_str(), bookmarkIndex);
                 if (detectedBookmarks.empty() || detectedBookmarks.back() != calculatedLines) { // Avoid duplicates for same line
                    detectedBookmarks.push_back(calculatedLines);
                    Serial.printf("DEBUG: Added detected bookmark for line: %d. Total detected: %u\n", calculatedLines, detectedBookmarks.size());
                 } else {
                    Serial.printf("DEBUG: Duplicate bookmark marker detected for line: %d. Skipping add.\n", calculatedLines);
                 }
            }
            // --- End Bookmark Detection ---

            // Store index point if interval is reached
            if (calculatedLines % INDEX_INTERVAL == 0) {
                // Store the position *after* the newline, which is the start of the next line
                lineIndex[calculatedLines] = file.position();
            }
            currentLine = "";             // Reset for the next line from the file
            continue; // Move to next character from file
        }

        // If it's not a newline, process the character for word wrapping

        // Check width *before* adding the character to the word buffer
        // This helps in deciding whether to break the line *before* the current word
        int currentLinePixelWidth = calculateStringWidth(currentLine);
        // Test width with potential new char added to word buffer
        int wordBufferPlusCharWidth = calculateStringWidth(wordBuffer + c);

        // Case 1: Adding the current character to the word buffer makes it exceed the line width
        if (currentLinePixelWidth + wordBufferPlusCharWidth > availableWidth)
        {
            // Subcase 1.1: The current line already has content.
            if (currentLine.length() > 0) {
                // --- Bookmark Detection ---
                if (currentLine.indexOf("%书签标志%") != -1) {
                    if (detectedBookmarks.empty() || detectedBookmarks.back() != calculatedLines) {
                        detectedBookmarks.push_back(calculatedLines);
                        Serial.printf("Detected bookmark marker at wrapped line: %d\n", calculatedLines);
                    }
                }
                // --- End Bookmark Detection ---
                // Count the current line (without the word buffer)
                calculatedLines++;
                // Store index point if interval is reached
                if (calculatedLines % INDEX_INTERVAL == 0) {
                    // Position is tricky here. It's the start of the wordBuffer line.
                    // Estimate based on current file position minus the buffer length + char we just read.
                    lineIndex[calculatedLines] = file.position() - (wordBuffer.length() + 1);
                }
                // Start the new line with the word buffer (which doesn't include 'c' yet)
                currentLine = wordBuffer;
                wordBuffer = ""; // Clear word buffer
                // Start new word buffer with the current character 'c' (unless it's leading whitespace)
                if (c != ' ' && c != '\t') {
                     wordBuffer += c;
                }
            }
            // Subcase 1.2: The current line is empty, meaning the word buffer itself (plus 'c') is too long.
            else {
                // We need to break the word buffer itself.
                String fittedPart = "";
                String remainingPart = "";
                int fittedWidth = 0;
                size_t wordOffset = 0;
                uint16_t baseSize = TEXT_FONT_SIZE * 16;

                // Iterate through the *existing* wordBuffer to see what fits
                while(wordOffset < wordBuffer.length()) {
                    String nextCharStr = fontManager.getNextCharacter(wordBuffer.c_str(), wordOffset);
                    if (nextCharStr.length() > 0) {
                        int charWidth = fontManager.isAscii(nextCharStr.c_str()) ? (baseSize / 2) : baseSize;
                        if (fittedWidth + charWidth <= availableWidth) {
                            fittedPart += nextCharStr;
                            fittedWidth += charWidth;
                        } else {
                            // Character doesn't fit, the rest goes to remainingPart
                            // Correctly get the substring from the *start* of the character that didn't fit
                            remainingPart = wordBuffer.substring(wordOffset - nextCharStr.length());
                            break;
                        }
                    } else {
                        break; // End of word buffer string
                    }
                }

                // Add the part that fits (if any)
                if (fittedPart.length() > 0) {
                    // --- Bookmark Detection ---
                    if (fittedPart.indexOf("%书签标志%") != -1) {
                         if (detectedBookmarks.empty() || detectedBookmarks.back() != calculatedLines) {
                            detectedBookmarks.push_back(calculatedLines);
                            Serial.printf("Detected bookmark marker at hard-wrapped line: %d\n", calculatedLines);
                         }
                    }
                    // --- End Bookmark Detection ---
                    calculatedLines++; // Count the fitted part as a line
                    // Store index point if interval is reached
                    if (calculatedLines % INDEX_INTERVAL == 0) {
                        // Position is start of remainingPart line. Estimate.
                         lineIndex[calculatedLines] = file.position() - (remainingPart.length() + 1);
                    }
                }
                // The remaining part becomes the new word buffer, starting with the current char 'c' (unless whitespace)
                wordBuffer = remainingPart;
                if (c != ' ' && c != '\t') {
                     wordBuffer += c;
                }
                currentLine = ""; // Ensure current line is empty
            }
        }
        // Case 2: Adding the character doesn't exceed the width
        else {
            // Add the character to the word buffer
            wordBuffer += c;

            // If the character is a word separator (space/tab), add the word buffer to the current line
            if (c == ' ' || c == '\t') {
                // Check if adding the word buffer (including space) fits
                if (calculateStringWidth(currentLine) + calculateStringWidth(wordBuffer) <= availableWidth) {
                    currentLine += wordBuffer; // Add word buffer (including the space/tab)
                    wordBuffer = "";           // Clear word buffer for the next word
                } else {
                    // Word buffer with space doesn't fit. Count current line.
                    if (currentLine.length() > 0) {
                         calculatedLines++;
                         // Store index point if interval is reached
                         if (calculatedLines % INDEX_INTERVAL == 0) {
                             // Position is start of wordBuffer line. Estimate.
                             lineIndex[calculatedLines] = file.position() - (wordBuffer.length() + 1);
                         }
                    }
                    // Start new line with the word buffer (including space)
                    currentLine = wordBuffer;
                    wordBuffer = "";
                }
            }
            // else: it's a normal character within a word, just keep accumulating in wordBuffer
        }
    } // End of while(file.available()) loop

    // --- Final progress update ---
    // Ensure 100% is shown, along with the final line count and final time
    updateLoadingProgress(totalSize, totalSize, calculatedLines, millis() - startTimeMillis);
    // A small delay might be needed here if the screen clears too quickly
    // delay(100); // Optional: delay 100ms to ensure 100% is visible

    // Add any remaining content from the buffers after the loop finishes
    if (wordBuffer.length() > 0) {
        // Check if the final word fits on the last line
        if (calculateStringWidth(currentLine) + calculateStringWidth(wordBuffer) <= availableWidth) {
            currentLine += wordBuffer;
        } else {
            // Doesn't fit, count the current line (if any)
            if (currentLine.length() > 0) {
                calculatedLines++;
                 // Store index point if interval is reached
                 if (calculatedLines % INDEX_INTERVAL == 0) {
                     // Position is start of wordBuffer line. Estimate.
                     lineIndex[calculatedLines] = file.position() - wordBuffer.length();
                 }
            }
            currentLine = wordBuffer; // Word buffer becomes the last line
                 }
            }
            // Count the very last line if it has content
            if (currentLine.length() > 0) {
                 // --- Bookmark Detection ---
                 int finalBookmarkIndex = currentLine.indexOf("%书签标志%");
                 if (finalBookmarkIndex != -1) {
                     Serial.printf("DEBUG: Found '%%书签标志%%' in final line content: '%s' at index %d\n", currentLine.c_str(), finalBookmarkIndex);
                     if (detectedBookmarks.empty() || detectedBookmarks.back() != calculatedLines) {
                        detectedBookmarks.push_back(calculatedLines);
                        Serial.printf("DEBUG: Added detected bookmark for final line: %d. Total detected: %u\n", calculatedLines, detectedBookmarks.size());
                     } else {
                         Serial.printf("DEBUG: Duplicate bookmark marker detected for final line: %d. Skipping add.\n", calculatedLines);
                     }
                 }
                 // --- End Bookmark Detection ---
                calculatedLines++;
                 // Store index point if interval is reached (unlikely for the very last line, but check)
                 if (calculatedLines % INDEX_INTERVAL == 0) {
             // Position is tricky, maybe store end of file? Or just rely on previous index.
             // Let's not store an index for the very last partial line.
         }
    }

    file.close();
    totalLines = calculatedLines; // Store the final calculated count
    fileLoaded = true; // Mark metadata as calculated *after* processing
    Serial.printf("File metadata calculated. Total wrapped lines: %d. Index points: %d\n", totalLines, lineIndex.size());

    // If calculation was successful (no error message), save to cache
    if (this->errorMessage.length() == 0 && totalLines > 0) {
        Serial.printf("DEBUG: Metadata calculation finished. Detected bookmarks count: %u. Calling saveMetadataToCache.\n", detectedBookmarks.size());
        saveMetadataToCache();
    } else {
        Serial.printf("DEBUG: Metadata calculation finished. Error: '%s', Total Lines: %d. Skipping saveMetadataToCache.\n", this->errorMessage.c_str(), totalLines);
    }
}

// Refactored drawContent: Reads file on demand, wraps, and draws visible lines.
void TextViewerPage::drawContent()
{
    int y = CONTENT_Y; // Starting Y position for drawing text
    int linesDrawn = 0; // Counter for lines drawn on the current screen

    // Clear the content area before drawing new text
    displayManager.getTFT()->fillRect(TEXT_MARGIN_X, y,
                                      SCREEN_WIDTH - TEXT_MARGIN_X * 2 - SCROLLBAR_WIDTH - SCROLLBAR_MARGIN,
                                      CONTENT_HEIGHT, // Use calculated content height
                                      TFT_BLACK); // Assuming black background

    // --- File Reading and On-the-Fly Wrapping/Drawing ---
    const char *pathCStr = filePath.c_str();
    File file = SDCard::getInstance().openFile(pathCStr);
    if (!file) {
        // Should not happen if calculateFileMetadata succeeded, but handle defensively
        displayManager.drawText("Error: Cannot reopen file.", TEXT_MARGIN_X, y, TEXT_FONT_SIZE, true);
        return;
    }

    int availableWidth = SCREEN_WIDTH - TEXT_MARGIN_X * 2 - SCROLLBAR_WIDTH - SCROLLBAR_MARGIN;
    if (availableWidth <= 0) {
        displayManager.drawText("Error: Screen too narrow.", TEXT_MARGIN_X, y, TEXT_FONT_SIZE, true);
        file.close();
        return;
    }

    // --- Check if metadata is loaded ---
    // Also check if an error occurred during loading and if scroll line is valid
    if (!fileLoaded || this->errorMessage.length() > 0 || currentScrollLine < 0 || (totalLines > 0 && currentScrollLine >= totalLines)) {
        String errorToShow = (this->errorMessage.length() > 0) ? this->errorMessage : "Error: Invalid state or scroll position.";
        displayManager.drawText(errorToShow.c_str(), TEXT_MARGIN_X, y, TEXT_FONT_SIZE, true);
        if (file) file.close();
        return;
    }

    // --- Use partial index to find starting point ---
    int seekLine = 0;
    size_t seekPos = 0;
    // Find the largest index key less than or equal to currentScrollLine
    auto it = lineIndex.upper_bound(currentScrollLine); // Find first element *greater* than currentScrollLine
    if (it != lineIndex.begin()) {
        --it; // Go back to the element less than or equal to currentScrollLine
        seekLine = it->first;
        seekPos = it->second;
    }
    // else: currentScrollLine is before the first index point (or index is empty), so start from beginning (seekLine=0, seekPos=0)

    Serial.printf("DrawContent: Target line %d. Seeking to index line %d at pos %u\n", currentScrollLine, seekLine, seekPos);

    if (!file.seek(seekPos)) {
        Serial.printf("Error: Failed to seek to index position %u\n", seekPos);
        displayManager.drawText("Error: Seek failed.", TEXT_MARGIN_X, y, TEXT_FONT_SIZE, true);
        file.close();
        return;
    }

    // --- Read from seeked position, count lines until currentScrollLine ---
    String currentLine = "";
    String wordBuffer = "";
    int linesProcessedSoFar = seekLine; // Start counting from the line we seeked to

    // Re-use the width calculation lambda
    auto calculateStringWidth = [&](const String& s) -> int {
        int totalWidth = 0;
        size_t offset = 0;
        uint16_t baseSize = TEXT_FONT_SIZE * 16;
        while (offset < s.length()) {
            String nextCharStr = fontManager.getNextCharacter(s.c_str(), offset);
            if (nextCharStr.length() > 0) {
                if (fontManager.isAscii(nextCharStr.c_str())) {
                    totalWidth += baseSize / 2;
                } else {
                    totalWidth += baseSize;
                }
            }
        }
        return totalWidth;
    };

    // Lambda to draw a line *if* it's within the visible range
    auto drawVisibleLine = [&](const String& lineToDraw) {
        if (linesProcessedSoFar >= currentScrollLine && linesDrawn < linesPerPage) {
            displayManager.drawText(lineToDraw.c_str(), TEXT_MARGIN_X, y + (linesDrawn * lineHeight), TEXT_FONT_SIZE, true);
            linesDrawn++;
        }
         linesProcessedSoFar++; // Increment *after* checking visibility
     };

    // bool touchDetected = false; // REMOVED: Flag for touch interruption

    // Read from the seeked position until enough lines are drawn or EOF
    while (file.available() && linesDrawn < linesPerPage) // Stop drawing if screen is full
    {
        // --- REMOVED: Check for touch interrupt ---
        // if (touchManager.isTouched()) { ... }
        // --- End removed touch check ---

        // Read character by character from the seeked position
        char c = file.read();

        // Handle line breaks first (CRLF, LF, CR)
        if (c == '\n' || c == '\r') {
            if (c == '\r' && file.peek() == '\n') {
                file.read(); // Consume the '\n' for CRLF
            }

            // Process the completed line (including the last word buffer content)
            if (wordBuffer.length() > 0) {
                if (calculateStringWidth(currentLine) + calculateStringWidth(wordBuffer) <= availableWidth) {
                    currentLine += wordBuffer;
                } else {
                    // Word doesn't fit after newline
                    if (currentLine.length() > 0) drawVisibleLine(currentLine); // Draw previous line if not empty
                    currentLine = wordBuffer; // Word becomes the new line
                }
                wordBuffer = "";
            }
            // Draw the completed line (which might be empty if file ended with newline)
            drawVisibleLine(currentLine);
            currentLine = "";
            continue; // Move to next char
        }

        // If it's not a newline, process the character for word wrapping
        int currentLinePixelWidth = calculateStringWidth(currentLine);
        int wordBufferPlusCharWidth = calculateStringWidth(wordBuffer + c);

        // Case 1: Adding the current character makes the word buffer exceed the line width
        if (currentLinePixelWidth + wordBufferPlusCharWidth > availableWidth) {
            // Subcase 1.1: The current line already has content.
            if (currentLine.length() > 0) {
                drawVisibleLine(currentLine); // Draw the current line
                currentLine = wordBuffer;     // Start new line with word buffer
                wordBuffer = "";
                if (c != ' ' && c != '\t') wordBuffer += c; // Start new word buffer unless whitespace
            }
            // Subcase 1.2: The current line is empty (long word needs breaking).
            else {
                String fittedPart = "";
                String remainingPart = "";
                int fittedWidth = 0;
                size_t wordOffset = 0;
                uint16_t baseSize = TEXT_FONT_SIZE * 16;
                while(wordOffset < wordBuffer.length()) {
                     String nextCharStr = fontManager.getNextCharacter(wordBuffer.c_str(), wordOffset);
                     if (nextCharStr.length() > 0) {
                        int charWidth = fontManager.isAscii(nextCharStr.c_str()) ? (baseSize / 2) : baseSize;
                        if (fittedWidth + charWidth <= availableWidth) {
                            fittedPart += nextCharStr;
                            fittedWidth += charWidth;
                        } else {
                            remainingPart = wordBuffer.substring(wordOffset - nextCharStr.length());
                            break;
                        }
                    } else break;
                }
                if (fittedPart.length() > 0) drawVisibleLine(fittedPart); // Draw the fitted part
                wordBuffer = remainingPart;
                if (c != ' ' && c != '\t') wordBuffer += c; // Start new word buffer unless whitespace
                currentLine = "";
            }
        }
        // Case 2: Adding the character doesn't exceed the width
        else {
            wordBuffer += c;
            if (c == ' ' || c == '\t') {
                if (calculateStringWidth(currentLine) + calculateStringWidth(wordBuffer) <= availableWidth) {
                    currentLine += wordBuffer;
                    wordBuffer = "";
                } else {
                    // Word buffer with space doesn't fit. Count current line.
                    if (currentLine.length() > 0) drawVisibleLine(currentLine); // Draw current line
                    currentLine = wordBuffer; // Start new line with word buffer (incl. space)
                    wordBuffer = "";
                }
            }
        }
         // Optimization: If we've already processed enough lines to get past the visible area,
         // we could potentially break early, but the file.available() check handles this.
    } // End while

    // Draw any remaining content from the buffers after the loop finishes
    if (wordBuffer.length() > 0) {
        if (calculateStringWidth(currentLine) + calculateStringWidth(wordBuffer) <= availableWidth) {
            currentLine += wordBuffer;
        } else {
            // Doesn't fit, count the current line (if any) and add the word buffer as a new line
            if (currentLine.length() > 0) drawVisibleLine(currentLine);
            currentLine = wordBuffer; // Word buffer becomes the last line
        }
    }
    // Draw the very last line if it has content and we haven't filled the page yet
    if (currentLine.length() > 0) {
        drawVisibleLine(currentLine); // Draw the very last line if needed
    }

    file.close(); // Close the file regardless of how the loop exited

    // --- REMOVED: Handle touch if detected ---
    // if (touchDetected) { ... }
    // --- End removed touch handling ---

    // Proceed to draw the scrollbar (if needed)
    // Note: drawScrollbar is now called from display(), so this call might be redundant
    // Let's remove it from here and rely on display() calling it after drawContent finishes.
    // drawScrollbar(); // Removed from here
}


void TextViewerPage::drawScrollbar()
{
    if (totalLines <= linesPerPage)
        return; // No scrollbar needed

    int scrollbarX = SCREEN_WIDTH - SCROLLBAR_WIDTH - SCROLLBAR_MARGIN;
    int scrollbarY = CONTENT_Y; // Align with text content area top
    int scrollbarHeight = CONTENT_HEIGHT; // Align with text content area height

    // Draw scrollbar track (clear previous first)
    displayManager.getTFT()->fillRect(scrollbarX, scrollbarY, SCROLLBAR_WIDTH, scrollbarHeight, TFT_BLACK); // Clear area
    displayManager.getTFT()->drawRect(scrollbarX, scrollbarY, SCROLLBAR_WIDTH, scrollbarHeight, TFT_DARKGREY);

    // Calculate handle size and position
    // Ensure totalLines is not zero to avoid division by zero
    float linesRatio = (totalLines > 0) ? (float)linesPerPage / totalLines : 1.0f;
    int handleHeight = std::max(10, (int)(linesRatio * scrollbarHeight));
    handleHeight = std::min(handleHeight, scrollbarHeight); // Ensure handle doesn't exceed track

    // Ensure totalLines > linesPerPage before calculating scroll position ratio
    // Correct calculation for handle position based on scroll range
    float scrollRatio = (totalLines > linesPerPage) ? (float)currentScrollLine / (totalLines - linesPerPage) : 0.0f;
    int handleY = scrollbarY + (int)(scrollRatio * (scrollbarHeight - handleHeight)); // Position relative to available space

    // Clamp handle Y position just in case
    handleY = std::max(scrollbarY, handleY);
    handleY = std::min(handleY, scrollbarY + scrollbarHeight - handleHeight);


    // Draw scrollbar handle
    displayManager.getTFT()->fillRect(scrollbarX + 1, handleY, SCROLLBAR_WIDTH - 2, handleHeight, TFT_LIGHTGREY);

    // --- Draw Bookmark Markers ---
    if (!bookmarks.empty() && totalLines > linesPerPage) {
        for (int bookmarkLine : bookmarks) {
            // Calculate the relative position of the bookmark line
            float bookmarkScrollRatio = (float)bookmarkLine / (totalLines - linesPerPage);
            // Calculate the Y position on the scrollbar track
            int markerY = scrollbarY + (int)(bookmarkScrollRatio * scrollbarHeight);

            // Clamp marker Y position within the track bounds
            markerY = std::max(scrollbarY, markerY);
            markerY = std::min(markerY, scrollbarY + scrollbarHeight - 1); // -1 to keep it within bounds

            // Draw a 1px horizontal yellow line for manual bookmarks
            displayManager.getTFT()->drawFastHLine(scrollbarX, markerY, SCROLLBAR_WIDTH, TFT_YELLOW);
        }
    }

    // --- Draw Detected Bookmark Markers (Green) ---
    if (!detectedBookmarks.empty() && totalLines > linesPerPage) {
        for (int detectedLine : detectedBookmarks) {
            // Calculate the relative position of the detected bookmark line
            float detectedScrollRatio = (float)detectedLine / (totalLines - linesPerPage);
            // Calculate the Y position on the scrollbar track
            int detectedMarkerY = scrollbarY + (int)(detectedScrollRatio * scrollbarHeight);

            // Clamp marker Y position within the track bounds
            detectedMarkerY = std::max(scrollbarY, detectedMarkerY);
            detectedMarkerY = std::min(detectedMarkerY, scrollbarY + scrollbarHeight - 1); // -1 to keep it within bounds

            // Draw a 1px horizontal green line for detected bookmarks (using hex code 0x07E0)
            displayManager.getTFT()->drawFastHLine(scrollbarX, detectedMarkerY, SCROLLBAR_WIDTH, 0x07E0); // Use hex code for green
        }
    }
}

void TextViewerPage::handleScroll(int touchY)
{
    // Simple scroll: tapping top half scrolls up, bottom half scrolls down
    int contentTopY = CONTENT_Y; // Use constant
    int contentHeight = CONTENT_HEIGHT; // Use constant
    int midPointY = contentTopY + contentHeight / 2;

    int scrollAmount = linesPerPage / 2; // Scroll by half a page
    if (scrollAmount < 1)
        scrollAmount = 1;

    int previousScrollLine = currentScrollLine; // Store previous position

    if (touchY < midPointY)
    {
        // Scroll Up
        currentScrollLine -= scrollAmount;
    }
    else
    {
        // Scroll Down
        currentScrollLine += scrollAmount;
    }

    // Clamp scroll position
    currentScrollLine = std::max(0, currentScrollLine);
    // Adjust clamping: max scroll should be totalLines - linesPerPage
    if (totalLines > linesPerPage) {
         currentScrollLine = std::min(currentScrollLine, totalLines - linesPerPage);
    } else {
         currentScrollLine = 0; // Cannot scroll if content fits
    }


    // Only redraw if scroll position actually changed
    if (currentScrollLine != previousScrollLine) {
        // --- Clear only the content area, not the whole screen ---
        int contentClearX = TEXT_MARGIN_X;
        int contentClearY = CONTENT_Y;
        int contentClearWidth = SCREEN_WIDTH - TEXT_MARGIN_X * 2 - SCROLLBAR_WIDTH - SCROLLBAR_MARGIN;
        int contentClearHeight = CONTENT_HEIGHT;
        displayManager.getTFT()->fillRect(contentClearX, contentClearY, contentClearWidth, contentClearHeight, TFT_BLACK);

        // Clear the scrollbar area separately
        int scrollbarX = SCREEN_WIDTH - SCROLLBAR_WIDTH - SCROLLBAR_MARGIN;
        displayManager.getTFT()->fillRect(scrollbarX, contentClearY, SCROLLBAR_WIDTH, contentClearHeight, TFT_BLACK); // Clear scrollbar track area

        // --- No need to redraw static elements ---
        // displayManager.getTFT()->fillRoundRect(BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT, 5, TFT_BLUE);
        // displayManager.getTFT()->drawRoundRect(BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT, 5, TFT_WHITE);
        // displayManager.drawCenteredText("Back", BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT, 1, false);
        // displayManager.getTFT()->drawFastHLine(0, BACK_BUTTON_Y + BACK_BUTTON_HEIGHT + TEXT_MARGIN_Y - 1, SCREEN_WIDTH, TFT_DARKGREY);

        // Now draw the updated content and scrollbar directly into the cleared areas
        drawContent();   // This function clears its own area, but clearing above ensures no artifacts
        drawScrollbar(); // This function clears and redraws its area
    }
}

/**
 * @brief Cleans up resources used by the TextViewerPage.
 * Called when navigating away from the page to free memory and reset state.
 * Saves the current scroll position to cache before clearing.
 */
void TextViewerPage::cleanup() {
    Serial.println("TextViewerPage::cleanup() called.");

    // Save current scroll position to cache before clearing state,
    // but only if the file was loaded successfully without errors.
    if (fileLoaded && this->errorMessage.length() == 0 && totalLines > 0) {
        Serial.println("Saving last scroll position before cleanup.");
        saveMetadataToCache(); // Save current state (including scroll line)
    } else {
        Serial.println("Skipping cache save on cleanup due to load error or empty file.");
    }

    lineIndex.clear(); // Clear the partial index map
    // Reset state variables
    filePath = "";
    currentScrollLine = 0;
    totalLines = 0;
    fileLoaded = false;
    errorMessage = "";
    // Note: std::map doesn't have shrink_to_fit like std::vector.
    // Clearing it is usually sufficient to release nodes.
    bookmarks.clear(); // Clear bookmarks vector
    bookmarks.shrink_to_fit(); // Try to release memory (optional)

    Serial.println("TextViewerPage resources cleaned up.");
    // Reset cache-related members
    cacheFilePath = "";
    useCache = false;
}

// --- New Cache Helper Methods ---

/**
 * @brief Tries to load file metadata from the unified JSON cache file.
 * Validates the cached original file size against the provided current size.
 * @param currentOriginalFileSize The current size of the original .txt file to validate against the cache.
 * @return true if loading was successful and size matches, false otherwise.
 */
bool TextViewerPage::loadMetadataFromCache(size_t currentOriginalFileSize) {
    const char* cachePathCStr = cacheFilePath.c_str();
    Serial.printf("DEBUG: Attempting to load metadata from JSON cache: %s\n", cachePathCStr);

    File cacheFile = SDCard::getInstance().openFile(cachePathCStr, FILE_READ);
    if (!cacheFile) {
        Serial.println("DEBUG: JSON cache file not found or could not be opened.");
        errorMessage = "Cache not found."; // Set a temporary error message if needed
        return false;
    }

    // --- Increased JSON document size ---
    // Estimate required size: Base + (IndexEntries * SizePerIndex) + (Bookmarks * SizePerBookmark)
    // Let's start with a larger base size and adjust if needed based on logs/testing.
    // Example: 10k index entries * ~20 bytes/entry = 200KB. 100 bookmarks * ~5 bytes = 0.5KB.
    // Add overhead for keys and structure. Let's try 256KB initially.
    // IMPORTANT: This might exceed available RAM on some ESP32 models! Monitor memory usage.
    const size_t jsonCapacity = JSON_OBJECT_SIZE(5) + JSON_ARRAY_SIZE(500) + JSON_ARRAY_SIZE(10000) + 256 * 1024; // Adjust capacity as needed
    DynamicJsonDocument doc(jsonCapacity); // Allocate on heap

    Serial.println("DEBUG: Deserializing JSON from cache file...");
    DeserializationError error = deserializeJson(doc, cacheFile);
    cacheFile.close(); // Close file immediately after parsing

    if (error) {
        Serial.print("DEBUG: Failed to deserialize JSON cache: ");
        Serial.println(error.c_str());
        errorMessage = "Error: Cache invalid (JSON).";
        return false;
    }
     Serial.println("DEBUG: JSON deserialized successfully.");

    // --- Validate cached file size ---
    if (!doc.containsKey("originalFileSize") || doc["originalFileSize"].as<size_t>() != currentOriginalFileSize) {
        Serial.printf("DEBUG: Cached file size (%u) does not match current file size (%u). Cache is invalid.\n",
                      doc["originalFileSize"].as<size_t>(), currentOriginalFileSize);
        errorMessage = "Cache invalid (size mismatch).";
        return false;
    }
    Serial.println("DEBUG: Cached file size matches current file size.");

    // --- Extract data from JSON ---
    if (!doc.containsKey("totalLines") || !doc.containsKey("lastScrollLine")) {
         Serial.println("DEBUG: JSON cache missing required fields (totalLines or lastScrollLine).");
         errorMessage = "Error: Cache invalid (missing fields).";
         return false;
    }

    totalLines = doc["totalLines"].as<int>();
    int cachedScrollLine = doc["lastScrollLine"].as<int>();

    // Restore last scroll position, ensuring it's valid
    currentScrollLine = std::max(0, cachedScrollLine);
    if (totalLines > linesPerPage) {
        currentScrollLine = std::min(currentScrollLine, totalLines - linesPerPage);
    } else {
        currentScrollLine = 0; // Cannot scroll if content fits
    }
    Serial.printf("DEBUG: Loaded TotalLines: %d, LastScrollLine: %d (Restored to: %d)\n", totalLines, cachedScrollLine, currentScrollLine);


    // --- Load Line Index ---
    lineIndex.clear();
    if (doc.containsKey("lineIndex")) {
        JsonArray indexArray = doc["lineIndex"].as<JsonArray>();
        if (!indexArray.isNull()) {
             Serial.printf("DEBUG: Loading %u line index entries from JSON...\n", indexArray.size());
             for (JsonObject entry : indexArray) {
                 if (entry.containsKey("l") && entry.containsKey("p")) {
                     lineIndex[entry["l"].as<int>()] = entry["p"].as<size_t>();
                 } else {
                      Serial.println("DEBUG: Warning - Invalid line index entry found in JSON cache.");
                 }
             }
             Serial.printf("DEBUG: Finished loading %u line index entries.\n", lineIndex.size());
        } else {
             Serial.println("DEBUG: 'lineIndex' key found but is not a valid array in JSON cache.");
        }
    } else {
        Serial.println("DEBUG: 'lineIndex' key not found in JSON cache.");
    }


    // --- Load Manual Bookmarks ---
    bookmarks.clear();
    if (doc.containsKey("bookmarks")) {
        JsonArray bmArray = doc["bookmarks"].as<JsonArray>();
        if (!bmArray.isNull()) {
            Serial.printf("DEBUG: Loading %u manual bookmarks from JSON...\n", bmArray.size());
            bookmarks.reserve(bmArray.size());
            for (JsonVariant v : bmArray) {
                bookmarks.push_back(v.as<int>());
            }
            std::sort(bookmarks.begin(), bookmarks.end()); // Ensure sorted
            Serial.printf("DEBUG: Finished loading %u manual bookmarks.\n", bookmarks.size());
        } else {
             Serial.println("DEBUG: 'bookmarks' key found but is not a valid array in JSON cache.");
        }
    } else {
        Serial.println("DEBUG: 'bookmarks' key not found in JSON cache.");
    }

    // --- Load Detected Bookmarks ---
    detectedBookmarks.clear();
     if (doc.containsKey("detectedBookmarks")) {
        JsonArray dbmArray = doc["detectedBookmarks"].as<JsonArray>();
        if (!dbmArray.isNull()) {
            Serial.printf("DEBUG: Loading %u detected bookmarks from JSON...\n", dbmArray.size());
            detectedBookmarks.reserve(dbmArray.size());
            for (JsonVariant v : dbmArray) {
                detectedBookmarks.push_back(v.as<int>());
            }
             // No need to sort detected bookmarks, they are saved in order of detection
             Serial.printf("DEBUG: Finished loading %u detected bookmarks.\n", detectedBookmarks.size());
        } else {
             Serial.println("DEBUG: 'detectedBookmarks' key found but is not a valid array in JSON cache.");
        }
    } else {
        Serial.println("DEBUG: 'detectedBookmarks' key not found in JSON cache.");
    }


    errorMessage = ""; // Clear any previous error message if loading succeeded
    Serial.println("DEBUG: Successfully loaded all data from JSON cache.");
    return true;
}

/**
 * @brief Saves the calculated file metadata (original size, total lines, partial index,
 * last scroll position, manual bookmarks, detected bookmarks) to a unified JSON cache file.
 * Overwrites existing cache file if present.
 */
void TextViewerPage::saveMetadataToCache() {
    Serial.println("DEBUG: Starting unified JSON cache save process.");
    if (filePath.length() == 0) {
        Serial.println("DEBUG: Skipping JSON cache save: No file path.");
        return;
    }
     // We proceed even if lineIndex is empty, to save other data like scroll position/bookmarks.

    const char* originalPathCStr = filePath.c_str();
    const char* cachePathCStr = cacheFilePath.c_str(); // Should be like /path/to/file.txt.cacheinfo

    // Get original file size again to store it in the cache
    size_t currentOriginalFileSize = 0;
    File originalFile = SDCard::getInstance().openFile(originalPathCStr);
    if (!originalFile) {
        Serial.println("DEBUG: Error! Could not open original file to get size for saving cache.");
        // Decide if we should proceed without the size? Probably not.
        return;
    } else {
        currentOriginalFileSize = originalFile.size();
        originalFile.close();
        Serial.printf("DEBUG: Current original file size for cache: %u\n", currentOriginalFileSize);
    }

    // --- Create JSON Document ---
    // Adjust capacity based on expected data size. See notes in loadMetadataFromCache.
    const size_t jsonCapacity = JSON_OBJECT_SIZE(6) + JSON_ARRAY_SIZE(bookmarks.size()) + JSON_ARRAY_SIZE(detectedBookmarks.size()) + JSON_ARRAY_SIZE(lineIndex.size()) + 256 * 1024; // Adjust capacity
    DynamicJsonDocument doc(jsonCapacity);

    Serial.println("DEBUG: Populating JSON document...");
    doc["originalFileSize"] = currentOriginalFileSize;
    doc["totalLines"] = totalLines;
    doc["lastScrollLine"] = currentScrollLine;

    // --- Add Line Index ---
    JsonArray indexArray = doc.createNestedArray("lineIndex");
    if (!lineIndex.empty()) {
        Serial.printf("DEBUG: Adding %u line index entries to JSON...\n", lineIndex.size());
        for (const auto& pair : lineIndex) {
            JsonObject entry = indexArray.createNestedObject();
            entry["l"] = pair.first;  // Line number
            entry["p"] = pair.second; // Position
        }
    } else {
         Serial.println("DEBUG: Line index is empty. Adding empty array to JSON.");
    }

    // --- Add Manual Bookmarks ---
    JsonArray bmArray = doc.createNestedArray("bookmarks");
     if (!bookmarks.empty()) {
        Serial.printf("DEBUG: Adding %u manual bookmarks to JSON...\n", bookmarks.size());
        for (int lineNum : bookmarks) {
            bmArray.add(lineNum);
        }
    } else {
         Serial.println("DEBUG: Manual bookmarks vector is empty. Adding empty array to JSON.");
    }

    // --- Add Detected Bookmarks ---
    JsonArray dbmArray = doc.createNestedArray("detectedBookmarks");
    if (!detectedBookmarks.empty()) {
        Serial.printf("DEBUG: Adding %u detected bookmarks to JSON...\n", detectedBookmarks.size());
        for (int lineNum : detectedBookmarks) {
            dbmArray.add(lineNum);
        }
    } else {
         Serial.println("DEBUG: Detected bookmarks vector is empty. Adding empty array to JSON.");
    }

    // --- Write JSON to File ---
    Serial.printf("DEBUG: Attempting to open JSON cache file for writing: %s\n", cachePathCStr);
    File cacheFile = SDCard::getInstance().openFile(cachePathCStr, FILE_WRITE);
    if (!cacheFile) {
        Serial.println("DEBUG: Error! Could not open JSON cache file for writing.");
        return;
    } else {
         Serial.println("DEBUG: Successfully opened JSON cache file for writing.");
    }

    Serial.println("DEBUG: Attempting to serialize JSON to file...");
    size_t bytesWritten = serializeJson(doc, cacheFile);
    cacheFile.close(); // Close immediately after writing
    Serial.println("DEBUG: Closed JSON cache file after writing.");

    if (bytesWritten > 0) {
        Serial.printf("DEBUG: Successfully saved unified JSON cache for %s (%u bytes written).\n",
                      filePath.c_str(), bytesWritten);
    } else {
        Serial.println("DEBUG: Error! Failed to write JSON data (serializeJson returned 0 bytes).");
        Serial.println("DEBUG: Attempting to remove potentially corrupted/empty JSON cache file.");
        if (SD.remove(cachePathCStr)) { // Use standard SD.remove()
            Serial.println("DEBUG: Removed empty/corrupted JSON cache file.");
        } else {
            Serial.println("DEBUG: Failed to remove JSON cache file.");
        }
    }
    Serial.println("DEBUG: Finished unified JSON cache save process.");

    // --- REMOVED: Redundant global JSON cache saving logic ---
    // The unified JSON cache saved above now contains all necessary data,
    // including detected bookmarks. The separate global cache is no longer needed.
}

// --- Bookmark Helper Methods ---

/**
 * @brief Adds or removes a bookmark at the current scroll line.
 */
void TextViewerPage::toggleBookmark() {
    if (!fileLoaded || totalLines <= 0) return; // Can't bookmark if file not loaded or empty

    int lineToBookmark = currentScrollLine;

    // Check if the bookmark already exists
    auto it = std::find(bookmarks.begin(), bookmarks.end(), lineToBookmark);

    if (it != bookmarks.end()) {
        // Bookmark exists, remove it
        bookmarks.erase(it);
        Serial.printf("Bookmark removed for line: %d\n", lineToBookmark);
    } else {
        // Bookmark doesn't exist, add it
        bookmarks.push_back(lineToBookmark);
        // Keep the vector sorted for efficient navigation
        std::sort(bookmarks.begin(), bookmarks.end());
        Serial.printf("Bookmark added for line: %d\n", lineToBookmark);
    }

    // Redraw the scrollbar immediately to show the updated markers
    drawScrollbar();
    // No need to save cache immediately, will be saved on cleanup or next load.
}

/**
 * @brief Jumps to the bookmark (manual or detected) immediately before the current scroll line.
 */
void TextViewerPage::goToPrevBookmark() {
    if (!fileLoaded) return;

    // 1. Combine manual and detected bookmarks
    std::vector<int> allBookmarks = bookmarks; // Start with manual bookmarks
    allBookmarks.insert(allBookmarks.end(), detectedBookmarks.begin(), detectedBookmarks.end()); // Add detected bookmarks

    if (allBookmarks.empty()) {
        Serial.println("No bookmarks (manual or detected) to navigate.");
        return; // Nothing to do
    }

    // 2. Sort and remove duplicates
    std::sort(allBookmarks.begin(), allBookmarks.end());
    allBookmarks.erase(std::unique(allBookmarks.begin(), allBookmarks.end()), allBookmarks.end());

    // 3. Find the first bookmark >= currentScrollLine in the combined list
    auto it = std::lower_bound(allBookmarks.begin(), allBookmarks.end(), currentScrollLine);

    // 4. If it's not the beginning, the previous element is the target
    if (it != allBookmarks.begin()) {
        --it; // Move to the bookmark strictly less than currentScrollLine
        int targetLine = *it;
        Serial.printf("Going to previous bookmark (combined): Line %d\n", targetLine);

        // Clamp and redraw
        currentScrollLine = std::max(0, targetLine);
        if (totalLines > linesPerPage) {
            currentScrollLine = std::min(currentScrollLine, totalLines - linesPerPage);
        } else {
            currentScrollLine = 0;
        }
        // Full redraw needed after jump
        display(); // Easiest way to trigger full redraw
    } else {
        Serial.println("No previous bookmark found.");
        // Optionally provide feedback to the user (e.g., flash screen?)
    }
}

/**
 * @brief Jumps to the bookmark (manual or detected) immediately after the current scroll line.
 */
void TextViewerPage::goToNextBookmark() {
     if (!fileLoaded) return;

    // 1. Combine manual and detected bookmarks
    std::vector<int> allBookmarks = bookmarks; // Start with manual bookmarks
    allBookmarks.insert(allBookmarks.end(), detectedBookmarks.begin(), detectedBookmarks.end()); // Add detected bookmarks

    if (allBookmarks.empty()) {
        Serial.println("No bookmarks (manual or detected) to navigate.");
        return; // Nothing to do
    }

    // 2. Sort and remove duplicates
    std::sort(allBookmarks.begin(), allBookmarks.end());
    allBookmarks.erase(std::unique(allBookmarks.begin(), allBookmarks.end()), allBookmarks.end());

    // 3. Find the first bookmark > currentScrollLine in the combined list
    auto it = std::upper_bound(allBookmarks.begin(), allBookmarks.end(), currentScrollLine);

    // 4. If it's not the end, this is the target
    if (it != allBookmarks.end()) {
        int targetLine = *it;
        Serial.printf("Going to next bookmark (combined): Line %d\n", targetLine);

        // Clamp and redraw
        currentScrollLine = std::max(0, targetLine);
         if (totalLines > linesPerPage) {
            currentScrollLine = std::min(currentScrollLine, totalLines - linesPerPage);
        } else {
            currentScrollLine = 0;
        }
        // Full redraw needed after jump
        display(); // Easiest way to trigger full redraw
    } else {
        Serial.println("No next bookmark found.");
        // Optionally provide feedback to the user
    }
}
