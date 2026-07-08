param(
  [string]$ChromiumSrc = "",
  [string]$ProjectRoot = "",
  [string]$DepotTools = "",
  [string]$VsInstall = "",
  [string]$ConfigPath = "",
  [ValidateSet("Prod", "Fast", "Both")]
  [string]$BuildProfile = "Prod",
  [ValidateSet("sccache", "none")]
  [string]$CompilerCache = "sccache",
  [string]$CompilerCacheDir = "",
  [string]$CompilerCacheSize = "50G",
  [int]$CompilerCacheRetries = 10,
  [int]$ResourceRetryJobs = 8,
  [string]$BuildDir = "out\CBProd",
  [string]$ArtifactRoot = "",
  [int]$Jobs = 0,
  [string]$BundleVersion = "",
  [switch]$SkipBuild,
  [switch]$StageExistingArtifacts,
  [switch]$SkipZip
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

function Resolve-ConfigPath {
  if ($ConfigPath) {
    return $ConfigPath
  }
  if ($env:CLAWBROWSER_WINDOWS_BUILD_CONFIG) {
    return $env:CLAWBROWSER_WINDOWS_BUILD_CONFIG
  }
  if ($env:LOCALAPPDATA) {
    return Join-Path $env:LOCALAPPDATA "Clawbrowser\windows-build.json"
  }
  return Join-Path $HOME ".clawbrowser-windows-build.json"
}

function Read-BuildConfig($Path) {
  if (!$Path -or !(Test-Path $Path -PathType Leaf)) {
    return $null
  }
  try {
    return Get-Content $Path -Raw | ConvertFrom-Json
  } catch {
    Write-Warning "Ignoring unreadable build config: $Path"
    return $null
  }
}

function Save-BuildConfig($Path, $ResolvedChromiumSrc, $ResolvedDepotTools, $ResolvedVsInstall) {
  if (!$Path) {
    return
  }

  $Mutex = [System.Threading.Mutex]::new($false, "Local\ClawbrowserWindowsBuildConfig")
  $HasMutex = $false
  try {
    $HasMutex = $Mutex.WaitOne([TimeSpan]::FromSeconds(15))
    if (!$HasMutex) {
      Write-Warning "Skipping build config cache write because another build process is using it: $Path"
      return
    }

    $Parent = Split-Path -Parent $Path
    if ($Parent) {
      New-Item -ItemType Directory -Force $Parent | Out-Null
    }

    $Json = [pscustomobject]@{
      ChromiumSrc = $ResolvedChromiumSrc
      DepotTools = $ResolvedDepotTools
      VsInstall = $ResolvedVsInstall
    } | ConvertTo-Json

    $TempPath = "$Path.$PID.tmp"
    Set-Content -Encoding ASCII -Path $TempPath -Value $Json
    Move-Item -Force -Path $TempPath -Destination $Path
  } catch {
    Write-Warning "Could not update build config cache $Path`: $($_.Exception.Message)"
  } finally {
    if ($HasMutex) {
      $Mutex.ReleaseMutex()
    }
    $Mutex.Dispose()
  }
}

function Resolve-Directory($Path, $Label) {
  if (!$Path) {
    throw "$Label was not provided."
  }
  if (!(Test-Path $Path -PathType Container)) {
    throw "$Label does not exist: $Path"
  }
  return (Resolve-Path $Path).Path
}

function Resolve-ProjectRoot {
  if ($ProjectRoot) {
    return Resolve-Directory $ProjectRoot "ProjectRoot"
  }
  return (Resolve-Path (Join-Path $ScriptDir "..")).Path
}

function Test-ChromiumSrcPath($Path) {
  return $Path -and
      (Test-Path (Join-Path $Path "chrome\BUILD.gn") -PathType Leaf) -and
      (Test-Path (Join-Path $Path "build\config\BUILDCONFIG.gn") -PathType Leaf) -and
      (Test-Path (Join-Path $Path ".gn") -PathType Leaf)
}

function Find-ChromiumSrcFrom($StartPath) {
  if (!$StartPath -or !(Test-Path $StartPath)) {
    return ""
  }

  $Candidate = (Resolve-Path $StartPath).Path
  while ($Candidate) {
    if (Test-ChromiumSrcPath $Candidate) {
      return $Candidate
    }

    $Parent = Split-Path -Parent $Candidate
    if (!$Parent -or $Parent -eq $Candidate) {
      break
    }
    $Candidate = $Parent
  }

  return ""
}

function Test-DepotToolsPath($Path) {
  return $Path -and
      (Test-Path (Join-Path $Path "gn.bat") -PathType Leaf) -and
      (Test-Path (Join-Path $Path "gclient.bat") -PathType Leaf) -and
      (Test-Path (Join-Path $Path "python3_bin_reldir.txt") -PathType Leaf)
}

function Resolve-ChromiumSrc {
  if ($ChromiumSrc) {
    return Resolve-Directory $ChromiumSrc "ChromiumSrc"
  }
  if ($env:CHROMIUM_SRC) {
    return Resolve-Directory $env:CHROMIUM_SRC "CHROMIUM_SRC"
  }
  if ($env:CHROMIUM_DIR) {
    return Resolve-Directory (Join-Path $env:CHROMIUM_DIR "src") "CHROMIUM_DIR\src"
  }
  if ($BuildConfig -and $BuildConfig.ChromiumSrc) {
    return Resolve-Directory $BuildConfig.ChromiumSrc "build config ChromiumSrc"
  }
  $FromCurrentDirectory = Find-ChromiumSrcFrom (Get-Location).Path
  if ($FromCurrentDirectory) {
    return $FromCurrentDirectory
  }
  throw "Pass -ChromiumSrc once, set CHROMIUM_SRC / CHROMIUM_DIR, run from inside chromium/src once, or create the build config at $BuildConfigPath."
}

function Resolve-DepotTools {
  if ($DepotTools) {
    return Resolve-Directory $DepotTools "DepotTools"
  }
  if ($env:DEPOT_TOOLS_DIR) {
    return Resolve-Directory $env:DEPOT_TOOLS_DIR "DEPOT_TOOLS_DIR"
  }
  if ($BuildConfig -and $BuildConfig.DepotTools -and (Test-DepotToolsPath $BuildConfig.DepotTools)) {
    return Resolve-Directory $BuildConfig.DepotTools "build config DepotTools"
  }
  $GnCommand = Get-Command "gn.bat" -ErrorAction SilentlyContinue
  if ($GnCommand -and (Test-DepotToolsPath (Split-Path -Parent $GnCommand.Source))) {
    return Split-Path -Parent $GnCommand.Source
  }
  if ($ChromiumSrc) {
    $ChromiumCheckoutRoot = Split-Path -Parent $ChromiumSrc
    $ChromiumCheckoutParent = Split-Path -Parent $ChromiumCheckoutRoot
    foreach ($Candidate in @(
        (Join-Path $ChromiumCheckoutParent "depot_tools"),
        (Join-Path $ChromiumCheckoutRoot "depot_tools")
      )) {
      if (Test-DepotToolsPath $Candidate) {
        return (Resolve-Path $Candidate).Path
      }
    }

    $VendoredDepotTools = Join-Path $ChromiumSrc "third_party\depot_tools"
    if (Test-DepotToolsPath $VendoredDepotTools) {
      return (Resolve-Path $VendoredDepotTools).Path
    }
  }
  throw "Pass -DepotTools, set DEPOT_TOOLS_DIR, or put depot_tools on PATH."
}

function Find-VsWhere {
  $Command = Get-Command "vswhere.exe" -ErrorAction SilentlyContinue
  if ($Command) {
    return $Command.Source
  }

  foreach ($Root in @(${env:ProgramFiles(x86)}, $env:ProgramFiles)) {
    if (!$Root) {
      continue
    }
    $Candidate = Join-Path $Root "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $Candidate -PathType Leaf) {
      return $Candidate
    }
  }

  return ""
}

function Test-VsCppInstall($Path) {
  if (!$Path -or !(Test-Path $Path -PathType Container)) {
    return $false
  }

  $VcVarsAll = Join-Path $Path "VC\Auxiliary\Build\vcvarsall.bat"
  if (!(Test-Path $VcVarsAll -PathType Leaf)) {
    return $false
  }

  $MsvcRoot = Join-Path $Path "VC\Tools\MSVC"
  if (!(Test-Path $MsvcRoot -PathType Container)) {
    return $false
  }

  $Compiler = Get-ChildItem $MsvcRoot -Filter cl.exe -Recurse -ErrorAction SilentlyContinue |
      Where-Object { $_.FullName -match "\\bin\\Hostx64\\x64\\cl\.exe$" } |
      Select-Object -First 1

  return [bool]$Compiler
}

function Find-VsInstallByFiles {
  $Roots = @()
  foreach ($Root in @(${env:ProgramFiles(x86)}, $env:ProgramFiles)) {
    if (!$Root) {
      continue
    }
    $VisualStudioRoot = Join-Path $Root "Microsoft Visual Studio"
    if (Test-Path $VisualStudioRoot -PathType Container) {
      $Roots += $VisualStudioRoot
    }
  }

  foreach ($Root in $Roots) {
    $Candidates = Get-ChildItem $Root -Directory -ErrorAction SilentlyContinue |
        ForEach-Object { Get-ChildItem $_.FullName -Directory -ErrorAction SilentlyContinue } |
        Sort-Object FullName -Descending
    foreach ($Candidate in $Candidates) {
      if (Test-VsCppInstall $Candidate.FullName) {
        return $Candidate.FullName
      }
    }
  }

  return ""
}

function Resolve-VsInstall {
  if ($VsInstall) {
    return Resolve-Directory $VsInstall "VsInstall"
  }
  if ($env:GYP_MSVS_OVERRIDE_PATH) {
    return Resolve-Directory $env:GYP_MSVS_OVERRIDE_PATH "GYP_MSVS_OVERRIDE_PATH"
  }
  if ($env:vs2026_install) {
    return Resolve-Directory $env:vs2026_install "vs2026_install"
  }
  if ($env:vs2022_install) {
    return Resolve-Directory $env:vs2022_install "vs2022_install"
  }
  if ($BuildConfig -and $BuildConfig.VsInstall -and (Test-VsCppInstall $BuildConfig.VsInstall)) {
    return Resolve-Directory $BuildConfig.VsInstall "build config VsInstall"
  }

  $VsWhere = Find-VsWhere
  if ($VsWhere) {
    $Detected = & $VsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($Detected -and (Test-VsCppInstall $Detected.Trim())) {
      return Resolve-Directory $Detected.Trim() "Visual Studio C++ installation"
    }
  }

  $DetectedByFiles = Find-VsInstallByFiles
  if ($DetectedByFiles) {
    return Resolve-Directory $DetectedByFiles "Visual Studio C++ installation"
  }

  throw "Pass -VsInstall, set GYP_MSVS_OVERRIDE_PATH / vs2026_install, or install Visual Studio C++ tools."
}

function Find-Executable($Name) {
  $Command = Get-Command $Name -ErrorAction SilentlyContinue
  if ($Command) {
    return $Command.Source
  }
  return ""
}

function Find-Sccache {
  if ($env:SCCACHE_PATH -and (Test-Path $env:SCCACHE_PATH -PathType Leaf)) {
    return (Resolve-Path $env:SCCACHE_PATH).Path
  }

  foreach ($Name in @("sccache.exe", "sccache")) {
    $CommandPath = Find-Executable $Name
    if ($CommandPath) {
      return $CommandPath
    }
  }

  $CandidateRoots = @()
  if ($env:LOCALAPPDATA) {
    $CandidateRoots += (Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Links")
    $CandidateRoots += (Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Packages")
  }
  if ($env:ProgramFiles) {
    $CandidateRoots += (Join-Path $env:ProgramFiles "WinGet\Links")
  }

  foreach ($Root in $CandidateRoots) {
    if (!(Test-Path $Root -PathType Container)) {
      continue
    }
    $Candidate = Get-ChildItem $Root -Filter sccache.exe -Recurse -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($Candidate) {
      return $Candidate.FullName
    }
  }

  return ""
}

function Resolve-CompilerCache {
  if ($CompilerCache -eq "none") {
    return ""
  }

  $SccachePath = Find-Sccache
  if (!$SccachePath) {
    throw "sccache is required for Windows compiler caching. Install Mozilla.sccache with winget, put sccache on PATH, or run with -CompilerCache none."
  }

  $SccacheDirPath = Split-Path -Parent $SccachePath
  $env:Path = "$SccacheDirPath;$env:Path"

  if (!$CompilerCacheDir) {
    if ($env:SCCACHE_DIR) {
      $CompilerCacheDir = $env:SCCACHE_DIR
    } elseif ($env:LOCALAPPDATA) {
      $CompilerCacheDir = Join-Path $env:LOCALAPPDATA "Clawbrowser\sccache"
    } else {
      $CompilerCacheDir = Join-Path $HOME ".cache\clawbrowser\sccache"
    }
  }

  New-Item -ItemType Directory -Force $CompilerCacheDir | Out-Null
  $env:SCCACHE_DIR = (Resolve-Path $CompilerCacheDir).Path
  if ($CompilerCacheSize) {
    $env:SCCACHE_CACHE_SIZE = $CompilerCacheSize
  }
  if (!$env:SCCACHE_IDLE_TIMEOUT) {
    $env:SCCACHE_IDLE_TIMEOUT = "86400"
  }
  if (!$env:SCCACHE_IGNORE_SERVER_IO_ERROR) {
    $env:SCCACHE_IGNORE_SERVER_IO_ERROR = "1"
  }
  if (!$env:SCCACHE_DIRECT) {
    $env:SCCACHE_DIRECT = "false"
  }
  if (!$env:SCCACHE_ERROR_LOG) {
    $CacheParent = Split-Path -Parent $env:SCCACHE_DIR
    $env:SCCACHE_ERROR_LOG = Join-Path $CacheParent "sccache-error.log"
  }

  $script:CompilerCacheExecutable = $SccachePath
  return "sccache"
}

function Initialize-CompilerCacheServer([switch]$Restart) {
  if (!$CompilerCacheWrapper -or !$script:CompilerCacheExecutable) {
    return
  }

  if ($Restart) {
    Write-Host "Restarting compiler cache server..."
    try {
      & $script:CompilerCacheExecutable --stop-server *> $null
    } catch {
    }
    Start-Sleep -Seconds 2
  } else {
    Write-Host "Starting compiler cache server..."
  }

  $PreviousErrorActionPreference = $ErrorActionPreference
  $ErrorActionPreference = "Continue"
  try {
    $Output = & $script:CompilerCacheExecutable --start-server 2>&1
    $ExitCode = $LASTEXITCODE
  } finally {
    $ErrorActionPreference = $PreviousErrorActionPreference
  }
  $OutputText = ($Output | Out-String)
  if ($Output -and $OutputText -notmatch "Address in use") {
    $Output | ForEach-Object { Write-Host $_ }
  }
  if ($ExitCode -eq 0) {
    Write-Host "Compiler cache server is running."
    return
  }

  if ($OutputText -match "Address in use") {
    if (Test-CompilerCacheServer) {
      Write-Host "Compiler cache server is already running."
      return
    }
  }

  throw "$script:CompilerCacheExecutable --start-server failed with exit code $ExitCode."
}

function Restart-CompilerCacheServer {
  if (!$CompilerCacheWrapper -or !$script:CompilerCacheExecutable) {
    return
  }

  Initialize-CompilerCacheServer -Restart
}

function Test-CompilerCacheServer {
  if (!$CompilerCacheWrapper -or !$script:CompilerCacheExecutable) {
    return $true
  }

  $PreviousErrorActionPreference = $ErrorActionPreference
  $ErrorActionPreference = "Continue"
  try {
    & $script:CompilerCacheExecutable --show-stats *> $null
    return ($LASTEXITCODE -eq 0)
  } finally {
    $ErrorActionPreference = $PreviousErrorActionPreference
  }
}

function Get-BuildSpecs {
  $BuildDirWasProvided = $PSBoundParameters.ContainsKey("BuildDir")

  if ($BuildProfile -eq "Both") {
    if ($BuildDirWasProvided) {
      throw "-BuildDir cannot be combined with -BuildProfile Both."
    }
    return @(
      [pscustomobject]@{ Profile = "Fast"; BuildDir = "out\CBFast"; ArtifactName = "clawbrowser-fast-windows-x64" },
      [pscustomobject]@{ Profile = "Prod"; BuildDir = "out\CBProd"; ArtifactName = "clawbrowser-prod-windows-x64" }
    )
  }

  $ResolvedBuildDir = $BuildDir
  if (!$BuildDirWasProvided -and $BuildProfile -eq "Fast") {
    $ResolvedBuildDir = "out\CBFast"
  }

  $ArtifactName = if ($BuildProfile -eq "Fast") {
    "clawbrowser-fast-windows-x64"
  } else {
    "clawbrowser-prod-windows-x64"
  }

  return @([pscustomobject]@{ Profile = $BuildProfile; BuildDir = $ResolvedBuildDir; ArtifactName = $ArtifactName })
}

$BuildConfigPath = Resolve-ConfigPath
$BuildConfig = Read-BuildConfig $BuildConfigPath
$ProjectRoot = Resolve-ProjectRoot
$ChromiumSrc = Resolve-ChromiumSrc
$DepotTools = Resolve-DepotTools
$VsInstall = Resolve-VsInstall
$CompilerCacheWrapper = Resolve-CompilerCache
Save-BuildConfig $BuildConfigPath $ChromiumSrc $DepotTools $VsInstall

if (!$ArtifactRoot) {
  $ArtifactRoot = Join-Path (Split-Path -Parent $ChromiumSrc) "artifacts"
}
$ArtifactRoot = if (Test-Path $ArtifactRoot) {
  (Resolve-Path $ArtifactRoot).Path
} else {
  New-Item -ItemType Directory -Force $ArtifactRoot | Out-Null
  (Resolve-Path $ArtifactRoot).Path
}

$env:Path = "$DepotTools;$env:Path"
$env:DEPOT_TOOLS_UPDATE = "0"
$env:DEPOT_TOOLS_WIN_TOOLCHAIN = "0"
$env:GYP_MSVS_OVERRIDE_PATH = $VsInstall
if (!$env:vs2026_install) {
  $env:vs2026_install = $VsInstall
}

function Write-FileAscii($Path, $Text) {
  $Parent = Split-Path -Parent $Path
  if ($Parent) {
    New-Item -ItemType Directory -Force $Parent | Out-Null
  }
  Set-Content -Encoding ASCII -Path $Path -Value $Text
}

function Resolve-BundleVersion($Value) {
  if (!$Value -and $env:CLAWBROWSER_BUNDLE_VERSION) {
    $Value = $env:CLAWBROWSER_BUNDLE_VERSION
  }
  if (!$Value) {
    return ""
  }

  $Trimmed = $Value.Trim()
  if (!$Trimmed) {
    return ""
  }
  if ($Trimmed -match "\s") {
    throw "BundleVersion must not contain whitespace: $Trimmed"
  }
  if ($Trimmed -notmatch "^\d+(\.\d+){1,3}$") {
    throw "BundleVersion must be a numeric dotted version such as 1.0.3: $Trimmed"
  }
  return $Trimmed
}

function Get-ChromiumVersionString {
  $VersionPath = Join-Path $ChromiumSrc "chrome\VERSION"
  if (!(Test-Path $VersionPath -PathType Leaf)) {
    return ""
  }

  $Values = @{}
  foreach ($Line in Get-Content $VersionPath) {
    if ($Line -match "^([A-Z]+)=(\d+)$") {
      $Values[$Matches[1]] = $Matches[2]
    }
  }

  foreach ($Key in @("MAJOR", "MINOR", "BUILD", "PATCH")) {
    if (!$Values.ContainsKey($Key)) {
      return ""
    }
  }
  return "$($Values["MAJOR"]).$($Values["MINOR"]).$($Values["BUILD"]).$($Values["PATCH"])"
}

function Write-BundleVersionManifest($Path, $ArtifactName) {
  if (!$BundleVersion) {
    return
  }

  $Data = [ordered]@{
    bundle_version = $BundleVersion
    chromium_version = Get-ChromiumVersionString
    artifact_name = $ArtifactName
  }
  $Json = $Data | ConvertTo-Json
  Write-FileAscii $Path ($Json + "`r`n")
  Write-Host "BUNDLE_VERSION=$BundleVersion"
  Write-Host "BUNDLE_VERSION_MANIFEST=$Path"
}

function Apply-ChromiumPatch($PatchPath) {
  if (!(Test-Path $PatchPath -PathType Leaf)) {
    throw "Chromium patch not found: $PatchPath"
  }

  $ApplyArgs = @("--ignore-space-change", "--ignore-whitespace")
  $PreviousErrorActionPreference = $ErrorActionPreference
  try {
    $ErrorActionPreference = "Continue"
    $CheckOutput = & git -C $ChromiumSrc apply @ApplyArgs --check $PatchPath 2>&1
    $CheckExitCode = $LASTEXITCODE
  } finally {
    $ErrorActionPreference = $PreviousErrorActionPreference
  }

  if ($CheckExitCode -eq 0) {
    try {
      $ErrorActionPreference = "Continue"
      & git -C $ChromiumSrc apply @ApplyArgs $PatchPath
      $ApplyExitCode = $LASTEXITCODE
    } finally {
      $ErrorActionPreference = $PreviousErrorActionPreference
    }
    if ($ApplyExitCode -ne 0) {
      throw "Failed to apply Chromium patch: $PatchPath"
    }
    return
  }

  try {
    $ErrorActionPreference = "Continue"
    & git -C $ChromiumSrc apply @ApplyArgs --reverse --check $PatchPath *> $null
    $ReverseCheckExitCode = $LASTEXITCODE
  } finally {
    $ErrorActionPreference = $PreviousErrorActionPreference
  }
  if ($ReverseCheckExitCode -eq 0) {
    return
  }

  throw "Could not apply Chromium patch $PatchPath`n$($CheckOutput -join [Environment]::NewLine)"
}

function Invoke-Checked($FilePath, [string[]]$Arguments) {
  & $FilePath @Arguments
  if ($LASTEXITCODE -ne 0) {
    throw "$FilePath $($Arguments -join ' ') failed with exit code $LASTEXITCODE."
  }
}

function Test-TransientCompilerCacheFailure($BuildDir) {
  if (!$CompilerCacheWrapper) {
    return $false
  }

  $SisoOutput = Join-Path $BuildDir "siso_output"
  if (!(Test-Path $SisoOutput -PathType Leaf)) {
    return $false
  }

  $Patterns = @(
    "sccache: error:",
    "Failed to send data to or receive data from server",
    "unable to open output file '-': 'invalid argument'"
  )
  return [bool](Select-String -Path $SisoOutput -Pattern $Patterns -SimpleMatch -Quiet)
}

function Test-TransientResourceFailure($BuildDir) {
  $SisoOutput = Join-Path $BuildDir "siso_output"
  if (!(Test-Path $SisoOutput -PathType Leaf)) {
    return $false
  }

  $Patterns = @(
    "Insufficient system resources exist to complete the requested service",
    "The paging file is too small for this operation to complete",
    "Not enough memory resources are available to process this command"
  )
  return [bool](Select-String -Path $SisoOutput -Pattern $Patterns -SimpleMatch -Quiet)
}

function Get-BuildArgumentsWithJobs([string[]]$Arguments, [int]$RetryJobs) {
  if ($RetryJobs -le 0) {
    return $Arguments
  }

  $Filtered = [System.Collections.Generic.List[string]]::new()
  for ($Index = 0; $Index -lt $Arguments.Count; $Index += 1) {
    $Argument = $Arguments[$Index]
    if ($Argument -eq "-j") {
      $Index += 1
      continue
    }
    if ($Argument -match "^-j\d+$") {
      continue
    }
    $Filtered.Add($Argument)
  }

  $InsertIndex = 0
  for ($Index = 0; $Index -lt $Filtered.Count; $Index += 1) {
    if ($Filtered[$Index] -eq "-C") {
      $InsertIndex = [Math]::Min($Index + 2, $Filtered.Count)
      break
    }
  }

  $Filtered.Insert($InsertIndex, "-j")
  $Filtered.Insert($InsertIndex + 1, "$RetryJobs")
  return $Filtered.ToArray()
}

function Get-BuildLogPath($BuildDir, $Retry) {
  $LogRoot = Join-Path $ArtifactRoot "logs"
  New-Item -ItemType Directory -Force $LogRoot | Out-Null
  $SafeBuildDir = $BuildDir -replace '[\\/:*?"<>|]', '_'
  $Timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
  return Join-Path $LogRoot "autoninja-$SafeBuildDir-try$Retry-$Timestamp.log"
}

function Invoke-AutoninjaLogged($Autoninja, [string[]]$Arguments, $BuildDir, $Retry) {
  $LogPath = Get-BuildLogPath $BuildDir $Retry
  Write-Host "Building with autoninja (try $Retry). Log: $LogPath"

  $PreviousErrorActionPreference = $ErrorActionPreference
  $ErrorActionPreference = "Continue"
  try {
    & $Autoninja @Arguments *> $LogPath
    $ExitCode = $LASTEXITCODE
  } finally {
    $ErrorActionPreference = $PreviousErrorActionPreference
  }

  return [pscustomobject]@{
    ExitCode = $ExitCode
    LogPath = $LogPath
  }
}

function Write-BuildFailureSummary($BuildDir, $LogPath) {
  $SisoOutput = Join-Path $BuildDir "siso_output"
  if (Test-Path $SisoOutput -PathType Leaf) {
    Write-Host "Last failure summary:"
    Get-Content $SisoOutput -Tail 40 | ForEach-Object { Write-Host $_ }
  } elseif (Test-Path $LogPath -PathType Leaf) {
    Write-Host "Last build log lines:"
    Get-Content $LogPath -Tail 80 | ForEach-Object { Write-Host $_ }
  }
}

function Invoke-AutoninjaWithCacheRetry($BuildDir, [string[]]$Arguments) {
  $Autoninja = Join-Path $DepotTools "autoninja.bat"
  $Retry = 0
  $CurrentArguments = $Arguments

  while ($true) {
    $Result = Invoke-AutoninjaLogged $Autoninja $CurrentArguments $BuildDir ($Retry + 1)
    if ($Result.ExitCode -eq 0) {
      return
    }

    $ExitCode = $Result.ExitCode
    $IsCacheFailure = Test-TransientCompilerCacheFailure $BuildDir
    $IsResourceFailure = Test-TransientResourceFailure $BuildDir
    if (!$IsCacheFailure -and !$IsResourceFailure) {
      Write-BuildFailureSummary $BuildDir $Result.LogPath
      throw "$Autoninja $($CurrentArguments -join ' ') failed with exit code $ExitCode. Full log: $($Result.LogPath)"
    }

    if ($Retry -ge $CompilerCacheRetries) {
      Write-BuildFailureSummary $BuildDir $Result.LogPath
      throw "$Autoninja $($CurrentArguments -join ' ') failed with exit code $ExitCode after $CompilerCacheRetries transient retries. Full log: $($Result.LogPath)"
    }

    $Retry += 1
    if ($IsResourceFailure) {
      $RetryJobs = $ResourceRetryJobs
      if ($Jobs -gt 0 -and $Jobs -lt $RetryJobs) {
        $RetryJobs = $Jobs
      }
      $CurrentArguments = Get-BuildArgumentsWithJobs $Arguments $RetryJobs
      Write-Host "Windows hit a temporary resource limit; retrying quietly with -j $RetryJobs ($Retry/$CompilerCacheRetries)."
    } else {
      $RetryJobs = $ResourceRetryJobs
      if ($Jobs -gt 0 -and $Jobs -lt $RetryJobs) {
        $RetryJobs = $Jobs
      }
      $CurrentArguments = Get-BuildArgumentsWithJobs $Arguments $RetryJobs
      Write-Host "Compiler cache had a temporary problem; retrying quietly with -j $RetryJobs ($Retry/$CompilerCacheRetries)."
    }
    if ($IsCacheFailure -and !(Test-CompilerCacheServer)) {
      Restart-CompilerCacheServer
    }
  }
}

function Assert-PathInside($ChildPath, $ParentPath, $Label) {
  $ResolvedParent = [System.IO.Path]::GetFullPath($ParentPath).TrimEnd('\', '/')
  $ResolvedChild = [System.IO.Path]::GetFullPath($ChildPath).TrimEnd('\', '/')
  $Prefix = $ResolvedParent + [System.IO.Path]::DirectorySeparatorChar
  if (!$ResolvedChild.StartsWith($Prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "$Label must stay inside $ResolvedParent, got $ResolvedChild"
  }
}

function Invoke-RobocopyMirror($Source, $Destination) {
  if (!(Test-Path $Source -PathType Container)) {
    throw "Source directory not found: $Source"
  }

  Assert-PathInside $Destination $ChromiumSrc "Mirror destination"
  New-Item -ItemType Directory -Force $Destination | Out-Null
  & robocopy $Source $Destination /MIR /NFL /NDL /NJH /NJS /NC /NS /NP
  $ExitCode = $LASTEXITCODE
  if ($ExitCode -ge 8) {
    throw "robocopy $Source $Destination failed with exit code $ExitCode."
  }
}

function Update-GeneratedBrowserTypes {
  $Generator = Join-Path $ProjectRoot "clawbrowser\schemas\generate_browser_types.py"
  $Schema = Join-Path $ProjectRoot "clawbrowser\schemas\browser_schema.json"
  $Header = Join-Path $ProjectRoot "clawbrowser\generated\fingerprint_types.h"
  $Source = Join-Path $ProjectRoot "clawbrowser\generated\fingerprint_types.cc"
  $Python = Join-Path $DepotTools "python3.bat"

  Invoke-Checked $Python @($Generator, "--schema", $Schema, "--header", $Header, "--source", $Source)
  Invoke-Checked $Python @($Generator, "--check", "--schema", $Schema, "--header", $Header, "--source", $Source)
}

function Sync-ProjectOverlay {
  Invoke-RobocopyMirror (Join-Path $ProjectRoot "clawbrowser") (Join-Path $ChromiumSrc "clawbrowser")

  $ApiSource = Join-Path $ProjectRoot "api\openapi.yaml"
  if (Test-Path $ApiSource -PathType Leaf) {
    $ApiDestinationDir = Join-Path $ChromiumSrc "api"
    Assert-PathInside $ApiDestinationDir $ChromiumSrc "API destination"
    New-Item -ItemType Directory -Force $ApiDestinationDir | Out-Null
    Copy-Item $ApiSource (Join-Path $ApiDestinationDir "openapi.yaml") -Force
  }
}

function Write-IcoFromPngSet($IconDir, $OutputPath) {
  $Sizes = @(16, 24, 32, 48, 64, 128, 256)
  $Images = @()
  foreach ($Size in $Sizes) {
    $Path = Join-Path $IconDir "product_logo_$Size.png"
    if (!(Test-Path $Path)) {
      throw "Missing icon PNG: $Path"
    }
    [byte[]]$Bytes = Get-WindowsIconPngBytes $Path $Size
    if ($Bytes.Length -lt 8 -or $Bytes[0] -ne 0x89 -or $Bytes[1] -ne 0x50) {
      throw "Not a PNG: $Path"
    }
    $Images += [pscustomobject]@{ Size = $Size; Bytes = $Bytes }
  }

  $Stream = New-Object System.IO.MemoryStream
  $Writer = New-Object System.IO.BinaryWriter($Stream)
  $Writer.Write([uint16]0)
  $Writer.Write([uint16]1)
  $Writer.Write([uint16]$Images.Count)

  $Offset = 6 + (16 * $Images.Count)
  foreach ($Image in $Images) {
    $Width = if ($Image.Size -ge 256) { 0 } else { $Image.Size }
    $Height = if ($Image.Size -ge 256) { 0 } else { $Image.Size }
    $Writer.Write([byte]$Width)
    $Writer.Write([byte]$Height)
    $Writer.Write([byte]0)
    $Writer.Write([byte]0)
    $Writer.Write([uint16]1)
    $Writer.Write([uint16]32)
    $Writer.Write([uint32]$Image.Bytes.Length)
    $Writer.Write([uint32]$Offset)
    $Offset += $Image.Bytes.Length
  }
  foreach ($Image in $Images) {
    $Writer.Write($Image.Bytes)
  }

  $Parent = Split-Path -Parent $OutputPath
  New-Item -ItemType Directory -Force $Parent | Out-Null
  [System.IO.File]::WriteAllBytes($OutputPath, $Stream.ToArray())
  $Writer.Dispose()
  $Stream.Dispose()
}

function Get-WindowsIconPngBytes($Path, $Size) {
  Add-Type -AssemblyName System.Drawing

  $Source = [System.Drawing.Bitmap]::FromFile($Path)
  try {
    $MinX = $Source.Width
    $MinY = $Source.Height
    $MaxX = -1
    $MaxY = -1

    for ($Y = 0; $Y -lt $Source.Height; $Y++) {
      for ($X = 0; $X -lt $Source.Width; $X++) {
        if ($Source.GetPixel($X, $Y).A -gt 0) {
          if ($X -lt $MinX) { $MinX = $X }
          if ($Y -lt $MinY) { $MinY = $Y }
          if ($X -gt $MaxX) { $MaxX = $X }
          if ($Y -gt $MaxY) { $MaxY = $Y }
        }
      }
    }

    if ($MaxX -lt $MinX -or $MaxY -lt $MinY) {
      return ,[System.IO.File]::ReadAllBytes($Path)
    }

    $BoundsWidth = $MaxX - $MinX + 1
    $BoundsHeight = $MaxY - $MinY + 1
    $Inset = if ($Size -le 32) { 0 } elseif ($Size -le 64) { 1 } else { 2 }
    $TargetSize = $Size - (2 * $Inset)
    $Scale = [Math]::Min($TargetSize / $BoundsWidth, $TargetSize / $BoundsHeight)
    $DrawWidth = [Math]::Max(1, [int][Math]::Round($BoundsWidth * $Scale))
    $DrawHeight = [Math]::Max(1, [int][Math]::Round($BoundsHeight * $Scale))
    $DrawX = [int][Math]::Floor(($Size - $DrawWidth) / 2)
    $DrawY = [int][Math]::Floor(($Size - $DrawHeight) / 2)

    $Canvas = New-Object System.Drawing.Bitmap($Size, $Size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
      $Graphics = [System.Drawing.Graphics]::FromImage($Canvas)
      try {
        $Graphics.Clear([System.Drawing.Color]::Transparent)
        $Graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceOver
        $Graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
        $Graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $Graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
        $Graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality

        $SourceRect = New-Object System.Drawing.Rectangle($MinX, $MinY, $BoundsWidth, $BoundsHeight)
        $DestRect = New-Object System.Drawing.Rectangle($DrawX, $DrawY, $DrawWidth, $DrawHeight)
        $Graphics.DrawImage($Source, $DestRect, $SourceRect, [System.Drawing.GraphicsUnit]::Pixel)
      } finally {
        $Graphics.Dispose()
      }

      $PngStream = New-Object System.IO.MemoryStream
      try {
        $Canvas.Save($PngStream, [System.Drawing.Imaging.ImageFormat]::Png)
        return ,$PngStream.ToArray()
      } finally {
        $PngStream.Dispose()
      }
    } finally {
      $Canvas.Dispose()
    }
  } finally {
    $Source.Dispose()
  }
}

function Sync-ClawbrowserBranding {
  $BrandingPath = Join-Path $ChromiumSrc "chrome\app\theme\chromium\BRANDING"
  $ExeVersionPath = Join-Path $ChromiumSrc "chrome\app\chrome_exe.ver"
  $IconDir = Join-Path $ProjectRoot "branding\icons\app\side-bite"
  $ThemeDir = Join-Path $ChromiumSrc "chrome\app\theme\chromium"
  $WinThemeDir = Join-Path $ThemeDir "win"
  $SvgPath = Join-Path $ProjectRoot "clawbrowser\resources\side_bite.svg"

  Write-FileAscii $BrandingPath @"
COMPANY_FULLNAME=Clawbrowser
COMPANY_SHORTNAME=Clawbrowser
PRODUCT_FULLNAME=Clawbrowser
PRODUCT_SHORTNAME=Clawbrowser
PRODUCT_INSTALLER_FULLNAME=Clawbrowser Installer
PRODUCT_INSTALLER_SHORTNAME=Clawbrowser Installer
COPYRIGHT=Copyright @LASTCHANGE_YEAR@ The Clawbrowser Authors. All rights reserved.
MAC_BUNDLE_ID=ai.clawbrowser.Clawbrowser
MAC_CREATOR_CODE=Cr24
MAC_TEAM_ID=
"@

  Write-FileAscii $ExeVersionPath @"
INTERNAL_NAME=clawbrowser_exe
ORIGINAL_FILENAME=clawbrowser.exe
"@

  foreach ($Size in @(16, 22, 24, 32, 48, 64, 128, 256, 512, 1024)) {
    Copy-Item (Join-Path $IconDir "product_logo_$Size.png") (Join-Path $ThemeDir "product_logo_$Size.png") -Force
  }
  Copy-Item (Join-Path $IconDir "product_logo_32.png") (Join-Path $ChromiumSrc "chrome\app\theme\default_100_percent\chromium\product_logo_32.png") -Force
  Copy-Item $SvgPath (Join-Path $ThemeDir "product_logo.svg") -Force
  Copy-Item $SvgPath (Join-Path $ThemeDir "product_logo_animation.svg") -Force

  $IcoPath = Join-Path $WinThemeDir "clawbrowser.ico"
  Write-IcoFromPngSet $IconDir $IcoPath
  foreach ($Name in @("chromium.ico", "app_list.ico", "chromium_doc.ico", "chromium_pdf.ico")) {
    Copy-Item $IcoPath (Join-Path $WinThemeDir $Name) -Force
  }
}

function Ensure-ClawbrowserResourceIds {
  $SpecPath = Join-Path $ChromiumSrc "tools\gritsettings\resource_ids.spec"
  if (!(Test-Path $SpecPath -PathType Leaf)) {
    throw "Chromium resource id spec not found: $SpecPath"
  }

  $Entry = @"
  "clawbrowser/verify/clawbrowser_verify.grd": {
    "includes": [9310],
  },

"@

  $Text = Get-Content $SpecPath -Raw
  $EntryPattern = '(?ms)^[ \t]*"clawbrowser/verify/clawbrowser_verify\.grd": \{\r?\n[ \t]*"includes": \[9310\],\r?\n[ \t]*\},\r?\n\r?\n?'
  $Text = [regex]::Replace($Text, $EntryPattern, "")

  $AnchorPattern = '(?ms)  "chromecast/renderer/resources/extensions_renderer_resources\.grd": \{\r?\n    "includes": \[9300\],\r?\n  \},\r?\n\r?\n'
  $Anchor = [regex]::Match($Text, $AnchorPattern)
  if (!$Anchor.Success) {
    throw "Could not find resource_ids.spec insertion point for Clawbrowser."
  }

  $Text = $Text.Insert($Anchor.Index + $Anchor.Length, $Entry)
  Set-Content -Encoding ASCII -Path $SpecPath -Value $Text
}

function Disable-GoogleApiKeysInfobar {
  $InfobarPath = Join-Path $ChromiumSrc "chrome\browser\ui\startup\infobar_utils.cc"
  if (!(Test-Path $InfobarPath -PathType Leaf)) {
    throw "Chromium startup infobar source not found: $InfobarPath"
  }

  $Marker = "Clawbrowser intentionally ships without Google API keys"
  $Text = Get-Content $InfobarPath -Raw
  if ($Text.Contains($Marker)) {
    return
  }

  $Pattern = @"
  if (!google_apis::HasAPIKeyConfigured()) {
    GoogleApiKeysInfoBarDelegate::Create(infobar_manager);
  }

"@

  $Replacement = @"
  // $Marker; avoid Chromium's missing-keys
  // startup infobar while keeping those keys unset.

"@

  if (!$Text.Contains($Pattern)) {
    throw "Could not find Google API keys infobar block in $InfobarPath"
  }

  Write-FileAscii $InfobarPath ($Text.Replace($Pattern, $Replacement))
}

function Enable-ClawbrowserWindowsExecutableName {
  $UtilConstantsCcPath = Join-Path $ChromiumSrc "chrome\installer\util\util_constants.cc"
  $ReleasePath = Join-Path $ChromiumSrc "chrome\installer\mini_installer\chrome.release"

  foreach ($Path in @($UtilConstantsCcPath, $ReleasePath)) {
    if (!(Test-Path $Path -PathType Leaf)) {
      throw "Chromium installer source not found: $Path"
    }
  }

  $Text = Get-Content $UtilConstantsCcPath -Raw
  $Constants = @(
    @('const wchar_t kChromeExe[] = L"chrome.exe";', 'const wchar_t kChromeExe[] = L"clawbrowser.exe";'),
    @('const wchar_t kChromeNewExe[] = L"new_chrome.exe";', 'const wchar_t kChromeNewExe[] = L"new_clawbrowser.exe";'),
    @('const wchar_t kChromeOldExe[] = L"old_chrome.exe";', 'const wchar_t kChromeOldExe[] = L"old_clawbrowser.exe";')
  )
  foreach ($Pair in $Constants) {
    if ($Text.Contains($Pair[0])) {
      $Text = $Text.Replace($Pair[0], $Pair[1])
    } elseif (!$Text.Contains($Pair[1])) {
      throw "Could not find Windows executable constant in $UtilConstantsCcPath`: $($Pair[0])"
    }
  }
  Write-FileAscii $UtilConstantsCcPath $Text

  $Text = Get-Content $ReleasePath -Raw
  $OldEntry = "chrome.exe: %(ChromeDir)s\"
  $NewEntry = "clawbrowser.exe: %(ChromeDir)s\"
  if ($Text.Contains($OldEntry)) {
    $Text = $Text.Replace($OldEntry, $NewEntry)
  } elseif (!$Text.Contains($NewEntry)) {
    throw "Could not find Windows mini installer browser entry in $ReleasePath"
  }
  Write-FileAscii $ReleasePath $Text
}

function Write-BuildArgs($Profile, $OutputDir) {
  $ArgsPath = Join-Path $ChromiumSrc "$OutputDir\args.gn"
  $Lines = @("is_debug = false")

  if ($Profile -eq "Prod") {
    $Lines += @(
      "is_official_build = true",
      "is_component_build = false",
      "chrome_pgo_phase = 0"
    )
  } elseif ($Profile -eq "Fast") {
    $Lines += @(
      "is_component_build = true"
    )
  } else {
    throw "Unknown build profile: $Profile"
  }

  $Lines += @(
    "symbol_level = 0",
    "blink_symbol_level = 0",
    "v8_symbol_level = 0",
    'target_cpu = "x64"',
    'root_extra_deps = [ "//clawbrowser" ]'
  )

  if ($CompilerCacheWrapper) {
    $Lines += "cc_wrapper = `"$CompilerCacheWrapper`""
  }

  Write-FileAscii $ArgsPath (($Lines -join "`r`n") + "`r`n")
}

function Copy-WithParents($Source, $StageDir, $RelativePath) {
  $RelativePath = $RelativePath -replace '/', '\'
  $Destination = Join-Path $StageDir $RelativePath
  New-Item -ItemType Directory -Force (Split-Path -Parent $Destination) | Out-Null
  Copy-Item $Source $Destination -Force
}

function Ensure-WindowsPrivateAssemblyLayout($ApplicationDir) {
  $Manifest = Get-ChildItem $ApplicationDir -File -Filter "*.manifest" -ErrorAction SilentlyContinue |
      Where-Object { $_.Name -match '^\d+\.\d+\.\d+\.\d+\.manifest$' } |
      Select-Object -First 1
  if (!$Manifest) {
    return
  }

  $ChromeElf = Join-Path $ApplicationDir "chrome_elf.dll"
  if (!(Test-Path $ChromeElf -PathType Leaf)) {
    throw "Windows private assembly manifest exists but chrome_elf.dll is missing: $ApplicationDir"
  }

  $AssemblyName = [System.IO.Path]::GetFileNameWithoutExtension($Manifest.Name)
  $AssemblyDir = Join-Path $ApplicationDir $AssemblyName
  New-Item -ItemType Directory -Force $AssemblyDir | Out-Null
  Copy-Item $Manifest.FullName (Join-Path $AssemblyDir $Manifest.Name) -Force
  Copy-Item $ChromeElf (Join-Path $AssemblyDir "chrome_elf.dll") -Force
}

function Prepare-WindowsInstallerInputs($OutputDir) {
  $OutDir = Join-Path $ChromiumSrc $OutputDir
  $ChromeExe = Join-Path $OutDir "chrome.exe"
  $ClawbrowserExe = Join-Path $OutDir "clawbrowser.exe"
  if (!(Test-Path $ChromeExe -PathType Leaf)) {
    throw "Built browser not found: $ChromeExe"
  }

  Copy-Item $ChromeExe $ClawbrowserExe -Force
  Write-Host "WINDOWS_INSTALLER_EXE=$ClawbrowserExe"
}

function Stage-Clawbrowser($OutputDir, $ArtifactName) {
  $OutDir = Join-Path $ChromiumSrc $OutputDir
  $ChromeExe = Join-Path $OutDir "chrome.exe"
  if (!(Test-Path $ChromeExe)) {
    throw "Built browser not found: $ChromeExe"
  }

  $PackageDir = Join-Path $ArtifactRoot $ArtifactName
  $StageDir = Join-Path $PackageDir "Clawbrowser"
  $ZipPath = "$PackageDir.zip"
  Remove-Item $PackageDir -Recurse -Force -ErrorAction SilentlyContinue
  Remove-Item $ZipPath -Force -ErrorAction SilentlyContinue
  New-Item -ItemType Directory -Force $StageDir | Out-Null

  Copy-Item $ChromeExe (Join-Path $StageDir "clawbrowser.exe") -Force

  $RuntimeDeps = & (Join-Path $DepotTools "gn.bat") desc $OutputDir //chrome runtime_deps --data
  if ($LASTEXITCODE -ne 0) {
    throw "gn desc $OutputDir //chrome runtime_deps --data failed with exit code $LASTEXITCODE."
  }
  foreach ($Dep in $RuntimeDeps) {
    $Rel = $Dep.Trim()
    if (!$Rel) { continue }
    $Rel = $Rel -replace '^[.][\\/]', ''
    $Rel = $Rel -replace '/', '\'
    if ($Rel -match '\.(pdb|lib|exp|TOC)$') { continue }
    if ([IO.Path]::GetFileName($Rel) -eq "chrome.exe") { continue }
    if ($Rel -match '^initialexe\\chrome\.exe') { continue }

    $OutCandidate = Join-Path $OutDir $Rel
    $SrcCandidate = Join-Path $ChromiumSrc $Rel
    if (Test-Path $OutCandidate -PathType Leaf) {
      Copy-WithParents $OutCandidate $StageDir $Rel
    } elseif (Test-Path $SrcCandidate -PathType Leaf) {
      Copy-WithParents $SrcCandidate $StageDir $Rel
    }
  }

  $RootExtensions = @(".dll", ".pak", ".bin", ".dat", ".json", ".manifest")
  Get-ChildItem $OutDir -File | ForEach-Object {
    if ($_.Name -eq "chrome.exe") { return }
    if ($_.Extension -in @(".pdb", ".lib", ".exp")) { return }
    if ($_.Extension -in $RootExtensions) {
      Copy-Item $_.FullName (Join-Path $StageDir $_.Name) -Force
    }
  }

  foreach ($Helper in @("chrome_proxy.exe", "chrome_pwa_launcher.exe", "elevation_service.exe", "notification_helper.exe", "elevated_tracing_service.exe", "crashpad_handler.exe", "chrome_crashpad_handler.exe")) {
    $Path = Join-Path $OutDir $Helper
    if (Test-Path $Path) {
      Copy-Item $Path (Join-Path $StageDir $Helper) -Force
    }
  }

  foreach ($Dir in @("locales", "swiftshader", "MEIPreload", "PrivacySandboxAttestationsPreloaded", "WidevineCdm", "resources")) {
    $Path = Join-Path $OutDir $Dir
    if (Test-Path $Path) {
      Copy-Item $Path (Join-Path $StageDir $Dir) -Recurse -Force
    }
  }

  $IconDir = Join-Path $ProjectRoot "branding\icons\app\side-bite"
  foreach ($Size in @(16, 22, 24, 32, 48, 64, 128, 256, 512, 1024)) {
    Copy-Item (Join-Path $IconDir "product_logo_$Size.png") (Join-Path $StageDir "product_logo_$Size.png") -Force
  }

  Ensure-WindowsPrivateAssemblyLayout $StageDir
  Write-BundleVersionManifest (Join-Path $StageDir "clawbrowser-release.json") $ArtifactName
  Write-BundleVersionManifest (Join-Path $ArtifactRoot "$ArtifactName.release.json") $ArtifactName

  if (Test-Path (Join-Path $StageDir "chrome.exe")) {
    throw "Packaged Clawbrowser must expose clawbrowser.exe only; unexpected chrome.exe in $StageDir"
  }

  if (!$SkipZip) {
    Compress-Archive -Path $StageDir -DestinationPath $ZipPath -Force
    Write-Host "ZIP=$ZipPath"
  }
  Write-Host "STAGE=$StageDir"
  Write-Host "EXE=$(Join-Path $StageDir 'clawbrowser.exe')"
}

function Stage-WindowsSetupArchive($OutputDir) {
  $OutDir = Join-Path $ChromiumSrc $OutputDir
  $MiniInstaller = Join-Path $OutDir "mini_installer.exe"
  if (!(Test-Path $MiniInstaller -PathType Leaf)) {
    throw "Built Windows installer not found: $MiniInstaller"
  }

  $ZipPath = Join-Path $ArtifactRoot "clawbrowser-win-amd64.zip"
  $TempDir = Join-Path $ArtifactRoot "__clawbrowser-win-amd64-$PID"
  Remove-Item $TempDir -Recurse -Force -ErrorAction SilentlyContinue
  Remove-Item $ZipPath -Force -ErrorAction SilentlyContinue
  New-Item -ItemType Directory -Force $TempDir | Out-Null
  try {
    $SetupPath = Join-Path $TempDir "setup.exe"
    Copy-Item $MiniInstaller $SetupPath -Force

    if (Test-Path (Join-Path $TempDir "chrome.exe")) {
      throw "Windows release archive must expose setup.exe only; unexpected chrome.exe in $TempDir"
    }

    if (!$SkipZip) {
      Compress-Archive -Path $SetupPath -DestinationPath $ZipPath -Force
      Write-Host "WINDOWS_RELEASE_ZIP=$ZipPath"
      Write-Host "WINDOWS_RELEASE_SETUP=setup.exe"
    }
    Write-BundleVersionManifest (Join-Path $ArtifactRoot "clawbrowser-win-amd64.release.json") "clawbrowser-win-amd64"
  } finally {
    Remove-Item $TempDir -Recurse -Force -ErrorAction SilentlyContinue
  }
}

$BundleVersion = Resolve-BundleVersion $BundleVersion
if ($BundleVersion) {
  Write-Host "Windows bundle version: $BundleVersion"
}

Set-Location $ChromiumSrc
Update-GeneratedBrowserTypes
Sync-ProjectOverlay
Sync-ClawbrowserBranding
Ensure-ClawbrowserResourceIds
Disable-GoogleApiKeysInfobar
Enable-ClawbrowserWindowsExecutableName
Get-ChildItem -Path (Join-Path $ProjectRoot "clawbrowser\patches") -Filter "*.patch" |
  Sort-Object Name |
  ForEach-Object { Apply-ChromiumPatch $_.FullName }
Initialize-CompilerCacheServer -Restart

$BuildSpecs = Get-BuildSpecs
foreach ($Spec in $BuildSpecs) {
  Write-Host "Configuring $($Spec.Profile) build: $($Spec.BuildDir)"
  if ($CompilerCacheWrapper) {
    Write-Host "Compiler cache: $CompilerCacheWrapper"
  }

  Write-BuildArgs $Spec.Profile $Spec.BuildDir
  Invoke-Checked (Join-Path $DepotTools "gn.bat") @("gen", $Spec.BuildDir)

  if ($StageExistingArtifacts) {
    Write-Host "Generated $($Spec.BuildDir). Staging existing artifacts without building."
    if ($Spec.Profile -eq "Prod") {
      Prepare-WindowsInstallerInputs $Spec.BuildDir
    }
    Stage-Clawbrowser $Spec.BuildDir $Spec.ArtifactName
    if ($Spec.Profile -eq "Prod") {
      Stage-WindowsSetupArchive $Spec.BuildDir
    }
    continue
  }

  if ($SkipBuild) {
    Write-Host "Generated $($Spec.BuildDir). Build skipped."
    continue
  }

  $BuildArgs = @("-C", $Spec.BuildDir)
  if ($Jobs -gt 0) {
    $BuildArgs += @("-j", "$Jobs")
  }
  $BuildArgs += "chrome"
  Invoke-AutoninjaWithCacheRetry $Spec.BuildDir $BuildArgs
  if ($Spec.Profile -eq "Prod") {
    Prepare-WindowsInstallerInputs $Spec.BuildDir
    $InstallerBuildArgs = @("-C", $Spec.BuildDir)
    if ($Jobs -gt 0) {
      $InstallerBuildArgs += @("-j", "$Jobs")
    }
    $InstallerBuildArgs += "mini_installer"
    Invoke-AutoninjaWithCacheRetry $Spec.BuildDir $InstallerBuildArgs
  }
  Stage-Clawbrowser $Spec.BuildDir $Spec.ArtifactName
  if ($Spec.Profile -eq "Prod") {
    Stage-WindowsSetupArchive $Spec.BuildDir
  }
}

if (!$SkipBuild -and $CompilerCacheWrapper) {
  & $CompilerCacheWrapper --show-stats
}
