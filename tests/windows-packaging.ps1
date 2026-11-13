# Run with powershell -NoProfile -File tests/windows-packaging.ps1 (or pwsh).
# Uses fake app/compiler helpers; does NOT compile or exercise a real installer.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$temp = Join-Path ([IO.Path]::GetTempPath()) ('omawrite packaging ' + [guid]::NewGuid())
$engine = (Get-Process -Id $PID).Path
$previousStamp = $env:OMAWRITE_STAMP
$previousSignature = $env:OMAWRITE_SIGNATURE
$previousIscc = $env:ISCC
try {
    foreach ($file in @('bin/setup-windows.ps1', 'bin/package-windows.ps1', 'tests/windows-packaging.ps1')) {
        $tokens = $null; $errors = $null
        [void][Management.Automation.Language.Parser]::ParseFile((Join-Path $root $file), [ref]$tokens, [ref]$errors)
        if ($errors) { throw ($errors | Out-String) }
    }
    New-Item -ItemType Directory -Path "$temp/dist/notices" -Force | Out-Null
    # Exercise the actual build.bat guard, including caches created before renaming.
    $batch = Get-Content -LiteralPath (Join-Path $root 'build.bat') -Raw
    $stampCommand = [regex]::Match($batch, '(?m)^powershell -NoProfile -Command "([^"\r\n]*\$env:OMAWRITE_STAMP[^"\r\n]*)"\r?$').Groups[1].Value
    if (!$stampCommand) { throw 'Missing toolchain stamp guard' }
    $env:OMAWRITE_STAMP = Join-Path $temp 'omawrite-toolchain.txt'
    $env:OMAWRITE_SIGNATURE = 'fake qmake|Qt|compiler|version|x64'
    function Invoke-StampCheck {
        $ErrorActionPreference = 'Continue'
        & $engine -NoProfile -Command $stampCommand 2>$null
        return $LASTEXITCODE
    }
    if ((Invoke-StampCheck) -ne 0) { throw 'Fresh stamp failed' }
    Remove-Item -LiteralPath $env:OMAWRITE_STAMP
    $legacyStamp = Join-Path $temp 'poncho-toolchain.txt'
    Set-Content -LiteralPath $legacyStamp -Value $env:OMAWRITE_SIGNATURE
    if ((Invoke-StampCheck) -ne 0 -or !(Test-Path -LiteralPath $env:OMAWRITE_STAMP)) { throw 'Matching legacy stamp was not adopted' }
    Set-Content -LiteralPath $legacyStamp -Value 'different old compiler'
    if ((Invoke-StampCheck) -eq 0) { throw 'Legacy toolchain mismatch bypassed guard' }
    Set-Content -LiteralPath $legacyStamp -Value $env:OMAWRITE_SIGNATURE
    Set-Content -LiteralPath $env:OMAWRITE_STAMP -Value 'different new compiler'
    if ((Invoke-StampCheck) -eq 0) { throw 'Current toolchain mismatch bypassed guard' }
    Set-Content -LiteralPath "$temp/dist/notices/COPYRIGHT.md" -Value 'test notice'
    $helper = Join-Path $temp 'helper.exe'
    if ($env:OS -eq 'Windows_NT') {
        Add-Type -OutputAssembly $helper -OutputType ConsoleApplication -TypeDefinition @'
using System;
using System.IO;
[assembly: System.Reflection.AssemblyFileVersion("6.3.0.0")]
public static class PackagingHelper {
    public static int Main(string[] args) {
        if (args.Length == 1 && args[0] == "--version") {
            Console.WriteLine("omawrite 1.2.3"); return 0;
        }
        string output = null; bool runtime = false;
        foreach (string a in args) {
            if (a.StartsWith("/DOutputDir=")) output = a.Substring(12);
            if (a == "/DNeedsVCRuntime=" + Environment.GetEnvironmentVariable("OMAWRITE_TEST_RUNTIME")) runtime = true;
        }
        if (output == null || !runtime) return 2;
        Directory.CreateDirectory(output);
        File.WriteAllText(Path.Combine(output, "Write-1.2.3-windows-x64-setup.exe"), "fake installer");
        return 0;
    }
}
'@
        # Run the actual CMD discovery routine, not a PowerShell reimplementation.
        $findIscc = [regex]::Match($batch, '(?ms)^:findIscc\r?\n.*?(?=^:installer\r?$)').Value
        if (!$findIscc) { throw 'Missing compiler discovery routine' }
        $probe = Join-Path $temp 'find-iscc.cmd'
        $body = "@echo off`nsetlocal EnableExtensions DisableDelayedExpansion`nset ARCH=x64`ncall :findIscc`nexit /b %errorlevel%`n" + $findIscc
        $body.Replace("`r`n", "`n").Replace("`n", "`r`n") | Set-Content -LiteralPath $probe -Encoding ASCII
        # A malformed IF must never dispatch our real setup script by accident.
        @('@echo off', 'echo unexpected>setup-was-called.txt', 'exit /b 73') |
            Set-Content -LiteralPath (Join-Path $temp 'setup.bat') -Encoding ASCII
        $compilerDir = Join-Path $temp 'Inno Setup 6'
        New-Item -ItemType Directory -Path $compilerDir | Out-Null
        $compiler = Join-Path $compilerDir 'ISCC.exe'
        Copy-Item -LiteralPath $helper -Destination $compiler
        Push-Location $temp
        try {
            foreach ($value in @($compiler, ('"' + $compiler + '"'))) {
                $env:ISCC = $value
                & $env:ComSpec /d /c $probe
                if ($LASTEXITCODE -ne 0) { throw "CMD rejected compiler path: $value" }
            }
            $env:ISCC = '"' + (Join-Path $temp 'Missing Inno Setup 6\ISCC.exe') + '"'
            $ErrorActionPreference = 'Continue'
            & $env:ComSpec /d /c $probe >$null 2>$null
            $code = $LASTEXITCODE
            $ErrorActionPreference = 'Stop'
            if ($code -eq 0) { throw 'Missing compiler was not refused' }
            if (Test-Path -LiteralPath 'setup-was-called.txt') { throw 'Compiler discovery accidentally executed setup.bat' }
            Write-Host 'PASS: native CMD compiler discovery accepts quoted/unquoted paths and safely rejects missing compilers.'
        } finally { Pop-Location }
    } else {
        # Portable mock permits local validation on macOS/Linux without Windows tools.
        @'
#!/usr/bin/env python3
import os, sys
if sys.argv[1:] == ['--version']:
    print('omawrite 1.2.3')
else:
    assert '/DNeedsVCRuntime=' + os.environ['OMAWRITE_TEST_RUNTIME'] in sys.argv
    path = next(a[12:] for a in sys.argv if a.startswith('/DOutputDir='))
    os.makedirs(path, exist_ok=True)
    open(os.path.join(path, 'Write-1.2.3-windows-x64-setup.exe'), 'w').write('fake installer')
'@ | Set-Content -LiteralPath $helper -Encoding ASCII
        & chmod +x $helper
        if ($LASTEXITCODE -ne 0) { throw 'Could not mark mock executable.' }
    }
    Copy-Item -LiteralPath $helper -Destination "$temp/dist/omawrite.exe"
    $packager = Join-Path $root 'bin/package-windows.ps1'
    foreach ($kit in @('win32-msvc', 'win32-g++')) {
        $env:OMAWRITE_TEST_RUNTIME = if ($kit -eq 'win32-msvc') { '1' } else { '0' }
        & $engine -NoProfile -File $packager -DistDir "$temp/dist" -Iscc $helper -Kit $kit
        if ($LASTEXITCODE -ne 0) { throw "Packaging failed for $kit" }
        $artifact = "$temp/installer/Write-1.2.3-windows-x64-setup.exe"
        if (!(Test-Path -LiteralPath $artifact)) { throw 'Missing expected artifact' }
        # PS 5.1 can promote redirected native stderr under Stop: these failures are expected.
        $ErrorActionPreference = 'Continue'
        & $engine -NoProfile -File $packager -DistDir "$temp/dist" -Iscc $helper -Kit $kit 2>$null
        $code = $LASTEXITCODE
        $ErrorActionPreference = 'Stop'
        if ($code -eq 0) { throw 'Existing installer was not refused' }
        Remove-Item -LiteralPath $artifact
    }
    Remove-Item -LiteralPath "$temp/dist/notices/COPYRIGHT.md"
    $ErrorActionPreference = 'Continue'
    & $engine -NoProfile -File $packager -DistDir "$temp/dist" -Iscc $helper -Kit win32-msvc 2>$null
    $code = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    if ($code -eq 0) { throw 'Missing deployment notices were not refused' }
    Write-Host 'PASS: script parsing, current/legacy toolchain stamps, MSVC/MinGW flags, spaces in paths, artifact naming, overwrite and incomplete-deployment guards.'
} finally {
    [Environment]::SetEnvironmentVariable('OMAWRITE_STAMP', $previousStamp, 'Process')
    [Environment]::SetEnvironmentVariable('OMAWRITE_SIGNATURE', $previousSignature, 'Process')
    [Environment]::SetEnvironmentVariable('ISCC', $previousIscc, 'Process')
    Remove-Item Env:OMAWRITE_TEST_RUNTIME -ErrorAction SilentlyContinue
    if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp -Recurse -Force }
}
# Expected failing child probes leave LASTEXITCODE nonzero. Report our own
# success explicitly, including to GitHub's Windows PowerShell wrapper.
exit 0
