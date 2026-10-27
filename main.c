#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_TITLE 100
#define MAX_AUTHOR 80
#define DATA_FILE "books.dat"
#define MAX_BOOKS 10000

typedef struct {
    int id;
    char title[MAX_TITLE];
    char author[MAX_AUTHOR];
    int year;
    int total;
    int available;
} Book;

static Book *books = NULL;
static size_t book_count = 0;
static size_t book_capacity = 0;
static HWND hMain, hList, hId, hTitle, hAuthor, hYear, hCopies, hSearch;
static HFONT hFont;
static HBRUSH hBgBrush, hInputBrush, hListBrush;
static COLORREF COLOR_BG = RGB(17, 24, 39);
static COLORREF COLOR_PANEL = RGB(31, 41, 55);
static COLORREF COLOR_INPUT = RGB(248, 250, 252);
static COLORREF COLOR_TEXT = RGB(229, 231, 235);
static COLORREF COLOR_DARK_TEXT = RGB(17, 24, 39);
static const char *DATA_PATH = DATA_FILE;

enum { ID_ID=101, ID_TITLE, ID_AUTHOR, ID_YEAR, ID_COPIES, ID_ADD, ID_UPDATE,
       ID_DELETE, ID_CLEAR, ID_SEARCH_EDIT, ID_SHOWALL, ID_LIST, ID_STATUS, ID_ABOUT,
       ID_ISSUE, ID_RETURN, ID_SEARCH_BUTTON };

static void set_status(const char *msg) {
    HWND s = GetDlgItem(hMain, ID_STATUS);
    if (s) SetWindowTextA(s, msg);
}

static int ensure_capacity(size_t needed) {
    if (needed <= book_capacity) return 1;
    size_t cap = book_capacity ? book_capacity * 2 : 16;
    while (cap < needed) cap *= 2;
    Book *p = (Book *)realloc(books, cap * sizeof(Book));
    if (!p) {
        MessageBoxA(hMain, "Not enough memory to store more books.", "Memory error", MB_ICONERROR);
        return 0;
    }
    books = p;
    book_capacity = cap;
    return 1;
}

static int find_index_by_id(int id) {
    for (size_t i=0; i<book_count; ++i) if (books[i].id == id) return (int)i;
    return -1;
}

static int save_books(void) {
    FILE *fp = fopen(DATA_PATH, "wb");
    if (!fp) {
        MessageBoxA(hMain, "Could not save books.dat. Check folder permissions.", "File error", MB_ICONERROR);
        return 0;
    }
    unsigned int count = (unsigned int)book_count;
    int ok = fwrite(&count, sizeof(count), 1, fp) == 1;
    if (ok && count) ok = fwrite(books, sizeof(Book), count, fp) == count;
    fclose(fp);
    if (!ok) MessageBoxA(hMain, "An error occurred while writing the data file.", "File error", MB_ICONERROR);
    return ok;
}

static void load_books(void) {
    FILE *fp = fopen(DATA_PATH, "rb");
    if (!fp) return; /* First run: no file yet. */
    unsigned int count = 0;
    if (fread(&count, sizeof(count), 1, fp) != 1 || count > MAX_BOOKS) {
        fclose(fp);
        MessageBoxA(hMain, "The data file is invalid or damaged. Starting with an empty list.", "Data file", MB_ICONWARNING);
        return;
    }
    if (count && !ensure_capacity(count)) { fclose(fp); return; }
    if (count && fread(books, sizeof(Book), count, fp) != count) {
        fclose(fp);
        book_count = 0;
        MessageBoxA(hMain, "Could not read all records. Starting with an empty list.", "Data file", MB_ICONWARNING);
        return;
    }
    book_count = count;
    fclose(fp);
}

static void add_control(const char *klass, const char *text, DWORD style,
                        int x, int y, int w, int h, int id) {
    style = (style & ~0x0000000F) | BS_OWNERDRAW;
    HWND c = CreateWindowExA(0, klass, text, WS_CHILD | WS_VISIBLE | style,
        x, y, w, h, hMain, (HMENU)(INT_PTR)id, GetModuleHandle(NULL), NULL);
    if (hFont) SendMessage(c, WM_SETFONT, (WPARAM)hFont, TRUE);
}

static void refresh_list(const char *filter) {
    SendMessageA(hList, LB_RESETCONTENT, 0, 0);
    char line[360];
    int shown = 0;
    for (size_t i=0; i<book_count; ++i) {
        Book *b = &books[i];
        if (filter && *filter) {
            char haystack[MAX_TITLE + MAX_AUTHOR + 8];
            snprintf(haystack, sizeof(haystack), "%s %s %d", b->title, b->author, b->id);
            char a[sizeof(haystack)], f[128];
            snprintf(a, sizeof(a), "%s", haystack);
            snprintf(f, sizeof(f), "%s", filter);
            for (char *p=a; *p; ++p) *p=(char)tolower((unsigned char)*p);
            for (char *p=f; *p; ++p) *p=(char)tolower((unsigned char)*p);
            if (!strstr(a, f)) continue;
        }
        snprintf(line, sizeof(line), "ID %-5d | %-28.28s | %-18.18s | %d | %d/%d",
                 b->id, b->title, b->author, b->year, b->available, b->total);
        SendMessageA(hList, LB_ADDSTRING, 0, (LPARAM)line);
        ++shown;
    }
    char status[128];
    snprintf(status, sizeof(status), "Books in catalogue: %zu    |    Showing: %d", book_count, shown);
    set_status(status);
}

static void clear_fields(void) {
    SetWindowTextA(hId, "");
    SetWindowTextA(hTitle, "");
    SetWindowTextA(hAuthor, "");
    SetWindowTextA(hYear, "");
    SetWindowTextA(hCopies, "1");
    SendMessageA(hList, LB_SETSEL, FALSE, (LPARAM)-1);
}

static int get_int(HWND control, int *out) {
    char s[64];
    GetWindowTextA(control, s, sizeof(s));
    if (!s[0]) return 0;
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (*end || v < 0 || v > 999999) return 0;
    *out = (int)v;
    return 1;
}

static int read_form(Book *b, int editing) {
    char idbuf[64];
    GetWindowTextA(hId, idbuf, sizeof(idbuf));
    if (!idbuf[0] || !get_int(hId, &b->id) || b->id <= 0) {
        MessageBoxA(hMain, "Enter a numeric Book ID greater than zero.", "Validation", MB_ICONWARNING);
        return 0;
    }
    GetWindowTextA(hTitle, b->title, sizeof(b->title));
    GetWindowTextA(hAuthor, b->author, sizeof(b->author));
    if (!b->title[0] || !b->author[0]) {
        MessageBoxA(hMain, "Title and Author are required.", "Validation", MB_ICONWARNING);
        return 0;
    }
    if (!get_int(hYear, &b->year) || b->year < 1000 || b->year > 2100) {
        MessageBoxA(hMain, "Enter a valid publication year (1000–2100).", "Validation", MB_ICONWARNING);
        return 0;
    }
    if (!get_int(hCopies, &b->total) || b->total < 1 || b->total > 999999) {
        MessageBoxA(hMain, "Total copies must be at least 1.", "Validation", MB_ICONWARNING);
        return 0;
    }
    if (!editing) b->available = b->total;
    return 1;
}

static void add_book(void) {
    Book b;
    memset(&b, 0, sizeof(b));
    if (!read_form(&b, 0)) return;
    if (find_index_by_id(b.id) >= 0) {
        MessageBoxA(hMain, "That Book ID already exists. Use a unique ID.", "Duplicate ID", MB_ICONWARNING);
        return;
    }
    if (!ensure_capacity(book_count + 1)) return;
    books[book_count++] = b;
    if (!save_books()) { --book_count; return; }
    refresh_list(NULL);
    clear_fields();
    set_status("Book added and saved successfully.");
}

static void update_book(void) {
    int id;
    if (!get_int(hId, &id) || id <= 0) {
        MessageBoxA(hMain, "Enter the ID of the book to update.", "Validation", MB_ICONWARNING);
        return;
    }
    int idx = find_index_by_id(id);
    if (idx < 0) {
        MessageBoxA(hMain, "Book ID not found.", "Not found", MB_ICONINFORMATION);
        return;
    }
    Book b = books[idx];
    if (!read_form(&b, 1)) return;
    if (b.id != id && find_index_by_id(b.id) >= 0) {
        MessageBoxA(hMain, "The new ID is already in use.", "Duplicate ID", MB_ICONWARNING);
        return;
    }
    int borrowed = books[idx].total - books[idx].available;
    if (b.total < borrowed) {
        MessageBoxA(hMain, "Total copies cannot be less than the number currently issued.", "Validation", MB_ICONWARNING);
        return;
    }
    b.available = b.total - borrowed;
    books[idx] = b;
    save_books();
    refresh_list(NULL);
    set_status("Book updated.");
}

static void delete_book(void) {
    int id;
    if (!get_int(hId, &id) || id <= 0) {
        MessageBoxA(hMain, "Enter the Book ID to delete.", "Validation", MB_ICONWARNING);
        return;
    }
    int idx = find_index_by_id(id);
    if (idx < 0) {
        MessageBoxA(hMain, "Book ID not found.", "Not found", MB_ICONINFORMATION);
        return;
    }
    if (books[idx].available != books[idx].total) {
        MessageBoxA(hMain, "This book has issued copies. Return all copies before deleting it.", "Cannot delete", MB_ICONWARNING);
        return;
    }
    char msg[180];
    snprintf(msg, sizeof(msg), "Delete book ID %d: %s?", books[idx].id, books[idx].title);
    if (MessageBoxA(hMain, msg, "Confirm deletion", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    memmove(&books[idx], &books[idx+1], (book_count-(size_t)idx-1)*sizeof(Book));
    --book_count;
    save_books();
    refresh_list(NULL);
    clear_fields();
    set_status("Book deleted.");
}

static void issue_return_book(int issue) {
    int id;
    if (!get_int(hId, &id) || id <= 0) {
        MessageBoxA(hMain, "Enter a Book ID first.", "Validation", MB_ICONWARNING);
        return;
    }
    int idx = find_index_by_id(id);
    if (idx < 0) {
        MessageBoxA(hMain, "Book ID not found.", "Not found", MB_ICONINFORMATION);
        return;
    }
    if (issue) {
        if (books[idx].available <= 0) {
            MessageBoxA(hMain, "No copies are currently available.", "Unavailable", MB_ICONWARNING);
            return;
        }
        --books[idx].available;
    } else {
        if (books[idx].available >= books[idx].total) {
            MessageBoxA(hMain, "All copies are already marked as available.", "Nothing to return", MB_ICONINFORMATION);
            return;
        }
        ++books[idx].available;
    }
    if (save_books()) {
        refresh_list(NULL);
        char msg[120];
        snprintf(msg, sizeof(msg), "Book %s successfully.", issue ? "issued" : "returned");
        set_status(msg);
    }
}

static void load_selected(void) {
    LRESULT sel = SendMessageA(hList, LB_GETCURSEL, 0, 0);
    if (sel == LB_ERR) return;
    char line[360];
    SendMessageA(hList, LB_GETTEXT, sel, (LPARAM)line);
    int id = 0;
    if (sscanf(line, "ID %d", &id) != 1) return;
    int idx = find_index_by_id(id);
    if (idx < 0) return;
    char buf[32];
    SetWindowTextA(hId, _itoa(books[idx].id, buf, 10));
    SetWindowTextA(hTitle, books[idx].title);
    SetWindowTextA(hAuthor, books[idx].author);
    SetWindowTextA(hYear, _itoa(books[idx].year, buf, 10));
    SetWindowTextA(hCopies, _itoa(books[idx].total, buf, 10));
}

static void draw_colored_button(const DRAWITEMSTRUCT *dis) {
    const int id = (int)dis->CtlID;
    COLORREF fill = RGB(37, 99, 235);       /* blue */
    if (id == ID_DELETE) fill = RGB(220, 38, 38);
    else if (id == ID_ABOUT+1) fill = RGB(5, 150, 105);
    else if (id == ID_ABOUT+2) fill = RGB(13, 148, 136);
    else if (id == ID_CLEAR) fill = RGB(100, 116, 139);
    else if (id == 1005) fill = RGB(124, 58, 237);
    else if (id == ID_SHOWALL) fill = RGB(71, 85, 105);
    else if (id == ID_UPDATE) fill = RGB(217, 119, 6);

    RECT r = dis->rcItem;
    HBRUSH brush = CreateSolidBrush(fill);
    FillRect(dis->hDC, &r, brush);
    DeleteObject(brush);

    if (dis->itemState & ODS_SELECTED) {
        InflateRect(&r, -2, -2);
        HBRUSH pressed = CreateSolidBrush(RGB(15, 23, 42));
        FrameRect(dis->hDC, &r, pressed);
        DeleteObject(pressed);
    }
    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, RGB(255, 255, 255));
    char label[80];
    GetWindowTextA(dis->hwndItem, label, sizeof(label));
    DrawTextA(dis->hDC, label, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_ERASEBKGND: {
        RECT r;
        GetClientRect(hwnd, &r);
        HBRUSH b = CreateSolidBrush(COLOR_BG);
        FillRect((HDC)wp, &r, b);
        DeleteObject(b);
        return 1;
    }
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wp;
        SetTextColor(dc, COLOR_TEXT);
        SetBkColor(dc, COLOR_BG);
        return (LRESULT)hBgBrush;
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wp;
        SetTextColor(dc, COLOR_DARK_TEXT);
        SetBkColor(dc, COLOR_INPUT);
        return (LRESULT)hInputBrush;
    }
    case WM_CTLCOLORLISTBOX: {
        HDC dc = (HDC)wp;
        SetTextColor(dc, COLOR_DARK_TEXT);
        SetBkColor(dc, COLOR_INPUT);
        return (LRESULT)hListBrush;
    }
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lp;
        if (dis && dis->CtlType == ODT_BUTTON) {
            draw_colored_button(dis);
            return TRUE;
        }
        break;
    }
    case WM_CREATE: {
        hMain = hwnd;
        hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        hBgBrush = CreateSolidBrush(COLOR_BG);
        hInputBrush = CreateSolidBrush(COLOR_INPUT);
        hListBrush = CreateSolidBrush(COLOR_INPUT);
        add_control("STATIC", "LIBRARY MANAGEMENT SYSTEM", SS_CENTER, 18, 12, 820, 28, 0);
        add_control("STATIC", "Book ID", 0, 20, 58, 80, 20, 0);
        hId = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL, 20, 80, 100, 25, hwnd, (HMENU)ID_ID, GetModuleHandle(NULL), NULL);
        add_control("STATIC", "Title", 0, 140, 58, 80, 20, 0);
        hTitle = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL, 140, 80, 210, 25, hwnd, (HMENU)ID_TITLE, GetModuleHandle(NULL), NULL);
        add_control("STATIC", "Author", 0, 370, 58, 80, 20, 0);
        hAuthor = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL, 370, 80, 180, 25, hwnd, (HMENU)ID_AUTHOR, GetModuleHandle(NULL), NULL);
        add_control("STATIC", "Year", 0, 570, 58, 60, 20, 0);
        hYear = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL, 570, 80, 80, 25, hwnd, (HMENU)ID_YEAR, GetModuleHandle(NULL), NULL);
        add_control("STATIC", "Total Copies", 0, 670, 58, 100, 20, 0);
        hCopies = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "1", WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL, 670, 80, 90, 25, hwnd, (HMENU)ID_COPIES, GetModuleHandle(NULL), NULL);
        add_control("BUTTON", "Add Book", BS_PUSHBUTTON, 20, 120, 100, 32, ID_ADD);
        add_control("BUTTON", "Update", BS_PUSHBUTTON, 130, 120, 90, 32, ID_UPDATE);
        add_control("BUTTON", "Delete", BS_PUSHBUTTON, 230, 120, 90, 32, ID_DELETE);
        add_control("BUTTON", "Issue Copy", BS_PUSHBUTTON, 330, 120, 100, 32,  ID_ISSUE);
        add_control("BUTTON", "Return Copy", BS_PUSHBUTTON, 440, 120, 100, 32, ID_RETURN);
        add_control("BUTTON", "Clear Form", BS_PUSHBUTTON, 550, 120, 100, 32, ID_CLEAR);
        add_control("STATIC", "Search title, author or ID:", 0, 20, 174, 180, 20, 0);
        hSearch = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL, 200, 170, 250, 25, hwnd, (HMENU)ID_SEARCH_EDIT, GetModuleHandle(NULL), NULL);
        add_control("BUTTON", "Search", BS_PUSHBUTTON, 460, 168, 90, 30, ID_SEARCH_BUTTON);
        add_control("BUTTON", "Show All", BS_PUSHBUTTON, 560, 168, 90, 30, ID_SHOWALL);
        hList = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", "", WS_CHILD|WS_VISIBLE|WS_VSCROLL|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT, 20, 215, 820, 270, hwnd, (HMENU)ID_LIST, GetModuleHandle(NULL), NULL);
        add_control("STATIC", "Select a row to load its details into the form. Issue/Return uses the Book ID field.", 0, 20, 493, 820, 22, 0);
        add_control("STATIC", "Ready.", 0, 20, 520, 820, 24, ID_STATUS);
        if (hFont) {
            HWND kids[] = {hId,hTitle,hAuthor,hYear,hCopies,hSearch,hList};
            for (size_t i=0;i<sizeof(kids)/sizeof(kids[0]);++i) SendMessage(kids[i], WM_SETFONT, (WPARAM)hFont, TRUE);
        }
        load_books();
        refresh_list(NULL);
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_ADD: add_book(); break;
        case ID_UPDATE: update_book(); break;
        case ID_DELETE: delete_book(); break;
        case ID_CLEAR: clear_fields(); set_status("Form cleared."); break;
        case ID_ISSUE: issue_return_book(1); break;
        case ID_RETURN: issue_return_book(0); break;
        case ID_SEARCH_BUTTON: {
            char q[128]; GetWindowTextA(hSearch, q, sizeof(q)); refresh_list(q); break;
        }
        case ID_SHOWALL: SetWindowTextA(hSearch, ""); refresh_list(NULL); break;
        case ID_LIST:
            if (HIWORD(wp) == LBN_DBLCLK) load_selected();
            break;
        }
        return 0;
    case WM_SIZE:
        /* This compact student project uses a fixed-size window. */
        return 0;
    case WM_DESTROY:
        free(books);
        books = NULL;
        if (hBgBrush) DeleteObject(hBgBrush);
        if (hInputBrush) DeleteObject(hInputBrush);
        if (hListBrush) DeleteObject(hListBrush);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine, int nShow) {
    (void)hPrev; (void)lpCmdLine;
    WNDCLASSA wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "LibraryManagementSystemWindow";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    if (!RegisterClassA(&wc)) {
        MessageBoxA(NULL, "Could not register application window.", "Startup error", MB_ICONERROR);
        return 1;
    }
    hMain = CreateWindowExA(0, wc.lpszClassName, "Library Management System - C Project",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 880, 610, NULL, NULL, hInst, NULL);
    if (!hMain) {
        MessageBoxA(NULL, "Could not create application window.", "Startup error", MB_ICONERROR);
        return 1;
    }
    ShowWindow(hMain, nShow);
    UpdateWindow(hMain);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
