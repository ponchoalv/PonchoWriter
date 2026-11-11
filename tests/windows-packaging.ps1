# Run with powershell -NoProfile -File tests/windows-packaging.ps1 (or pwsh).
# Uses fake app/compiler helpers; does NOT compile or exercise a real installer.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$temp = Join-Path ([IO.Path]::GetTempPath()) ('poncho packaging ' + [guid]::NewGuid())
$engine = (Get-Process -Id $PID).Path
try {
    foreach ($file in @('bin/setup-windows.ps1', 'bin/package-windows.ps1', 'tests/windows-packaging.ps1')) {
        $tokens = $null; $errors = $null
        [void][Management.Automation.Language.Parser]::ParseFile((Join-Path $root $file), [ref]$tokens, [ref]$errors)
        if ($errors) { throw ($errors | Out-String) }
    }
    New-Item -ItemType Directory -Path "$temp/dist/notices" -Force | Out-Null
    Set-Content -LiteralPath "$temp/dist/notices/COPYRIGHT.md" -Value 'test notice'
    $helper = Join-Path $temp 'helper.exe'
    if ($env:OS -eq 'Windows_NT') {
        Add-Type -OutputAssembly $helper -OutputType ConsoleApplication -TypeDefinition @'
using System;
using System.IO;
public static class PackagingHelper {
    public static int Main(string[] args) {
        if (args.Length == 1 && args[0] == "--version") {
            Console.WriteLine("ponchowriter 1.2.3"); return 0;
        }
        string output = null; bool runtime = false;
        foreach (string a in args) {
            if (a.StartsWith("/DOutputDir=")) output = a.Substring(12);
            if (a == "/DNeedsVCRuntime=" + Environment.GetEnvironmentVariable("PONCHO_TEST_RUNTIME")) runtime = true;
        }
        if (output == null || !runtime) return 2;
        Directory.CreateDirectory(output);
        File.WriteAllText(Path.Combine(output, "PonchoWriter-1.2.3-windows-x64-setup.exe"), "fake installer");
        return 0;
    }
}
'@
    } else {
        # Portable mock permits local validation on macOS/Linux without Windows tools.
        @'
#!/usr/bin/env python3
import os, sys
if sys.argv[1:] == ['--version']:
    print('ponchowriter 1.2.3')
else:
    assert '/DNeedsVCRuntime=' + os.environ['PONCHO_TEST_RUNTIME'] in sys.argv
    path = next(a[12:] for a in sys.argv if a.startswith('/DOutputDir='))
    os.makedirs(path, exist_ok=True)
    open(os.path.join(path, 'PonchoWriter-1.2.3-windows-x64-setup.exe'), 'w').write('fake installer')
'@ | Set-Content -LiteralPath $helper -Encoding ASCII
        & chmod +x $helper
        if ($LASTEXITCODE -ne 0) { throw 'Could not mark mock executable.' }
    }
    Copy-Item -LiteralPath $helper -Destination "$temp/dist/ponchowriter.exe"
    $packager = Join-Path $root 'bin/package-windows.ps1'
    foreach ($kit in @('win32-msvc', 'win32-g++')) {
        $env:PONCHO_TEST_RUNTIME = if ($kit -eq 'win32-msvc') { '1' } else { '0' }
        & $engine -NoProfile -File $packager -DistDir "$temp/dist" -Iscc $helper -Kit $kit
        if ($LASTEXITCODE -ne 0) { throw "Packaging failed for $kit" }
        $artifact = "$temp/installer/PonchoWriter-1.2.3-windows-x64-setup.exe"
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
    Write-Host 'PASS: script parsing, MSVC/MinGW flags, spaces in paths, artifact naming, overwrite and incomplete-deployment guards.'
} finally {
    Remove-Item Env:PONCHO_TEST_RUNTIME -ErrorAction SilentlyContinue
    if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp -Recurse -Force }
}
