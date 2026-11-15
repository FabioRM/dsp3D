# SPDX-License-Identifier: BSD-3-Clause
param([ValidateSet('f746','f769')][string]$Board='f746',[string]$Output='build/capture.bmp')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
Push-Location $projectRoot
try {
    $width=if($Board -eq 'f746'){480}else{800};$height=if($Board -eq 'f746'){272}else{480}
    $targetPath=[IO.Path]::GetFullPath((Join-Path $projectRoot $Output))
    New-Item -ItemType Directory -Force (Split-Path $targetPath) | Out-Null
    $rawPath=$targetPath+'.rgb565'
    $dump='set framebuffer [mrw 0x400168ac]; dump_image {'+$rawPath.Replace('\','/')+'} $framebuffer '+($width*$height*2)
    & tools/local/openocd/bin/openocd.exe -s tools/local/openocd/scripts -f interface/stlink-dap.cfg -f target/stm32f7x.cfg -c 'adapter speed 1800' -c init -c halt -c $dump -c resume -c shutdown
    if($LASTEXITCODE){throw 'Framebuffer capture failed'}
    & tools/local/python/python.exe tools/rgb565_to_bmp.py $rawPath $targetPath $width $height
    if($LASTEXITCODE){throw 'Framebuffer conversion failed'}
    Write-Host "Saved $targetPath"
} finally {Pop-Location}
