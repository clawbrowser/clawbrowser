param(
  [ValidateSet("prod", "fast", "both")]
  [string]$Profile = "prod",
  [int]$Jobs = 0,
  [ValidateSet("sccache", "none")]
  [string]$CompilerCache = "sccache",
  [switch]$SkipBuild,
  [switch]$SkipZip
)

$ErrorActionPreference = "Stop"

$CommonArguments = @{
  Jobs = $Jobs
  CompilerCache = $CompilerCache
  SkipBuild = $SkipBuild
  SkipZip = $SkipZip
}

switch ($Profile) {
  "prod" {
    & (Join-Path $PSScriptRoot "build_windows_prod.ps1") @CommonArguments
  }
  "fast" {
    & (Join-Path $PSScriptRoot "build_windows_fast.ps1") @CommonArguments
  }
  "both" {
    & (Join-Path $PSScriptRoot "build_windows_fast.ps1") @CommonArguments
    & (Join-Path $PSScriptRoot "build_windows_prod.ps1") @CommonArguments
  }
}
