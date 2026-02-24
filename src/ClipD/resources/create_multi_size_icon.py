#!/usr/bin/env python3
"""
Create a proper Windows ICO file with multiple sizes (16x16, 32x32, 48x48)
"""

import struct

def create_icon_data(width, height):
    """Create bitmap data for one icon size"""

    # BITMAPINFOHEADER (40 bytes)
    bmp_header = struct.pack('<IIIHHIIIIII',
        40,              # biSize
        width,           # biWidth
        height * 2,      # biHeight (double for XOR+AND masks)
        1,               # biPlanes
        32,              # biBitCount (32-bit RGBA for better quality)
        0,               # biCompression (BI_RGB)
        0,               # biSizeImage
        0, 0,            # biXPelsPerMeter, biYPelsPerMeter
        0, 0             # biClrUsed, biClrImportant
    )

    # Create pixel data (BGRA format)
    pixels = []
    for y in range(height):
        for x in range(width):
            # Scale coordinates
            fx = x / width
            fy = y / height

            # Gradient purple background
            r = int(102 + (118 - 102) * fx)
            g = int(126 + (75 - 126) * fy)
            b = int(234 + (162 - 234) * (fx + fy) / 2)
            a = 255

            # Draw clipboard
            if 0.3 <= fx <= 0.75 and 0.2 <= fy <= 0.85:
                # Clip (dark gray)
                if 0.4 <= fx <= 0.6 and 0.15 <= fy <= 0.35:
                    r, g, b = 70, 70, 75
                # Active item (blue)
                elif 0.35 <= fx <= 0.7 and 0.55 <= fy <= 0.7:
                    r, g, b = 102, 126, 234
                # Clipboard body (white)
                else:
                    r, g, b = 245, 245, 245

            # Store as BGRA
            pixels.extend([b, g, r, a])

    # Convert to bytes
    pixel_data = bytes(pixels)

    # Calculate row size (must be multiple of 4)
    bytes_per_pixel = 4
    row_size = ((width * bytes_per_pixel + 3) // 4) * 4

    # Pad each row to alignment
    aligned_pixels = b''
    for y in range(height):
        row_start = y * width * bytes_per_pixel
        row_end = row_start + width * bytes_per_pixel
        row_data = pixel_data[row_start:row_end]
        # Add padding
        padding = row_size - (width * bytes_per_pixel)
        aligned_pixels += row_data + b'\x00' * padding

    # AND mask (required but all transparent for 32-bit icons)
    and_mask_size = ((width + 31) // 32) * 4 * height
    and_mask = b'\x00' * and_mask_size

    return bmp_header + aligned_pixels + and_mask

def create_ico_file():
    """Create ICO file with multiple sizes"""

    sizes = [16, 32, 48]
    images = []
    data_offset = 6 + (16 * len(sizes))  # Header + directory entries

    for size in sizes:
        data = create_icon_data(size, size)
        images.append((size, data))

    # ICO header
    header = struct.pack('<HHH', 0, 1, len(sizes))

    # Directory entries
    directory = b''
    for size, data in images:
        directory += struct.pack('<BBBBHHII',
            size if size < 256 else 0,  # Width
            size if size < 256 else 0,  # Height
            0,                          # Color count
            0,                          # Reserved
            1,                          # Planes
            32,                         # Bits per pixel
            len(data),                  # Size
            data_offset                 # Offset
        )
        data_offset += len(data)

    # Combine all
    ico_data = header + directory
    for _, data in images:
        ico_data += data

    return ico_data

if __name__ == '__main__':
    ico_data = create_ico_file()

    filename = 'icon.ico'
    with open(filename, 'wb') as f:
        f.write(ico_data)

    print(f'Generated {filename} ({len(ico_data)} bytes)')
    print(f'Contains: 16x16, 32x32, 48x48 pixels (32-bit BGRA)')
