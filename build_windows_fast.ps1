param(
  [int]$Jobs = 0,
  [int]$CompilerCacheRetries = 20,
  [int]$ResourceRetryJobs = 8,
  [ValidateSet("sccache", "none")]
  [string]$CompilerCache = "sccache",
  [switch]$SkipBuild,
  [switch]$SkipZip
)

$ErrorActionPreference = "Stop"

function Resolve-DefaultJobs {
  $FallbackJobs = 12
  try {
    $Computer = Get-CimInstance Win32_ComputerSystem
    $LogicalProcessors = [int]$Computer.NumberOfLogicalProcessors
    $MemoryGb = [double]$Computer.TotalPhysicalMemory / 1GB
    if ($LogicalProcessors -le 0 -or $MemoryGb -le 0) {
      return $FallbackJobs
    }

    $MemoryLimitedJobs = [Math]::Max(8, [Math]::Ceiling($MemoryGb * 0.75))
    return [Math]::Max(1, [Math]::Min($LogicalProcessors, $MemoryLimitedJobs))
  } catch {
    return $FallbackJobs
  }
}

$ResolvedJobs = $Jobs
if ($ResolvedJobs -le 0) {
  $ResolvedJobs = Resolve-DefaultJobs
}

$BuildScript = Join-Path $PSScriptRoot "scripts\build_windows_prod_clawbrowser.ps1"
$BuildArguments = @{
  BuildProfile = "Fast"
  Jobs = $ResolvedJobs
  CompilerCacheRetries = $CompilerCacheRetries
  ResourceRetryJobs = $ResourceRetryJobs
  CompilerCache = $CompilerCache
  SkipBuild = $SkipBuild
  SkipZip = $SkipZip
}

Write-Host "Building Clawbrowser for Windows (Fast, jobs=$ResolvedJobs, cache=$CompilerCache)"
& $BuildScript @BuildArguments
