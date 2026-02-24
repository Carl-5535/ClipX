# ClipX Icon

This directory contains the ClipX application icon.

## Files

- `icon.svg` - Source vector icon (Scalable Vector Graphics)
- `icon.ico` - Windows icon file (needs to be generated)

## Generating icon.ico from icon.svg

### Method 1: Online Tools (Easiest)

1. Visit one of these online converters:
   - https://cloudconvert.com/svg-to-ico
   - https://convertio.co/svg-ico/
   - https://www.aconvert.com/image/svg-to-ico/

2. Upload `icon.svg`

3. Configure output settings:
   - Sizes: 16x16, 32x32, 48x48, 256x256
   - Format: ICO

4. Download and save as `icon.ico` in this directory

### Method 2: Using ImageMagick (Command Line)

```bash
# Install ImageMagick first, then run:
magick convert -background none icon.svg -define icon:auto-resize=256,48,32,16 icon.ico
```

### Method 3: Using GIMP

1. Open GIMP
2. File > Open > select `icon.svg`
3. Export As > `icon.ico`
4. In the export dialog, check all size options (16, 32, 48, 256)
5. Click Export

## After Generating icon.ico

Once you have `icon.ico` in this directory, rebuild the project:

```bash
cd build
cmake --build . --config Release
```

The icon will be automatically embedded in ClipD.exe and Overlay.exe.
