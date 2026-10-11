param (
    [Parameter(Mandatory = $true)][string]$PhpVersion,
    [Parameter(Mandatory = $true)][string]$SwowSourceDirectory
)

$ErrorActionPreference = 'Stop'
if ([version]$PhpVersion -ge [version]'8.4') {
    exit 0
}

# Match the VS16 toolset selection used by Swow's Windows workflow.
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$compiler = & $vswhere -latest -products '*' `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -find 'VC\Tools\MSVC\14.29.*\bin\Hostx64\x64\cl.exe' | Select-Object -First 1
if ($compiler) {
    'SWOW_WINDOWS_TOOLSET=14.29' | Out-File $env:GITHUB_ENV -Append
    exit 0
}

$installer = Join-Path $env:RUNNER_TEMP 'vs_buildtools.exe'
Invoke-WebRequest -Uri 'https://aka.ms/vs/16/release/vs_buildtools.exe' -OutFile $installer
$config = Join-Path $SwowSourceDirectory '.github/workflows/winext/.vsconfig'
$process = Start-Process -FilePath $installer -Wait -PassThru -ArgumentList @(
    '--quiet', '--wait', '--norestart', '--config', $config
)
if ($process.ExitCode -notin @(0, 3010)) {
    throw "Visual Studio setup failed: $($process.ExitCode)"
}
