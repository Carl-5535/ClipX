#!/usr/bin/env python3
"""Generate a simple ICO file for ClipX"""

import struct
import zlib

def create_simple_ico():
    """Create a simple ICO file with bitmap data"""

    # ICO header
    ico_header = struct.pack('<HHH', 0, 1, 1)  # Reserved, Type (1=ICO), Count

    # Create a simple 32x32 bitmap
    width, height = 32, 32

    # BMP header for 32x32 24bpp image (BITMAPINFOHEADER = 40 bytes)
    bmp_info_header = struct.pack('<IHHIIIIIIII',
        40,           # biSize
        width,        # biWidth
        height * 2,   # biHeight (2x for ICO)
        1,            # biPlanes
        24,           # biBitCount
        0,            # biCompression (BI_RGB)
        0,            # biSizeImage
        0,            # biXPelsPerMeter
        0,            # biYPelsPerMeter
        0,            # biClrUsed
        0             # biClrImportant
    )

    # Create pixel data (gradient purple background with white clipboard)
    pixels = []
    for y in range(height):
        for x in range(width):
            # Create gradient purple background
            r = int(102 + (118 - 102) * x / width)
            g = int(126 + (75 - 126) * y / height)
            b = int(234 + (162 - 234) * (x + y) / (width + height))

            # Simple clipboard shape in center
            if 10 <= x <= 22 and 6 <= y <= 26:
                if 13 <= x <= 19 and 5 <= y <= 10:  # Clip
                    r, g, b = 70, 70, 75
                elif 12 <= x <= 20 and 18 <= y <= 22:  # Active item
                    r, g, b = 102, 126, 234
                else:  # Clipboard body
                    r, g, b = 245, 245, 245

            # Store as BGR
            pixels.extend([b, g, r])

    # Add padding for 32-bit alignment
    row_size = ((width * 24 + 31) // 32) * 4
    padding = row_size - (width * 3)

    pixel_data = b''
    for y in range(height):
        row_start = y * width * 3
        row_end = row_start + width * 3
        pixel_data += bytes(pixels[row_start:row_end])
        pixel_data += b'\x00' * padding

    # AND mask (1bpp, required for ICO)
    and_mask = b'\x00' * (width * height // 8)

    # Combine BMP header and pixel data
    bmp_data = bmp_info_header + pixel_data + and_mask
    data_size = len(bmp_data)

    # ICO directory entry
    ico_dir = struct.pack('<BBBBHHII',
        width,          # Width
        height,         # Height
        0,              # Color count (0 for >8bpp)
        0,              # Reserved
        1,              # Planes
        32,             # Bits per pixel
        data_size,      # Size of image data
        22              # Offset (6 + 16)
    )

    # Combine all parts
    ico_data = ico_header + ico_dir + bmp_data

    return ico_data

if __name__ == '__main__':
    ico_data = create_simple_ico()
    with open('icon.ico', 'wb') as f:
        f.write(ico_data)
    print(f'Generated icon.ico ({len(ico_data)} bytes)')
