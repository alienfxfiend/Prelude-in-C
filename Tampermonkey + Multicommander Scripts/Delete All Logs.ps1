# --- DEFINE THE FUNCTION for RULE-BASED SILENT PERMANENT DELETION ---
function Start-SilentPermanentCleanup {
    [CmdletBinding()]
    param (
        [Parameter(Mandatory = $true)]
        [object[]]$Rules
    )

    Write-Host "Preparing to SILENTLY and PERMANENTLY DELETE items based on defined rules..." -ForegroundColor Red

    # --- Process each rule ---
    foreach ($rule in $Rules) {
        # Use a switch statement to handle different rule types
        switch ($rule.RuleType) {
            # Rule: Empty an entire folder of all its contents
            'EmptyFolder' {
                Write-Host "`n--- Processing Rule: Empty Folder ---" -ForegroundColor Magenta
                Write-Host "Target: $($rule.Path)"
                if (Test-Path $rule.Path -PathType Container) {
                    $itemsInFolder = @(Get-ChildItem -Path $rule.Path -Force)
                    if ($itemsInFolder.Count -gt 0) {
                        foreach ($item in $itemsInFolder) {
                            try {
                                Remove-Item -Path $item.FullName -Recurse -Force -Confirm:$false
                                Write-Host "  -> PERMANENTLY DELETED: $($item.Name)"
                            } catch {
                                Write-Error "Failed to delete '$($item.FullName)'. Error: $_"
                            }
                        }
                    } else {
                        Write-Host "  Folder is already empty." -ForegroundColor Green
                    }
                } else {
                    Write-Warning "Folder '$($rule.Path)' does not exist. Skipping rule."
                }
            }

            # Rule: Delete a single, specific file
            'SpecificFile' {
                Write-Host "`n--- Processing Rule: Delete Specific File ---" -ForegroundColor Magenta
                Write-Host "Target: $($rule.Path)"
                if (Test-Path $rule.Path -PathType Leaf) {
                    try {
                        Remove-Item -Path $rule.Path -Force -Confirm:$false
                        Write-Host "  -> PERMANENTLY DELETED: $($rule.Path)"
                    } catch {
                        Write-Error "Failed to delete '$($rule.Path)'. Error: $_"
                    }
                } else {
                    Write-Warning "File '$($rule.Path)' does not exist. Skipping rule."
                }
            }

            # Rule: Delete files matching a pattern (e.g., *.log) within a folder
            'DeleteByPattern' {
                Write-Host "`n--- Processing Rule: Delete Files by Pattern ---" -ForegroundColor Magenta
                Write-Host "Target: $($rule.Path) | Pattern: $($rule.Pattern)"
                if (Test-Path $rule.Path -PathType Container) {
                    # Get only files (-File) that match the filter/pattern
                    $filesToDelete = @(Get-ChildItem -Path $rule.Path -Filter $rule.Pattern -File -Force)
                    if ($filesToDelete.Count -gt 0) {
                        foreach ($file in $filesToDelete) {
                            try {
                                Remove-Item -Path $file.FullName -Force -Confirm:$false
                                Write-Host "  -> PERMANENTLY DELETED: $($file.Name)"
                            } catch {
                                Write-Error "Failed to delete '$($file.FullName)'. Error: $_"
                            }
                        }
                    } else {
                        Write-Host "  No files matching pattern '$($rule.Pattern)' found in folder." -ForegroundColor Green
                    }
                } else {
                    Write-Warning "Folder '$($rule.Path)' does not exist. Skipping rule."
                }
            }

            # Default case for any unknown rule types
            default {
                Write-Warning "Unknown RuleType '$($rule.RuleType)' for path '$($rule.Path)'. Skipping rule."
            }
        }
    }

    Write-Host "`nSilent deletion process complete." -ForegroundColor Green
}
# --- END OF FUNCTION DEFINITION ---


# --- DEFINE YOUR CLEANUP RULES HERE ---
#
# Each rule is a PowerShell object with the following properties:
#   - Path      : The file or folder path to act upon.
#   - RuleType  : What to do. Valid options are:
#                 'EmptyFolder'     - Deletes all contents of the folder.
#                 'SpecificFile'    - Deletes the single file specified.
#                 'DeleteByPattern' - Deletes files in a folder that match a pattern.
#   - Pattern   : (Required for 'DeleteByPattern') The file pattern to match (e.g., '*.log', 'temp_*.*').
#
# To add a new rule, just add a new [PSCustomObject] to the array below.
#
$cleanupRules = @(
    # --- Rules to Empty Folders ---
    [PSCustomObject]@{ Path = "D:\Games\AOE II HD\Logs\";                          RuleType = 'EmptyFolder' },
    [PSCustomObject]@{ Path = "D:\Games\AOMX-TOTD\Logs\";                        RuleType = 'EmptyFolder' },
    [PSCustomObject]@{ Path = "D:\Games\ORA-RV\Support\Replays\rv\";             RuleType = 'EmptyFolder' },
    [PSCustomObject]@{ Path = "D:\Games\ORA-SP\Support\Replays\sp\";             RuleType = 'EmptyFolder' },
    [PSCustomObject]@{ Path = "D:\Games\ORA\Support\Replays\cnc\";               RuleType = 'EmptyFolder' },
    [PSCustomObject]@{ Path = "D:\Games\ORA\Support\Replays\d2k\";               RuleType = 'EmptyFolder' },
    [PSCustomObject]@{ Path = "D:\Games\ORA\Support\Replays\ra\";                RuleType = 'EmptyFolder' },

    # --- Rules to Delete Specific Files ---
    [PSCustomObject]@{ Path = "D:\Games\Delta Force 3\DDrawCompat-DFLW.log";               RuleType = 'SpecificFile' },
    [PSCustomObject]@{ Path = "D:\Games\Delta Force 4\DDrawCompat-DFTFD.log";              RuleType = 'SpecificFile' },
    [PSCustomObject]@{ Path = "D:\Games\Gangsters\dxwrapper-gangsters.log";                RuleType = 'SpecificFile' },
    [PSCustomObject]@{ Path = "D:\Games\Gearhead Garage\Output.log";                       RuleType = 'SpecificFile' },
    [PSCustomObject]@{ Path = "D:\Games\GTA-VC\cleo.log";                                  RuleType = 'SpecificFile' },
    [PSCustomObject]@{ Path = "D:\Games\GTA-VC\DDrawCompat-gta-vc.log";                    RuleType = 'SpecificFile' },
    [PSCustomObject]@{ Path = "D:\Games\GTAIII\DDrawCompat-gta3.log";                      RuleType = 'SpecificFile' },
    [PSCustomObject]@{ Path = "D:\Games\GTAIII\DDrawCompat-Gta3_MyTy_Mo_NOCD_Crack.log";   RuleType = 'SpecificFile' },
    [PSCustomObject]@{ Path = "D:\Games\Midtown Madness\Open1560.log";                     RuleType = 'SpecificFile' },
    [PSCustomObject]@{ Path = "D:\Games\Midtown-Madness-2\mm2hook.log";                    RuleType = 'SpecificFile' },
    [PSCustomObject]@{ Path = "D:\Games\Monopoly 2008\DDrawCompat-MonopolyPB.log";         RuleType = 'SpecificFile' },
    [PSCustomObject]@{ Path = "D:\Games\Monopoly 2008\MonopolyPB_crash.log";               RuleType = 'SpecificFile' },
    [PSCustomObject]@{ Path = "D:\Games\Monopoly Here And Now Edition\DDrawCompat-Monopoly.log"; RuleType = 'SpecificFile' },
    [PSCustomObject]@{ Path = "D:\Games\Moto\DDrawCompat-MCM2.log";                        RuleType = 'SpecificFile' },
    [PSCustomObject]@{ Path = "D:\Games\TestDrv6\DDrawCompat-TD6.log";                     RuleType = 'SpecificFile' },
    [PSCustomObject]@{ Path = "D:\Games\The Sims\DDrawCompat-Sims.log";                    RuleType = 'SpecificFile' },
    [PSCustomObject]@{ Path = "D:\Games\Virtual Pool 3 DL\DDrawCompat-vp3.log";            RuleType = 'SpecificFile' },

    # --- NEW Rule to Delete *.log files inside a specific folder ---
    # I've used the path that was commented out in your original script as an example.
    #[PSCustomObject]@{ Path = "D:\Games\SWGBS\Game\Logs\"; RuleType = 'DeleteByPattern'; Pattern = '*.log' }
    
    # You can easily add more pattern-based rules like this:
    # [PSCustomObject]@{ Path = "C:\Windows\Temp\"; RuleType = 'DeleteByPattern'; Pattern = '*.tmp' }
)
# --- END OF DEFINITIONS ---


# --- EXECUTE THE SCRIPT ---
Start-SilentPermanentCleanup -Rules $cleanupRules

# --- Keeps the window open after execution ---
Read-Host -Prompt "Execution complete. Press Enter to exit."