param (
    [Parameter(Mandatory = $true)][string]$ModulePath,
    [Parameter(Mandatory = $true)][string]$PhpVersion,
    [Parameter(Mandatory = $true)][ValidateSet('nts', 'ts')][string]$ThreadSafety,
    [Parameter(Mandatory = $true)][string]$Compiler,
    [string]$Architecture = 'x86_64'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$version = & php -r '$v = require "tools/versions.php"; echo $v["mongodb-extension"]["version"];'
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $ModulePath -PathType Leaf)) {
    throw 'Cannot identify the built MongoDB module or distribution version'
}
$name = "php_mongodb-$version-$PhpVersion-$ThreadSafety-$Compiler-$Architecture"
$staging = Join-Path $root "dist/windows/$name"
New-Item $staging -ItemType Directory -Force | Out-Null
Copy-Item $ModulePath (Join-Path $staging "$name.dll")
foreach ($file in @('LICENSE', 'NOTICE', 'THIRD_PARTY_NOTICES')) {
    Copy-Item (Join-Path $root $file) $staging
}
foreach ($file in @('src/libmongoc/COPYING', 'src/libmongoc/THIRD_PARTY_NOTICES', 'src/libmongocrypt/LICENSE')) {
    $target = Join-Path $staging $file
    New-Item (Split-Path $target -Parent) -ItemType Directory -Force | Out-Null
    Copy-Item (Join-Path $root $file) $target
}
$archive = Join-Path $root "dist/$name.zip"
Compress-Archive -Path (Join-Path $staging '*') -DestinationPath $archive -Force
$checksum = (Get-FileHash $archive -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText("$archive.sha256", "$checksum  $name.zip`n")
Write-Output $archive
