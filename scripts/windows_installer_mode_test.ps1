$ErrorActionPreference = "Stop"
$Path = Join-Path $PSScriptRoot "build_windows_prod_clawbrowser.ps1"
$Tokens = $null
$Errors = $null
$Ast = [System.Management.Automation.Language.Parser]::ParseFile($Path, [ref]$Tokens, [ref]$Errors)
if ($Errors.Count -gt 0) { throw "Windows build script has parse errors: $Errors" }
$Definition = $Ast.Find({ param($Node)
  $Node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and
  $Node.Name -eq "Assert-WindowsInstallerBuildMode"
}, $true)
if (!$Definition) { throw "Installer reuse guard missing" }
# Execute this isolated pure guard, never the build script's entry point.
Invoke-Expression $Definition.Extent.Text
$Rejected = $false
try { Assert-WindowsInstallerBuildMode "Prod" $true }
catch {
  if ($_.Exception.Message -notlike "Cannot reuse an unverified*") { throw }
  $Rejected = $true
}
if (!$Rejected) { throw "Old production installer reuse was accepted" }
Assert-WindowsInstallerBuildMode "Prod" $false
Assert-WindowsInstallerBuildMode "Dev" $true
$Calls = @($Ast.FindAll({ param($Node)
  $Node -is [System.Management.Automation.Language.CommandAst] -and
  $Node.GetCommandName() -eq "Assert-WindowsInstallerBuildMode"
}, $true))
if ($Calls.Count -ne 1) { throw "Build entry point must invoke the guard exactly once" }
$StartupGuard = $Ast.Find({ param($Node)
  $Node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and
  $Node.Name -eq "Assert-ClawbrowserStartupHook"
}, $true)
if (!$StartupGuard) { throw "Clawbrowser startup-hook smoke guard missing" }
$StartupGuardText = $StartupGuard.Extent.Text
if (!$StartupGuardText.Contains('--list --json') -or
    !$StartupGuardText.Contains('CLAWBROWSER_CONFIG_DIR') -or
    !$StartupGuardText.Contains('WaitForExit(30000)')) {
  throw "Startup-hook smoke guard must execute the built browser with an isolated config and bounded timeout"
}
$StartupCalls = @($Ast.FindAll({ param($Node)
  $Node -is [System.Management.Automation.Language.CommandAst] -and
  $Node.GetCommandName() -eq "Assert-ClawbrowserStartupHook"
}, $true))
if ($StartupCalls.Count -ne 2) {
  throw "Fresh and staged browser paths must each invoke the startup-hook smoke guard"
}
$Stage = $Ast.Find({ param($Node)
  $Node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and
  $Node.Name -eq "Stage-WindowsSetupArchive"
}, $true)
if (!$Stage) { throw "Setup archive stage missing" }
$StageText = $Stage.Extent.Text
$CopyAt = $StageText.IndexOf('Copy-Item $MiniInstaller $SetupPath')
$VerifyAt = $StageText.IndexOf('windows_installer_payload.py')
$ZipAt = $StageText.IndexOf('Compress-Archive')
if ($CopyAt -lt 0 -or $VerifyAt -le $CopyAt -or $ZipAt -le $VerifyAt -or
    !$StageText.Contains('"--installer", $SetupPath')) {
  throw "Copied setup PE must pass payload verification before being archived"
}
Write-Host "PASS: fresh production build allowed, stale installer reuse rejected, development reuse allowed"
