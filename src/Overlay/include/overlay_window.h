#pragma once

#include <string>
#include <vector>
#include <functional>
#include "common/windows.h"
#include "common/types.h"

namespace clipx {

struct UIEntry {
    int64_t id;
    std::string preview;
    std::string sourceApp;
    std::string timestampStr;
    ClipboardDataType type;
    bool isFavorited;
    int copyCount;
    std::vector<std::string> tags;
};

class OverlayWindow {
public:
    using OnEntrySelectedCallback = std::function<void(int64_t id)>;
    using OnCloseCallback = std::function<void()>;
    using OnSearchCallback = std::function<void(const std::string&)>;
    using OnAddTagCallback = std::function<void(int64_t entryId, const std::string& tag)>;
    using OnDeleteCallback = std::function<void(int64_t entryId)>;
    using OnGetTagsCallback = std::function<std::vector<std::string>(int64_t entryId)>;
    using OnGetAllTagsCallback = std::function<std::vector<std::pair<std::string, int>>()>;

    OverlayWindow();
    ~OverlayWindow();

    bool Initialize(HINSTANCE hInstance);
    void Show();
    void Hide();

    void SetEntries(const std::vector<UIEntry>& entries);
    void SetOnEntrySelected(OnEntrySelectedCallback callback);
    void SetOnClose(OnCloseCallback callback);
    void SetOnSearch(OnSearchCallback callback);
    void SetOnAddTag(OnAddTagCallback callback);
    void SetOnDelete(OnDeleteCallback callback);
    void SetOnGetTags(OnGetTagsCallback callback);
    void SetOnGetAllTags(OnGetAllTagsCallback callback);

    HWND GetHwnd() const { return m_hwnd; }

private:
    static LRESULT CALLBACK WndProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void OnPaint();
    void OnKeyDown(WPARAM vk);
    void OnChar(WPARAM ch);
    void OnMouseMove(int x, int y);
    void OnMouseDown(int x, int y, bool isRight);
    void OnMouseUp(int x, int y, bool isRight);
    void OnMouseWheel(int delta);

    void UpdateLayout();
    void InvalidateItem(int index);
    int GetItemAtPosition(int x, int y);
    int GetTagAtPosition(int x, int y);  // Returns -2 for "All", -1 for none, 0+ for tag index
    void SelectItem(int index);
    void EnsureVisible(int index);
    void Scroll(int delta);
    void ShowContextMenu(int x, int y, int itemIndex);
    std::wstring ShowSimpleInputDialog(const std::wstring& title, const std::wstring& prompt);
    int GetTextWidth(const std::string& text);

    HINSTANCE m_hInstance = nullptr;
    HWND m_hwnd = nullptr;
    int m_width = 400;
    int m_height = 500;
    int m_maxHeight = 600;

    // Layout constants
    int m_searchBarHeight = 40;
    int m_itemHeight = 60;
    int m_toolbarHeight = 40;
    int m_padding = 8;
    int m_borderRadius = 8;
    int m_tagPanelHeight = 36;

    // State
    std::vector<UIEntry> m_entries;
    std::string m_searchText;
    int m_selectedIndex = 0;
    int m_scrollOffset = 0;
    int m_hoverIndex = -1;
    int m_visibleItemCount = 0;
    bool m_searchFocused = true;
    bool m_showingDialog = false;  // Prevents auto-hide when showing child dialogs
    bool m_isSearching = false;    // Prevents auto-hide during search operations
    bool m_caretVisible = false;   // Caret visibility state for search box

    // Tag panel state
    std::vector<std::pair<std::string, int>> m_allTags;  // Tag name and count
    std::string m_selectedTag;  // Currently selected tag filter
    bool m_showTagPanel = true;  // Whether tag panel is visible
    int m_hoverTagIndex = -1;  // Hovered tag in tag panel

    // Callbacks
    OnEntrySelectedCallback m_onEntrySelected;
    OnCloseCallback m_onClose;
    OnSearchCallback m_onSearch;
    OnAddTagCallback m_onAddTag;
    OnDeleteCallback m_onDelete;
    OnGetTagsCallback m_onGetTags;
    OnGetAllTagsCallback m_onGetAllTags;

    // GDI objects
    HDC m_memDC = nullptr;
    HBITMAP m_memBitmap = nullptr;
    HBITMAP m_oldBitmap = nullptr;
    HFONT m_font = nullptr;
    HFONT m_fontBold = nullptr;
    HFONT m_fontSmall = nullptr;

    // Colors - Glass theme (dark semi-transparent)
    COLORREF m_bgColor = RGB(32, 32, 32);              // Dark background
    COLORREF m_itemColor = RGB(45, 45, 45);             // Slightly lighter items
    COLORREF m_itemHoverColor = RGB(60, 60, 65);        // Hover state
    COLORREF m_itemSelectedColor = RGB(0, 90, 158);     // Selection accent
    COLORREF m_textColor = RGB(240, 240, 240);          // Light text
    COLORREF m_textSecondaryColor = RGB(160, 160, 160); // Secondary text
    COLORREF m_borderColor = RGB(60, 60, 65);           // Subtle borders
    COLORREF m_accentColor = RGB(0, 120, 212);          // Accent blue
    COLORREF m_searchBgColor = RGB(40, 40, 45);         // Search background
    COLORREF m_tagBgColor = RGB(0, 90, 140);            // Tag background
    COLORREF m_tagTextColor = RGB(200, 230, 255);       // Tag text
};

} // namespace clipx
