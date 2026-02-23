#include "overlay_window.h"
#include "renderer.h"
#include "common/logger.h"
#include "common/utils.h"
#include <windowsx.h>
#include <algorithm>

namespace clipx {

OverlayWindow::OverlayWindow() = default;

OverlayWindow::~OverlayWindow() {
    if (m_memDC) {
        SelectObject(m_memDC, m_oldBitmap);
        DeleteObject(m_memBitmap);
        DeleteDC(m_memDC);
    }
    if (m_font) DeleteObject(m_font);
    if (m_fontBold) DeleteObject(m_fontBold);
    if (m_fontSmall) DeleteObject(m_fontSmall);
    if (m_hwnd) DestroyWindow(m_hwnd);
}

bool OverlayWindow::Initialize(HINSTANCE hInstance) {
    m_hInstance = hInstance;

    // Register window class
    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProcStatic;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = nullptr;
    wcex.lpszClassName = L"ClipX_OverlayWindow";

    if (!RegisterClassExW(&wcex)) {
        LOG_ERROR("Failed to register window class");
        return false;
    }

    // Create fonts
    LOGFONTW lf = {};
    lf.lfHeight = -14;
    lf.lfWeight = FW_NORMAL;
    wcscpy_s(lf.lfFaceName, L"Segoe UI");
    m_font = CreateFontIndirectW(&lf);

    lf.lfWeight = FW_BOLD;
    m_fontBold = CreateFontIndirectW(&lf);

    lf.lfHeight = -11;
    lf.lfWeight = FW_NORMAL;
    m_fontSmall = CreateFontIndirectW(&lf);

    // Initialize renderer
    renderer::Initialize();

    // Create window (initially hidden)
    m_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"ClipX_OverlayWindow",
        L"ClipX",
        WS_POPUP,
        0, 0, m_width, m_height,
        nullptr, nullptr, hInstance, this
    );

    if (!m_hwnd) {
        LOG_ERROR("Failed to create window");
        return false;
    }

    // Set window opacity
    SetLayeredWindowAttributes(m_hwnd, 0, static_cast<BYTE>(255 * 0.95), LWA_ALPHA);

    LOG_INFO("OverlayWindow initialized");
    return true;
}

void OverlayWindow::Show() {
    // Position window near cursor
    POINT cursorPos;
    GetCursorPos(&cursorPos);

    // Get monitor info
    HMONITOR monitor = MonitorFromPoint(cursorPos, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {sizeof(MONITORINFO)};
    GetMonitorInfo(monitor, &mi);

    // Calculate position
    int x = cursorPos.x - m_width / 2;
    int y = cursorPos.y - 100;

    // Ensure window is within monitor bounds
    x = std::max<int>(mi.rcMonitor.left, std::min<int>(x, mi.rcMonitor.right - m_width));
    y = std::max<int>(mi.rcMonitor.top, std::min<int>(y, mi.rcMonitor.bottom - m_maxHeight));

    SetWindowPos(m_hwnd, nullptr, x, y, m_width, m_height, SWP_NOZORDER | SWP_NOACTIVATE);

    // Reset state
    m_searchText.clear();
    m_selectedIndex = 0;
    m_scrollOffset = 0;
    m_searchFocused = true;
    m_caretVisible = false;  // Will be set to true in WM_SETFOCUS
    m_selectedTag.clear();
    m_hoverTagIndex = -1;

    // Load all tags for tag panel
    if (m_onGetAllTags) {
        m_allTags = m_onGetAllTags();
    }

    ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
    SetForegroundWindow(m_hwnd);
    SetFocus(m_hwnd);

    UpdateLayout();
    LOG_DEBUG("Overlay shown");
}

void OverlayWindow::Hide() {
    ShowWindow(m_hwnd, SW_HIDE);
    if (m_onClose) {
        m_onClose();
    }
    LOG_DEBUG("Overlay hidden");
}

void OverlayWindow::SetEntries(const std::vector<UIEntry>& entries) {
    m_entries = entries;
    UpdateLayout();
}

void OverlayWindow::SetOnEntrySelected(OnEntrySelectedCallback callback) {
    m_onEntrySelected = std::move(callback);
}

void OverlayWindow::SetOnClose(OnCloseCallback callback) {
    m_onClose = std::move(callback);
}

void OverlayWindow::SetOnSearch(OnSearchCallback callback) {
    m_onSearch = std::move(callback);
}

void OverlayWindow::SetOnAddTag(OnAddTagCallback callback) {
    m_onAddTag = std::move(callback);
}

void OverlayWindow::SetOnDelete(OnDeleteCallback callback) {
    m_onDelete = std::move(callback);
}

void OverlayWindow::SetOnGetTags(OnGetTagsCallback callback) {
    m_onGetTags = std::move(callback);
}

void OverlayWindow::SetOnGetAllTags(OnGetAllTagsCallback callback) {
    m_onGetAllTags = std::move(callback);
}

LRESULT CALLBACK OverlayWindow::WndProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    OverlayWindow* window = nullptr;

    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        window = reinterpret_cast<OverlayWindow*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
    } else {
        window = reinterpret_cast<OverlayWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (window) {
        return window->WndProc(hwnd, msg, wParam, lParam);
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT OverlayWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT:
            OnPaint();
            return 0;

        case WM_KEYDOWN:
            OnKeyDown(wParam);
            return 0;

        case WM_CHAR:
            OnChar(wParam);
            return 0;

        case WM_IME_CHAR:
            // Handle IME input (Chinese, Japanese, etc.)
            OnChar(wParam);
            return 0;

        case WM_SETFOCUS:
            CreateCaret(m_hwnd, nullptr, 2, 16);
            m_caretVisible = true;
            SetCaretPos(m_padding + 30 + GetTextWidth(m_searchText), m_padding + 12);
            ShowCaret(m_hwnd);
            return 0;

        case WM_KILLFOCUS:
            m_caretVisible = false;
            HideCaret(m_hwnd);
            DestroyCaret();
            if (!m_showingDialog && !m_isSearching) {
                Hide();
            }
            return 0;

        case WM_MOUSEMOVE:
            OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;

        case WM_LBUTTONDOWN:
            OnMouseDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), false);
            return 0;

        case WM_RBUTTONDOWN:
            OnMouseDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), true);
            return 0;

        case WM_LBUTTONUP:
            OnMouseUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), false);
            return 0;

        case WM_MOUSEWHEEL:
            OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
            return 0;

        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE && !m_showingDialog && !m_isSearching) {
                Hide();
            }
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

void OverlayWindow::OnPaint() {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(m_hwnd, &ps);

    // Create or resize memory DC
    RECT clientRect;
    GetClientRect(m_hwnd, &clientRect);
    int width = clientRect.right - clientRect.left;
    int height = clientRect.bottom - clientRect.top;

    if (!m_memDC || !m_memBitmap) {
        m_memDC = CreateCompatibleDC(hdc);
        m_memBitmap = CreateCompatibleBitmap(hdc, width, height);
        m_oldBitmap = static_cast<HBITMAP>(SelectObject(m_memDC, m_memBitmap));
    }

    // Clear background
    renderer::FillRect(m_memDC, &clientRect, m_bgColor);

    // Draw rounded border
    RECT borderRect = {0, 0, width, height};
    renderer::DrawRoundedRect(m_memDC, &borderRect, m_borderRadius, m_bgColor, m_borderColor, 1);

    // Draw search bar
    RECT searchRect = {m_padding, m_padding, width - m_padding, m_padding + m_searchBarHeight};
    renderer::DrawRoundedRect(m_memDC, &searchRect, 4, m_searchBgColor, m_borderColor, 1);

    // Draw search icon and text
    std::wstring searchText = utils::Utf8ToWide(m_searchText.empty() ? "Search..." : m_searchText);
    COLORREF searchColor = m_searchText.empty() ? m_textSecondaryColor : m_textColor;
    RECT textRect = {searchRect.left + 30, searchRect.top + 5, searchRect.right - 10, searchRect.bottom - 5};
    SelectObject(m_memDC, m_font);
    renderer::DrawText(m_memDC, searchText, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE, searchColor);

    // Draw magnifying glass icon
    renderer::DrawIcon(m_memDC, searchRect.left + 8, searchRect.top + 12, 16, "search", m_textSecondaryColor);

    // Draw tag panel
    int tagPanelY = m_searchBarHeight + m_padding * 2;
    if (m_showTagPanel && !m_allTags.empty()) {
        RECT tagPanelRect = {m_padding, tagPanelY, width - m_padding, tagPanelY + m_tagPanelHeight};
        renderer::FillRect(m_memDC, &tagPanelRect, RGB(250, 250, 250));

        // Draw tag panel border
        renderer::DrawLine(m_memDC, tagPanelRect.left, tagPanelRect.bottom - 1, tagPanelRect.right, tagPanelRect.bottom - 1, m_borderColor);

        // Draw tags horizontally
        int tagX = tagPanelRect.left + 8;
        int tagY = tagPanelRect.top + 6;
        int tagHeight = 22;
        int tagPaddingX = 8;
        int tagPaddingY = 4;

        SelectObject(m_memDC, m_fontSmall);

        // Draw "All" tag first (to clear filter)
        std::wstring allTagText = L"All";
        bool isAllSelected = m_selectedTag.empty();
        COLORREF allTagBg = isAllSelected ? m_accentColor : RGB(230, 230, 230);
        COLORREF allTagTextCol = isAllSelected ? RGB(255, 255, 255) : m_textColor;

        SIZE allTextSize;
        GetTextExtentPoint32W(m_memDC, allTagText.c_str(), static_cast<int>(allTagText.length()), &allTextSize);
        int allTagWidth = allTextSize.cx + tagPaddingX * 2;

        RECT allTagRect = {tagX, tagY, tagX + allTagWidth, tagY + tagHeight};
        renderer::DrawRoundedRect(m_memDC, &allTagRect, 4, allTagBg);
        RECT allTextRect = {tagX + tagPaddingX, tagY, tagX + allTagWidth - tagPaddingX, tagY + tagHeight};
        renderer::DrawText(m_memDC, allTagText, &allTextRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE, allTagTextCol);

        tagX += allTagWidth + 8;

        // Draw each tag
        for (size_t i = 0; i < m_allTags.size() && tagX < tagPanelRect.right - 8; i++) {
            const auto& [tagName, count] = m_allTags[i];
            std::wstring tagText = utils::Utf8ToWide(tagName) + L" (" + std::to_wstring(count) + L")";

            bool isSelected = m_selectedTag == tagName;
            bool isHovered = static_cast<int>(i) == m_hoverTagIndex;
            COLORREF tagBg = isSelected ? m_accentColor : (isHovered ? RGB(210, 230, 250) : m_tagBgColor);
            COLORREF tagTextCol = isSelected ? RGB(255, 255, 255) : m_tagTextColor;

            SIZE textSize;
            GetTextExtentPoint32W(m_memDC, tagText.c_str(), static_cast<int>(tagText.length()), &textSize);
            int tagWidth = textSize.cx + tagPaddingX * 2;

            if (tagX + tagWidth > tagPanelRect.right - 8) {
                // Not enough space, show "..." indicator
                std::wstring moreText = L"...";
                SIZE moreSize;
                GetTextExtentPoint32W(m_memDC, moreText.c_str(), static_cast<int>(moreText.length()), &moreSize);
                int moreWidth = moreSize.cx + tagPaddingX * 2;

                if (tagX + moreWidth <= tagPanelRect.right - 8) {
                    RECT moreRect = {tagX, tagY, tagX + moreWidth, tagY + tagHeight};
                    renderer::DrawRoundedRect(m_memDC, &moreRect, 4, RGB(230, 230, 230));
                    RECT moreTextRect = {tagX + tagPaddingX, tagY, tagX + moreWidth - tagPaddingX, tagY + tagHeight};
                    renderer::DrawText(m_memDC, moreText, &moreTextRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE, m_textSecondaryColor);
                }
                break;
            }

            RECT tagRect = {tagX, tagY, tagX + tagWidth, tagY + tagHeight};
            renderer::DrawRoundedRect(m_memDC, &tagRect, 4, tagBg);
            RECT textRect = {tagX + tagPaddingX, tagY, tagX + tagWidth - tagPaddingX, tagY + tagHeight};
            renderer::DrawText(m_memDC, tagText, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE, tagTextCol);

            tagX += tagWidth + 6;
        }

        tagPanelY += m_tagPanelHeight;
    }

    // Draw items
    int itemY = tagPanelY;
    m_visibleItemCount = 0;

    for (size_t i = static_cast<size_t>(m_scrollOffset); i < m_entries.size() && itemY < height - m_toolbarHeight; i++) {
        int itemIndex = static_cast<int>(i);
        RECT itemRect = {m_padding, itemY, width - m_padding, itemY + m_itemHeight};

        // Determine item colors
        COLORREF itemColor = m_itemColor;
        if (itemIndex == m_selectedIndex) {
            itemColor = m_itemSelectedColor;
        } else if (itemIndex == m_hoverIndex) {
            itemColor = m_itemHoverColor;
        }

        // Draw item background
        renderer::DrawRoundedRect(m_memDC, &itemRect, 4, itemColor);

        // Draw icon
        std::string iconType = renderer::GetTypeIcon(static_cast<int>(m_entries[i].type));
        if (m_entries[i].isFavorited) {
            iconType = "favorite";
        }
        renderer::DrawIcon(m_memDC, itemRect.left + 8, itemRect.top + 15, 24, iconType,
                          m_entries[i].isFavorited ? RGB(255, 180, 0) : m_accentColor);

        // Draw preview text
        SelectObject(m_memDC, m_font);
        std::wstring preview = utils::Utf8ToWide(m_entries[i].preview);
        RECT previewRect = {itemRect.left + 40, itemRect.top + 8, itemRect.right - 8, itemRect.top + 32};
        renderer::DrawText(m_memDC, preview, &previewRect, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS, m_textColor);

        // Draw secondary info (source app and timestamp)
        SelectObject(m_memDC, m_fontSmall);
        std::wstring secondary = utils::Utf8ToWide(m_entries[i].sourceApp + " · " + m_entries[i].timestampStr);
        RECT secondaryRect = {itemRect.left + 40, itemRect.top + 36, itemRect.right - 8, itemRect.bottom - 8};
        renderer::DrawText(m_memDC, secondary, &secondaryRect, DT_LEFT | DT_TOP | DT_SINGLELINE, m_textSecondaryColor);

        // Draw tags on right side
        if (!m_entries[i].tags.empty()) {
            int tagX = itemRect.right - 8;
            int tagY = itemRect.top + 8;
            int tagHeight = 18;
            int tagPadding = 6;
            int maxTags = 3;  // Maximum tags to show
            int tagsShown = 0;

            for (auto it = m_entries[i].tags.rbegin(); it != m_entries[i].tags.rend() && tagsShown < maxTags; ++it, ++tagsShown) {
                std::wstring tagText = utils::Utf8ToWide(*it);

                // Calculate tag width
                SIZE textSize;
                SelectObject(m_memDC, m_fontSmall);
                GetTextExtentPoint32W(m_memDC, tagText.c_str(), static_cast<int>(tagText.length()), &textSize);
                int tagWidth = textSize.cx + tagPadding * 2;

                tagX -= tagWidth + 4;
                if (tagX < itemRect.left + 40) break;  // Don't overlap with text

                // Draw tag background
                RECT tagRect = {tagX, tagY, tagX + tagWidth, tagY + tagHeight};
                renderer::DrawRoundedRect(m_memDC, &tagRect, 3, m_tagBgColor);

                // Draw tag text
                RECT tagTextRect = {tagX + tagPadding, tagY, tagX + tagWidth - tagPadding, tagY + tagHeight};
                renderer::DrawText(m_memDC, tagText, &tagTextRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE, m_tagTextColor);
            }

            // If more tags than shown, show +N indicator
            if (m_entries[i].tags.size() > static_cast<size_t>(maxTags)) {
                int moreCount = static_cast<int>(m_entries[i].tags.size()) - maxTags;
                std::wstring moreText = L"+" + std::to_wstring(moreCount);

                SIZE textSize;
                SelectObject(m_memDC, m_fontSmall);
                GetTextExtentPoint32W(m_memDC, moreText.c_str(), static_cast<int>(moreText.length()), &textSize);
                int moreWidth = textSize.cx + tagPadding * 2;

                tagX -= moreWidth + 4;
                if (tagX >= itemRect.left + 40) {
                    RECT moreRect = {tagX, tagY, tagX + moreWidth, tagY + tagHeight};
                    renderer::DrawRoundedRect(m_memDC, &moreRect, 3, RGB(230, 230, 230));

                    RECT moreTextRect = {tagX + tagPadding, tagY, tagX + moreWidth - tagPadding, tagY + tagHeight};
                    renderer::DrawText(m_memDC, moreText, &moreTextRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE, m_textSecondaryColor);
                }
            }
        }

        itemY += m_itemHeight + 4;
        m_visibleItemCount++;
    }

    // Draw toolbar
    RECT toolbarRect = {m_padding, height - m_toolbarHeight - m_padding, width - m_padding, height - m_padding};
    renderer::FillRect(m_memDC, &toolbarRect, m_bgColor);
    renderer::DrawLine(m_memDC, toolbarRect.left, toolbarRect.top, toolbarRect.right, toolbarRect.top, m_borderColor);

    SelectObject(m_memDC, m_fontSmall);
    std::wstring statsText = L"Total: " + std::to_wstring(m_entries.size()) + L" items";
    RECT statsRect = {toolbarRect.left + 8, toolbarRect.top + 10, toolbarRect.right - 8, toolbarRect.bottom - 10};
    renderer::DrawText(m_memDC, statsText, &statsRect, DT_LEFT | DT_VCENTER, m_textSecondaryColor);

    // Draw shortcut hints
    std::wstring shortcutsText = L"Enter=Select  Esc=Close  Del=Delete";
    renderer::DrawText(m_memDC, shortcutsText, &statsRect, DT_RIGHT | DT_VCENTER, m_textSecondaryColor);

    // Copy to screen
    BitBlt(hdc, 0, 0, width, height, m_memDC, 0, 0, SRCCOPY);

    EndPaint(m_hwnd, &ps);
}

void OverlayWindow::OnKeyDown(WPARAM vk) {
    switch (vk) {
        case VK_ESCAPE:
            Hide();
            break;

        case VK_RETURN:
            if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_entries.size())) {
                if (m_onEntrySelected) {
                    m_onEntrySelected(m_entries[m_selectedIndex].id);
                }
                Hide();
            }
            break;

        case VK_UP:
            if (m_selectedIndex > 0) {
                m_selectedIndex--;
                EnsureVisible(m_selectedIndex);
                InvalidateRect(m_hwnd, nullptr, FALSE);
            }
            break;

        case VK_DOWN:
            if (m_selectedIndex < static_cast<int>(m_entries.size()) - 1) {
                m_selectedIndex++;
                EnsureVisible(m_selectedIndex);
                InvalidateRect(m_hwnd, nullptr, FALSE);
            }
            break;

        case VK_DELETE:
            // Delete selected item - handled by main
            break;

        case VK_BACK:
            if (!m_searchText.empty()) {
                // Remove last UTF-8 character (not just one byte)
                // UTF-8 continuation bytes start with 10xxxxxx (0x80-0xBF)
                // Find the start of the last character
                size_t len = m_searchText.length();
                while (len > 0 && (static_cast<unsigned char>(m_searchText[len - 1]) & 0xC0) == 0x80) {
                    len--;
                }
                if (len > 0) {
                    len--;  // Remove the leading byte
                }
                m_searchText = m_searchText.substr(0, len);

                // Update caret position
                SetCaretPos(m_padding + 30 + GetTextWidth(m_searchText), m_padding + 12);

                // Trigger search callback with protection against focus loss
                if (m_onSearch) {
                    m_isSearching = true;
                    m_onSearch(m_searchText);
                    m_isSearching = false;
                }
                InvalidateRect(m_hwnd, nullptr, FALSE);
            }
            break;

        case 'F':
            if (GetKeyState(VK_CONTROL) & 0x8000) {
                m_searchFocused = true;
                InvalidateRect(m_hwnd, nullptr, FALSE);
            }
            break;
    }
}

void OverlayWindow::OnChar(WPARAM ch) {
    // Support all printable characters including Chinese
    // WM_CHAR sends UTF-16 code units
    // Skip control characters (0-31) but accept all others including Unicode
    if (ch >= 32) {
        // Handle UTF-16 characters
        // For BMP characters (including most Chinese), ch is the character itself
        // For characters outside BMP, Windows sends surrogate pairs (high then low)
        if (ch < 0xD800 || ch > 0xDFFF) {
            // BMP character (or low surrogate if we missed the high one)
            wchar_t wch = static_cast<wchar_t>(ch);
            std::string utf8 = utils::WideToUtf8(std::wstring(1, wch));
            m_searchText += utf8;
        } else if (ch >= 0xD800 && ch <= 0xDBFF) {
            // High surrogate - store for combining with low surrogate
            // For simplicity, we'll handle this case by converting the surrogate pair
            // This is a simplified approach - in practice, the high and low surrogates
            // come in separate WM_CHAR messages
            wchar_t highSurrogate = static_cast<wchar_t>(ch);
            // Store high surrogate temporarily (will be combined with low surrogate)
            // For now, just store the character
            std::wstring ws(1, highSurrogate);
            m_searchText += utils::WideToUtf8(ws);
        } else {
            // Low surrogate (0xDC00 - 0xDFFF)
            // This should ideally combine with previous high surrogate
            wchar_t lowSurrogate = static_cast<wchar_t>(ch);
            std::wstring ws(1, lowSurrogate);
            m_searchText += utils::WideToUtf8(ws);
        }

        m_searchFocused = true;

        // Update caret position (always update if window has focus)
        SetCaretPos(m_padding + 30 + GetTextWidth(m_searchText), m_padding + 12);

        // Trigger search callback with protection against focus loss
        if (m_onSearch) {
            m_isSearching = true;
            m_onSearch(m_searchText);
            m_isSearching = false;
        }
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void OverlayWindow::OnMouseMove(int x, int y) {
    // Check for tag hover
    int tagIndex = GetTagAtPosition(x, y);
    if (tagIndex != m_hoverTagIndex) {
        m_hoverTagIndex = tagIndex;
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }

    // Check for item hover
    int itemIndex = GetItemAtPosition(x, y);
    if (itemIndex != m_hoverIndex) {
        m_hoverIndex = itemIndex;
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void OverlayWindow::OnMouseDown(int x, int y, bool isRight) {
    // Check for tag click first
    int tagIndex = GetTagAtPosition(x, y);
    if (tagIndex != -1) {
        if (tagIndex == -2) {
            // "All" clicked - clear filter
            m_selectedTag.clear();
            m_searchText.clear();
            if (m_onSearch) {
                m_onSearch("");
            }
        } else if (tagIndex >= 0 && tagIndex < static_cast<int>(m_allTags.size())) {
            // Tag clicked - filter by tag
            m_selectedTag = m_allTags[tagIndex].first;
            if (m_onSearch) {
                m_onSearch(m_selectedTag);
            }
        }
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }

    int itemIndex = GetItemAtPosition(x, y);
    if (itemIndex >= 0) {
        m_selectedIndex = itemIndex;
        InvalidateRect(m_hwnd, nullptr, FALSE);

        if (isRight) {
            ShowContextMenu(x, y, itemIndex);
        }
    }
}

void OverlayWindow::OnMouseUp(int x, int y, bool isRight) {
    if (!isRight) {
        int itemIndex = GetItemAtPosition(x, y);
        if (itemIndex >= 0 && itemIndex == m_selectedIndex) {
            if (m_onEntrySelected) {
                m_onEntrySelected(m_entries[itemIndex].id);
            }
            Hide();
        }
    }
}

void OverlayWindow::OnMouseWheel(int delta) {
    Scroll(-delta / WHEEL_DELTA);
}

void OverlayWindow::UpdateLayout() {
    // Calculate window height based on items
    int tagPanelHeight = (m_showTagPanel && !m_allTags.empty()) ? m_tagPanelHeight : 0;
    int contentHeight = m_searchBarHeight + m_padding * 2 + tagPanelHeight + m_toolbarHeight + m_padding;
    int itemsHeight = static_cast<int>(m_entries.size()) * (m_itemHeight + 4);
    m_height = std::min(contentHeight + itemsHeight, m_maxHeight);

    // Adjust visible item count
    int availableHeight = m_height - contentHeight;
    m_visibleItemCount = availableHeight / (m_itemHeight + 4);

    if (m_hwnd) {
        RECT rect;
        GetWindowRect(m_hwnd, &rect);
        SetWindowPos(m_hwnd, nullptr, rect.left, rect.top, m_width, m_height, SWP_NOZORDER);
    }

    InvalidateRect(m_hwnd, nullptr, FALSE);
}

int OverlayWindow::GetItemAtPosition(int x, int y) {
    int tagPanelHeight = (m_showTagPanel && !m_allTags.empty()) ? m_tagPanelHeight : 0;
    int itemY = m_searchBarHeight + m_padding * 2 + tagPanelHeight;

    for (size_t i = static_cast<size_t>(m_scrollOffset); i < m_entries.size(); i++) {
        if (y >= itemY && y < itemY + m_itemHeight) {
            return static_cast<int>(i);
        }
        itemY += m_itemHeight + 4;
    }

    return -1;
}

int OverlayWindow::GetTagAtPosition(int x, int y) {
    if (!m_showTagPanel || m_allTags.empty()) return -1;

    int tagPanelTop = m_searchBarHeight + m_padding * 2;
    int tagPanelBottom = tagPanelTop + m_tagPanelHeight;

    if (y < tagPanelTop || y > tagPanelBottom) return -1;
    if (x < m_padding + 8) return -1;

    int tagX = m_padding + 8;
    int tagY = tagPanelTop + 6;
    int tagHeight = 22;
    int tagPaddingX = 8;

    SelectObject(m_memDC, m_fontSmall);

    // Check "All" tag
    std::wstring allTagText = L"All";
    SIZE allTextSize;
    GetTextExtentPoint32W(m_memDC, allTagText.c_str(), static_cast<int>(allTagText.length()), &allTextSize);
    int allTagWidth = allTextSize.cx + tagPaddingX * 2;

    if (x >= tagX && x < tagX + allTagWidth && y >= tagY && y < tagY + tagHeight) {
        return -2;  // Special value for "All" tag
    }
    tagX += allTagWidth + 8;

    // Check each tag
    for (size_t i = 0; i < m_allTags.size(); i++) {
        const auto& [tagName, count] = m_allTags[i];
        std::wstring tagText = utils::Utf8ToWide(tagName) + L" (" + std::to_wstring(count) + L")";

        SIZE textSize;
        GetTextExtentPoint32W(m_memDC, tagText.c_str(), static_cast<int>(tagText.length()), &textSize);
        int tagWidth = textSize.cx + tagPaddingX * 2;

        if (x >= tagX && x < tagX + tagWidth && y >= tagY && y < tagY + tagHeight) {
            return static_cast<int>(i);
        }
        tagX += tagWidth + 6;
    }

    return -1;
}

void OverlayWindow::SelectItem(int index) {
    if (index >= 0 && index < static_cast<int>(m_entries.size())) {
        m_selectedIndex = index;
        EnsureVisible(index);
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void OverlayWindow::EnsureVisible(int index) {
    int visibleStart = m_scrollOffset;
    int visibleEnd = m_scrollOffset + m_visibleItemCount - 1;

    if (index < visibleStart) {
        m_scrollOffset = index;
    } else if (index > visibleEnd) {
        m_scrollOffset = index - m_visibleItemCount + 1;
    }
}

void OverlayWindow::Scroll(int delta) {
    int newOffset = m_scrollOffset + delta;
    int maxOffset = std::max(0, static_cast<int>(m_entries.size()) - m_visibleItemCount);
    m_scrollOffset = std::max(0, std::min(newOffset, maxOffset));
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void OverlayWindow::ShowContextMenu(int x, int y, int itemIndex) {
    if (itemIndex < 0 || itemIndex >= static_cast<int>(m_entries.size())) {
        return;
    }

    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, 1, L"Add Tag");
    AppendMenuW(hMenu, MF_STRING, 3, L"View Tags");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, 2, L"Delete");

    // Convert to screen coordinates
    POINT pt = {x, y};
    ClientToScreen(m_hwnd, &pt);

    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD, pt.x, pt.y, 0, m_hwnd, NULL);
    DestroyMenu(hMenu);

    switch (cmd) {
        case 1: { // Add Tag - show input dialog
            std::wstring tag = ShowSimpleInputDialog(L"Add Tag", L"Enter tag name:");
            if (!tag.empty() && m_onAddTag) {
                std::string tagUtf8 = utils::WideToUtf8(tag);
                m_onAddTag(m_entries[itemIndex].id, tagUtf8);
                // Update the entry's local tags list
                m_entries[itemIndex].tags.push_back(tagUtf8);
                // Refresh tag panel
                if (m_onGetAllTags) {
                    m_allTags = m_onGetAllTags();
                }
                InvalidateRect(m_hwnd, nullptr, FALSE);
            }
            break;
        }
        case 2: // Delete
            if (m_onDelete) {
                m_onDelete(m_entries[itemIndex].id);
                // Note: entries will be updated via SetEntries callback from main.cpp
                // Refresh tag panel
                if (m_onGetAllTags) {
                    m_allTags = m_onGetAllTags();
                }
            }
            break;
        case 3: { // View Tags
            if (m_onGetTags) {
                // Set flag to prevent auto-hide when showing message box
                m_showingDialog = true;
                std::vector<std::string> tags = m_onGetTags(m_entries[itemIndex].id);
                m_showingDialog = false;

                if (tags.empty()) {
                    m_showingDialog = true;
                    MessageBoxW(m_hwnd, L"No tags for this entry", L"Tags", MB_OK | MB_ICONINFORMATION);
                    m_showingDialog = false;
                } else {
                    std::wstring tagList = L"Tags:\n\n";
                    for (const auto& tag : tags) {
                        tagList += L"- " + utils::Utf8ToWide(tag) + L"\n";
                    }
                    m_showingDialog = true;
                    MessageBoxW(m_hwnd, tagList.c_str(), L"Tags", MB_OK | MB_ICONINFORMATION);
                    m_showingDialog = false;
                }
            }
            break;
        }
    }
}

// Simple input dialog created programmatically (no resource needed)
struct SimpleInputDialogData {
    std::wstring result;
};

static INT_PTR CALLBACK SimpleInputDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    SimpleInputDialogData* data = nullptr;

    if (message == WM_INITDIALOG) {
        data = reinterpret_cast<SimpleInputDialogData*>(lParam);
        SetWindowLongPtr(hDlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(data));
        SetFocus(GetDlgItem(hDlg, 1001));
        return FALSE;
    }

    data = reinterpret_cast<SimpleInputDialogData*>(GetWindowLongPtr(hDlg, GWLP_USERDATA));

    switch (message) {
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDOK: {
                    wchar_t buffer[256] = {0};
                    GetDlgItemTextW(hDlg, 1001, buffer, 256);
                    if (data) {
                        data->result = buffer;
                    }
                    EndDialog(hDlg, IDOK);
                    return TRUE;
                }
                case IDCANCEL:
                    EndDialog(hDlg, IDCANCEL);
                    return TRUE;
            }
            break;

        case WM_CLOSE:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
    }

    return FALSE;
}

// Helper to align pointer to DWORD boundary
template<typename T>
inline T* AlignToDword(T* ptr) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    addr = (addr + 3) & ~3;
    return reinterpret_cast<T*>(addr);
}

std::wstring OverlayWindow::ShowSimpleInputDialog(const std::wstring& title, const std::wstring& prompt) {
    SimpleInputDialogData data;

    // Create dialog template in memory with proper DWORD alignment
    // We need a buffer large enough to hold everything
    std::vector<BYTE> buffer(2048);
    BYTE* ptr = buffer.data();

    // DLGTEMPLATE structure
    DLGTEMPLATE* dlg = reinterpret_cast<DLGTEMPLATE*>(ptr);
    dlg->style = WS_POPUP | WS_BORDER | WS_SYSMENU | WS_CAPTION | DS_MODALFRAME | DS_CENTER | DS_SHELLFONT;
    dlg->dwExtendedStyle = 0;
    dlg->cdit = 4;  // 4 dialog items: prompt, edit, OK, Cancel
    dlg->x = 0;
    dlg->y = 0;
    dlg->cx = 200;  // Dialog units
    dlg->cy = 80;

    ptr += sizeof(DLGTEMPLATE);

    // Menu (0 = none)
    *reinterpret_cast<WORD*>(ptr) = 0;
    ptr += sizeof(WORD);

    // Window class (0 = default dialog class)
    *reinterpret_cast<WORD*>(ptr) = 0;
    ptr += sizeof(WORD);

    // Dialog title (empty - we'll set it via SetWindowText)
    *reinterpret_cast<WORD*>(ptr) = 0;
    ptr += sizeof(WORD);

    // Font (for DS_SHELLFONT)
    *reinterpret_cast<WORD*>(ptr) = 9;  // Point size
    ptr += sizeof(WORD);

    // Font name (must be WCHAR aligned)
    const wchar_t* fontName = L"Segoe UI";
    wcscpy_s(reinterpret_cast<wchar_t*>(ptr), 16, fontName);
    ptr += (wcslen(fontName) + 1) * sizeof(wchar_t);

    // Align to DWORD for first DLGITEMTEMPLATE
    ptr = AlignToDword(ptr);

    // Helper lambda to add a dialog item
    auto AddItem = [&](DWORD style, short x, short y, short cx, short cy, WORD id, const wchar_t* text, WORD ctrlClass) {
        DLGITEMTEMPLATE* item = reinterpret_cast<DLGITEMTEMPLATE*>(ptr);
        item->style = style;
        item->dwExtendedStyle = 0;
        item->x = x;
        item->y = y;
        item->cx = cx;
        item->cy = cy;
        item->id = id;
        ptr += sizeof(DLGITEMTEMPLATE);

        // Window class (0xFFFF followed by ordinal)
        *reinterpret_cast<WORD*>(ptr) = 0xFFFF;
        ptr += sizeof(WORD);
        *reinterpret_cast<WORD*>(ptr) = ctrlClass;
        ptr += sizeof(WORD);

        // Text
        size_t textLen = wcslen(text);
        wcscpy_s(reinterpret_cast<wchar_t*>(ptr), textLen + 1, text);
        ptr += (textLen + 1) * sizeof(wchar_t);

        // Creation data (0 = none)
        *reinterpret_cast<WORD*>(ptr) = 0;
        ptr += sizeof(WORD);

        // Align to DWORD for next item
        ptr = AlignToDword(ptr);
    };

    // Control class ordinals
    const WORD BUTTON_CLASS = 0x0080;
    const WORD EDIT_CLASS = 0x0081;
    const WORD STATIC_CLASS = 0x0082;

    // Add controls (order matters for tab order)
    // 1. Prompt text
    AddItem(WS_CHILD | WS_VISIBLE | SS_LEFT, 7, 7, 186, 12, 1000, prompt.c_str(), STATIC_CLASS);

    // 2. Edit control
    AddItem(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, 7, 24, 186, 14, 1001, L"", EDIT_CLASS);

    // 3. OK button (default)
    AddItem(WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 90, 50, 50, 14, IDOK, L"OK", BUTTON_CLASS);

    // 4. Cancel button
    AddItem(WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 143, 50, 50, 14, IDCANCEL, L"Cancel", BUTTON_CLASS);

    // Create the dialog
    HWND hwndOwner = m_hwnd ? m_hwnd : GetDesktopWindow();

    // Set flag to prevent auto-hide when dialog is shown
    m_showingDialog = true;

    // Use DialogBoxIndirectParamW to pass data
    INT_PTR result = DialogBoxIndirectParamW(
        m_hInstance,
        dlg,
        hwndOwner,
        SimpleInputDialogProc,
        reinterpret_cast<LPARAM>(&data)
    );

    // Reset flag after dialog closes
    m_showingDialog = false;

    if (result == IDOK && !data.result.empty()) {
        return data.result;
    }

    return L"";
}

int OverlayWindow::GetTextWidth(const std::string& text) {
    if (!m_font || text.empty()) return 0;

    std::wstring wtext = utils::Utf8ToWide(text);

    // Get window DC and select font
    HDC hdc = GetDC(m_hwnd);
    if (!hdc) return 0;

    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, m_font));

    // Use DrawText with DT_CALCRECT to get accurate text width
    RECT rect = {0, 0, 0, 0};
    DrawTextW(hdc, wtext.c_str(), static_cast<int>(wtext.length()), &rect, DT_LEFT | DT_SINGLELINE | DT_CALCRECT);

    SelectObject(hdc, oldFont);
    ReleaseDC(m_hwnd, hdc);

    return rect.right;
}

} // namespace clipx
