[CmdletBinding()]
param([switch]$SelfTest, [string]$RoutineRoot)
$ErrorActionPreference = 'Stop'
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$pin = 'f768aeaaf69b63819fe0500ed64f456b60ba02f7'
$oldCL = $env:CL
Push-Location $root
try {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (!(Test-Path $vswhere)) { throw 'Visual Studio Installer / C++ Build Tools not found.' }
    $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (!$vs) { throw 'Install Visual Studio 2026 C++ Build Tools (v145) and a Windows SDK.' }
    if (!$RoutineRoot) {
        $RoutineRoot = Join-Path $root 'temp/deps/routine'
        if (!(Test-Path $RoutineRoot)) {
            & git -c core.autocrlf=false clone https://github.com/henrypp/routine.git $RoutineRoot
            if ($LASTEXITCODE) { throw 'routine clone failed.' }
            & git -C $RoutineRoot checkout --detach $pin
            if ($LASTEXITCODE) { throw 'routine checkout failed.' }
        }
        $head = & git -C $RoutineRoot rev-parse HEAD
        if ($head -ne $pin) { throw 'Dependency checkout is not at the required pinned revision.' }
    }
    $prepared = & "$PSScriptRoot/prepare.ps1" -RoutineRoot $RoutineRoot
    $pkg = 'packages/Microsoft.Windows.CppWinRT.2.0.230706.1'
    if (!(Test-Path "$pkg/build/native/Microsoft.Windows.CppWinRT.props")) {
        New-Item -ItemType Directory -Force packages | Out-Null
        Invoke-WebRequest 'https://api.nuget.org/v3-flatcontainer/microsoft.windows.cppwinrt/2.0.230706.1/microsoft.windows.cppwinrt.2.0.230706.1.nupkg' -OutFile packages/cppwinrt.zip -UseBasicParsing
        Expand-Archive packages/cppwinrt.zip $pkg -Force
    }
    $buildArgs = @('simplewall.sln', '/p:Configuration=Release', '/p:Platform=x64', "/p:ForceImportBeforeCppTargets=$PSScriptRoot/build.props", "/p:PublicRoutineRoot=$prepared", '/m', '/verbosity:minimal', '/nologo')
    if ($SelfTest) {
        $env:CL = "$oldCL /DSW_COMPAT_SELFTEST"
        $buildArgs += @("/p:OutDir=$root/temp/selftest-bin/", "/p:IntDir=$root/temp/selftest-obj/")
    } elseif ($oldCL -match 'SW_COMPAT_SELFTEST') {
        throw 'Remove SW_COMPAT_SELFTEST from the CL environment before a production build.'
    }
    & "$vs/MSBuild/Current/Bin/amd64/MSBuild.exe" @buildArgs
    if ($LASTEXITCODE) { throw "Build failed: $LASTEXITCODE" }
    if ($SelfTest) {
        $run = Start-Process "$root/temp/selftest-bin/simplewall.exe" -WindowStyle Hidden -PassThru -Wait
        Get-Content "$root/temp/selftest-bin/simplewall.exe.test.txt"
        if ($run.ExitCode) { throw "Self-test failed: $($run.ExitCode)" }
    }
} finally {
    $env:CL = $oldCL
    Pop-Location
}
