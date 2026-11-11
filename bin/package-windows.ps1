[CmdletBinding()]
param([Parameter(Mandatory)][string]$DistDir, [Parameter(Mandatory)][string]$Iscc,
    [Parameter(Mandatory)][ValidateSet('win32-msvc', 'win32-g++')][string]$Kit)
$ErrorActionPreference = 'Stop'
try {
    $DistDir = (Resolve-Path -LiteralPath $DistDir).Path
    $exe = Join-Path $DistDir 'ponchowriter.exe'
    if (!(Test-Path -LiteralPath $exe) -or !(Test-Path -LiteralPath "$DistDir\notices\COPYRIGHT.md")) {
        throw 'A complete build.bat --deploy folder with notices is required.'
    }
    $output = & $exe --version
    if ($LASTEXITCODE -ne 0) { throw 'Could not read the deployed application version.' }
    $match = [regex]::Match(($output -join "`n"), '(?im)^ponchowriter\s+(\d+\.\d+\.\d+)\s*$')
    if (!$match.Success) { throw "Unexpected --version output: $output" }
    $version = $match.Groups[1].Value
    $destination = Join-Path (Split-Path $DistDir -Parent) 'installer'
    $artifact = Join-Path $destination "PonchoWriter-$version-windows-x64-setup.exe"
    if (Test-Path -LiteralPath $artifact) { throw "Installer already exists; move it first: $artifact" }
    $needsVCRuntime = if ($Kit -eq 'win32-msvc') { 1 } else { 0 }
    & $Iscc "/DNeedsVCRuntime=$needsVCRuntime" "/DDistDir=$DistDir" "/DAppVersion=$version" "/DOutputDir=$destination" (Join-Path (Split-Path $PSScriptRoot -Parent) 'packaging\windows.iss')
    if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath $artifact)) { throw 'Inno Setup compilation failed.' }
    Write-Host "Unsigned installer: $artifact"
    Write-Host 'Requires VC++ x64 Redistributable on MSVC targets. Node/Mermaid are separate. Not redistribution-cleared.'
} catch { Write-Error $_; exit 1 }
