#include "common/windows.h"
#include "renderer.h"
#include "common/utils.h"
#include "common/types.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <uxtheme.h>
#include <dwmapi.h>

namespace clipx {
namespace renderer {

static HBRUSH CreateSolidBrushCached(COLORREF color) {
    return CreateSolidBrush(color);
}

void Initialize() {
    // Enable per-monitor DPI awareness
    // This would typically be done at application startup
}

void Cleanup() {
    // Clean up any cached resources
}

void FillRect(HDC hdc, const RECT* rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrushCached(color);
    ::FillRect(hdc, rect, brush);
    DeleteObject(brush);
}

void DrawRoundedRect(HDC hdc, const RECT* rect, int radius, COLORREF fillColor, COLORREF borderColor, int borderWidth) {
    // Get dimensions
    int width = rect->right - rect->left;
    int height = rect->bottom - rect->top;

    // Create a rounded rectangle region
    HRGN rgn = CreateRoundRectRgn(rect->left, rect->top, rect->right + 1, rect->bottom + 1, radius * 2, radius * 2);

    // Fill the region
    HBRUSH fillBrush = CreateSolidBrush(fillColor);
    FillRgn(hdc, rgn, fillBrush);
    DeleteObject(fillBrush);

    // Draw border if specified
    if (borderWidth > 0 && borderColor != 0) {
        HBRUSH borderBrush = CreateSolidBrush(borderColor);
        FrameRgn(hdc, rgn, borderBrush, borderWidth, borderWidth);
        DeleteObject(borderBrush);
    }

    DeleteObject(rgn);
}

void DrawText(HDC hdc, const std::wstring& text, const RECT* rect, UINT format, COLORREF color) {
    SetTextColor(hdc, color);
    SetBkMode(hdc, TRANSPARENT);

    // Create a copy of the rect because DrawText might modify it
    RECT textRect = *rect;
    ::DrawTextW(hdc, text.c_str(), -1, &textRect, format);
}

void DrawIcon(HDC hdc, int x, int y, int size, const std::string& iconType, COLORREF color) {
    // Draw a simple colored circle or shape to represent the icon
    HBRUSH brush = CreateSolidBrush(color);
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    HGDIOBJ oldPen = SelectObject(hdc, pen);

    if (iconType == "text") {
        // Draw a document-like shape
        Rectangle(hdc, x + 4, y + 2, x + size - 4, y + size - 2);
    } else if (iconType == "image") {
        // Draw a picture frame
        Rectangle(hdc, x + 2, y + 2, x + size - 2, y + size - 2);
        MoveToEx(hdc, x + 2, y + size / 2, nullptr);
        LineTo(hdc, x + size - 2, y + size / 2);
    } else if (iconType == "files") {
        // Draw a folder shape
        POINT pts[] = {
            {x + 2, y + 6},
            {x + 8, y + 6},
            {x + 10, y + 8},
            {x + size - 2, y + 8},
            {x + size - 2, y + size - 2},
            {x + 2, y + size - 2}
        };
        Polygon(hdc, pts, 6);
    } else if (iconType == "favorite") {
        // Draw a star
        int cx = x + size / 2;
        int cy = y + size / 2;
        int r = size / 2 - 2;
        POINT pts[10];
        for (int i = 0; i < 10; i++) {
            double angle = i * M_PI / 5 - M_PI / 2;
            int radius = (i % 2 == 0) ? r : r / 2;
            pts[i].x = cx + static_cast<int>(radius * cos(angle));
            pts[i].y = cy + static_cast<int>(radius * sin(angle));
        }
        Polygon(hdc, pts, 10);
    } else {
        // Default: draw a circle
        Ellipse(hdc, x + 2, y + 2, x + size - 2, y + size - 2);
    }

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void DrawLine(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color, int width) {
    HPEN pen = CreatePen(PS_SOLID, width, color);
    HGDIOBJ oldPen = SelectObject(hdc, pen);

    MoveToEx(hdc, x1, y1, nullptr);
    LineTo(hdc, x2, y2);

    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

std::string GetTypeIcon(int type) {
    switch (static_cast<ClipboardDataType>(type)) {
        case ClipboardDataType::Text: return "text";
        case ClipboardDataType::Html: return "text";
        case ClipboardDataType::Rtf: return "text";
        case ClipboardDataType::Image: return "image";
        case ClipboardDataType::Files: return "files";
        default: return "text";
    }
}

} // namespace renderer
} // namespace clipx
