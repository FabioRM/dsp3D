# SPDX-License-Identifier: BSD-3-Clause
[CmdletBinding()]
param([switch]$Benchmark)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
Push-Location $projectRoot
try {
    $gcc=Join-Path $PSScriptRoot 'local/host-gnu/bin/gcc.exe'
    $python=Join-Path $PSScriptRoot 'local/python/python.exe'
    if(!(Test-Path $gcc) -or !(Test-Path $python)){throw 'Local native compiler or Python missing. See tools/README.md.'}
    New-Item -ItemType Directory -Force build/native | Out-Null
    $flags=@('-std=c11','-O3','-g','-Wall','-Wextra','-Wpedantic','-Werror','-ffp-contract=fast','-fno-strict-aliasing','-UNDEBUG','-Iinc','-Iassets/generated')
    $core=@('src/dsp3D.c','src/dsp3D_model.c','src/dsp3D_geometry.c','src/dsp3D_math.c','src/dsp3D_material.c','src/dsp3D_scene.c','src/dsp3D_scene_io.c','src/dsp3D_asset.c','assets/generated/metamask.c','assets/generated/suzanne.c')
    & $gcc @flags @core tests/test_renderer.c -lm -o build/native/tests.exe
    if($LASTEXITCODE){throw 'Native test compilation failed'}
    & build/native/tests.exe
    if($LASTEXITCODE){throw 'Native renderer tests failed'}
    & $gcc @flags @core tests/test_asset.c -lm -o build/native/asset-tests.exe
    if($LASTEXITCODE){throw 'Asset test compilation failed'}
    & $python tests/test_assets.py
    if($LASTEXITCODE){throw 'Extended asset tests failed'}
    & $python tests/test_converter.py
    if($LASTEXITCODE){throw 'Converter tests failed'}
    & $gcc @flags @core -Iexamples/scenes examples/desktop/preview.c examples/scenes/scenes.c -lm -o build/native/preview.exe
    if($LASTEXITCODE){throw 'Native preview compilation failed'}
    & build/native/preview.exe metamask build/native/metamask.bmp 0
    if($LASTEXITCODE){throw 'MetaMask preview failed'}
    & build/native/preview.exe suzanne build/native/suzanne.bmp 1
    if($LASTEXITCODE){throw 'Suzanne preview failed'}
    & build/native/preview.exe scene build/native/scene.bmp 1
    if($LASTEXITCODE){throw 'Multi-object scene failed'}
    if($Benchmark){& build/native/preview.exe --benchmark | Set-Content build/native/benchmark.csv;if($LASTEXITCODE){throw 'Benchmark failed'}}
} finally {Pop-Location}
