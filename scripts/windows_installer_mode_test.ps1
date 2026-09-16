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
Write-Host "PASS: fresh production build allowed, stale installer reuse rejected, development reuse allowed"
