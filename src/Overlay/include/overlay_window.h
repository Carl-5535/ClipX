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

    // Colors
    COLORREF m_bgColor = RGB(243, 243, 243);
    COLORREF m_itemColor = RGB(255, 255, 255);
    COLORREF m_itemHoverColor = RGB(229, 243, 255);
    COLORREF m_itemSelectedColor = RGB(204, 232, 255);
    COLORREF m_textColor = RGB(26, 26, 26);
    COLORREF m_textSecondaryColor = RGB(102, 102, 102);
    COLORREF m_borderColor = RGB(224, 224, 224);
    COLORREF m_accentColor = RGB(0, 120, 212);
    COLORREF m_searchBgColor = RGB(255, 255, 255);
    COLORREF m_tagBgColor = RGB(200, 230, 255);
    COLORREF m_tagTextColor = RGB(0, 80, 150);
};

} // namespace clipx
