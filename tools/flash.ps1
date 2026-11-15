# SPDX-License-Identifier: BSD-3-Clause
[CmdletBinding()]
param([ValidateSet('f746','f769')][string]$Board='f746',[switch]$Scalar)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$variant=$Board;if($Scalar){$variant+='-scalar'}
$firmware=Join-Path $projectRoot "build/$variant/viewer.elf"
$openocd=Join-Path $PSScriptRoot 'local/openocd/bin/openocd.exe'
$scripts=Join-Path $PSScriptRoot 'local/openocd/scripts'
if(!(Test-Path $firmware)){throw 'Build the selected firmware first.'}
if(!(Test-Path $openocd)){throw 'Local OpenOCD missing. See tools/README.md.'}
$deviceId=if($Board -eq 'f746'){'0x449'}else{'0x451'}
$check='set device_id [expr {[mrw 0xe0042000] & 0xfff}]; if {$device_id != '+$deviceId+'} { error "Connected MCU does not match the requested board" }'
$image=$firmware.Replace('\','/')
& $openocd -s $scripts -f interface/stlink-dap.cfg -f target/stm32f7x.cfg -c 'adapter speed 1800' -c init -c halt -c $check -c "flash write_image erase {$image}" -c "verify_image {$image}" -c 'reset run' -c shutdown
if($LASTEXITCODE){throw 'Programming or verification failed.'}
