<#  MC_Cleanup.ps1
    Deletes:
      (1) contents of specified folders (keeps the folder)
      (2) files matching patterns in specified folders (keeps the folder)
      (3) explicitly listed files (full paths)

    Run with -WhatIf for a safe dry-run.

### Safe test first (recommended)
Run manually once:
```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "C:\Scripts\MC_Cleanup.ps1" -WhatIf
```
If it looks correct, run without `-WhatIf`.
#>

[CmdletBinding()]
param(
    [switch]$WhatIf
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# ----------------------- CONFIG (EDIT THESE) -----------------------

# (1) Folders to EMPTY (folder itself is kept)
$EmptyTheseFolders = @(
    'D:\Temp\Cache',
    'C:\SomeFolder\ToEmpty'
)

# (2) Delete specific file types inside a folder (folder is kept)
#     Recurse=$true means include subfolders.
$DeleteFiletypesInFolders = @(
    @{ Path = 'C:\Logs'; Pattern = '*.log'; Recurse = $true },
    @{ Path = 'D:\Work'; Pattern = '*.tmp'; Recurse = $false }
)

# (3) Explicit full file paths to delete (FILES ONLY)
$DeleteTheseFiles = @(
    'C:\SomeApp\old_trace.log',
    'D:\Work\hardcoded_file_to_delete.txt'
)

# Safety guard: refuse to operate on drive roots like C:\ or D:\
$RefuseDriveRoots = $true

# Optional: basic blocklist (add/remove as you wish)
$BlockedExactFolders = @(
    $env:SystemRoot,                 # usually C:\Windows
    $env:ProgramFiles,               # usually C:\Program Files
    ${env:ProgramFiles(x86)}         # usually C:\Program Files (x86)
) | Where-Object { $_ } | ForEach-Object { $_.TrimEnd('\') }

# --------------------- END CONFIG ----------------------------------


$LogPath = Join-Path $env:TEMP "MC_Cleanup_$(Get-Date -Format 'yyyyMMdd_HHmmss').log"
Start-Transcript -Path $LogPath | Out-Null

$hadErrors = $false

function Get-FullPathSafe([string]$Path) {
    # Returns a normalized full path even if it doesn't exist (if it’s syntactically valid)
    try { return [System.IO.Path]::GetFullPath($Path) } catch { return $null }
}

function Assert-SafeFolder([string]$Folder) {
    $full = Get-FullPathSafe $Folder
    if (-not $full) { throw "Invalid folder path: $Folder" }

    $trimmed = $full.TrimEnd('\')

    if ($RefuseDriveRoots) {
        # C:\  (length 3) or "C:" (length 2)
        if ($trimmed -match '^[A-Za-z]:$' -or $trimmed -match '^[A-Za-z]:\\$' -or $trimmed.Length -le 3) {
            throw "Refusing to operate on drive root (or too-high-level path): $full"
        }
    }

    if ($BlockedExactFolders -contains $trimmed) {
        throw "Refusing to operate on blocked folder: $full"
    }
}

function Clear-FolderContents([string]$Folder) {
    if (-not (Test-Path -LiteralPath $Folder -PathType Container)) {
        Write-Host "[SKIP] Folder not found: $Folder"
        return
    }

    Assert-SafeFolder $Folder

    Write-Host "[EMPTY] $Folder"
    # Delete everything INSIDE the folder, but not the folder itself:
    Get-ChildItem -LiteralPath $Folder -Force -ErrorAction Stop |
        Remove-Item -Force -Recurse -ErrorAction Stop -WhatIf:$WhatIf
}

function Delete-FilesByPattern([string]$Folder, [string]$Pattern, [bool]$Recurse) {
    if (-not (Test-Path -LiteralPath $Folder -PathType Container)) {
        Write-Host "[SKIP] Folder not found: $Folder"
        return
    }

    Assert-SafeFolder $Folder

    Write-Host "[DEL-TYPE] $Pattern in $Folder (Recurse=$Recurse)"

    $gciParams = @{
        LiteralPath = $Folder
        Filter      = $Pattern
        File        = $true
        Force       = $true
        ErrorAction = 'Stop'
    }
    if ($Recurse) { $gciParams.Recurse = $true }

    # Avoid traversing reparse points (junctions/symlinks) when recursing:
    $files = Get-ChildItem @gciParams | Where-Object { -not $_.Attributes.ToString().Contains('ReparsePoint') }

    if ($files.Count -eq 0) {
        Write-Host "  (none found)"
        return
    }

    $files | Remove-Item -Force -ErrorAction Stop -WhatIf:$WhatIf
}

function Delete-ExplicitFile([string]$FilePath) {
    if (-not (Test-Path -LiteralPath $FilePath -PathType Leaf)) {
        Write-Host "[SKIP] File not found: $FilePath"
        return
    }

    Write-Host "[DEL-FILE] $FilePath"
    Remove-Item -LiteralPath $FilePath -Force -ErrorAction Stop -WhatIf:$WhatIf
}

try {
    foreach ($folder in $EmptyTheseFolders) {
        try { Clear-FolderContents $folder } catch { $hadErrors = $true; Write-Host "[ERROR] $($_.Exception.Message)" }
    }

    foreach ($rule in $DeleteFiletypesInFolders) {
        try { Delete-FilesByPattern -Folder $rule.Path -Pattern $rule.Pattern -Recurse ([bool]$rule.Recurse) }
        catch { $hadErrors = $true; Write-Host "[ERROR] $($_.Exception.Message)" }
    }

    foreach ($file in $DeleteTheseFiles) {
        try { Delete-ExplicitFile $file } catch { $hadErrors = $true; Write-Host "[ERROR] $($_.Exception.Message)" }
    }
}
finally {
    Stop-Transcript | Out-Null
    Write-Host "Log: $LogPath"
}

if ($hadErrors) { exit 1 } else { exit 0 }