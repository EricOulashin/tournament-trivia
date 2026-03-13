/*    TRIVCONFIG.CPP
      Configuration program for Tournament Trivia.
      Reads and writes settings.dat in the portable 290-byte format
      compatible with the original 32-bit Windows game.

      Build:  g++ -std=c++20 -o trivconfig trivconfig.cpp -lncurses
*/

#include <ncurses.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <filesystem>

// Must match the game's MAX_TRIVIA_FILES
static const int MAX_TRIVIA_FILES = 10;

// Settings structure matching the game's GameSettings layout.
// All I/O is done field-by-field to match the 290-byte Windows format.
struct Settings
{
    short nCurMonth;
    short nDifficulty;
    char  szPreviousWinner[60];
    short nPreviousHighScore;
    short nMaxClues;
    int   nClueFrequency;       // stored as 4-byte int (Windows 32-bit time_t)
    int   nQuestionFrequency;   // stored as 4-byte int
    char  bVerifySubmissions;   // stored as 1-byte char
    char  szExtraFiles[MAX_TRIVIA_FILES][21];
    char  bListSysops;          // stored as 1-byte char
    short nPlayerTimeout;
};

static const int SETTINGS_FILE_SIZE = 290;
static const char* SETTINGS_FILE = "settings.dat";

// -----------------------------------------------------------------------
// Load / Save in the exact 290-byte Windows-compatible binary format
// -----------------------------------------------------------------------

static void setDefaults(Settings& s)
{
    memset(&s, 0, sizeof(s));
    s.nCurMonth          = 1;
    s.nDifficulty        = 0;
    strcpy(s.szPreviousWinner, "none");
    s.nPreviousHighScore = 0;
    s.nMaxClues          = 3;
    s.nClueFrequency     = 12;
    s.nQuestionFrequency = 50;
    s.bVerifySubmissions = 1;
    strcpy(s.szExtraFiles[0], "database.enc");
    strcpy(s.szExtraFiles[1], "custom.txt");
    s.bListSysops        = 1;
    s.nPlayerTimeout     = 300;
}

static bool loadSettings(Settings& s)
{
    setDefaults(s);

    std::ifstream f(SETTINGS_FILE, std::ios::in | std::ios::binary);
    if (!f)
        return false;

    f.seekg(0, std::ios::end);
    long fileSize = f.tellg();
    f.seekg(0, std::ios::beg);

    if (fileSize != SETTINGS_FILE_SIZE)
        return false;   // unknown format, keep defaults

    f.read((char*)&s.nCurMonth, 2);
    f.read((char*)&s.nDifficulty, 2);
    f.read(s.szPreviousWinner, 60);
    f.read((char*)&s.nPreviousHighScore, 2);
    f.read((char*)&s.nMaxClues, 2);
    f.read((char*)&s.nClueFrequency, 4);
    f.read((char*)&s.nQuestionFrequency, 4);
    f.read(&s.bVerifySubmissions, 1);
    f.read((char*)s.szExtraFiles, 210);
    f.read(&s.bListSysops, 1);
    f.read((char*)&s.nPlayerTimeout, 2);
    f.close();

    // Validate
    if (s.nMaxClues < 0 || s.nMaxClues > 4)
        s.nMaxClues = 3;
    if (s.nQuestionFrequency < 25 || s.nQuestionFrequency > 75)
        s.nQuestionFrequency = 50;
    s.nClueFrequency = s.nQuestionFrequency / (s.nMaxClues + 1);
    if (s.nPlayerTimeout < 60 || s.nPlayerTimeout > 600)
        s.nPlayerTimeout = 300;
    if (strlen(s.szExtraFiles[0]) == 0 || s.szExtraFiles[0][0] < 32)
        strcpy(s.szExtraFiles[0], "database.enc");

    return true;
}

static bool saveSettings(const Settings& s)
{
    std::ofstream f(SETTINGS_FILE, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!f)
        return false;

    f.write((const char*)&s.nCurMonth, 2);
    f.write((const char*)&s.nDifficulty, 2);
    f.write(s.szPreviousWinner, 60);
    f.write((const char*)&s.nPreviousHighScore, 2);
    f.write((const char*)&s.nMaxClues, 2);
    f.write((const char*)&s.nClueFrequency, 4);
    f.write((const char*)&s.nQuestionFrequency, 4);
    f.write(&s.bVerifySubmissions, 1);
    f.write((const char*)s.szExtraFiles, 210);
    f.write(&s.bListSysops, 1);
    f.write((const char*)&s.nPlayerTimeout, 2);
    f.flush();
    f.close();
    return true;
}

// -----------------------------------------------------------------------
// Ncurses helpers
// -----------------------------------------------------------------------

static void drawTitle(int cols)
{
    attron(A_BOLD | COLOR_PAIR(1));
    mvhline(0, 0, ' ', cols);
    mvprintw(0, (cols - 36) / 2, "Tournament Trivia - Configuration");
    attroff(A_BOLD | COLOR_PAIR(1));
}

// Prompt the user for a string value at the bottom of the screen.
// Returns true if the user provided input, false if they pressed Escape.
static bool promptString(const char* label, char* buf, int maxLen)
{
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    // Clear prompt area
    move(rows - 2, 0); clrtoeol();
    move(rows - 1, 0); clrtoeol();

    attron(COLOR_PAIR(3));
    mvprintw(rows - 2, 2, "%s", label);
    mvprintw(rows - 1, 2, "> ");
    attroff(COLOR_PAIR(3));

    echo();
    curs_set(1);
    char input[256] = "";
    mvgetnstr(rows - 1, 4, input, maxLen < 255 ? maxLen : 255);
    noecho();
    curs_set(0);

    // Clear prompt area
    move(rows - 2, 0); clrtoeol();
    move(rows - 1, 0); clrtoeol();

    if (strlen(input) == 0)
        return false;

    strncpy(buf, input, maxLen);
    buf[maxLen] = '\0';
    return true;
}

// Prompt for an integer value. Returns true if valid input received.
static bool promptInt(const char* label, int& value, int minVal, int maxVal)
{
    char buf[32];
    char fullLabel[256];
    snprintf(fullLabel, sizeof(fullLabel), "%s (%d-%d)", label, minVal, maxVal);

    if (!promptString(fullLabel, buf, 10))
        return false;

    int v = atoi(buf);
    if (v < minVal || v > maxVal)
    {
        int rows, cols;
        getmaxyx(stdscr, rows, cols);
        attron(COLOR_PAIR(2));
        mvprintw(rows - 2, 2, "Value out of range. Press any key.");
        attroff(COLOR_PAIR(2));
        getch();
        move(rows - 2, 0); clrtoeol();
        return false;
    }

    value = v;
    return true;
}

// Ask a yes/no question. Returns 1 for yes, 0 for no, -1 for cancel.
static int promptYesNo(const char* label)
{
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    move(rows - 2, 0); clrtoeol();
    move(rows - 1, 0); clrtoeol();

    attron(COLOR_PAIR(3));
    mvprintw(rows - 2, 2, "%s [y/n]: ", label);
    attroff(COLOR_PAIR(3));

    int ch = getch();
    move(rows - 2, 0); clrtoeol();

    if (ch == 'y' || ch == 'Y') return 1;
    if (ch == 'n' || ch == 'N') return 0;
    return -1;
}

static void showMessage(const char* msg, int colorPair = 3)
{
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    move(rows - 2, 0); clrtoeol();
    attron(COLOR_PAIR(colorPair));
    mvprintw(rows - 2, 2, "%s", msg);
    attroff(COLOR_PAIR(colorPair));
    getch();
    move(rows - 2, 0); clrtoeol();
}

// -----------------------------------------------------------------------
// Menu rendering
// -----------------------------------------------------------------------

enum MenuItem
{
    MI_QUESTION_FREQ = 0,
    MI_MAX_CLUES,
    MI_PLAYER_TIMEOUT,
    MI_LIST_SYSOPS,
    MI_VERIFY_SUBMISSIONS,
    MI_SEP1,               // separator
    MI_FILE_FIRST,          // first file slot
    MI_FILE_LAST = MI_FILE_FIRST + MAX_TRIVIA_FILES - 1,
    MI_SEP2,
    MI_PREV_WINNER,
    MI_PREV_SCORE,
    MI_SEP3,
    MI_SAVE,
    MI_EXIT,
    MI_COUNT
};

static bool isSeparator(int item)
{
    return item == MI_SEP1 || item == MI_SEP2 || item == MI_SEP3;
}

static void drawMenu(const Settings& s, int selected, bool dirty)
{
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    drawTitle(cols);

    int y = 2;
    int labelCol = 4;
    int valueCol = 38;

    auto drawItem = [&](int idx, const char* label, const char* value)
    {
        if (y >= rows - 3) return;

        if (idx == selected)
        {
            attron(A_REVERSE | COLOR_PAIR(4));
            mvhline(y, 0, ' ', cols);
        }

        mvprintw(y, labelCol, "%-30s", label);

        if (idx == selected)
            attroff(A_REVERSE | COLOR_PAIR(4));

        if (idx == selected)
            attron(A_BOLD | A_REVERSE | COLOR_PAIR(4));
        else
            attron(A_BOLD | COLOR_PAIR(5));

        mvprintw(y, valueCol, "%s", value);

        if (idx == selected)
            attroff(A_BOLD | A_REVERSE | COLOR_PAIR(4));
        else
            attroff(A_BOLD | COLOR_PAIR(5));

        y++;
    };

    auto drawSep = [&](int idx)
    {
        if (y >= rows - 3) return;
        mvhline(y, 2, ACS_HLINE, cols - 4);
        y++;
    };

    char buf[128];

    // Gameplay settings
    attron(COLOR_PAIR(6) | A_BOLD);
    mvprintw(y, labelCol, "Gameplay Settings");
    attroff(COLOR_PAIR(6) | A_BOLD);
    y++;

    snprintf(buf, sizeof(buf), "%d seconds", s.nQuestionFrequency);
    drawItem(MI_QUESTION_FREQ, "Question Frequency", buf);

    snprintf(buf, sizeof(buf), "%d", s.nMaxClues);
    drawItem(MI_MAX_CLUES, "Max Clues per Question", buf);

    snprintf(buf, sizeof(buf), "%d seconds", s.nPlayerTimeout);
    drawItem(MI_PLAYER_TIMEOUT, "Player Inactivity Timeout", buf);

    drawItem(MI_LIST_SYSOPS, "Show Sysops on Score List", s.bListSysops ? "Yes" : "No");
    drawItem(MI_VERIFY_SUBMISSIONS, "Verify Question Submissions", s.bVerifySubmissions ? "Yes" : "No");

    drawSep(MI_SEP1);

    // Question files
    attron(COLOR_PAIR(6) | A_BOLD);
    mvprintw(y, labelCol, "Question Files");
    attroff(COLOR_PAIR(6) | A_BOLD);
    y++;

    for (int i = 0; i < MAX_TRIVIA_FILES; i++)
    {
        char label[32];
        snprintf(label, sizeof(label), "File #%d", i + 1);
        const char* val = (strlen(s.szExtraFiles[i]) > 0) ? s.szExtraFiles[i] : "[none]";
        drawItem(MI_FILE_FIRST + i, label, val);
    }

    drawSep(MI_SEP2);

    // Previous winner (informational)
    attron(COLOR_PAIR(6) | A_BOLD);
    mvprintw(y, labelCol, "Monthly Winner Record");
    attroff(COLOR_PAIR(6) | A_BOLD);
    y++;

    drawItem(MI_PREV_WINNER, "Previous Month's Winner", s.szPreviousWinner);

    snprintf(buf, sizeof(buf), "%d", s.nPreviousHighScore);
    drawItem(MI_PREV_SCORE, "Previous Month's High Score", buf);

    drawSep(MI_SEP3);

    // Actions
    drawItem(MI_SAVE, dirty ? ">>> Save Settings <<<" : "    Save Settings", "");
    drawItem(MI_EXIT, "    Exit", "");

    // Status bar
    attron(COLOR_PAIR(1));
    mvhline(rows - 3, 0, ' ', cols);
    if (dirty)
        mvprintw(rows - 3, 2, " [Modified] ");
    mvprintw(rows - 3, cols - 42, " Up/Down: Navigate  Enter: Edit  Q: Quit ");
    attroff(COLOR_PAIR(1));
}

// -----------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------

int main()
{
    Settings settings;
    Settings original;
    bool dirty = false;

    loadSettings(settings);
    memcpy(&original, &settings, sizeof(Settings));

    // Init ncurses
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    if (has_colors())
    {
        start_color();
        use_default_colors();
        init_pair(1, COLOR_WHITE, COLOR_BLUE);    // title bar / status bar
        init_pair(2, COLOR_RED, -1);               // errors
        init_pair(3, COLOR_CYAN, -1);              // prompts
        init_pair(4, COLOR_WHITE, COLOR_CYAN);     // selected item
        init_pair(5, COLOR_YELLOW, -1);            // values
        init_pair(6, COLOR_GREEN, -1);             // section headers
    }

    int selected = MI_QUESTION_FREQ;

    while (true)
    {
        erase();
        drawMenu(settings, selected, dirty);
        refresh();

        int ch = getch();

        switch (ch)
        {
        case KEY_UP:
        case 'k':
            do {
                selected--;
                if (selected < 0) selected = MI_COUNT - 1;
            } while (isSeparator(selected));
            break;

        case KEY_DOWN:
        case 'j':
            do {
                selected++;
                if (selected >= MI_COUNT) selected = 0;
            } while (isSeparator(selected));
            break;

        case KEY_HOME:
            selected = MI_QUESTION_FREQ;
            break;

        case KEY_END:
            selected = MI_EXIT;
            break;

        case 'q':
        case 'Q':
            goto do_exit;

        case '\n':
        case '\r':
        case KEY_ENTER:
        {
            if (selected == MI_EXIT)
                goto do_exit;

            if (selected == MI_SAVE)
            {
                if (saveSettings(settings))
                {
                    memcpy(&original, &settings, sizeof(Settings));
                    dirty = false;
                    showMessage("Settings saved successfully.");
                }
                else
                    showMessage("Error: Could not write settings.dat!", 2);
                break;
            }

            if (selected == MI_QUESTION_FREQ)
            {
                int v = settings.nQuestionFrequency;
                if (promptInt("Question frequency in seconds", v, 25, 75))
                {
                    settings.nQuestionFrequency = (short)v;
                    settings.nClueFrequency = settings.nQuestionFrequency / (settings.nMaxClues + 1);
                    dirty = (memcmp(&settings, &original, sizeof(Settings)) != 0);
                }
            }
            else if (selected == MI_MAX_CLUES)
            {
                int v = settings.nMaxClues;
                if (promptInt("Maximum clues per question", v, 0, 4))
                {
                    settings.nMaxClues = (short)v;
                    settings.nClueFrequency = settings.nQuestionFrequency / (settings.nMaxClues + 1);
                    dirty = (memcmp(&settings, &original, sizeof(Settings)) != 0);
                }
            }
            else if (selected == MI_PLAYER_TIMEOUT)
            {
                int v = settings.nPlayerTimeout;
                if (promptInt("Player inactivity timeout in seconds", v, 60, 600))
                {
                    settings.nPlayerTimeout = (short)v;
                    dirty = (memcmp(&settings, &original, sizeof(Settings)) != 0);
                }
            }
            else if (selected == MI_LIST_SYSOPS)
            {
                settings.bListSysops = settings.bListSysops ? 0 : 1;
                dirty = (memcmp(&settings, &original, sizeof(Settings)) != 0);
            }
            else if (selected == MI_VERIFY_SUBMISSIONS)
            {
                settings.bVerifySubmissions = settings.bVerifySubmissions ? 0 : 1;
                dirty = (memcmp(&settings, &original, sizeof(Settings)) != 0);
            }
            else if (selected >= MI_FILE_FIRST && selected <= MI_FILE_LAST)
            {
                int idx = selected - MI_FILE_FIRST;
                char buf[22] = "";
                if (promptString("Enter filename (or blank for none)", buf, 20))
                {
                    strncpy(settings.szExtraFiles[idx], buf, 20);
                    settings.szExtraFiles[idx][20] = '\0';
                }
                else
                {
                    // User pressed enter with no input — clear the file
                    settings.szExtraFiles[idx][0] = '\0';
                }
                // Ensure at least one valid file
                bool hasFile = false;
                for (int i = 0; i < MAX_TRIVIA_FILES; i++)
                    if (strlen(settings.szExtraFiles[i]) > 0) hasFile = true;
                if (!hasFile)
                    strcpy(settings.szExtraFiles[0], "database.enc");
                dirty = (memcmp(&settings, &original, sizeof(Settings)) != 0);
            }
            else if (selected == MI_PREV_WINNER)
            {
                char buf[60];
                if (promptString("Previous month's winner name", buf, 59))
                {
                    strncpy(settings.szPreviousWinner, buf, 59);
                    settings.szPreviousWinner[59] = '\0';
                    dirty = (memcmp(&settings, &original, sizeof(Settings)) != 0);
                }
            }
            else if (selected == MI_PREV_SCORE)
            {
                int v = settings.nPreviousHighScore;
                if (promptInt("Previous month's high score", v, 0, 32767))
                {
                    settings.nPreviousHighScore = (short)v;
                    dirty = (memcmp(&settings, &original, sizeof(Settings)) != 0);
                }
            }
            break;
        }

        case ' ':
            // Space toggles boolean items
            if (selected == MI_LIST_SYSOPS)
            {
                settings.bListSysops = settings.bListSysops ? 0 : 1;
                dirty = (memcmp(&settings, &original, sizeof(Settings)) != 0);
            }
            else if (selected == MI_VERIFY_SUBMISSIONS)
            {
                settings.bVerifySubmissions = settings.bVerifySubmissions ? 0 : 1;
                dirty = (memcmp(&settings, &original, sizeof(Settings)) != 0);
            }
            break;
        }

        continue;

    do_exit:
        if (dirty)
        {
            int answer = promptYesNo("Settings have been modified. Save before exiting?");
            if (answer == 1)
            {
                if (saveSettings(settings))
                    showMessage("Settings saved.");
                else
                    showMessage("Error: Could not write settings.dat!", 2);
                break;
            }
            else if (answer == 0)
                break;
            // answer == -1 means cancel, go back to menu
            continue;
        }
        break;
    }

    endwin();
    return 0;
}
