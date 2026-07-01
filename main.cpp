#ifndef WINVER
#define WINVER 0x0601
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <string>
#include <regex>
#include <sstream>
#include <vector>
#include <fstream>
#include <iomanip>
#include <thread>

// Enable modern visual styles (Common Controls v6)
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// Control IDs
#define IDC_INPUT_EDIT            101
#define IDC_OUTPUT_EDIT           102
#define IDC_REMOVE_NEWLINES_CHECK 103
#define IDC_PROCESS_BUTTON        104
#define IDC_COPY_BUTTON           105
#define IDC_EXPORT_PDF_BUTTON     106
#define IDC_EXPORT_FILE_BUTTON    107
#define IDC_CLEAR_BUTTON          108
#define IDC_FETCH_URL_BUTTON      109
#define IDC_URL_EDIT              110

// Function Declarations
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK ButtonSubclassProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
std::wstring ProcessTranscript(const std::wstring& input, bool removeNewlines);
void TriggerProcessing(HWND hWndInput, HWND hWndOutput, HWND hWndCheckbox);
void CopyToClipboard(HWND hWnd, const std::wstring& text);
void ExportToFileText(HWND hWnd, const std::wstring& text);
void ExportToFilePDF(HWND hWnd, const std::wstring& text);
int ParseTimestampToSeconds(const std::wstring& tsStr);
std::vector<std::wstring> WordWrap(const std::wstring& text, size_t maxCharsLine);
std::string EscapePDFText(const std::wstring& wstr);
bool ExtractResource(int resourceId, const std::wstring& outputPath);
std::wstring GetHelperCommand(const std::wstring& escapedUrl, bool& outIsScript);
std::wstring FetchTranscriptFromURL(const std::wstring& urlOrId);

struct TranscriptLine {
    int timeInSeconds = -1;
    std::wstring cleanedText;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Initialize common controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);
    
    const wchar_t szClassName[] = L"YouTubeTranscribeExtractorWindow";
    
    WNDCLASSEXW wincl;
    wincl.hInstance = hInstance;
    wincl.lpszClassName = szClassName;
    wincl.lpfnWndProc = WndProc;
    wincl.style = CS_DBLCLKS;
    wincl.cbSize = sizeof(WNDCLASSEXW);
    
    wincl.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wincl.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
    wincl.hCursor = LoadCursor(NULL, IDC_ARROW);
    wincl.lpszMenuName = NULL;
    wincl.cbClsExtra = 0;
    wincl.cbWndExtra = 0;
    // Set background to a solid premium dark color
    wincl.hbrBackground = CreateSolidBrush(RGB(24, 24, 24));
    
    if (!RegisterClassExW(&wincl)) {
        return 0;
    }
    
    // Position window in center of screen
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int windowWidth = 820;
    int windowHeight = 650;
    int x = (screenWidth - windowWidth) / 2;
    int y = (screenHeight - windowHeight) / 2;
    
    HWND hWnd = CreateWindowExW(
        0,
        szClassName,
        L"YouTube Transcript Extractor & Cleaner",
        WS_OVERLAPPEDWINDOW,
        x,
        y,
        windowWidth,
        windowHeight,
        HWND_DESKTOP,
        NULL,
        hInstance,
        NULL
    );
    
    // Enable Immersive Dark Mode for the window frame/title bar on Windows 10/11
    BOOL useDarkMode = TRUE;
    DwmSetWindowAttribute(hWnd, 20, &useDarkMode, sizeof(useDarkMode)); // DWMWA_USE_IMMERSIVE_DARK_MODE = 20
    
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    
    MSG messages;
    while (GetMessage(&messages, NULL, 0, 0)) {
        TranslateMessage(&messages);
        DispatchMessage(&messages);
    }
    
    return static_cast<int>(messages.wParam);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    static HWND hWndUrlLabel = NULL;
    static HWND hWndUrlEdit = NULL;
    static HWND hWndInputLabel = NULL;
    static HWND hWndInputEdit = NULL;
    static HWND hWndOutputLabel = NULL;
    static HWND hWndOutputEdit = NULL;
    static HWND hWndCheckbox = NULL;
    static HWND hWndFetchBtn = NULL;
    static HWND hWndProcessBtn = NULL;
    static HWND hWndCopyBtn = NULL;
    static HWND hWndExportPdfBtn = NULL;
    static HWND hWndExportFileBtn = NULL;
    static HWND hWndClearBtn = NULL;
    static HFONT hFont = NULL;
    static HFONT hLabelFont = NULL;
    static HBRUSH hBgBrush = NULL;
    static HBRUSH hEditBgBrush = NULL;
    
    switch (message) {
        case WM_CREATE: {
            hBgBrush = CreateSolidBrush(RGB(24, 24, 24));
            hEditBgBrush = CreateSolidBrush(RGB(33, 33, 33));

            // Create UI Controls
            hWndUrlLabel = CreateWindowW(L"STATIC", L"YouTube Link / Video ID:", 
                WS_CHILD | WS_VISIBLE, 
                0, 0, 0, 0, hWnd, NULL, NULL, NULL);

            hWndUrlEdit = CreateWindowW(L"EDIT", L"", 
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 
                0, 0, 0, 0, hWnd, (HMENU)IDC_URL_EDIT, NULL, NULL);

            hWndInputLabel = CreateWindowW(L"STATIC", L"Raw Transcript (Paste here):", 
                WS_CHILD | WS_VISIBLE, 
                0, 0, 0, 0, hWnd, NULL, NULL, NULL);
                
            hWndInputEdit = CreateWindowW(L"EDIT", L"", 
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | WS_VSCROLL | ES_AUTOVSCROLL | ES_WANTRETURN, 
                0, 0, 0, 0, hWnd, (HMENU)IDC_INPUT_EDIT, NULL, NULL);
                
            hWndCheckbox = CreateWindowW(L"BUTTON", L"Format as paragraph", 
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 
                0, 0, 0, 0, hWnd, (HMENU)IDC_REMOVE_NEWLINES_CHECK, NULL, NULL);
            SendMessage(hWndCheckbox, BM_SETCHECK, BST_CHECKED, 0); // Check by default
            
            // Buttons with BS_OWNERDRAW style for custom modern drawing
            hWndFetchBtn = CreateWindowW(L"BUTTON", L"Fetch URL", 
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW, 
                0, 0, 0, 0, hWnd, (HMENU)IDC_FETCH_URL_BUTTON, NULL, NULL);

            hWndProcessBtn = CreateWindowW(L"BUTTON", L"Clean Text", 
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW, 
                0, 0, 0, 0, hWnd, (HMENU)IDC_PROCESS_BUTTON, NULL, NULL);
                
            hWndCopyBtn = CreateWindowW(L"BUTTON", L"Copy Clean Text", 
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW, 
                0, 0, 0, 0, hWnd, (HMENU)IDC_COPY_BUTTON, NULL, NULL);
                
            hWndExportPdfBtn = CreateWindowW(L"BUTTON", L"Export PDF", 
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW, 
                0, 0, 0, 0, hWnd, (HMENU)IDC_EXPORT_PDF_BUTTON, NULL, NULL);

            hWndExportFileBtn = CreateWindowW(L"BUTTON", L"Export Text", 
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW, 
                0, 0, 0, 0, hWnd, (HMENU)IDC_EXPORT_FILE_BUTTON, NULL, NULL);
                
            hWndClearBtn = CreateWindowW(L"BUTTON", L"Clear All", 
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW, 
                0, 0, 0, 0, hWnd, (HMENU)IDC_CLEAR_BUTTON, NULL, NULL);
                
            hWndOutputLabel = CreateWindowW(L"STATIC", L"Neat Cleaned Transcript:", 
                WS_CHILD | WS_VISIBLE, 
                0, 0, 0, 0, hWnd, NULL, NULL, NULL);
                
            hWndOutputEdit = CreateWindowW(L"EDIT", L"", 
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | WS_VSCROLL | ES_AUTOVSCROLL | ES_READONLY, 
                0, 0, 0, 0, hWnd, (HMENU)IDC_OUTPUT_EDIT, NULL, NULL);
                
            // Subclass buttons to track hover events
            SetWindowSubclass(hWndFetchBtn, ButtonSubclassProc, 0, 0);
            SetWindowSubclass(hWndProcessBtn, ButtonSubclassProc, 0, 0);
            SetWindowSubclass(hWndCopyBtn, ButtonSubclassProc, 0, 0);
            SetWindowSubclass(hWndExportPdfBtn, ButtonSubclassProc, 0, 0);
            SetWindowSubclass(hWndExportFileBtn, ButtonSubclassProc, 0, 0);
            SetWindowSubclass(hWndClearBtn, ButtonSubclassProc, 0, 0);

            // Load fonts (Segoe UI is standard, clean, and modern on Windows)
            hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
                
            hLabelFont = CreateFontW(16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            
            SendMessage(hWndUrlLabel, WM_SETFONT, (WPARAM)hLabelFont, TRUE);
            SendMessage(hWndUrlEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hWndInputLabel, WM_SETFONT, (WPARAM)hLabelFont, TRUE);
            SendMessage(hWndInputEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hWndCheckbox, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hWndFetchBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hWndProcessBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hWndCopyBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hWndExportPdfBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hWndExportFileBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hWndClearBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hWndOutputLabel, WM_SETFONT, (WPARAM)hLabelFont, TRUE);
            SendMessage(hWndOutputEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
            
            break;
        }
        case WM_SIZE: {
            int W = LOWORD(lParam);
            int H = HIWORD(lParam);
            
            // Layout calculations with 10px margins
            // Top URL bar
            MoveWindow(hWndUrlLabel, 10, 12, 170, 20, TRUE);
            MoveWindow(hWndUrlEdit, 185, 9, W - 185 - 10 - 95 - 10, 24, TRUE);
            MoveWindow(hWndFetchBtn, W - 10 - 95, 6, 95, 30, TRUE);
            
            int E_H = (H - 145) / 2;
            if (E_H < 50) E_H = 50;
            
            MoveWindow(hWndInputLabel, 10, 45, W - 20, 20, TRUE);
            MoveWindow(hWndInputEdit, 10, 65, W - 20, E_H, TRUE);
            
            int rowY = 65 + E_H + 10;
            
            int chkW = 160;
            MoveWindow(hWndCheckbox, 10, rowY + 3, chkW, 24, TRUE);
            
            int btnW_Clear = 75;
            int btnW_ExportFile = 95;
            int btnW_ExportPdf = 95;
            int btnW_Copy = 100;
            int btnW_Process = 90;
            
            int xClear = W - 10 - btnW_Clear;
            int xExportFile = xClear - 10 - btnW_ExportFile;
            int xExportPdf = xExportFile - 10 - btnW_ExportPdf;
            int xCopy = xExportPdf - 10 - btnW_Copy;
            int xProcess = xCopy - 10 - btnW_Process;
            
            // Layout clamping for smaller windows
            if (xProcess < chkW + 15) {
                xProcess = chkW + 15;
                xCopy = xProcess + btnW_Process + 5;
                xExportPdf = xCopy + btnW_Copy + 5;
                xExportFile = xExportPdf + btnW_ExportPdf + 5;
                xClear = xExportFile + btnW_ExportFile + 5;
            }
            
            MoveWindow(hWndClearBtn, xClear, rowY, btnW_Clear, 30, TRUE);
            MoveWindow(hWndExportFileBtn, xExportFile, rowY, btnW_ExportFile, 30, TRUE);
            MoveWindow(hWndExportPdfBtn, xExportPdf, rowY, btnW_ExportPdf, 30, TRUE);
            MoveWindow(hWndCopyBtn, xCopy, rowY, btnW_Copy, 30, TRUE);
            MoveWindow(hWndProcessBtn, xProcess, rowY, btnW_Process, 30, TRUE);
            
            int label2Y = rowY + 30 + 10;
            MoveWindow(hWndOutputLabel, 10, label2Y, W - 20, 20, TRUE);
            MoveWindow(hWndOutputEdit, 10, label2Y + 20, W - 20, E_H, TRUE);
            
            break;
        }
        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, RGB(240, 240, 240));
            SetBkColor(hdc, RGB(33, 33, 33));
            return (INT_PTR)hEditBgBrush;
        }
        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            HWND hwndChild = (HWND)lParam;
            
            if (hwndChild == hWndOutputEdit) {
                SetTextColor(hdc, RGB(220, 220, 220));
                SetBkColor(hdc, RGB(33, 33, 33));
                return (INT_PTR)hEditBgBrush;
            } else {
                SetTextColor(hdc, RGB(220, 220, 220));
                SetBkMode(hdc, TRANSPARENT);
                return (INT_PTR)hBgBrush;
            }
        }
        case WM_DRAWITEM: {
            DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
            if (dis->CtlType == ODT_BUTTON) {
                // Get button label
                wchar_t buttonText[128];
                GetWindowTextW(dis->hwndItem, buttonText, 128);
                
                // Track states
                BOOL isHovered = (GetWindowLongPtrW(dis->hwndItem, GWLP_USERDATA) == 1);
                BOOL isPressed = (dis->itemState & ODS_SELECTED);
                
                COLORREF colorBg;
                COLORREF colorText = RGB(245, 245, 245);
                
                if (dis->CtlID == IDC_PROCESS_BUTTON) {
                    // Accent button (Vibrant Royal Blue)
                    if (isPressed) {
                        colorBg = RGB(10, 85, 200);
                    } else if (isHovered) {
                        colorBg = RGB(40, 130, 255);
                    } else {
                        colorBg = RGB(13, 110, 253);
                    }
                } else if (dis->CtlID == IDC_FETCH_URL_BUTTON) {
                    // Accent button (Vibrant Forest Green)
                    if (isPressed) {
                        colorBg = RGB(30, 130, 50);
                    } else if (isHovered) {
                        colorBg = RGB(50, 190, 85);
                    } else {
                        colorBg = RGB(40, 167, 69);
                    }
                } else {
                    // Standard buttons (Dark Slate Grey)
                    if (isPressed) {
                        colorBg = RGB(35, 35, 35);
                    } else if (isHovered) {
                        colorBg = RGB(75, 75, 75);
                    } else {
                        colorBg = RGB(55, 55, 55);
                    }
                }
                
                HDC hDC = dis->hDC;
                RECT rc = dis->rcItem;
                
                // Draw rounded rectangle background (6px radius)
                HRGN hRgn = CreateRoundRectRgn(rc.left, rc.top, rc.right + 1, rc.bottom + 1, 6, 6);
                HBRUSH hBrush = CreateSolidBrush(colorBg);
                FillRgn(hDC, hRgn, hBrush);
                DeleteObject(hBrush);
                DeleteObject(hRgn);
                
                // Print centered text
                SetTextColor(hDC, colorText);
                SetBkMode(hDC, TRANSPARENT);
                HGDIOBJ oldFont = SelectObject(hDC, hFont);
                DrawTextW(hDC, buttonText, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SelectObject(hDC, oldFont);
                
                return TRUE;
            }
            break;
        }
        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            int wmEvent = HIWORD(wParam);
            
            if (wmId == IDC_INPUT_EDIT && wmEvent == EN_CHANGE) {
                // Real-time transcript processing (only if not currently loading)
                int len = GetWindowTextLengthW(hWndInputEdit);
                if (len > 0) {
                    std::vector<wchar_t> testBuf(32);
                    GetWindowTextW(hWndInputEdit, testBuf.data(), 32);
                    std::wstring firstChars(testBuf.data());
                    if (firstChars.rfind(L"Fetching transcript...", 0) != 0) {
                        TriggerProcessing(hWndInputEdit, hWndOutputEdit, hWndCheckbox);
                    }
                } else {
                    TriggerProcessing(hWndInputEdit, hWndOutputEdit, hWndCheckbox);
                }
            }
            else if (wmId == IDC_REMOVE_NEWLINES_CHECK && wmEvent == BN_CLICKED) {
                // Update format in real-time when formatting preference changes
                TriggerProcessing(hWndInputEdit, hWndOutputEdit, hWndCheckbox);
            }
            else if (wmId == IDC_FETCH_URL_BUTTON) {
                int len = GetWindowTextLengthW(hWndUrlEdit);
                if (len == 0) {
                    MessageBoxW(hWnd, L"Please enter a YouTube video URL or Video ID first in the URL field!", L"Notice", MB_OK | MB_ICONWARNING);
                    break;
                }
                
                std::vector<wchar_t> buf(len + 1);
                GetWindowTextW(hWndUrlEdit, buf.data(), len + 1);
                std::wstring urlOrId(buf.data());
                
                // Disable controls
                EnableWindow(hWndFetchBtn, FALSE);
                EnableWindow(hWndProcessBtn, FALSE);
                SetWindowTextW(hWndInputEdit, L"Fetching transcript... Please wait.");
                SetWindowTextW(hWndOutputEdit, L"");
                
                // Spawn background thread to fetch URL without freezing GUI
                std::thread t([hWnd, urlOrId]() {
                    std::wstring result = FetchTranscriptFromURL(urlOrId);
                    
                    if (result.rfind(L"ERROR:", 0) == 0) {
                        // Error, notify user and clear input
                        MessageBoxW(hWnd, result.c_str(), L"Fetch Error", MB_OK | MB_ICONERROR);
                        SendMessageW(hWndInputEdit, WM_SETTEXT, 0, (LPARAM)L"");
                    } else {
                        // Success, insert transcript
                        SendMessageW(hWndInputEdit, WM_SETTEXT, 0, (LPARAM)result.c_str());
                    }
                    
                    // Re-enable controls
                    SendMessageW(hWndFetchBtn, WM_ENABLE, TRUE, 0);
                    SendMessageW(hWndProcessBtn, WM_ENABLE, TRUE, 0);
                    
                    // Trigger processing
                    PostMessageW(hWnd, WM_COMMAND, MAKEWPARAM(IDC_PROCESS_BUTTON, BN_CLICKED), 0);
                });
                t.detach();
            }
            else if (wmId == IDC_PROCESS_BUTTON) {
                TriggerProcessing(hWndInputEdit, hWndOutputEdit, hWndCheckbox);
            }
            else if (wmId == IDC_COPY_BUTTON) {
                int len = GetWindowTextLengthW(hWndOutputEdit);
                std::vector<wchar_t> buffer(len + 1);
                GetWindowTextW(hWndOutputEdit, buffer.data(), len + 1);
                CopyToClipboard(hWnd, std::wstring(buffer.data()));
            }
            else if (wmId == IDC_EXPORT_PDF_BUTTON) {
                int len = GetWindowTextLengthW(hWndOutputEdit);
                std::vector<wchar_t> buffer(len + 1);
                GetWindowTextW(hWndOutputEdit, buffer.data(), len + 1);
                ExportToFilePDF(hWnd, std::wstring(buffer.data()));
            }
            else if (wmId == IDC_EXPORT_FILE_BUTTON) {
                int len = GetWindowTextLengthW(hWndOutputEdit);
                std::vector<wchar_t> buffer(len + 1);
                GetWindowTextW(hWndOutputEdit, buffer.data(), len + 1);
                ExportToFileText(hWnd, std::wstring(buffer.data()));
            }
            else if (wmId == IDC_CLEAR_BUTTON) {
                SetWindowTextW(hWndUrlEdit, L"");
                SetWindowTextW(hWndInputEdit, L"");
                SetWindowTextW(hWndOutputEdit, L"");
                SetFocus(hWndUrlEdit);
            }
            break;
        }
        case WM_GETMINMAXINFO: {
            // Enforce minimum window size so elements are never hidden or scrambled
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;
            mmi->ptMinTrackSize.x = 800;
            mmi->ptMinTrackSize.y = 450;
            break;
        }
        case WM_DESTROY: {
            if (hFont) DeleteObject(hFont);
            if (hLabelFont) DeleteObject(hLabelFont);
            if (hBgBrush) DeleteObject(hBgBrush);
            if (hEditBgBrush) DeleteObject(hEditBgBrush);
            PostQuitMessage(0);
            break;
        }
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Subclass window procedure for managing button hover redraws
LRESULT CALLBACK ButtonSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch (uMsg) {
        case WM_MOUSEMOVE: {
            TRACKMOUSEEVENT tme;
            tme.cbSize = sizeof(TRACKMOUSEEVENT);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hWnd;
            if (TrackMouseEvent(&tme)) {
                LONG_PTR hover = GetWindowLongPtrW(hWnd, GWLP_USERDATA);
                if (hover == 0) {
                    SetWindowLongPtrW(hWnd, GWLP_USERDATA, 1);
                    InvalidateRect(hWnd, NULL, TRUE);
                }
            }
            break;
        }
        case WM_MOUSELEAVE: {
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
            InvalidateRect(hWnd, NULL, TRUE);
            break;
        }
        case WM_NCDESTROY: {
            RemoveWindowSubclass(hWnd, ButtonSubclassProc, uIdSubclass);
            break;
        }
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

std::wstring ProcessTranscript(const std::wstring& input, bool removeNewlines) {
    // Enhanced regex to match starting timestamps (e.g. "1:09") followed by arbitrary sequences of
    // duration numbers, time units (minutes, seconds, etc. in multiple languages), and connectives (e.g. "1 Minute, 9 Sekunden").
    static const std::wregex timestamp_regex(
        L"\\b\\d{1,2}:\\d{2}(?::\\d{2})?(?:\\s*\\d*\\s*(?:Minuten|Minute|minutes|minute|minuten|minuut|mins|min|Sekunden|Sekunde|seconds|second|secondes|seconde|secondi|secondo|segundos|segundo|secs|sec|Stunden|Stunde|hours|hour|horas|hora|heures|heure|uren|uur|hrs|hr|und|and|et|y|,|(?:s|S|m|M|h|H)(?=[^a-zA-Z]|$))\\s*)*\\s*"
    );
    static const std::wregex time_only_regex(L"\\b\\d{1,2}:\\d{2}(?::\\d{2})?");
    static const std::wregex brackets_regex(L"\\[[^\\]]*\\]");
    
    std::wstringstream ss(input);
    std::wstring line;
    std::vector<TranscriptLine> parsedLines;
    int lastSeenTime = -1;
    
    while (std::getline(ss, line)) {
        std::wsmatch match;
        int timeSecs = -1;
        std::wstring textAfter = line;
        
        if (std::regex_search(line, match, timestamp_regex)) {
            std::wstring timestampBlock = match.str();
            textAfter = line.substr(match.position() + match.length());
            
            std::wsmatch timeMatch;
            if (std::regex_search(timestampBlock, timeMatch, time_only_regex)) {
                timeSecs = ParseTimestampToSeconds(timeMatch.str());
                if (timeSecs != -1) {
                    lastSeenTime = timeSecs;
                }
            }
        }
        
        if (timeSecs == -1) {
            timeSecs = lastSeenTime;
        }
        
        // Strip bracketed comments/annotations
        std::wstring cleanedText = std::regex_replace(textAfter, brackets_regex, L"");
        
        // Trim leading and trailing spaces
        size_t start = cleanedText.find_first_not_of(L" \t\r\n");
        if (start != std::wstring::npos) {
            size_t end = cleanedText.find_last_not_of(L" \t\r\n");
            cleanedText = cleanedText.substr(start, end - start + 1);
        } else {
            cleanedText.clear();
        }
        
        // Collapse multiple spaces inside the line
        if (!cleanedText.empty()) {
            std::wstring collapsed;
            bool inSpace = false;
            for (wchar_t c : cleanedText) {
                if (c == L' ' || c == L'\t') {
                    if (!inSpace) {
                        collapsed += L' ';
                        inSpace = true;
                    }
                } else {
                    collapsed += c;
                    inSpace = false;
                }
            }
            cleanedText = collapsed;
        }
        
        // Only keep lines that are not empty after stripping comments/annotations
        if (!cleanedText.empty()) {
            TranscriptLine tl;
            tl.timeInSeconds = timeSecs;
            tl.cleanedText = cleanedText;
            parsedLines.push_back(tl);
        }
    }
    
    // Assemble formatted string with paragraph breaks where the pause > 10s and last sentence ends with a full stop
    std::wstring formatted;
    for (size_t i = 0; i < parsedLines.size(); ++i) {
        if (i > 0) {
            const auto& prev = parsedLines[i - 1];
            const auto& curr = parsedLines[i];
            
            bool isParagraphBreak = false;
            if (prev.timeInSeconds != -1 && curr.timeInSeconds != -1) {
                int timeDiff = curr.timeInSeconds - prev.timeInSeconds;
                if (timeDiff > 10) {
                    wchar_t lastChar = prev.cleanedText.back();
                    if (lastChar == L'.' || lastChar == L'?' || lastChar == L'!') {
                        isParagraphBreak = true;
                    }
                }
            }
            
            if (removeNewlines) {
                if (isParagraphBreak) {
                    formatted += L"\r\n\r\n";
                } else {
                    formatted += L" ";
                }
            } else {
                if (isParagraphBreak) {
                    formatted += L"\r\n\r\n";
                } else {
                    formatted += L"\r\n";
                }
            }
        }
        formatted += parsedLines[i].cleanedText;
    }
    
    return formatted;
}

int ParseTimestampToSeconds(const std::wstring& tsStr) {
    std::vector<int> parts;
    std::wstringstream wss(tsStr);
    std::wstring part;
    while (std::getline(wss, part, L':')) {
        try {
            parts.push_back(std::stoi(part));
        } catch (...) {
            return -1;
        }
    }
    if (parts.size() == 2) {
        return parts[0] * 60 + parts[1];
    } else if (parts.size() == 3) {
        return parts[0] * 3600 + parts[1] * 60 + parts[2];
    }
    return -1;
}

void TriggerProcessing(HWND hWndInput, HWND hWndOutput, HWND hWndCheckbox) {
    int len = GetWindowTextLengthW(hWndInput);
    if (len == 0) {
        SetWindowTextW(hWndOutput, L"");
        return;
    }
    
    std::vector<wchar_t> buffer(len + 1);
    GetWindowTextW(hWndInput, buffer.data(), len + 1);
    std::wstring inputStr(buffer.data());
    
    bool removeNewlines = (SendMessage(hWndCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED);
    
    std::wstring cleaned = ProcessTranscript(inputStr, removeNewlines);
    
    SetWindowTextW(hWndOutput, cleaned.c_str());
}

void CopyToClipboard(HWND hWnd, const std::wstring& text) {
    if (text.empty()) {
        MessageBoxW(hWnd, L"No text to copy!", L"Notice", MB_OK | MB_ICONWARNING);
        return;
    }
    if (!OpenClipboard(hWnd)) {
        MessageBoxW(hWnd, L"Failed to open clipboard.", L"Error", MB_OK | MB_ICONERROR);
        return;
    }
    EmptyClipboard();
    
    size_t size = (text.length() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
    if (hMem) {
        wchar_t* pMem = static_cast<wchar_t*>(GlobalLock(hMem));
        if (pMem) {
            wcscpy_s(pMem, text.length() + 1, text.c_str());
            GlobalUnlock(hMem);
            if (!SetClipboardData(CF_UNICODETEXT, hMem)) {
                GlobalFree(hMem);
            }
        }
    }
    CloseClipboard();
    
    MessageBoxW(hWnd, L"Cleaned text copied to clipboard!", L"Success", MB_OK | MB_ICONINFORMATION);
}

void ExportToFileText(HWND hWnd, const std::wstring& text) {
    if (text.empty()) {
        MessageBoxW(hWnd, L"No text to export!", L"Notice", MB_OK | MB_ICONWARNING);
        return;
    }
    
    OPENFILENAMEW ofn;
    wchar_t szFile[MAX_PATH] = L"transcript_cleaned.txt";
    
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
    ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = L"txt";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
    
    if (GetSaveFileNameW(&ofn)) {
        std::ofstream outFile(ofn.lpstrFile, std::ios::out | std::ios::binary);
        if (outFile.is_open()) {
            // Write UTF-8 BOM so Notepad/editors recognize encoding correctly
            const unsigned char bom[] = {0xEF, 0xBB, 0xBF};
            outFile.write(reinterpret_cast<const char*>(bom), 3);
            
            // Convert wchar_t (UTF-16) to char (UTF-8)
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.length()), NULL, 0, NULL, NULL);
            std::string strTo(size_needed, 0);
            WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.length()), &strTo[0], size_needed, NULL, NULL);
            
            outFile.write(strTo.c_str(), strTo.length());
            outFile.close();
            MessageBoxW(hWnd, L"File exported successfully!", L"Success", MB_OK | MB_ICONINFORMATION);
        } else {
            MessageBoxW(hWnd, L"Failed to write to file.", L"Error", MB_OK | MB_ICONERROR);
        }
    }
}

std::vector<std::wstring> WordWrap(const std::wstring& text, size_t maxCharsLine) {
    std::vector<std::wstring> lines;
    std::wstringstream wss(text);
    std::wstring paragraph;
    
    while (std::getline(wss, paragraph)) {
        if (paragraph.empty()) {
            lines.push_back(L"");
            continue;
        }
        
        std::wstringstream pwss(paragraph);
        std::wstring word;
        std::wstring currentLine;
        
        while (pwss >> word) {
            if (currentLine.empty()) {
                currentLine = word;
            } else if (currentLine.length() + 1 + word.length() <= maxCharsLine) {
                currentLine += L" " + word;
            } else {
                lines.push_back(currentLine);
                currentLine = word;
            }
        }
        if (!currentLine.empty()) {
            lines.push_back(currentLine);
        }
    }
    return lines;
}

std::string EscapePDFText(const std::wstring& wstr) {
    std::string result;
    for (wchar_t wc : wstr) {
        // Map common Unicode typography to Latin-1/ASCII compatibility
        wchar_t mapped = wc;
        if (wc == L'\u2019' || wc == L'\u2018') mapped = L'\'';
        else if (wc == L'\u201C' || wc == L'\u201D') mapped = L'"';
        else if (wc == L'\u2013' || wc == L'\u2014') mapped = L'-';
        else if (wc == L'\u2026') {
            result += "...";
            continue;
        }
        
        char c;
        if (mapped < 256) {
            c = (char)mapped;
        } else {
            c = '?'; // fallback for non-supported characters
        }
        
        if (c == '(' || c == ')' || c == '\\') {
            result += '\\';
            result += c;
        } else {
            result += c;
        }
    }
    return result;
}

void ExportToFilePDF(HWND hWnd, const std::wstring& text) {
    if (text.empty()) {
        MessageBoxW(hWnd, L"No text to export!", L"Notice", MB_OK | MB_ICONWARNING);
        return;
    }
    
    OPENFILENAMEW ofn;
    wchar_t szFile[MAX_PATH] = L"transcript_cleaned.pdf";
    
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
    ofn.lpstrFilter = L"PDF Files (*.pdf)\0*.pdf\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = L"pdf";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
    
    if (GetSaveFileNameW(&ofn)) {
        std::ofstream outFile(ofn.lpstrFile, std::ios::out | std::ios::binary);
        if (!outFile.is_open()) {
            MessageBoxW(hWnd, L"Failed to create PDF file.", L"Error", MB_OK | MB_ICONERROR);
            return;
        }
        
        // Wrap text to fit margins (~82 characters per line)
        std::vector<std::wstring> lines = WordWrap(text, 82);
        
        // PDF metrics
        double pageHeight = 842;
        double pageWidth = 595;
        double marginX = 50;
        double marginY = 50;
        double lineSpacing = 15;
        double contentHeight = pageHeight - 2 * marginY;
        int linesPerPage = (int)(contentHeight / lineSpacing);
        
        // Paginate text lines
        std::vector<std::vector<std::wstring>> pages;
        std::vector<std::wstring> currentPage;
        
        for (const auto& line : lines) {
            currentPage.push_back(line);
            // Leave 4 lines height for title on the first page
            int limit = pages.empty() ? (linesPerPage - 4) : linesPerPage;
            if (currentPage.size() >= limit) {
                pages.push_back(currentPage);
                currentPage.clear();
            }
        }
        if (!currentPage.empty() || pages.empty()) {
            pages.push_back(currentPage);
        }
        
        // Generate PDF
        std::stringstream pdf;
        std::vector<size_t> objOffsets;
        
        auto writeObjHeader = [&](int id) {
            objOffsets.push_back(pdf.tellp());
            pdf << id << " 0 obj\r\n";
        };
        
        // Write PDF Header
        pdf << "%PDF-1.4\r\n";
        
        int objCatalog = 1;
        int objPagesParent = 2;
        int objFontRegular = 3;
        int objFontBold = 4;
        
        // Catalog
        writeObjHeader(objCatalog);
        pdf << "<< /Type /Catalog /Pages " << objPagesParent << " 0 R >>\r\nendobj\r\n";
        
        // Helvetica Font descriptor (regular)
        writeObjHeader(objFontRegular);
        pdf << "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>\r\nendobj\r\n";
        
        // Helvetica Font descriptor (bold)
        writeObjHeader(objFontBold);
        pdf << "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold /Encoding /WinAnsiEncoding >>\r\nendobj\r\n";
        
        int pageCount = (int)pages.size();
        int firstPageObj = 5;
        
        // Pages Object (parent catalog)
        writeObjHeader(objPagesParent);
        pdf << "<< /Type /Pages /Kids [";
        for (int i = 0; i < pageCount; ++i) {
            pdf << (firstPageObj + 2 * i) << " 0 R ";
        }
        pdf << "] /Count " << pageCount << " >>\r\nendobj\r\n";
        
        // Render pages
        for (int i = 0; i < pageCount; ++i) {
            int pageObjId = firstPageObj + 2 * i;
            int contentObjId = pageObjId + 1;
            
            // Page Descriptor Object
            writeObjHeader(pageObjId);
            pdf << "<< /Type /Page /Parent " << objPagesParent << " 0 R\r\n"
                << "   /MediaBox [0 0 595 842]\r\n"
                << "   /Resources << /Font << /F1 " << objFontRegular << " 0 R /F2 " << objFontBold << " 0 R >> >>\r\n"
                << "   /Contents " << contentObjId << " 0 R >>\r\nendobj\r\n";
            
            // Page Content stream object
            std::stringstream streamContent;
            streamContent << "BT\r\n";
            
            double currentY = pageHeight - marginY;
            
            if (i == 0) {
                // Header Title on Page 1
                streamContent << "/F2 16 Tf\r\n"
                              << "18 TL\r\n"
                              << "50 " << (currentY - 20) << " Td\r\n"
                              << "(" << EscapePDFText(L"YouTube Transcript Export") << ") Tj T*\r\n";
                currentY -= 40;
                
                // Metadata line
                streamContent << "/F1 9 Tf\r\n"
                              << "11 TL\r\n"
                              << "(" << EscapePDFText(L"Generated by YouTube Transcript Extractor") << ") Tj T*\r\n";
                currentY -= 20;
                
                // Horizontal divider line
                streamContent << "ET\r\n"
                              << "0.5 w\r\n"
                              << "50 " << currentY << " m 545 " << currentY << " l S\r\n"
                              << "BT\r\n";
                currentY -= 25;
                
                streamContent << "/F1 11 Tf\r\n"
                              << "15 TL\r\n"
                              << "50 " << currentY << " Td\r\n";
            } else {
                // Normal pages start immediately at top
                streamContent << "/F1 11 Tf\r\n"
                              << "15 TL\r\n"
                              << "50 " << currentY << " Td\r\n";
            }
            
            // Output lines
            for (const auto& line : pages[i]) {
                if (line.empty()) {
                    // Empty paragraph break spacing
                    streamContent << "0 -15 Td\r\n";
                } else {
                    streamContent << "(" << EscapePDFText(line) << ") Tj T*\r\n";
                }
            }
            
            // Draw centered footer page number
            std::wstring pageNumStr = L"Page " + std::to_wstring(i + 1) + L" of " + std::to_wstring(pageCount);
            double pageNumX = 297.0 - (pageNumStr.length() * 2.5); // Center calculation based on average character width
            
            streamContent << "ET\r\n"
                          << "BT\r\n"
                          << "/F1 9 Tf\r\n"
                          << pageNumX << " " << (marginY - 15) << " Td\r\n"
                          << "(" << EscapePDFText(pageNumStr) << ") Tj\r\n"
                          << "ET\r\n";
            
            std::string streamStr = streamContent.str();
            
            writeObjHeader(contentObjId);
            pdf << "<< /Length " << streamStr.length() << " >>\r\nstream\r\n"
                << streamStr << "endstream\r\nendobj\r\n";
        }
        
        // Write cross-reference table (xref)
        size_t xrefPos = pdf.tellp();
        int totalObjects = firstPageObj + 2 * pageCount - 1;
        
        pdf << "xref\r\n"
            << "0 " << (totalObjects + 1) << "\r\n"
            << "0000000000 65535 f\r\n";
        
        for (int i = 0; i < totalObjects; ++i) {
            std::ostringstream oss;
            oss << std::setw(10) << std::setfill('0') << objOffsets[i] << " 00000 n\r\n";
            pdf << oss.str();
        }
        
        // Write trailer catalog root
        pdf << "trailer\r\n"
            << "<< /Size " << (totalObjects + 1) << "\r\n"
            << "   /Root " << objCatalog << " 0 R >>\r\n"
            << "startxref\r\n"
            << xrefPos << "\r\n"
            << "%%EOF\r\n";
        
        std::string pdfData = pdf.str();
        outFile.write(pdfData.c_str(), pdfData.length());
        outFile.close();
        
        MessageBoxW(hWnd, L"Transcript exported as PDF successfully!", L"Success", MB_OK | MB_ICONINFORMATION);
    }
}

bool ExtractResource(int resourceId, const std::wstring& outputPath) {
    HRSRC hRes = FindResourceW(NULL, MAKEINTRESOURCEW(resourceId), (LPCWSTR)RT_RCDATA);
    if (!hRes) return false;
    
    HGLOBAL hData = LoadResource(NULL, hRes);
    if (!hData) return false;
    
    LPVOID pData = LockResource(hData);
    DWORD size = SizeofResource(NULL, hRes);
    if (!pData || size == 0) return false;
    
    std::ofstream outFile(outputPath.c_str(), std::ios::out | std::ios::binary);
    if (!outFile.is_open()) return false;
    
    outFile.write(reinterpret_cast<const char*>(pData), size);
    outFile.close();
    return true;
}

std::wstring GetHelperCommand(const std::wstring& escapedUrl, bool& outIsScript) {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring exeDir(exePath);
    size_t lastSlash = exeDir.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        exeDir = exeDir.substr(0, lastSlash);
    }
    
    // 1. Check if get_transcript.py is in the same directory (development flow)
    std::wstring pathSame = exeDir + L"\\get_transcript.py";
    DWORD attribSame = GetFileAttributesW(pathSame.c_str());
    if (attribSame != INVALID_FILE_ATTRIBUTES && !(attribSame & FILE_ATTRIBUTE_DIRECTORY)) {
        outIsScript = true;
        return L"py \"" + pathSame + L"\" \"" + escapedUrl + L"\"";
    }
    
    // 2. Check parent directory (for cmake-build-debug builds in CLion)
    std::wstring pathParent = exeDir + L"\\..\\get_transcript.py";
    DWORD attribParent = GetFileAttributesW(pathParent.c_str());
    if (attribParent != INVALID_FILE_ATTRIBUTES && !(attribParent & FILE_ATTRIBUTE_DIRECTORY)) {
        outIsScript = true;
        return L"py \"" + pathParent + L"\" \"" + escapedUrl + L"\"";
    }
    
    // 3. Fallback to extracting the embedded get_transcript.exe from resources
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    std::wstring helperPath = std::wstring(tempPath) + L"YouTubeTranscribeExtractor_get_transcript.exe";
    
    bool exists = false;
    DWORD attribTemp = GetFileAttributesW(helperPath.c_str());
    if (attribTemp != INVALID_FILE_ATTRIBUTES && !(attribTemp & FILE_ATTRIBUTE_DIRECTORY)) {
        exists = true;
    }
    
    if (!exists) {
        // Try to extract
        ExtractResource(101, helperPath);
    }
    
    outIsScript = false;
    return L"\"" + helperPath + L"\" \"" + escapedUrl + L"\"";
}

std::wstring FetchTranscriptFromURL(const std::wstring& urlOrId) {
    // Sanitize/escape input to prevent command injections
    std::wstring escaped = L"";
    for (wchar_t c : urlOrId) {
        if (c == L'"' || c == L'\\' || c == L'&' || c == L'|' || c == L'>' || c == L'<') {
            // skip shell control tokens
        } else {
            escaped += c;
        }
    }
    
    bool isScript = false;
    std::wstring cmd = L"\"" + GetHelperCommand(escaped, isScript) + L" 2>&1\"";
    
    // Launch process and read raw UTF-8 bytes from the pipe
    FILE* fp = _wpopen(cmd.c_str(), L"r");
    if (!fp) {
        if (isScript) {
            return L"ERROR: Failed to launch script. Make sure Python and youtube-transcript-api are installed.";
        } else {
            return L"ERROR: Failed to launch embedded helper executable.";
        }
    }
    
    std::string outputBytes;
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fp)) {
        outputBytes += buffer;
    }
    
    int exitCode = _pclose(fp);
    
    // Convert UTF-8 bytes to std::wstring
    std::wstring output = L"";
    if (!outputBytes.empty()) {
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, outputBytes.c_str(), static_cast<int>(outputBytes.length()), NULL, 0);
        if (size_needed > 0) {
            output.resize(size_needed);
            MultiByteToWideChar(CP_UTF8, 0, outputBytes.c_str(), static_cast<int>(outputBytes.length()), &output[0], size_needed);
        }
    }
    
    if (exitCode != 0) {
        if (output.find(L"ERROR:") != std::wstring::npos) {
            return output;
        }
        return L"ERROR: execution failed.\n\nCommand:\n" + cmd + L"\n\nExit code: " + std::to_wstring(exitCode) + L"\n\nOutput Details:\n" + output;
    }
    
    return output;
}