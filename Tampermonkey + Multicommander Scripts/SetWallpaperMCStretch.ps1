param([string]$ImagePath)

# Load WinForms so MessageBox works
# Add-Type -AssemblyName System.Windows.Forms

# Debug: Show what path was received (remove this line after testing)
# [System.Windows.Forms.MessageBox]::Show("Wallpaper changed to:`n$ImagePath", "Success")
# [System.Windows.Forms.MessageBox]::Show($ImagePath)

Set-ItemProperty -Path 'HKCU:\Control Panel\Desktop' -Name WallpaperStyle -Value '2'
Set-ItemProperty -Path 'HKCU:\Control Panel\Desktop' -Name TileWallpaper -Value '0'

$code = @"
using System;
using System.Runtime.InteropServices;
public class Wallpaper {
    [DllImport("user32.dll", CharSet = CharSet.Auto)]
    public static extern int SystemParametersInfo(int uAction, int uParam, string lpvParam, int fuWinIni);
}
"@

Add-Type -TypeDefinition $code -ErrorAction SilentlyContinue
[Wallpaper]::SystemParametersInfo(20, 0, $ImagePath, 3)

<#
Stretch: `WallpaperStyle=2`, `TileWallpaper=0`  (used above)
Fill: `WallpaperStyle=10`, `TileWallpaper=0`
Fit: `WallpaperStyle=6`, `TileWallpaper=0`
Center: `WallpaperStyle=0`, `TileWallpaper=0`
Tile: `WallpaperStyle=0`, `TileWallpaper=1`
#>