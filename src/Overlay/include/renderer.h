#pragma once

#include <windows.h>
#include <string>

namespace clipx {
namespace renderer {

// Initialize GDI resources
void Initialize();
void Cleanup();

// Drawing functions
void FillRect(HDC hdc, const RECT* rect, COLORREF color);
void DrawRoundedRect(HDC hdc, const RECT* rect, int radius, COLORREF fillColor, COLORREF borderColor = 0, int borderWidth = 0);
void DrawText(HDC hdc, const std::wstring& text, const RECT* rect, UINT format, COLORREF color);
void DrawIcon(HDC hdc, int x, int y, int size, const std::string& iconType, COLORREF color);
void DrawLine(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color, int width = 1);

// Helper to get type icon
std::string GetTypeIcon(int type);

} // namespace renderer
} // namespace clipx
