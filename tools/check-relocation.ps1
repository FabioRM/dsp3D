# SPDX-License-Identifier: BSD-3-Clause
# Copy build inputs and the compiler to a path containing spaces, restrict PATH,
# and build both targets. The retained copy is ignored under build/.
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$destination=Join-Path $projectRoot 'build/relocation check'
New-Item -ItemType Directory -Force $destination | Out-Null
foreach($part in @('src','inc','ports','examples','assets','third_party')){
    Copy-Item -LiteralPath (Join-Path $projectRoot $part) -Destination $destination -Recurse -Force
}
New-Item -ItemType Directory -Force (Join-Path $destination 'tools/local') | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'local/arm-gnu') -Destination (Join-Path $destination 'tools/local') -Recurse -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'build.ps1') -Destination (Join-Path $destination 'tools') -Force
$previousPath=$env:PATH
try {
    $env:PATH=Join-Path $env:SystemRoot 'System32'
    & (Join-Path $destination 'tools/build.ps1') -Board f746 -Rebuild
    & (Join-Path $destination 'tools/build.ps1') -Board f769 -Rebuild
} finally {$env:PATH=$previousPath}
foreach($board in @('f746','f769')){
    $original=Join-Path $projectRoot "build/$board/viewer.bin"
    $relocated=Join-Path $destination "build/$board/viewer.bin"
    if((Get-FileHash -LiteralPath $original).Hash -ne (Get-FileHash -LiteralPath $relocated).Hash){
        throw "Relocated $board binary differs from the original build"
    }
}
Write-Host 'PASS: both relocated builds produce identical firmware with a restricted PATH.'
