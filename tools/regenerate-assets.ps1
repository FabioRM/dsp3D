# SPDX-License-Identifier: BSD-3-Clause
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
Push-Location $projectRoot
try {
    $python=Join-Path $PSScriptRoot 'local/python/python.exe'
    & $python tools/convert_model.py assets/source/metamask.stl sdcard/models/metamask.d3m --symbol metamask --crease 0 --color ed8a28 --c-dir assets/generated
    if($LASTEXITCODE){throw 'MetaMask conversion failed'}
    & $python tools/convert_model.py assets/source/suzanne.stl sdcard/models/suzanne.d3m --symbol suzanne --crease 180 --color 79b8e8 --c-dir assets/generated
    if($LASTEXITCODE){throw 'Suzanne conversion failed'}
} finally {Pop-Location}
