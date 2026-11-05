param([string]$ProjectRoot,[string]$ArtifactRoot,[string]$SourceSha)
$ErrorActionPreference='Stop'
if($env:RUNNER_ENVIRONMENT -ne 'github-hosted'){throw 'This helper requires a disposable hosted runner'}
$ProjectRoot=(Resolve-Path -LiteralPath $ProjectRoot).Path
$ArtifactRoot=[IO.Path]::GetFullPath($ArtifactRoot)
if(!$ArtifactRoot.StartsWith(([IO.Path]::GetFullPath($env:GITHUB_WORKSPACE)+[IO.Path]::DirectorySeparatorChar),[StringComparison]::OrdinalIgnoreCase)){throw 'Artifacts must stay in the job workspace'}
if($SourceSha -notmatch '^[0-9a-f]{40}$' -or (& git -C $ProjectRoot rev-parse HEAD).Trim() -ne $SourceSha){throw 'Unexpected project source'}
$checkout='D:/cb'
$depot='D:/depot_tools'
if((Test-Path -LiteralPath $checkout) -or (Test-Path -LiteralPath $depot)){throw 'Expected fresh short-path checkout directories'}
if((Get-PSDrive D).Free -lt 100GB){throw 'Insufficient initial native build capacity'}
New-Item -ItemType Directory -Path $checkout,$ArtifactRoot | Out-Null
Start-Transcript -Path (Join-Path $ArtifactRoot 'native-build.log')
try {
  $env:GIT_CONFIG_GLOBAL='D:/native-builder.gitconfig'
  $env:GIT_CONFIG_COUNT='1'
  $env:GIT_CONFIG_KEY_0='core.longpaths'
  $env:GIT_CONFIG_VALUE_0='true'
  $env:DEPOT_TOOLS_UPDATE='0'
  $env:DEPOT_TOOLS_WIN_TOOLCHAIN='0'
  $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
  $vs=(& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
  if(!$vs){throw 'Visual Studio C++ toolchain not found'}
  $env:vs2026_install=$vs
  $env:GYP_MSVS_OVERRIDE_PATH=$vs
  & git clone --depth=1 https://chromium.googlesource.com/chromium/tools/depot_tools.git $depot
  if($LASTEXITCODE -ne 0){throw 'depot_tools clone failed'}
  $env:PATH="$depot;$env:PATH"
  & "$depot/bootstrap/win_tools.bat"
  if($LASTEXITCODE -ne 0){throw 'Official pinned depot Python bootstrap failed'}
  $pin=Get-Content -LiteralPath (Join-Path $ProjectRoot 'scripts/clawbrowser_pin.conf') -Raw
  $revision=[regex]::Match($pin,'(?m)^CHROMIUM_REVISION=["'']?([0-9a-f]{40})').Groups[1].Value
  if($revision.Length -ne 40){throw 'Missing pinned Chromium revision'}
  "solutions = [{'name': 'src', 'url': 'https://chromium.googlesource.com/chromium/src.git', 'managed': False, 'custom_deps': {}, 'custom_vars': {}}]`ntarget_os = ['win']`n" | Set-Content -LiteralPath "$checkout/.gclient" -Encoding utf8NoBOM
  Push-Location $checkout
  try {
    & "$depot/gclient.bat" sync --no-history --nohooks --jobs 4 --revision "src@$revision"
    if($LASTEXITCODE -ne 0){throw 'Pinned Chromium dependency sync failed'}
    if((& git -C src rev-parse HEAD).Trim() -ne $revision){throw 'Wrong Chromium checkout'}
    & "$depot/gclient.bat" runhooks
    if($LASTEXITCODE -ne 0){throw 'Chromium hooks failed'}
  }finally{Pop-Location}
  & "$ProjectRoot/scripts/build_windows_prod_clawbrowser.ps1" -ChromiumSrc "$checkout/src" -ProjectRoot $ProjectRoot -DepotTools $depot -VsInstall $vs -BuildProfile Prod -CompilerCache none -Jobs 4 -ResourceRetryJobs 2 -BundleVersion 1.0.12 -ArtifactRoot $ArtifactRoot -ConfigPath "$ArtifactRoot/build-config.json"
  if($LASTEXITCODE -ne 0){throw 'Production native browser/installer build failed'}
  $build="$checkout/src/out/CBProd"
  Push-Location "$checkout/src"
  try {
    & "$depot/autoninja.bat" -C $build -j 4 clawbrowser_unittests clawbrowser_browser_unittests
    if($LASTEXITCODE -ne 0){throw 'Native test compilation failed'}
    foreach($entry in @(@('core','clawbrowser_unittests'),@('browser','clawbrowser_browser_unittests'))){
      $name=$entry[0];$executable=$entry[1]
      & "$build/$executable.exe" "--gtest_output=json:$ArtifactRoot/native-$name-tests.json" *> "$ArtifactRoot/native-$name-tests.log"
      if($LASTEXITCODE -ne 0){throw "Native $name tests failed"}
    }
  }finally{Pop-Location}
  @{version='1.0.12';project_commit=$SourceSha;chromium_revision=$revision;pipeline_commit=$env:GITHUB_SHA;depot_commit=(& git -C $depot rev-parse HEAD).Trim();build_profile='production';target='windows-amd64';native_tests_passed=$true;startup_hook_passed=$true;production_ready=$false;published=$false} | ConvertTo-Json | Set-Content "$ArtifactRoot/source.json" -Encoding utf8NoBOM
  Copy-Item -LiteralPath "$build/args.gn" -Destination "$ArtifactRoot/args.gn"
  $lines=Get-ChildItem -LiteralPath $ArtifactRoot -File | Where-Object Name -NotIn @('SHA256SUMS','native-build.log') | Sort-Object Name | ForEach-Object { '{0}  {1}' -f (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant(),$_.Name }
  $lines | Set-Content "$ArtifactRoot/SHA256SUMS" -Encoding utf8NoBOM
}finally{Stop-Transcript}
