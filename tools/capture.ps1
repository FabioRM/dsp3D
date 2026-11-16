# SPDX-License-Identifier: BSD-3-Clause
param([ValidateSet('f746','f769')][string]$Board='f746',[string]$Output='build/capture.bmp',[switch]$ResetView,[ValidateSet('stlink','jlink')][string]$Probe='stlink')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
Push-Location $projectRoot
try {
    if($Probe -eq 'jlink') {
        if($ResetView){throw 'Use the viewer reset command x before a J-Link capture.'}
        & tools/local/python/python.exe tools/jlink_capture.py --board $Board $Output
        if($LASTEXITCODE){throw 'J-Link framebuffer capture failed'}
        return
    }
    $width=if($Board -eq 'f746'){480}else{800};$height=if($Board -eq 'f746'){272}else{480}
    $targetPath=[IO.Path]::GetFullPath((Join-Path $projectRoot $Output))
    New-Item -ItemType Directory -Force (Split-Path $targetPath) | Out-Null
    $rawPath=$targetPath+'.rgb565'
    $dump='set framebuffer [mrw 0x400168ac]; dump_image {'+$rawPath.Replace('\','/')+'} $framebuffer '+($width*$height*2)
    $deviceId=if($Board -eq 'f746'){'0x449'}else{'0x451'}
    $check='if {([mrw 0xe0042000] & 0xfff) != '+$deviceId+'} {error "Connected MCU does not match the requested board"}'
    $commands=@('-c',$check)
    if($ResetView){
        # This option is specific to the bundled viewer and its current ELF symbols.
        $symbols=& tools/local/arm-gnu/bin/arm-none-eabi-nm.exe -n "build/$Board/viewer.elf"
        if($LASTEXITCODE){throw 'Cannot inspect viewer symbols'}
        $pose=$null;$rotation=$null
        foreach($line in $symbols){
            if($line -match '^([0-9a-fA-F]+) [a-zA-Z] transform$'){$pose=[Convert]::ToUInt32($Matches[1],16)}
            if($line -match '^([0-9a-fA-F]+) [a-zA-Z] autorotate$'){$rotation=[Convert]::ToUInt32($Matches[1],16)}
        }
        if($null -eq $pose -or $null -eq $rotation){throw 'The viewer transform/autorotate symbols are missing'}
        $reset="mww $pose 0 3; mww $($pose+24) 0x3f866666; mww $rotation 0; resume; sleep 200; halt"
        $commands+=@('-c',$reset)
    }
    & tools/local/openocd/bin/openocd.exe -s tools/local/openocd/scripts -f interface/stlink-dap.cfg -c 'set ENABLE_LOW_POWER 1' -f target/stm32f7x.cfg -c 'adapter speed 1800' -c init -c halt @commands -c $dump -c resume -c shutdown
    if($LASTEXITCODE){throw 'Framebuffer capture failed'}
    & tools/local/python/python.exe tools/rgb565_to_bmp.py $rawPath $targetPath $width $height
    if($LASTEXITCODE){throw 'Framebuffer conversion failed'}
    Write-Host "Saved $targetPath"
} finally {Pop-Location}
