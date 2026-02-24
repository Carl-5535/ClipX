# Create desktop shortcut for ClipX
$WshShell = New-Object -ComObject WScript.Shell
$Desktop = $WshShell.SpecialFolders("Desktop")
$Shortcut = $WshShell.CreateShortcut("$Desktop\ClipX.lnk")
$Shortcut.TargetPath = "D:\project\AI\ClipX\build\bin\Release\ClipD.exe"
$Shortcut.WorkingDirectory = "D:\project\AI\ClipX\build\bin\Release"
$Shortcut.Description = "ClipX Clipboard Manager"
$Shortcut.Save()

Write-Host "Desktop shortcut created: $Desktop\ClipX.lnk"
Write-Host ""
Write-Host "Icon should now be visible on the desktop."
Write-Host "If the icon still doesn't show correctly:"
Write-Host "1. Right-click the shortcut and select Properties"
Write-Host "2. Click 'Change Icon...'"
Write-Host "3. Browse to: D:\project\AI\ClipX\build\bin\Release\ClipD.exe"
