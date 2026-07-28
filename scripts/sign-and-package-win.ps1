[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)]
  [ValidateSet("Package", "Verify")]
  [string] $Action,

  [Parameter(Mandatory = $true)]
  [string] $BuildDirectory,

  [Parameter(Mandatory = $true)]
  [string] $OutputDirectory,

  [Parameter(Mandatory = $true)]
  [ValidatePattern("^\d+\.\d+\.\d+(?:[-+][0-9A-Za-z.-]+)?$")]
  [string] $Version,

  [switch] $RequireSignedInputs,
  [switch] $TestInstall
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$buildDir = (Resolve-Path $BuildDirectory).Path
$outputDir = [System.IO.Path]::GetFullPath($OutputDirectory)
$installerPath = Join-Path $outputDir "RVRSE-$Version-Windows-Setup.exe"
$zipPath = Join-Path $outputDir "RVRSE-$Version-Windows.zip"
$vst3Binary = Join-Path $buildDir "RVRSE.vst3\Contents\x86_64-win\RVRSE.vst3"
$clapBinary = Join-Path $buildDir "RVRSE.clap"
$standaloneBinary = Join-Path $buildDir "RVRSE.exe"
$signedBinaries = @($vst3Binary, $clapBinary, $standaloneBinary)

function Assert-FileExists([string] $Path) {
  if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
    throw "Required file not found: $Path"
  }
}

function Find-SignTool {
  $command = Get-Command "signtool.exe" -ErrorAction SilentlyContinue
  if ($command) {
    return $command.Source
  }

  $kitsRoot = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\bin"
  $candidate = Get-ChildItem -LiteralPath $kitsRoot -Filter "signtool.exe" -Recurse |
    Where-Object { $_.FullName -match "\\x64\\signtool\.exe$" } |
    Sort-Object FullName -Descending |
    Select-Object -First 1
  if (-not $candidate) {
    throw "signtool.exe was not found in PATH or the Windows 10 SDK."
  }
  return $candidate.FullName
}

function Assert-ValidSignature([string] $Path) {
  Assert-FileExists $Path
  $signTool = Find-SignTool
  & $signTool verify /pa /all /v $Path
  if ($LASTEXITCODE -ne 0) {
    throw "Signature verification failed: $Path"
  }
}

function Find-InnoCompiler {
  $command = Get-Command "ISCC.exe" -ErrorAction SilentlyContinue
  if ($command) {
    return $command.Source
  }

  $defaultPath = Join-Path ${env:ProgramFiles(x86)} "Inno Setup 6\ISCC.exe"
  if (Test-Path -LiteralPath $defaultPath -PathType Leaf) {
    return $defaultPath
  }
  throw "ISCC.exe was not found. Install Inno Setup 6."
}

function New-WindowsPackages {
  foreach ($binary in $signedBinaries) {
    Assert-FileExists $binary
    if ($RequireSignedInputs) {
      Assert-ValidSignature $binary
    }
  }

  New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
  $compiler = Find-InnoCompiler
  $issPath = Join-Path $repoRoot "RVRSE\installer\RVRSE.iss"
  & $compiler "/DBuildOutputDir=$buildDir" "/DRepoRoot=$repoRoot" "/O$outputDir" $issPath
  if ($LASTEXITCODE -ne 0) {
    throw "Inno Setup failed with exit code $LASTEXITCODE."
  }

  $rawInstaller = Join-Path $outputDir "RVRSE Installer.exe"
  Assert-FileExists $rawInstaller
  Move-Item -LiteralPath $rawInstaller -Destination $installerPath -Force

  $stagingDir = Join-Path $outputDir "zip-staging"
  if (Test-Path -LiteralPath $stagingDir) {
    Remove-Item -LiteralPath $stagingDir -Recurse -Force
  }
  New-Item -ItemType Directory -Path $stagingDir | Out-Null
  Copy-Item -LiteralPath (Join-Path $buildDir "RVRSE.vst3") -Destination $stagingDir -Recurse
  Copy-Item -LiteralPath $clapBinary, $standaloneBinary -Destination $stagingDir
  Copy-Item -LiteralPath (Join-Path $repoRoot "RVRSE\installer\INSTALL-Windows.txt") -Destination (Join-Path $stagingDir "INSTALL.txt")
  Copy-Item -LiteralPath (Join-Path $repoRoot "RVRSE\manual\RVRSE manual.pdf") -Destination $stagingDir
  if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
  }
  Compress-Archive -Path (Join-Path $stagingDir "*") -DestinationPath $zipPath
  Remove-Item -LiteralPath $stagingDir -Recurse -Force

  Write-Host "Created $installerPath"
  Write-Host "Created $zipPath"
}

function Test-WindowsPackages {
  foreach ($binary in $signedBinaries) {
    if ($RequireSignedInputs) {
      Assert-ValidSignature $binary
    } else {
      Assert-FileExists $binary
    }
  }
  if ($RequireSignedInputs) {
    Assert-ValidSignature $installerPath
  } else {
    Assert-FileExists $installerPath
  }
  Assert-FileExists $zipPath

  if (-not $TestInstall) {
    return
  }

  $installLog = Join-Path $outputDir "install.log"
  $installArgs = @(
    "/VERYSILENT",
    "/SUPPRESSMSGBOXES",
    "/NORESTART",
    "/SP-",
    "/LOG=$installLog"
  )
  $process = Start-Process -FilePath $installerPath -ArgumentList $installArgs -Wait -PassThru
  if ($process.ExitCode -ne 0) {
    throw "Installer failed with exit code $($process.ExitCode). See $installLog"
  }

  $installedFiles = @(
    (Join-Path $env:ProgramFiles "RVRSE\RVRSE.exe"),
    (Join-Path $env:CommonProgramFiles "VST3\RVRSE.vst3\Contents\x86_64-win\RVRSE.vst3"),
    (Join-Path $env:CommonProgramFiles "CLAP\RVRSE.clap")
  )
  foreach ($file in $installedFiles) {
    Assert-FileExists $file
  }

  $uninstaller = Join-Path $env:ProgramFiles "RVRSE\unins000.exe"
  Assert-FileExists $uninstaller
  $process = Start-Process -FilePath $uninstaller -ArgumentList @(
    "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART"
  ) -Wait -PassThru
  if ($process.ExitCode -ne 0) {
    throw "Uninstaller failed with exit code $($process.ExitCode)."
  }
  foreach ($file in $installedFiles) {
    if (Test-Path -LiteralPath $file) {
      throw "Uninstaller left an installed artifact behind: $file"
    }
  }
}

switch ($Action) {
  "Package" { New-WindowsPackages }
  "Verify" { Test-WindowsPackages }
}
