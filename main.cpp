/**
 * @file main.cpp
 * @brief Ncurses presentation tier for the marketplace network client.
 *
 * Defines the interactive terminal UI, local input helpers, and screen flows
 * that call the shared `ApiClient` to communicate with the server.
 */

#include "ApiClient.h"
#include <ncurses.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cctype>

using namespace std;

/**
 * @brief Global API client used by the presentation tier.
 *
 * Initializes the client from the `MARKETPLACE_HOST` and `MARKETPLACE_PORT`
 * environment variables, falling back to `127.0.0.1:9090` when unset so all
 * ncurses screens share one consistent network configuration.
 */
static ApiClient g_api(
    []()
    {
        // Decide the destination host once at startup; every later screen reuses this shared client config.
        const char *host = getenv("MARKETPLACE_HOST"); // Optional host override read from the environment.
        // Respect an environment override so the UI can target a remote server without recompiling.
        if (host && *host)
            return string(host);
        // Fall back to localhost for the normal "client and server on the same machine" workflow.
        return string("127.0.0.1");
    }(),
    []()
    {
        // Decide the destination port once at startup so all later TCP connects target the same server port.
        const char *port = getenv("MARKETPLACE_PORT"); // Optional port override read from the environment.
        if (port && *port)
        {
            int p = atoi(port); // Parsed port number read from the MARKETPLACE_PORT environment variable.
            // Only accept valid TCP port numbers before handing the value to the API client.
            if (p > 0 && p <= 65535)
                return p;
        }
        // Port 9090 is the default listening port used by server_main.cpp.
        return 9090;
    }()); // Shared API client used by all ncurses screens for network requests.
/** Category options available when creating or tagging listings. */
static const vector<string> kCategories = {
    "Textbook", "Furniture", "Electronics", "Clothing", "Sports", "Other"}; // Category options available when creating or tagging listings.
/** Search-category options, including the `All` sentinel for no category filter. */
static const vector<string> kSearchCategories = {
    "All", "Textbook", "Furniture", "Electronics", "Clothing", "Sports", "Other"}; // Search-category options, including the All sentinel for no category filter.
/** Search sort labels presented in the price-order picker. */
static const vector<string> kPriceSortOptions = {
    "None", "Low to High", "High to Low"}; // Search sort labels presented in the price-order picker.

/**
 * @brief Renders an editable text field with optional masking/highlight.
 *
 * Draws the current field contents, optionally masks secret input, and applies
 * reverse-video highlighting when the field is focused.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param row Row coordinate.
 * @param col Column coordinate.
 * @param width Render width of field area.
 * @param value Current field value.
 * @param secret Whether value is displayed as `*`.
 * @param selected Whether field is currently focused.
 */
void drawEditableField(int row, int col, int width, const string &value, bool secret, bool selected)
{
    if (selected)
        attron(A_REVERSE);
    mvprintw(row, col, "%*s", width, "");
    if (secret)
    {
        for (int i = 0; i < static_cast<int>(value.size()) && i < width; i++)
        {
            mvprintw(row, col + i, "*");
        }
    }
    else
    {
        mvprintw(row, col, "%.*s", width, value.c_str());
    }
    if (selected)
        attroff(A_REVERSE);
}

/**
 * @brief Normalizes a user-entered local photo path for file reads.
 *
 * Trims surrounding whitespace and removes one matching pair of outer quotes
 * so pasted shell-style paths can be used for local file access.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param rawPath Path text entered in the UI.
 * @return Trimmed path with one matching outer quote pair removed.
 */
static string normalizeLocalPhotoPath(string rawPath)
{
    size_t start = 0; // Index of the first non-whitespace character in the raw path.
    while (start < rawPath.size() && isspace(static_cast<unsigned char>(rawPath[start])))
        start++;

    size_t end = rawPath.size(); // One-past-the-end index trimmed backward over trailing whitespace.
    while (end > start && isspace(static_cast<unsigned char>(rawPath[end - 1])))
        end--;

    string normalized = rawPath.substr(start, end - start); // Trimmed copy of the user-entered path.
    if (normalized.size() >= 2)
    {
        char first = normalized.front(); // First character used to detect wrapped quotes.
        char last = normalized.back();   // Last character used to detect wrapped quotes.
        if ((first == '"' && last == '"') || (first == '\'' && last == '\''))
        {
            normalized = normalized.substr(1, normalized.size() - 2);
        }
    }
    return normalized;
}

/**
 * @brief Detects likely image extension by magic bytes.
 *
 * Inspects the leading bytes of decoded image data and chooses a likely file
 * extension for the cached output file.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param raw Raw decoded image bytes.
 * @return Extension including leading dot.
 */
static string sniffImageExtension(const vector<uint8_t> &raw)
{
    if (raw.size() >= 3 && raw[0] == 0xff && raw[1] == 0xd8 && raw[2] == 0xff)
        return ".jpg";
    if (raw.size() >= 8 && raw[0] == 0x89 && raw[1] == 'P' && raw[2] == 'N' && raw[3] == 'G')
        return ".png";
    if (raw.size() >= 12 && raw[0] == 'R' && raw[1] == 'I' && raw[2] == 'F' && raw[3] == 'F')
        return ".webp";
    return ".bin";
}

/**
 * @brief Executes a shell command and treats exit code 0 as success.
 *
 * Runs a viewer-launch command and collapses the result into a simple boolean
 * success value used by the image-opening flow.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param cmd Command string.
 * @return True when command succeeds.
 */
static bool tryOpenCmd(const string &cmd)
{
    return std::system(cmd.c_str()) == 0;
}

/**
 * @brief Saves raw image bytes to a cache file and opens it in OS viewer.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param rawBytes Raw image bytes.
 * @param err Output status/error text.
 * @return True when file is written and open command succeeds.
 */
static bool saveAndOpenImageRaw(const vector<uint8_t> &rawBytes, string &err)
{
    if (rawBytes.empty())
    {
        err = "Invalid image data";
        return false;
    }
    const string ext = sniffImageExtension(rawBytes); // Best-effort extension inferred from the downloaded image bytes.
    const std::filesystem::path dir = "client_cache"; // Local cache directory used for temporary image viewing files.
    std::error_code ec; // Non-throwing filesystem error sink for cache-directory creation.
    std::filesystem::create_directories(dir, ec);
    const std::filesystem::path outPath = std::filesystem::absolute(dir / ("viewed_photo" + ext)); // Absolute output file path handed to the OS image viewer.

    std::ofstream out(outPath, std::ios::binary | std::ios::trunc); // Output stream that writes the downloaded image bytes to disk.
    if (!out || !out.write(reinterpret_cast<const char *>(rawBytes.data()), static_cast<std::streamsize>(rawBytes.size())))
    {
        err = "Failed to write image file";
        return false;
    }

    const bool opened = tryOpenCmd("xdg-open \"" + outPath.string() + "\" >/dev/null 2>&1");
    if (!opened)
    {
        err = "Open failed (saved to " + outPath.string() + ")";
        return false;
    }
    err = "Opened: " + outPath.string();
    return true;
}

/**
 * @brief Extracts display username from email prefix.
 *
 * Produces a friendlier display name by trimming the domain portion from an
 * email address when possible.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param email Full email address.
 * @return Username portion before '@', or original input.
 */
string usernameFromEmail(const string &email)
{
    size_t at = email.find('@'); // Position of the domain separator in the email address.
    if (at == string::npos || at == 0)
        return email;
    return email.substr(0, at);
}

/**
 * @brief Draws standardized page title and top separator.
 *
 * Renders the common page-header treatment used across screens, including the
 * centered title text and top divider line.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param title Screen title text.
 */
void drawTitle(const string &title)
{
    int width = getmaxx(stdscr);                          // Current terminal width used for centered title placement.
    int x = (width - static_cast<int>(title.size())) / 2; // Left column where the title should start to appear centered.
    attron(A_BOLD);
    if (title.rfind("Mustang", 0) == 0)
    {
        attron(COLOR_PAIR(1));
        mvprintw(1, x, "Mustang");
        attroff(COLOR_PAIR(1));
        mvprintw(1, x + 7, "%s", title.substr(7).c_str());
    }
    else
    {
        mvprintw(1, x, "%s", title.c_str());
    }
    attroff(A_BOLD);
    mvhline(2, 0, '-', width);
}

/**
 * @brief Collects one line of terminal input from a fixed position.
 *
 * Reads printable keyboard input directly from ncurses, supports backspacing,
 * and returns the final line once Enter is pressed.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param row Row coordinate.
 * @param col Column coordinate.
 * @param maxLen Maximum input length.
 * @param secret Whether to mask characters (e.g., password).
 * @return Captured input text.
 */
string getInput(int row, int col, int maxLen, bool secret = false)
{
    string input; // Characters collected so far for this text field.
    int ch;       // Latest key code read from ncurses input.
    move(row, col);
    while ((ch = getch()) != '\n')
    {
        if ((ch == KEY_BACKSPACE || ch == 127) && !input.empty())
        {
            // Remove the last character and redraw the field contents in place.
            input.pop_back();
            mvprintw(row, col, "%*s", maxLen, "");
            move(row, col);
            if (secret)
            {
                for (int i = 0; i < static_cast<int>(input.size()); i++)
                    mvprintw(row, col + i, "*");
            }
            else
            {
                mvprintw(row, col, "%s", input.c_str());
            }
        }
        else if (ch >= 32 && static_cast<int>(input.size()) < maxLen)
        {
            // Accept printable characters up to the field's max length.
            input += static_cast<char>(ch);
            mvprintw(row, col + static_cast<int>(input.size()) - 1, "%c", secret ? '*' : ch);
        }
        // Keep the terminal cursor synced with the logical end of the field.
        move(row, col + static_cast<int>(input.size()));
        refresh();
    }
    return input;
}

/**
 * @brief Collects input with ESC-cancel behavior.
 *
 * Reads editable field input like `getInput`, but allows the user to cancel
 * the interaction immediately by pressing Escape.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param row Row coordinate.
 * @param col Column coordinate.
 * @param maxLen Maximum input length.
 * @param out Output text.
 * @param secret Whether to mask characters.
 * @return True when submitted with Enter, false when cancelled by ESC.
 */
bool getInputOrCancel(int row, int col, int maxLen, string &out, bool secret = false)
{
    out.clear();
    int ch; // Latest key code read from ncurses input.
    move(row, col);
    while ((ch = getch()) != '\n')
    {
        if (ch == 27)
            return false; // ESC cancels
        if ((ch == KEY_BACKSPACE || ch == 127 || ch == 8) && !out.empty())
        {
            // Remove the last character and redraw the field contents in place.
            out.pop_back();
            mvprintw(row, col, "%*s", maxLen, "");
            move(row, col);
            if (secret)
            {
                for (int i = 0; i < static_cast<int>(out.size()); i++)
                    mvprintw(row, col + i, "*");
            }
            else
            {
                mvprintw(row, col, "%s", out.c_str());
            }
        }
        else if (ch >= 32 && static_cast<int>(out.size()) < maxLen)
        {
            // Accept printable characters up to the field's max length.
            out += static_cast<char>(ch);
            mvprintw(row, col + static_cast<int>(out.size()) - 1, "%c", secret ? '*' : ch);
        }
        // Keep the terminal cursor synced with the logical end of the field.
        move(row, col + static_cast<int>(out.size()));
        refresh();
    }
    return true;
}

/**
 * @brief Inline left/right category picker.
 *
 * Presents the create/tag category options on one row and lets the user move
 * horizontally until a category is confirmed or cancelled.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param row Row coordinate.
 * @param col Starting column.
 * @param selectedCategory Output selected category.
 * @return True when confirmed, false when cancelled.
 */
bool selectCategoryInline(int row, int col, string &selectedCategory)
{
    int selected = 0; // Index of the currently highlighted category option.
    while (true)
    {
        mvprintw(row, col, "%*s", 90, "");
        int x = col; // Current draw column while laying out category options horizontally.
        for (int i = 0; i < static_cast<int>(kCategories.size()); i++)
        {
            if (i > 0)
            {
                mvprintw(row, x, " | ");
                x += 3;
            }
            if (i == selected)
                attron(A_REVERSE);
            mvprintw(row, x, "%s", kCategories[i].c_str());
            if (i == selected)
                attroff(A_REVERSE);
            x += static_cast<int>(kCategories[i].size());
        }
        mvprintw(row + 1, col, "Left/Right to choose, Enter confirm, Esc cancel");
        refresh();

        int key = getch(); // Latest navigation key pressed inside the category picker.
        if (key == 27)
            return false;
        if (key == KEY_LEFT)
        {
            selected = (selected - 1 + static_cast<int>(kCategories.size())) % static_cast<int>(kCategories.size());
        }
        else if (key == KEY_RIGHT)
        {
            selected = (selected + 1) % static_cast<int>(kCategories.size());
        }
        else if (key == '\n')
        {
            selectedCategory = kCategories[selected];
            return true;
        }
    }
}

/**
 * @brief Dedicated category picker for search including the `All` option.
 *
 * Displays a full-screen category selection UI for search filters and returns
 * the selected category label when confirmed.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param selectedCategory Input/output selected category label.
 * @return True when confirmed, false when cancelled.
 */
static bool selectSearchCategoryScreen(string &selectedCategory)
{
    int selected = 0; // Index of the currently highlighted search-category option.
    for (int i = 0; i < static_cast<int>(kSearchCategories.size()); i++)
    {
        if (kSearchCategories[i] == selectedCategory)
        {
            selected = i;
            break;
        }
    }
    while (true)
    {
        clear();
        drawTitle("Search Category");
        mvprintw(4, 2, "Choose category:");
        int x = 4; // Current draw column while laying out category options horizontally.
        for (int i = 0; i < static_cast<int>(kSearchCategories.size()); i++)
        {
            if (i == selected)
                attron(A_REVERSE);
            mvprintw(6, x, "%s", kSearchCategories[i].c_str());
            if (i == selected)
                attroff(A_REVERSE);
            x += static_cast<int>(kSearchCategories[i].size()) + 3;
        }
        mvprintw(8, 2, "Left/Right to choose, Enter confirm, Esc cancel");
        refresh();

        int key = getch(); // Latest navigation key pressed inside the search-category picker.
        if (key == 27)
            return false;
        if (key == KEY_LEFT)
            selected = (selected - 1 + static_cast<int>(kSearchCategories.size())) % static_cast<int>(kSearchCategories.size());
        else if (key == KEY_RIGHT)
            selected = (selected + 1) % static_cast<int>(kSearchCategories.size());
        else if (key == '\n')
        {
            selectedCategory = kSearchCategories[selected];
            return true;
        }
    }
}

/**
 * @brief Dedicated search price sort picker.
 *
 * Displays a full-screen sort selector for search results and returns the
 * chosen price-order label when confirmed.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param selectedSort Input/output selected sort label.
 * @return True when confirmed, false when cancelled.
 */
static bool selectSearchPriceSortScreen(string &selectedSort)
{
    int selected = 0; // Index of the currently highlighted price-sort option.
    for (int i = 0; i < static_cast<int>(kPriceSortOptions.size()); i++)
    {
        if (kPriceSortOptions[i] == selectedSort)
        {
            selected = i;
            break;
        }
    }
    while (true)
    {
        clear();
        drawTitle("Search Price");
        mvprintw(4, 2, "Choose price ordering:");
        for (int i = 0; i < static_cast<int>(kPriceSortOptions.size()); i++)
        {
            if (i == selected)
                attron(A_REVERSE);
            mvprintw(6 + i, 4, "%s", kPriceSortOptions[i].c_str());
            if (i == selected)
                attroff(A_REVERSE);
        }
        mvprintw(10, 2, "Up/Down to choose, Enter confirm, Esc cancel");
        refresh();

        int key = getch(); // Latest navigation key pressed inside the price-sort picker.
        if (key == 27)
            return false;
        if (key == KEY_UP)
            selected = (selected - 1 + static_cast<int>(kPriceSortOptions.size())) % static_cast<int>(kPriceSortOptions.size());
        else if (key == KEY_DOWN)
            selected = (selected + 1) % static_cast<int>(kPriceSortOptions.size());
        else if (key == '\n')
        {
            selectedSort = kPriceSortOptions[selected];
            return true;
        }
    }
}

/**
 * @brief Collects search keyword plus filter selections in a navigable form.
 *
 * Drives the multi-row search form that captures keyword text, category, and
 * price ordering before the actual search request is made.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param keyword Input/output keyword string.
 * @param category Input/output selected category label.
 * @param priceSort Input/output selected price sort label.
 * @return True when Enter action is selected, false when cancelled.
 */
static bool collectSearchForm(string &keyword, string &category, string &priceSort)
{
    int activeRow = 0;                 // Tracks vertical form focus: 0=keyword, 1=filters, 2=submit.
    int activeFilter = 0;              // Tracks horizontal filter focus inside the Filters row: 0=category, 1=price.
    constexpr int kCategoryWidth = 11; // longest current label: Electronics
    constexpr int kPriceWidth = 11;    // longest current label: Low to High/High to Low
    while (true)
    {
        clear();
        drawTitle("Search Listings");
        mvprintw(4, 2, "Keyword: ");
        drawEditableField(4, 11, 60, keyword, false, activeRow == 0);

        mvprintw(6, 2, "Filters:");
        mvprintw(6, 11, "Category:");
        // Render category as the left filter column on the shared Filters row.
        if (activeRow == 1 && activeFilter == 0)
            attron(A_REVERSE);
        mvprintw(6, 21, "%-*s", kCategoryWidth, category.c_str());
        if (activeRow == 1 && activeFilter == 0)
            attroff(A_REVERSE);

        mvprintw(6, 21 + kCategoryWidth + 1, "| Price:");
        // Render price sort as the right filter column on the shared Filters row.
        if (activeRow == 1 && activeFilter == 1)
            attron(A_REVERSE);
        mvprintw(6, 21 + kCategoryWidth + 11, "%-*s", kPriceWidth, priceSort.c_str());
        if (activeRow == 1 && activeFilter == 1)
            attroff(A_REVERSE);

        if (activeRow == 2)
            attron(A_REVERSE);
        mvprintw(8, 2, "Enter");
        if (activeRow == 2)
            attroff(A_REVERSE);
        mvprintw(10, 2, "Up/Down navigates rows. Left/Right switches Filters. Enter opens/selects. Esc cancels.");
        if (activeRow == 0)
            move(4, 11 + static_cast<int>(keyword.size()));
        else if (activeRow == 1 && activeFilter == 0)
            move(6, 21);
        else if (activeRow == 1 && activeFilter == 1)
            move(6, 21 + kCategoryWidth + 11);
        else
            move(8, 2);
        refresh();

        int ch = getch(); // Latest navigation/edit key pressed on the search form.
        if (ch == 27)
            return false;
        if (ch == '\t' || ch == KEY_DOWN)
        {
            // Move vertically through keyword -> filters -> enter rows.
            activeRow = (activeRow + 1) % 3;
            continue;
        }
        if (ch == KEY_UP)
        {
            // Move vertically upward through the same three rows.
            activeRow = (activeRow + 2) % 3;
            continue;
        }
        if (activeRow == 1 && ch == KEY_LEFT)
        {
            // Switch horizontal focus between Category and Price within the Filters row.
            activeFilter = (activeFilter + 1) % 2;
            continue;
        }
        if (activeRow == 1 && ch == KEY_RIGHT)
        {
            // Switch horizontal focus between Category and Price within the Filters row.
            activeFilter = (activeFilter + 1) % 2;
            continue;
        }
        if (activeRow == 0)
        {
            if (ch == '\n')
            {
                // Enter on the keyword row advances focus to the Filters row.
                activeRow = 1;
                continue;
            }
            // Edit the keyword inline while the keyword row is focused.
            if ((ch == KEY_BACKSPACE || ch == 127 || ch == 8) && !keyword.empty())
                keyword.pop_back();
            else if (ch >= 32 && static_cast<int>(keyword.size()) < 60)
                keyword.push_back(static_cast<char>(ch));
            continue;
        }
        if (ch != '\n')
            continue;
        if (activeRow == 1 && activeFilter == 0)
        {
            // Open the dedicated category picker and only commit on confirm.
            string next = category; // Tentative category selection that only commits if the picker confirms.
            if (selectSearchCategoryScreen(next))
                category = next;
        }
        else if (activeRow == 1 && activeFilter == 1)
        {
            // Open the dedicated price-sort picker and only commit on confirm.
            string next = priceSort; // Tentative price-sort selection that only commits if the picker confirms.
            if (selectSearchPriceSortScreen(next))
                priceSort = next;
        }
        else if (activeRow == 2)
        {
            // Enter on the submit row runs the search with the current form state.
            return true;
        }
    }
}

/**
 * @brief Collects login/register credentials in a two-field form.
 *
 * Displays the shared authentication form and captures the email/password pair
 * used by either the login or registration flow.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param title Form title.
 * @param email Output email.
 * @param password Output password.
 * @return True when submitted, false when cancelled.
 */
bool collectAuthForm(const string &title, string &email, string &password)
{
    int activeField = 0; // Tracks which auth input row is focused: 0=email, 1=password.
    while (true)
    {
        clear();
        drawTitle(title);
        mvprintw(4, 2, "Email: ");
        mvprintw(5, 2, "Password: ");
        mvprintw(7, 2, "Tab/Up/Down to switch fields. Enter on Password to submit. Esc to cancel.");
        drawEditableField(4, 9, 60, email, false, activeField == 0);
        drawEditableField(5, 12, 60, password, true, activeField == 1);
        int cursorCol = (activeField == 0 ? 9 : 12) + // Column where the text cursor should appear for the focused field.
                        static_cast<int>((activeField == 0 ? email : password).size());
        move(activeField == 0 ? 4 : 5, cursorCol);
        refresh();

        int ch = getch(); // Latest navigation/edit key pressed on the auth form.
        if (ch == 27)
            return false; // ESC
        if (ch == '\t' || ch == KEY_DOWN)
        {
            activeField = (activeField + 1) % 2;
            continue;
        }
        if (ch == KEY_UP)
        {
            activeField = (activeField + 1) % 2;
            continue;
        }
        if (ch == '\n')
        {
            if (activeField == 0)
            {
                // Enter on email advances into the password field instead of submitting.
                activeField = 1;
                continue;
            }
            return true;
        }

        // Route printable/backspace input into whichever auth field currently has focus.
        string &target = (activeField == 0) ? email : password;
        if ((ch == KEY_BACKSPACE || ch == 127 || ch == 8) && !target.empty())
        {
            target.pop_back();
        }
        else if (ch >= 32 && static_cast<int>(target.size()) < 60)
        {
            target += static_cast<char>(ch);
        }
    }
}

// photoAction: 0 keep, 1 edit, 2 delete
/**
 * @brief Collects listing edit fields and photo action choice.
 *
 * Displays the editable listing form for sellers and returns updated text
 * fields together with the selected photo action.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param title Output title text (may be blank to keep old value).
 * @param description Output description text.
 * @param priceStr Output price text.
 * @param photoAction Output photo action enum (0 keep, 1 edit, 2 delete).
 * @return True when submitted, false when cancelled.
 */
bool collectListingEditForm(string &title, string &description, string &priceStr, int &photoAction)
{
    int activeField = 0; // Tracks which edit-form row is focused: 0=title, 1=description, 2=price, 3=photo action.
    while (true)
    {
        clear();
        drawTitle("Edit Listing");
        mvprintw(4, 2, "New Title:       ");
        mvprintw(5, 2, "New Description: ");
        mvprintw(6, 2, "New Price:       ");
        // Pad the whole line so selector can't overwrite the label
        mvprintw(7, 2, "%-30s", "Edit / Delete Photo:");
        mvprintw(9, 2, "Leave blanks to keep unchanged. Esc cancels.");

        drawEditableField(4, 19, 60, title, false, activeField == 0);
        drawEditableField(5, 19, 60, description, false, activeField == 1);
        drawEditableField(6, 19, 10, priceStr, false, activeField == 2);
        // Photo action selector (arrow navigable)
        {
            const char *opts[3] = {"Keep", "Edit", "Delete"}; // Labels shown in the photo-action selector.
            int x = 33;                                       // Current draw column for the photo-action selector options.
            for (int i = 0; i < 3; i++)
            {
                if (activeField == 3 && i == photoAction)
                    attron(A_REVERSE);
                mvprintw(7, x, "%s", opts[i]);
                if (activeField == 3 && i == photoAction)
                    attroff(A_REVERSE);
                x += static_cast<int>(strlen(opts[i])) + 3;
            }
        }

        if (activeField == 0)
            move(4, 19 + static_cast<int>(title.size()));
        else if (activeField == 1)
            move(5, 19 + static_cast<int>(description.size()));
        else if (activeField == 2)
            move(6, 19 + static_cast<int>(priceStr.size()));
        else if (activeField == 3)
            move(7, 33); // selector
        refresh();

        int ch = getch(); // Latest navigation/edit key pressed on the edit-listing form.
        if (ch == 27)
            return false; // ESC
        if (ch == '\t' || ch == KEY_DOWN)
        {
            activeField = (activeField + 1) % 4;
            continue;
        }
        if (ch == KEY_UP)
        {
            activeField = (activeField + 3) % 4;
            continue;
        }
        if (activeField == 3)
        {
            // Photo action is controlled only through left/right selection.
            if (ch == KEY_LEFT)
                photoAction = (photoAction + 2) % 3;
            else if (ch == KEY_RIGHT)
                photoAction = (photoAction + 1) % 3;
            else if (ch == '\n')
            {
                // Enter on the selector submits the entire edit form.
                return true;
            }
            continue;
        }
        if (ch == '\n')
        {
            if (activeField < 3)
            {
                // Enter on a text field advances to the next row instead of submitting immediately.
                activeField++;
                continue;
            }
            return true;
        }

        // Route printable/backspace input into the currently focused text field.
        string *target = nullptr; // Pointer to the text field currently receiving typed characters.
        int maxLen = 60;          // Maximum length allowed for the currently focused text field.
        if (activeField == 0)
        {
            target = &title;
            maxLen = 60;
        }
        else if (activeField == 1)
        {
            target = &description;
            maxLen = 60;
        }
        else if (activeField == 2)
        {
            target = &priceStr;
            maxLen = 10;
        }
        else
        {
            continue;
        }

        if ((ch == KEY_BACKSPACE || ch == 127 || ch == 8) && !target->empty())
        {
            target->pop_back();
        }
        else if (ch >= 32 && static_cast<int>(target->size()) < maxLen)
        {
            target->push_back(static_cast<char>(ch));
        }
    }
}

/**
 * @brief Prompts for a replacement photo path on a dedicated screen.
 *
 * Opens a focused prompt used when a seller chooses to replace a listing photo
 * during the edit flow.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param outPath Output local path.
 * @return True on submit, false on cancel.
 */
static bool promptEditPhotoPath(string &outPath)
{
    clear();
    drawTitle("Edit Photo");
    mvprintw(4, 2, "Local photo path: ");
    mvprintw(6, 2, "Enter submits. Esc cancels and keeps photo unchanged.");
    refresh();
    return getInputOrCancel(4, 20, 120, outPath, false);
}

/**
 * @brief Collects create-listing form fields including category and optional photo path.
 *
 * Displays the create-listing form and captures the core listing fields plus
 * the optional local photo path selected by the user.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param title Output title.
 * @param description Output description.
 * @param priceStr Output price text.
 * @param category Output selected category.
 * @param photoPath Output optional local photo path.
 * @return True when submitted, false when cancelled.
 */
bool collectCreateListingForm(string &title, string &description, string &priceStr, string &category, string &photoPath)
{
    int activeField = 0;      // Tracks which create-form row is focused: 0=title, 1=description, 2=price, 3=category, 4=photo.
    int selectedCategory = 0; // Index of the currently highlighted category option on the create form.
    if (!category.empty())
    {
        for (int i = 0; i < static_cast<int>(kCategories.size()); i++)
        {
            if (kCategories[i] == category)
            {
                selectedCategory = i;
                break;
            }
        }
    }

    while (true)
    {
        clear();
        drawTitle("Create Listing");
        mvprintw(4, 2, "Title:       ");
        mvprintw(5, 2, "Description: ");
        mvprintw(6, 2, "Price:       ");
        mvprintw(7, 2, "Category:    ");
        mvprintw(8, 2, "Local photo: ");
        mvprintw(10, 2, "Enter on photo submits; Esc cancels.");

        drawEditableField(4, 15, 60, title, false, activeField == 0);
        drawEditableField(5, 15, 60, description, false, activeField == 1);
        drawEditableField(6, 15, 10, priceStr, false, activeField == 2);

        int x = 15; // Current draw column while laying out category options horizontally.
        for (int i = 0; i < static_cast<int>(kCategories.size()); i++)
        {
            if (i > 0)
            {
                mvprintw(7, x, " | ");
                x += 3;
            }
            if (activeField == 3 && i == selectedCategory)
                attron(A_REVERSE);
            mvprintw(7, x, "%s", kCategories[i].c_str());
            if (activeField == 3 && i == selectedCategory)
                attroff(A_REVERSE);
            x += static_cast<int>(kCategories[i].size());
        }

        drawEditableField(8, 15, 100, photoPath, false, activeField == 4);

        if (activeField <= 2)
        {
            int row = (activeField == 0 ? 4 : (activeField == 1 ? 5 : 6));                                                    // Row where the cursor should appear for the focused text field.
            int col = 15 + static_cast<int>((activeField == 0 ? title : (activeField == 1 ? description : priceStr)).size()); // Column where the cursor should appear for the focused text field.
            move(row, col);
        }
        else if (activeField == 3)
        {
            move(7, 15);
        }
        else
        {
            move(8, 15 + static_cast<int>(photoPath.size()));
        }
        refresh();

        int ch = getch(); // Latest navigation/edit key pressed on the create-listing form.
        if (ch == 27)
            return false; // ESC

        if (activeField == 3)
        {
            if (ch == KEY_LEFT)
            {
                selectedCategory = (selectedCategory - 1 + static_cast<int>(kCategories.size())) % static_cast<int>(kCategories.size());
            }
            else if (ch == KEY_RIGHT)
            {
                selectedCategory = (selectedCategory + 1) % static_cast<int>(kCategories.size());
            }
            else if (ch == KEY_UP)
            {
                // Move back to Price while preserving the current category selection.
                activeField = 2; // keep current selectedCategory
            }
            else if (ch == KEY_DOWN)
            {
                // Move forward to the optional photo path field.
                activeField = 4;
            }
            else if (ch == '\n')
            {
                // Commit the current category choice and advance to the photo field.
                category = kCategories[selectedCategory];
                activeField = 4;
            }
            continue;
        }

        if (activeField == 4)
        {
            if (ch == KEY_UP)
            {
                activeField = 3;
                continue;
            }
            // Enter on the photo row is the create-form submit action.
            if (ch == '\n')
            {
                category = kCategories[selectedCategory];
                return true;
            }
            if ((ch == KEY_BACKSPACE || ch == 127 || ch == 8) && !photoPath.empty())
                photoPath.pop_back();
            else if (ch >= 32 && static_cast<int>(photoPath.size()) < 100)
                photoPath.push_back(static_cast<char>(ch));
            continue;
        }

        if (ch == '\t' || ch == KEY_DOWN)
        {
            activeField = (activeField < 3) ? activeField + 1 : 4;
            continue;
        }
        if (ch == KEY_UP)
        {
            activeField = (activeField > 0) ? activeField - 1 : 0;
            continue;
        }
        if (ch == '\n')
        {
            activeField = (activeField < 3) ? activeField + 1 : 4;
            continue;
        }

        string *target = nullptr; // Pointer to the create-form text field currently receiving input.
        int maxLen = 60;          // Maximum length allowed for the currently focused create-form text field.
        if (activeField == 0)
        {
            target = &title;
            maxLen = 60;
        }
        else if (activeField == 1)
        {
            target = &description;
            maxLen = 60;
        }
        else
        {
            target = &priceStr;
            maxLen = 10;
        }

        if ((ch == KEY_BACKSPACE || ch == 127 || ch == 8) && !target->empty())
        {
            target->pop_back();
        }
        else if (ch >= 32 && static_cast<int>(target->size()) < maxLen)
        {
            target->push_back(static_cast<char>(ch));
        }
    }
}

/**
 * @brief Collects rating fields using arrow-key row navigation.
 *
 * Displays the seller-rating form and captures both the numeric score and the
 * optional free-form comment before submission.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param ratingStr Output 1-5 rating text.
 * @param comment Output review text.
 * @return True when submitted, false when cancelled.
 */
bool collectRatingForm(string &ratingStr, string &comment)
{
    int activeField = 0; // Tracks which rating-form row is focused: 0=rating, 1=comment.
    while (true)
    {
        clear();
        drawTitle("Rate Seller");
        mvprintw(4, 2, "Rating (1-5): ");
        mvprintw(5, 2, "Comment:      ");
        mvprintw(7, 2, "Up/Down moves between rows. Enter on comment submits. Esc cancels.");

        drawEditableField(4, 16, 1, ratingStr, false, activeField == 0);
        drawEditableField(5, 16, 120, comment, false, activeField == 1);

        if (activeField == 0)
            move(4, 16 + static_cast<int>(ratingStr.size()));
        else
            move(5, 16 + static_cast<int>(comment.size()));
        refresh();

        int ch = getch(); // Latest navigation/edit key pressed on the rating form.
        if (ch == 27)
            return false;
        if (ch == '\t' || ch == KEY_DOWN)
        {
            activeField = (activeField + 1) % 2;
            continue;
        }
        if (ch == KEY_UP)
        {
            activeField = (activeField + 1) % 2;
            continue;
        }
        if (ch == '\n')
        {
            if (activeField == 0)
            {
                // Enter on the numeric rating field advances into the comment field.
                activeField = 1;
                continue;
            }
            return true;
        }

        // Route printable/backspace input into the currently focused rating field.
        string &target = (activeField == 0) ? ratingStr : comment;
        int maxLen = (activeField == 0) ? 1 : 120; // Maximum length allowed for the currently focused rating-form field.
        if ((ch == KEY_BACKSPACE || ch == 127 || ch == 8) && !target.empty())
        {
            target.pop_back();
        }
        else if (ch >= 32 && static_cast<int>(target.size()) < maxLen)
        {
            target += static_cast<char>(ch);
        }
    }
}

/**
 * @brief Displays a message banner and waits for user acknowledgement.
 *
 * Shows a simple status message and pauses the current flow until the user
 * presses a key to continue.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param row Starting row for message.
 * @param msg Message text.
 */
void showMessage(int row, const string &msg)
{
    mvprintw(row, 2, "%s", msg.c_str());
    mvprintw(row + 2, 2, "Press any key to continue...");
    refresh();
    getch();
}

/**
 * @brief Shows a list of listings and returns selected item.
 *
 * Renders a numbered list of listings, waits for a numeric selection, and
 * returns either the chosen item or a sentinel item on cancel.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param title Screen title.
 * @param listings Listing collection to render.
 * @return Selected listing, or default item with id -1 when cancelled.
 */
ApiListing showListingList(const string &title, vector<ApiListing> &listings)
{
    ApiListing none; // Sentinel "no selection" listing returned when the user backs out.
    if (listings.empty())
    {
        clear();
        showMessage(4, "No listings found.");
        return none;
    }
    while (true)
    {
        clear();
        drawTitle(title);
        int row = 4; // Current row used to draw the listing list.
        for (int i = 0; i < static_cast<int>(listings.size()); i++)
        {
            mvprintw(row++, 4, "%d. %s ($%.2f) [%s]", i + 1, listings[i].title.c_str(), listings[i].price, listings[i].status.c_str());
        }
        mvprintw(row + 1, 2, "Select (0 to go back): ");
        refresh();
        string selStr = getInput(row + 1, 25, 5); // Raw numeric selection typed by the user.
        int sel = 0;                              // Parsed numeric selection index.
        try
        {
            sel = stoi(selStr);
        }
        catch (...)
        {
            sel = 0;
        }
        if (sel == 0)
            return none;
        if (sel >= 1 && sel <= static_cast<int>(listings.size()))
            return listings[sel - 1];
    }
}

/**
 * @brief Displays listing detail actions for search/watchlist contexts.
 *
 * Shows the buyer-facing listing detail screen and drives follow-up actions
 * such as watchlist changes, offer submission, and photo viewing.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param l Listing to display.
 * @param fromWatchlist True when opened from watchlist screen.
 */
void showListingDetail(const ApiListing &l, bool fromWatchlist = false)
{
    while (true)
    {
        string ratingDisplay; // Seller rating string shown on the buyer-facing listing detail screen.
        string ratingMsg;     // Auxiliary API message returned when fetching the seller rating.
        // Refresh the seller's public rating summary from the server before drawing the detail screen.
        if (!g_api.getSellerRating(l.sellerId, ratingDisplay, ratingMsg) || ratingDisplay.empty())
        {
            ratingDisplay = "Unavailable";
        }
        clear();
        drawTitle(l.title);
        mvprintw(4, 2, "Price:       $%.2f", l.price);
        mvprintw(5, 2, "Category:    %s", l.category.c_str());
        mvprintw(6, 2, "Seller:      %s", usernameFromEmail(l.sellerName).c_str());
        mvprintw(7, 2, "Rating:      %s", ratingDisplay.c_str());
        mvprintw(8, 2, "Status:      %s", l.status.c_str());
        mvprintw(9, 2, "Description: %s", l.description.c_str());
        mvprintw(10, 2, "Listing ID:  %d", l.id);
        mvhline(10, 0, '-', getmaxx(stdscr));
        mvhline(11, 0, '-', getmaxx(stdscr));
        mvprintw(12, 4, "1. Add to Watchlist");
        if (fromWatchlist)
            mvprintw(13, 4, "2. Remove from Watchlist");
        if (l.status == "active")
        {
            int offerRow = fromWatchlist ? 14 : 13;
            mvprintw(offerRow, 4, fromWatchlist ? "3. Make Offer" : "2. Make Offer");
        }
        int photoRow = fromWatchlist ? 15 : 14;
        mvprintw(photoRow, 4, fromWatchlist ? "4. View Photo" : "3. View Photo");
        mvprintw(photoRow + 1, 4, "0. Back");
        mvprintw(photoRow + 3, 2, "Choice: ");
        refresh();

        int ch = getch();
        if (ch == '1')
        {
            string msg; // User-facing result of the add-to-watchlist request.
            g_api.addToWatchlist(l.id, msg);
            clear();
            showMessage(4, msg);
        }
        else if (ch == '2' && fromWatchlist)
        {
            string msg; // User-facing result of the remove-from-watchlist request.
            g_api.removeFromWatchlist(l.id, msg);
            clear();
            showMessage(4, msg);
            if (msg == "Removed from Watchlist")
                break;
        }
        else if ((ch == '2' && !fromWatchlist && l.status == "active") ||
                 (ch == '3' && fromWatchlist && l.status == "active"))
        {
            clear();
            drawTitle("Make Offer");
            mvprintw(4, 2, "Listing: %s", l.title.c_str());
            mvprintw(5, 2, "Current Price: $%.2f", l.price);
            mvprintw(7, 2, "Offer Price: ");
            refresh();

            string offerStr = getInput(7, 15, 12); // Offer amount typed by the buyer.
            double offerPrice = 0.0; // Parsed numeric offer value submitted to the API.
            try
            {
                offerPrice = stod(offerStr);
            }
            catch (...)
            {
                offerPrice = 0.0;
            }

            string msg; // User-facing result of the make-offer request.
            g_api.makeOffer(l.id, offerPrice, msg);
            clear();
            showMessage(4, msg);
        }
        else if ((ch == '3' && !fromWatchlist) || (ch == '4' && fromWatchlist))
        {
            // View Photo: now retrieves raw bytes
            string msg;
            vector<uint8_t> rawBytes; // Raw image bytes fetched from server
            if (g_api.viewPhotoRaw(l.id, rawBytes, msg))
            {
                clear();
                string openMsg;
                if (saveAndOpenImageRaw(rawBytes, openMsg))
                    msg = openMsg;
                else
                    msg = openMsg;
            }
            clear();
            showMessage(4, msg);
        }
        else if (ch == '0')
        {
            break;
        }
    }
}

/**
 * @brief Collects create-listing inputs and submits create request.
 *
 * Runs the create-listing form, validates any local photo path, and submits
 * the completed request through the API client.
 * @author Muhammad Naheen Mahboob (mmahbo)
 */
void showCreateListingScreen()
{
    string title, description, priceStr, category, photoPath; // Create-form fields collected from the user.
    if (!collectCreateListingForm(title, description, priceStr, category, photoPath))
        return;
    double price = -1.0; // Parsed numeric price submitted to the API.
    try
    {
        price = stod(priceStr);
    }
    catch (...)
    {
    }

    string msg;  // User-facing result of the create request.
    // Normalize the local photo path once before it is handed to ApiClient for
    // multipart upload; an empty string still means "no photo".
    const string normalizedPhotoPath = normalizeLocalPhotoPath(photoPath); // Cleaned local upload path passed to the API client.
    // Submit the create-listing request once all local preprocessing is complete.
    // This is the point where the UI hands the finished form data to the network client.
    g_api.createListing(title, description, price, category, msg, normalizedPhotoPath);
    clear();
    showMessage(4, msg);
}

/**
 * @brief Finds one of current user's listings by id.
 *
 * Reloads the current seller's listings from the server and searches that set
 * for the requested listing id.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param listingId Target listing id.
 * @param out Output listing on success.
 * @return True when listing is found.
 */
bool getMyListingById(int listingId, ApiListing &out)
{
    vector<ApiListing> listings; // Current seller's listings loaded from the API for lookup by id.
    string msg;                  // Auxiliary API message returned while loading the seller's listings.
    // Reload the seller's current listing snapshot from the server before searching locally by id.
    if (!g_api.getMyListings(listings, msg))
        return false;
    for (const ApiListing &l : listings)
    {
        if (l.id == listingId)
        {
            out = l;
            return true;
        }
    }
    return false;
}

/**
 * @brief Shows owner-focused listing detail/actions screen.
 *
 * Displays the seller-facing detail view for one listing and drives edit,
 * tag, offer-management, delete, and photo actions.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param listingId Listing id to display.
 */
void showMyListingDetail(int listingId)
{
    while (true)
    {
        ApiListing l; // Current listing snapshot loaded for the owner-detail screen.
        if (!getMyListingById(listingId, l))
        {
            clear();
            showMessage(4, "Listing is no longer available.");
            break;
        }

        string ratingDisplay; // Seller rating string shown on the owner-detail screen.
        string ratingMsg;     // Auxiliary API message returned when fetching the seller rating.
        // Pull the current seller rating from the server so the owner sees live profile info.
        if (!g_api.getSellerRating(l.sellerId, ratingDisplay, ratingMsg) || ratingDisplay.empty())
        {
            ratingDisplay = "Unavailable";
        }
        clear();
        drawTitle(l.title);
        mvprintw(4, 2, "Price:       $%.2f", l.price);
        mvprintw(5, 2, "Category:    %s", l.category.c_str());
        mvprintw(6, 2, "Seller:      %s", usernameFromEmail(l.sellerName).c_str());
        mvprintw(7, 2, "Rating:      %s", ratingDisplay.c_str());
        mvprintw(8, 2, "Status:      %s", l.status.c_str());
        mvprintw(9, 2, "Description: %s", l.description.c_str());
        mvprintw(10, 2, "Listing ID:  %d", l.id);
        mvhline(10, 0, '-', getmaxx(stdscr));
        mvhline(11, 0, '-', getmaxx(stdscr));
        if (l.status == "active")
        {
            mvprintw(13, 4, "1. Edit Listing");
            mvprintw(14, 4, "2. Tag Listing");
            mvprintw(15, 4, "3. View Incoming Offers");
            mvprintw(16, 4, "4. Delete Listing");
            mvprintw(17, 4, "5. View Photo");
        }
        else
        {
            mvprintw(13, 4, "1. Delete Listing");
            mvprintw(14, 4, "2. View Photo");
        }
        mvprintw(19, 4, "0. Back");
        mvprintw(21, 2, "Choice: ");
        refresh();

        int ch = getch(); // Latest menu choice pressed on the owner-detail screen.
        if (ch == '1' && l.status == "active")
        {
            string title, description, priceStr; // Edit-form field overrides typed by the seller.
            int photoAction = 0;                 // Selected photo action: 0=keep, 1=edit, 2=delete.
            if (!collectListingEditForm(title, description, priceStr, photoAction))
                continue;

            // Blank fields mean "keep current value"
            // Keep existing listing values when input fields are left blank.
            if (title.empty())
                title = l.title;
            if (description.empty())
                description = l.description;
            double price = l.price; // Final numeric price submitted after merging blank-field defaults.
            if (!priceStr.empty())
            {
                try
                {
                    price = stod(priceStr);
                }
                catch (...)
                {
                    price = -1.0;
                }
            }
            string msg;                      // User-facing result of the edit request.
            const string *filePtr = nullptr; // Optional pointer used when sending path/remove photo instructions.
            string filePathValue;            // Stable storage for either a delete marker or a local upload path.
            if (photoAction == 2)
            { // delete
                filePathValue = "";
                filePtr = &filePathValue; // send file_path="" to trigger delete
            }
            else if (photoAction == 1)
            {                     // edit
                string photoPath; // Local replacement photo path typed by the seller.
                if (promptEditPhotoPath(photoPath) && !photoPath.empty())
                {
                    // Persist the normalized path in stable storage so the pointer
                    // remains valid for the later ApiClient call below.
                    filePathValue = normalizeLocalPhotoPath(photoPath);
                    filePtr = &filePathValue;
                }
            }
            // Send the merged edit payload to the server after local field and photo selection.
            g_api.editListing(l.id, title, description, price, msg, filePtr);
            clear();
            showMessage(4, msg);
            if (msg == "Listing updated")
                break;
        }
        else if (ch == '2' && l.status == "active")
        {
            clear();
            drawTitle("Tag Listing");
            mvprintw(4, 2, "Category:   ");
            mvprintw(6, 2, "Esc cancels and returns.");
            refresh();
            string category; // Category chosen in the tag-listing picker.
            if (!selectCategoryInline(4, 14, category))
                continue;
            string msg; // User-facing result of the tag-listing request.
            // Submit the selected category to the server so it can update the listing row.
            g_api.tagListing(l.id, category, msg);
            clear();
            showMessage(4, msg);
        }
        else if ((ch == '4' && l.status == "active") || (ch == '1' && l.status != "active"))
        {
            string msg; // User-facing result of the delete-listing request.
            // Request a server-side delete/soft-delete, since only the server can mutate listing state.
            g_api.removeListing(l.id, msg);
            clear();
            showMessage(4, msg);
            if (msg == "Listing removed")
                break;
        }
        else if (ch == '3' && l.status == "active")
        {
            while (true)
            {
                vector<ApiOffer> offers; // All incoming offers loaded for the current seller.
                string msg;              // Auxiliary API message returned while loading incoming offers.
                // Refresh incoming offers from the server each time the seller opens this sub-screen.
                g_api.getIncomingOffers(offers, msg);
                vector<ApiOffer> listingOffers; // Incoming offers filtered down to the currently viewed listing.
                for (const ApiOffer &o : offers)
                {
                    if (o.listingId == l.id)
                        listingOffers.push_back(o);
                }

                if (listingOffers.empty())
                {
                    clear();
                    showMessage(4, "No incoming offers.");
                    break;
                }

                int selected = 0;  // Index of the currently highlighted incoming offer.
                bool back = false; // True once the seller chooses to leave the incoming-offers screen.
                while (!back)
                {
                    clear();
                    drawTitle("Incoming Offers");
                    int row = 4; // Current row used to draw the incoming-offers list.
                    for (int i = 0; i < static_cast<int>(listingOffers.size()); i++)
                    {
                        if (i == selected)
                            attron(A_REVERSE);
                        mvprintw(row++, 2, "Offer: $%.2f for %s from %s",
                                 listingOffers[i].offerPrice,
                                 listingOffers[i].listingTitle.c_str(),
                                 usernameFromEmail(listingOffers[i].buyerName).c_str());
                        if (i == selected)
                            attroff(A_REVERSE);
                    }

                    mvhline(row + 1, 0, '-', getmaxx(stdscr));
                    mvprintw(row + 3, 2, "Hovered offer:");
                    mvprintw(row + 4, 4, "Offer: $%.2f for %s from %s",
                             listingOffers[selected].offerPrice,
                             listingOffers[selected].listingTitle.c_str(),
                             usernameFromEmail(listingOffers[selected].buyerName).c_str());
                    mvprintw(row + 6, 2, "Up/Down navigate, Enter select, 0 back");
                    refresh();

                    int key = getch(); // Latest navigation/menu key pressed on the incoming-offers screen.
                    if (key == '0')
                    {
                        back = true;
                        break;
                    }
                    if (key == KEY_UP)
                    {
                        selected = (selected - 1 + static_cast<int>(listingOffers.size())) % static_cast<int>(listingOffers.size());
                    }
                    else if (key == KEY_DOWN)
                    {
                        selected = (selected + 1) % static_cast<int>(listingOffers.size());
                    }
                    else if (key == '\n')
                    {
                        ApiOffer chosen = listingOffers[selected]; // Offer currently selected for accept/reject action.
                        clear();
                        drawTitle("Offer Action");
                        mvprintw(4, 2, "Offer: $%.2f for %s from %s",
                                 chosen.offerPrice,
                                 chosen.listingTitle.c_str(),
                                 usernameFromEmail(chosen.buyerName).c_str());
                        mvprintw(6, 4, "1. Accept");
                        mvprintw(7, 4, "2. Reject");
                        mvprintw(8, 4, "0. Cancel");
                        mvprintw(10, 2, "Choice: ");
                        refresh();

                        int action = getch(); // Latest action-menu key pressed for the selected offer.
                        if (action == '0')
                            continue;

                        string resp; // User-facing result of the accept/reject request.
                        // Send exactly one server-side state transition for the selected offer.
                        if (action == '1')
                            g_api.acceptOffer(chosen.offerId, resp);
                        else if (action == '2')
                            g_api.rejectOffer(chosen.offerId, resp);
                        else
                            resp = "Invalid choice";

                        clear();
                        showMessage(4, resp);
                        break;
                    }
                }

                if (back)
                    break;
            }
        }
        else if ((ch == '5' && l.status == "active") || (ch == '2' && l.status != "active"))
        {
            string msg;
            vector<uint8_t> rawBytes;
            if (g_api.viewPhotoRaw(l.id, rawBytes, msg))
            {
                clear();
                string openMsg;
                if (saveAndOpenImageRaw(rawBytes, openMsg))
                    msg = openMsg;
                else
                    msg = openMsg;
            }
            clear();
            showMessage(4, msg);
        }
        else if (ch == '0')
        {
            break;
        }
    }
}

/**
 * @brief Displays and handles "My Listings" selection flow.
 *
 * Repeatedly loads the seller's listings, displays the selection screen, and
 * opens the chosen listing in the seller-focused detail flow.
 * @author Muhammad Naheen Mahboob (mmahbo)
 */
void showMyListingsScreen()
{
    while (true)
    {
        vector<ApiListing> listings; // Seller listings loaded for the My Listings screen.
        string msg;                  // Auxiliary API message returned while loading My Listings.
        // Reload the seller's listing collection from the server before drawing the picker.
        g_api.getMyListings(listings, msg);
        ApiListing selected = showListingList("My Listings", listings); // Listing chosen from the My Listings screen.
        if (selected.id == -1)
            break;
        showMyListingDetail(selected.id);
    }
}

/**
 * @brief Displays seller menu (view or create listings).
 *
 * Presents the top-level seller menu that routes into either the owned
 * listings screen or the create-listing flow.
 * @author Muhammad Naheen Mahboob (mmahbo)
 */
void showSellingMenu()
{
    while (true)
    {
        clear();
        drawTitle("My Listings");
        mvprintw(4, 4, "1. View My Listings");
        mvprintw(5, 4, "2. Create Listing");
        mvprintw(6, 4, "0. Back");
        mvprintw(8, 2, "Choice: ");
        refresh();
        int ch = getch(); // Latest menu choice pressed on the seller menu.
        if (ch == '1')
            showMyListingsScreen();
        else if (ch == '2')
            showCreateListingScreen();
        else if (ch == '0')
            break;
    }
}

/**
 * @brief Runs keyword search flow and opens selected listing detail.
 *
 * Collects search filters, requests matching listings from the server, and
 * routes the selected result into the appropriate detail screen.
 * @author Muhammad Naheen Mahboob (mmahbo)
 */
void showSearchScreen()
{
    string keyword;                 // Search keyword typed by the user.
    string category = "All";        // Category filter label currently selected in the search form.
    string priceSortLabel = "None"; // Price-sort label currently selected in the search form.
    // Gather all local filter inputs before issuing the actual network request.
    if (!collectSearchForm(keyword, category, priceSortLabel))
        return;
    vector<ApiListing> listings; // Search results returned by the API.
    string msg;                  // Auxiliary API message returned by the search endpoint.
    string priceSort = "none";   // Stable API sort token derived from the UI label.
    if (priceSortLabel == "Low to High")
        priceSort = "asc";
    else if (priceSortLabel == "High to Low")
        priceSort = "desc";
    // Convert the UI label into the API's stable sort token before searching.
    // One API call sends the keyword/category/sort bundle to the server for DB-backed search.
    g_api.searchListings(keyword, listings, msg, category, priceSort);
    if (listings.empty())
    {
        clear();
        showMessage(4, "No Results Found");
        return;
    }
    ApiListing selected = showListingList("Search Results", listings); // Listing chosen from the search results.
    if (selected.id == -1)
        return;
    if (selected.sellerId == g_api.getCurrentUserId())
    {
        // Reuse the seller detail screen when the search result belongs to the current user.
        showMyListingDetail(selected.id);
        return;
    }
    showListingDetail(selected);
}

/**
 * @brief Fetches and displays a single recommendation.
 *
 * Requests the current recommendation from the server and optionally lets the
 * user drill into the suggested listing.
 * @author Muhammad Naheen Mahboob (mmahbo)
 */
void showRecommendationsScreen()
{
    ApiListing rec; // Recommended listing returned by the API.
    string msg;     // Auxiliary API message returned by the recommendation endpoint.
    // Fetch the current recommendation on entry instead of caching stale client-side data.
    bool ok = g_api.getRecommendation(rec, msg); // True when a recommendation row was fetched successfully.
    clear();
    if (!ok || rec.id == -1)
    {
        showMessage(4, "No listings yet - check back once users start posting!");
        return;
    }
    // Recommendation screen displays a single suggestion and lets the user drill into it.
    mvprintw(4, 2, "Recommended for you:");
    mvprintw(6, 4, "1. %s ($%.2f)", rec.title.c_str(), rec.price);
    mvprintw(8, 2, "Select (1 to view, 0 to go back): ");
    refresh();
    if (getch() == '1')
        showListingDetail(rec);
}

/**
 * @brief Shows watchlist and opens selected item.
 *
 * Loads the authenticated user's watchlist from the server and opens the
 * selected listing in the buyer-facing detail flow.
 * @author Muhammad Naheen Mahboob (mmahbo)
 */
void showWatchlistScreen()
{
    vector<ApiListing> listings; // Watchlist rows returned by the API.
    string msg;                  // Auxiliary API message returned while loading the watchlist.
    // Load the watchlist from the server so the screen reflects the latest listing status.
    g_api.getWatchlist(listings, msg);
    ApiListing selected = showListingList("My Watchlist", listings); // Listing chosen from the watchlist.
    if (selected.id != -1)
        showListingDetail(selected, true);
}

/**
 * @brief Displays current user's outgoing offers.
 *
 * Loads and renders the buyer's outgoing offer history as a simple read-only
 * screen.
 * @author Muhammad Naheen Mahboob (mmahbo)
 */
void showMyOffersScreen()
{
    vector<ApiOffer> offers; // Outgoing offers returned by the API.
    string msg;              // Auxiliary API message returned while loading outgoing offers.
    // Pull the buyer's current outgoing-offer history from the server before rendering.
    g_api.getMyOffers(offers, msg);
    clear();
    drawTitle("My Offers");
    int row = 4; // Current row used to draw the outgoing-offers screen.
    if (offers.empty())
    {
        mvprintw(row, 2, "No offers made yet.");
    }
    else
    {
        for (const ApiOffer &o : offers)
        {
            mvprintw(row++, 2, "Offer: $%.2f for %s listed by %s %s",
                     o.offerPrice,
                     o.listingTitle.c_str(),
                     usernameFromEmail(o.ownerName).c_str(),
                     o.status.c_str());
        }
    }
    showMessage(row + 2, "");
}

// Story 19: My Purchases + Rate Seller
/**
 * @brief Displays purchases and allows rating the seller.
 *
 * Loads the current buyer's purchases, lets the user select one, and drives
 * the seller-rating flow for eligible purchases.
 * @author Muhammad Naheen Mahboob (mmahbo)
 */
void showMyPurchasesScreen()
{
    vector<ApiListing> purchases; // Purchased listings returned by the API.
    string msg;                   // Auxiliary API message returned while loading purchases.
    // Start by loading the buyer's current purchases from the server.
    g_api.getMyPurchases(purchases, msg);

    if (purchases.empty())
    {
        clear();
        showMessage(4, "No purchases found.");
        return;
    }

    // Use same navigation pattern as other list screens
    ApiListing selected = showListingList("My Purchases", purchases); // Purchase chosen from the purchases list.
    if (selected.id == -1)
        return;

    // Rate seller screen
    while (true)
    {
        clear();
        drawTitle("Rate Seller");
        mvprintw(4, 2, "Item:         %s", selected.title.c_str());
        mvprintw(5, 2, "Seller:       %s", usernameFromEmail(selected.sellerName).c_str());
        string ratingDisplay; // Seller rating string shown on the rate-seller screen.
        string ratingMsg;     // Auxiliary API message returned when fetching the seller rating.
        // Refresh the seller's public rating summary from the server before drawing the screen.
        if (!g_api.getSellerRating(selected.sellerId, ratingDisplay, ratingMsg) || ratingDisplay.empty())
        {
            ratingDisplay = "Unavailable";
        }
        mvprintw(6, 2, "Avg Rating:   %s", ratingDisplay.c_str());
        mvprintw(8, 4, "1. Rate Seller");
        mvprintw(9, 4, "0. Back");
        mvprintw(11, 2, "Choice: ");
        refresh();

        int ch = getch(); // Latest menu choice pressed on the rate-seller screen.
        if (ch == '0')
            break;
        if (ch == '1')
        {
            string eligibility; // Precheck result describing whether this purchase can still be rated.
            // Ask the server whether this exact purchase is still eligible for a new review.
            if (!g_api.canRateSeller(selected.id, selected.sellerId, eligibility))
            {
                // Stop before opening the form if this purchase was already rated or is invalid.
                clear();
                showMessage(4, eligibility);
                break;
            }
            clear();
            drawTitle("Rate Seller");
            mvprintw(4, 2, "Item:         %s", selected.title.c_str());
            mvprintw(5, 2, "Seller:       %s", usernameFromEmail(selected.sellerName).c_str());
            mvprintw(6, 2, "Avg Rating:   %s", ratingDisplay.c_str());
            refresh();

            string ratingStr, comment; // Rating-form fields typed by the user.
            if (!collectRatingForm(ratingStr, comment))
                continue;
            int rating = 0; // Parsed numeric rating value submitted to the API.
            try
            {
                rating = stoi(ratingStr);
            }
            catch (...)
            {
                rating = 0;
            }
            clear();
            string result; // User-facing outcome of the final rating submission.
            // Submit the final rating using the selected purchase listing id as the review key.
            // The server does the real validation/storage; the UI just forwards the completed form.
            g_api.rateSeller(selected.id, selected.sellerId, rating, comment, result);
            showMessage(4, result);
            break;
        }
    }
}

/**
 * @brief Displays admin tools (currently ban user flow).
 *
 * Presents the simple admin panel used to collect an access code and submit
 * ban-user requests to the server.
 * @author Muhammad Naheen Mahboob (mmahbo)
 */
void showAdminPanel()
{
    clear();
    drawTitle("Admin Panel");
    mvprintw(4, 2, "Access Code: ");
    refresh();
    string code = getInput(4, 15, 20, true); // Admin access code entered for this admin-panel session.
    while (true)
    {
        clear();
        drawTitle("Admin Panel");
        mvprintw(4, 4, "1. Ban User");
        mvprintw(5, 4, "0. Back");
        mvprintw(7, 2, "Choice: ");
        refresh();
        int ch = getch(); // Latest menu choice pressed on the admin panel.
        if (ch == '1')
        {
            clear();
            drawTitle("Ban User");
            mvprintw(4, 2, "User ID to ban: ");
            refresh();
            string idStr = getInput(4, 18, 10); // Raw user-id text typed for the ban action.
            int id = -1;                        // Parsed target user id for the ban action.
            try
            {
                id = stoi(idStr);
            }
            catch (...)
            {
            }
            string msg; // User-facing result of the ban-user request.
            // Send the admin moderation request to the server so it can verify the access code.
            g_api.banUser(code, id, msg);
            clear();
            showMessage(4, msg);
        }
        else if (ch == '0')
        {
            break;
        }
    }
}

/**
 * @brief Main authenticated navigation menu.
 *
 * Displays the primary post-login navigation screen and dispatches each menu
 * choice into the corresponding authenticated UI flow.
 * @author Muhammad Naheen Mahboob (mmahbo)
 */
void showMainMenu()
{
    // Stay in this menu only while the shared ApiClient still holds a valid session token from the server.
    // Any branch below may trigger its own request/response round-trip through g_api.
    while (g_api.isLoggedIn())
    {
        clear();
        drawTitle("MustangMarketplace");
        mvprintw(4, 2, "Logged in as: %s", g_api.getCurrentEmail().c_str());
        mvhline(5, 0, '-', getmaxx(stdscr));
        mvprintw(8, 4, "1. Search Listings");
        mvprintw(9, 4, "2. Recommendations");
        mvprintw(10, 4, "3. My Watchlist");
        mvprintw(12, 4, "4. My Listings");
        mvprintw(13, 4, "5. My Offers");
        mvprintw(14, 4, "6. My Purchases");
        mvprintw(15, 4, "7. Notifications");
        mvprintw(16, 4, "8. Admin Panel");
        mvprintw(17, 4, "0. Logout");
        mvprintw(19, 2, "Choice: ");
        refresh();

        int ch = getch(); // Latest menu choice pressed on the main authenticated menu.
        if (ch == '1')
            showSearchScreen();
        else if (ch == '2')
            showRecommendationsScreen();
        else if (ch == '3')
            showWatchlistScreen();
        else if (ch == '4')
            showSellingMenu();
        else if (ch == '5')
            showMyOffersScreen();
        else if (ch == '6')
            showMyPurchasesScreen();
        else if (ch == '7')
        {
            vector<string> notes; // Notification messages returned by the API.
            string msg;           // Auxiliary API message returned while loading notifications.
            // Fetch notifications on demand so this screen always shows the newest server-side events.
            g_api.getNotifications(notes, msg);
            clear();
            drawTitle("Notifications");
            int row = 4; // Current row used to draw the notifications screen.
            if (notes.empty())
            {
                mvprintw(row, 2, "No notifications.");
            }
            else
            {
                for (const string &n : notes)
                {
                    mvprintw(row++, 2, "- %s", n.c_str());
                }
            }
            showMessage(row + 2, "");
        }
        else if (ch == '8')
            showAdminPanel();
        else if (ch == '0')
        {
            // Explicit logout invalidates the server-side session token before clearing the client state.
            g_api.logout();
            clear();
            showMessage(4, "Logged out successfully.");
        }
    }
}

/**
 * @brief Unauthenticated landing screen for login/register/exit.
 *
 * Displays the initial entry screen and routes the user into authentication or
 * exit flows until the program terminates.
 * @author Muhammad Naheen Mahboob (mmahbo)
 */
void showLandingScreen()
{
    while (true)
    {
        clear();
        drawTitle("MustangMarketplace");
        mvprintw(4, 2, "Welcome! Please choose an option:");
        mvprintw(6, 4, "1. Login");
        mvprintw(7, 4, "2. Register");
        mvprintw(8, 4, "3. Exit");
        mvprintw(10, 2, "Choice: ");
        refresh();

        int ch = getch(); // Latest menu choice pressed on the unauthenticated landing screen.
        if (ch == '3')
            break;
        if (ch != '1' && ch != '2')
            continue;

        string email;                                                                        // Email collected from the login/register form.
        string password;                                                                     // Password collected from the login/register form.
        bool submitted = collectAuthForm(ch == '1' ? "Login" : "Register", email, password); // True when the auth form was submitted instead of cancelled.
        if (!submitted)
            continue;

        string msg; // User-facing result of the login/register request.
        // Dispatch to login or register based on the landing-screen choice.
        // This is the first network hop from the UI: credentials are sent to the server for auth.
        // A successful call also caches the returned token inside g_api for future authenticated requests.
        bool ok = (ch == '1') ? g_api.login(email, password, msg) : g_api.registerUser(email, password, msg); // True when the selected auth flow succeeded.
        clear();
        showMessage(4, msg);
        // Enter the authenticated menu immediately after a successful auth flow.
        if (ok)
            showMainMenu();
    }
}

/**
 * @brief Program entry point that initializes ncurses and starts UI loop.
 *
 * Prepares the ncurses runtime, runs the landing screen loop, and restores the
 * terminal before the client process exits.
 * @return Process exit code.
 */
int main()
{
    // Initialize ncurses input/output behavior before drawing any UI screens.
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    start_color();
    init_color(COLOR_MAGENTA, 310, 149, 514);
    init_pair(1, COLOR_MAGENTA, COLOR_BLACK);

    // Run the landing screen until the user exits, then restore the terminal state.
    showLandingScreen();
    endwin();
    return 0;
}
