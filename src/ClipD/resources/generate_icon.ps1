# Simple PowerShell script to generate a basic ICO file for ClipX
# This creates a minimal 64x64 icon as a placeholder

Add-Type -AssemblyName System.Drawing

# Create a 64x64 bitmap
$bmp = New-Object System.Drawing.Bitmap 64, 64

# Use Graphics to draw
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = "AntiAlias"

# Background gradient
$rect = New-Object System.Drawing.Rectangle(0, 0, 64, 64)
$brush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(`
    $rect, `
    [System.Drawing.Color]::FromArgb(102, 126, 234), `
    [System.Drawing.Color]::FromArgb(118, 75, 162), `
    1.0, `
    [System.Drawing.Drawing2D.LinearGradientMode]::Vertical)
$g.FillRectangle($brush, $rect)

# Clipboard body
$clipboardBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(245, 245, 245))
$clipboardRect = New-Object System.Drawing.Rectangle(16, 12, 32, 42)
$g.FillRectangle($clipboardBrush, $clipboardRect)

# Clipboard clip (dark gray)
$clipBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(70, 70, 75))
$clipRect = New-Object System.Drawing.Rectangle(24, 8, 16, 10)
$g.FillRectangle($clipBrush, $clipRect)

# Draw lines representing content
$linePen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(180, 180, 185), 2)
$g.DrawLine($linePen, 20, 26, 44, 26)
$g.DrawLine($linePen, 20, 32, 40, 32)
$g.DrawLine($linePen, 20, 38, 42, 38)

# Draw active item with accent color
$activeBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(102, 126, 234))
$activeRect = New-Object System.Drawing.Rectangle(18, 42, 28, 8)
$g.FillRectangle($activeBrush, $activeRect)

# Save as ICO format
$ms = New-Object System.IO.MemoryStream
$bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
$pngBytes = $ms.ToArray()
$ms.Close()

# ICO file structure
$icoHeader = [byte[]]@(0, 0, 1, 0, 1, 0)  # Reserved, Type (1=ICO), Count
$iconDir = New-Object byte[] 16
$iconDir[0] = 64  # Width (256 = 0)
$iconDir[1] = 64  # Height
$iconDir[2] = 0   # Color palette (0 = no palette)
$iconDir[3] = 0   # Reserved
$iconDir[4] = 1   # Color planes
$iconDir[5] = 32  # Bits per pixel
$imageSize = $pngBytes.Length
$iconDir[6] = [byte]($imageSize % 256)
$iconDir[7] = [byte]([math]::Floor($imageSize / 256))
$iconDir[8] = [byte]([math]::Floor($imageSize / 65536))
$iconDir[9] = [byte]([math]::Floor($imageSize / 16777216))
$iconDir[10] = 0  # Reserved
$offset = 22
$iconDir[12] = [byte]($offset % 256)
$iconDir[13] = [byte]([math]::Floor($offset / 256))
$iconDir[14] = [byte]([math]::Floor($offset / 65536))
$iconDir[15] = [byte]([math]::Floor($offset / 16777216))

# Combine all parts
$icoData = New-Object byte[] (22 + $pngBytes.Length)
[System.Buffer]::BlockCopy($icoHeader, 0, $icoData, 0, 6)
[System.Buffer]::BlockCopy($iconDir, 0, $icoData, 6, 16)
[System.Buffer]::BlockCopy($pngBytes, 0, $icoData, 22, $pngBytes.Length)

[System.IO.File]::WriteAllBytes("icon.ico", $icoData)

$g.Dispose()
$bmp.Dispose()

Write-Host "icon.ico generated successfully at (64x64 PNG icon)"
