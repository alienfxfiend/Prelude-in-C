param([string]$ImagePath)

# Validate file exists
if (-not (Test-Path $ImagePath)) {
    Write-Error "File not found: $ImagePath"
    exit 1
}

# Set style: Stretch
Set-ItemProperty -Path 'HKCU:\Control Panel\Desktop' -Name WallpaperStyle -Value '2'
Set-ItemProperty -Path 'HKCU:\Control Panel\Desktop' -Name TileWallpaper -Value '0'

# Apply wallpaper via Win32 API
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Wallpaper {
    [DllImport("user32.dll", CharSet = CharSet.Auto)]
    public static extern int SystemParametersInfo(int uAction, int uParam, string lpvParam, int fuWinIni);
}
"@ -ErrorAction SilentlyContinue

# SPI_SETDESKWALLPAPER = 0x0014 (20)
# SPIF_UPDATEINIFILE | SPIF_SENDCHANGE = 3
[Wallpaper]::SystemParametersInfo(20, 0, $ImagePath, 3)

<#
Style Reference:
  Stretch : WallpaperStyle=2,  TileWallpaper=0
  Fill    : WallpaperStyle=10, TileWallpaper=0
  Fit     : WallpaperStyle=6,  TileWallpaper=0
  Center  : WallpaperStyle=0,  TileWallpaper=0
  Tile    : WallpaperStyle=0,  TileWallpaper=1
  Span    : WallpaperStyle=22, TileWallpaper=0
#>

<#
Stretch: `WallpaperStyle=2`, `TileWallpaper=0`  (used above)
Fill: `WallpaperStyle=10`, `TileWallpaper=0`
Fit: `WallpaperStyle=6`, `TileWallpaper=0`
Center: `WallpaperStyle=0`, `TileWallpaper=0`
Tile: `WallpaperStyle=0`, `TileWallpaper=1`
#>