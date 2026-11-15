# SPDX-License-Identifier: BSD-3-Clause
[CmdletBinding()]
param([ValidateSet('f746','f769')][string]$Board='f746',[switch]$Scalar,[ValidateSet('stlink','jlink')][string]$Probe='stlink')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$variant=$Board;if($Scalar){$variant+='-scalar'}
$firmware=Join-Path $projectRoot "build/$variant/viewer.elf"
$openocd=Join-Path $PSScriptRoot 'local/openocd/bin/openocd.exe'
$scripts=Join-Path $PSScriptRoot 'local/openocd/scripts'
if(!(Test-Path $firmware)){throw 'Build the selected firmware first.'}
$deviceId=if($Board -eq 'f746'){'0x449'}else{'0x451'}
if($Probe -eq 'jlink'){
    $programmer=Join-Path $PSScriptRoot 'local/programmer/bin/STM32_Programmer_CLI.exe'
    if(!(Test-Path $programmer)){throw 'Local CubeProgrammer missing. See tools/README.md.'}
    $connection=& $programmer -c port=JLINK 2>&1
    $result=$LASTEXITCODE
    $connection | Write-Output
    if($result -or (($connection -join "`n") -notmatch ('Device ID\s*:\s*'+$deviceId+'\b'))){
        throw 'J-Link connection failed or the connected MCU does not match the requested board.'
    }
    # J-Link needs an explicit vector-table start; -rst stops command processing
    # with exit code 1 in CubeProgrammer 2.23, even after a verified download.
    & $programmer -c port=JLINK -d $firmware -v -g 0x08000000
    if($LASTEXITCODE){throw 'J-Link programming, verification, or startup failed.'}
    return
}
if(!(Test-Path $openocd)){throw 'Local OpenOCD missing. See tools/README.md.'}
$check='set device_id [expr {[mrw 0xe0042000] & 0xfff}]; if {$device_id != '+$deviceId+'} { error "Connected MCU does not match the requested board" }'
$image=$firmware.Replace('\','/')
& $openocd -s $scripts -f interface/stlink-dap.cfg -f target/stm32f7x.cfg -c 'adapter speed 1800' -c 'reset_config srst_only srst_nogate connect_assert_srst' -c init -c 'reset halt' -c $check -c "flash write_image erase {$image}" -c "verify_image {$image}" -c 'reset run' -c shutdown
if($LASTEXITCODE){throw 'Programming or verification failed.'}
