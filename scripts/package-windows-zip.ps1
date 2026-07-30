[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)]
  [string] $BuildDirectory,

  [Parameter(Mandatory = $true)]
  [string] $OutputDirectory,

  [Parameter(Mandatory = $true)]
  [ValidatePattern("^\d+\.\d+\.\d+(?:[-+][0-9A-Za-z.-]+)?$")]
  [string] $Version
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$buildDir = (Resolve-Path $BuildDirectory).Path
$outputDir = [System.IO.Path]::GetFullPath($OutputDirectory)
$zipPath = Join-Path $outputDir "RVRSE-$Version-Windows.zip"
$stagingDir = Join-Path $outputDir "zip-staging"

$artifacts = @(
  @{ Source = (Join-Path $buildDir "RVRSE.vst3"); Name = "RVRSE.vst3"; Type = "Container" },
  @{ Source = (Join-Path $buildDir "RVRSE.clap"); Name = "RVRSE.clap"; Type = "Leaf" },
  @{ Source = (Join-Path $buildDir "RVRSE.exe"); Name = "RVRSE.exe"; Type = "Leaf" },
  @{ Source = (Join-Path $repoRoot "RVRSE\installer\INSTALL-Windows.txt"); Name = "INSTALL.txt"; Type = "Leaf" },
  @{ Source = (Join-Path $repoRoot "RVRSE\manual\RVRSE manual.pdf"); Name = "RVRSE manual.pdf"; Type = "Leaf" }
)

foreach ($artifact in $artifacts) {
  if (-not (Test-Path -LiteralPath $artifact.Source -PathType $artifact.Type)) {
    throw "Required Windows ZIP artifact not found: $($artifact.Source)"
  }
}

New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
if (Test-Path -LiteralPath $stagingDir) {
  Remove-Item -LiteralPath $stagingDir -Recurse -Force
}

try {
  New-Item -ItemType Directory -Path $stagingDir | Out-Null
  foreach ($artifact in $artifacts) {
    Copy-Item -LiteralPath $artifact.Source `
      -Destination (Join-Path $stagingDir $artifact.Name) `
      -Recurse
  }

  if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
  }
  Compress-Archive -Path (Join-Path $stagingDir "*") -DestinationPath $zipPath

  Add-Type -AssemblyName System.IO.Compression.FileSystem
  $archive = [System.IO.Compression.ZipFile]::OpenRead($zipPath)
  try {
    $entries = @($archive.Entries | ForEach-Object { $_.FullName.Replace("\", "/") })
    $requiredEntries = @(
      "RVRSE.vst3/Contents/x86_64-win/RVRSE.vst3",
      "RVRSE.clap",
      "RVRSE.exe",
      "INSTALL.txt",
      "RVRSE manual.pdf"
    )
    foreach ($entry in $requiredEntries) {
      if ($entries -notcontains $entry) {
        throw "Windows ZIP is missing required entry: $entry"
      }
    }
  }
  finally {
    $archive.Dispose()
  }
}
finally {
  if (Test-Path -LiteralPath $stagingDir) {
    Remove-Item -LiteralPath $stagingDir -Recurse -Force
  }
}

Write-Host "Created and verified $zipPath"
