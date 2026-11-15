# SPDX-License-Identifier: BSD-3-Clause
[CmdletBinding()]
param([ValidateSet('f746','f769')][string]$Board='f746',[switch]$Rebuild,[switch]$Scalar)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
Push-Location $projectRoot
try {
    $toolRoot=Join-Path $PSScriptRoot 'local\arm-gnu\bin'
    $gcc=Join-Path $toolRoot 'arm-none-eabi-gcc.exe'
    $ar=Join-Path $toolRoot 'arm-none-eabi-ar.exe'
    $objcopy=Join-Path $toolRoot 'arm-none-eabi-objcopy.exe'
    $size=Join-Path $toolRoot 'arm-none-eabi-size.exe'
    if(!(Test-Path -LiteralPath $gcc)){throw 'Local ARM toolchain missing. See tools/README.md.'}
    $sdk='third_party/STM32CubeF7'
    $hal="$sdk/Drivers/STM32F7xx_HAL_Driver"
    $device="$sdk/Drivers/CMSIS/Device/ST/STM32F7xx"
    $fat="$sdk/Middlewares/Third_Party/FatFs/src"
    if($Board -eq 'f746'){$port='stm32f746g_discovery';$bspName='STM32746G-Discovery';$bspPrefix='stm32746g_discovery';$chip='STM32F746xx';$startup='stm32f746xx';$fpu='fpv5-sp-d16'}
    else {$port='stm32f769i_discovery';$bspName='STM32F769I-Discovery';$bspPrefix='stm32f769i_discovery';$chip='STM32F769xx';$startup='stm32f769xx';$fpu='fpv5-d16'}
    $bsp="$sdk/Drivers/BSP/$bspName"
    $output="build/$Board";if($Scalar){$output+='-scalar'}
    New-Item -ItemType Directory -Force "$output/obj" | Out-Null
    $cpu=@('-mcpu=cortex-m7','-mthumb',"-mfpu=$fpu",'-mfloat-abi=hard')
    $includes=@('inc','assets/generated','ports/common',"ports/$port","$hal/Inc","$hal/Inc/Legacy","$device/Include","$sdk/Drivers/CMSIS/Include","$sdk/Drivers/CMSIS/DSP/Include",$bsp,$fat,"$sdk/Utilities/Fonts")
    $flags=@($cpu)+@('-std=c11','-O3','-g3','-ffunction-sections','-fdata-sections','-fno-strict-aliasing','-ffp-contract=fast','-Wall','-Wextra','-Wno-unused-parameter',"-D$chip",'-DUSE_HAL_DRIVER','-DARM_MATH_CM7')
    if(!$Scalar){$flags+='-DDSP3D_CMSIS'}
    $flags+='-D__FPU_PRESENT=1U'
    if($Board -eq 'f769'){$flags+='-DBOARD_F769'}
    foreach($include in $includes){$flags+="-I$include"}
    $halUnits=@('hal','hal_cortex','hal_rcc','hal_rcc_ex','hal_gpio','hal_dma','hal_dma2d','hal_flash','hal_flash_ex','hal_pwr','hal_pwr_ex','hal_i2c','hal_i2c_ex','hal_uart','hal_uart_ex','hal_sdram','hal_ltdc','hal_ltdc_ex','hal_sd','ll_fmc','ll_sdmmc')
    if($Board -eq 'f769'){$halUnits+='hal_dsi'}
    $sources=@('src/dsp3D.c','src/dsp3D_model.c','src/dsp3D_geometry.c','examples/viewer/main.c','ports/common/board.c','ports/common/sd_diskio.c','ports/common/syscalls.c','assets/generated/metamask.c','assets/generated/suzanne.c',"$fat/ff.c","$fat/option/ccsbcs.c","$device/Source/Templates/system_stm32f7xx.c","$device/Source/Templates/gcc/startup_$startup.s")
    foreach($unit in $halUnits){$sources+="$hal/Src/stm32f7xx_$unit.c"}
    foreach($suffix in @('','_lcd','_sdram','_ts','_sd')){$sources+="$bsp/$bspPrefix$suffix.c"}
    if($Board -eq 'f746'){$sources+="$sdk/Drivers/BSP/Components/ft5336/ft5336.c"}
    else {$sources+="$sdk/Drivers/BSP/Components/ft6x06/ft6x06.c"}
    if($Board -eq 'f769'){foreach($part in @('otm8009a','nt35510','adv7533')){$sources+="$sdk/Drivers/BSP/Components/$part/$part.c"}}
    # Recompile when any project input changes. This favors correctness over a fragile header cache.
    $config=($flags -join "`n")
    $signature=Join-Path $output 'flags.txt'
    $changed=$Rebuild -or !(Test-Path $signature) -or (Get-Content $signature -Raw) -ne $config
    $headerTime=(Get-ChildItem inc,ports,assets/generated,"$sdk/Drivers",$fat -Recurse -File -Include *.h,*.inc | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1).LastWriteTimeUtc
    $objects=@();$libraryObjects=@()
    foreach($source in $sources){
        if(!(Test-Path -LiteralPath $source)){throw "Source missing: $source"}
        $object=Join-Path "$output/obj" (($source -replace '[/\\:.]','_')+'.o')
        if($changed -or !(Test-Path $object) -or (Get-Item $source).LastWriteTimeUtc -gt (Get-Item $object).LastWriteTimeUtc -or $headerTime -gt (Get-Item $object).LastWriteTimeUtc){
            Write-Host "CC $source"
            & $gcc @flags -c $source -o $object
            if($LASTEXITCODE){throw "Compilation failed: $source"}
        }
        if($source -like 'src/*'){$libraryObjects+=$object}else{$objects+=$object}
    }
    [IO.File]::WriteAllText((Join-Path $projectRoot $signature),$config)
    & $ar rcs "$output/libdsp3d.a" @libraryObjects
    if($LASTEXITCODE){throw 'Library archive failed'}
    $link=@($cpu)+@('-Wl,--gc-sections','-Wl,--print-memory-usage',"-Wl,-Map=$output/viewer.map",'-specs=nano.specs','-specs=nosys.specs',"-Tports/$port/flash.ld")
    & $gcc @link @objects "$output/libdsp3d.a" "$sdk/Drivers/CMSIS/Lib/GCC/libarm_cortexM7lfsp_math.a" -lm -lc -o "$output/viewer.elf"
    if($LASTEXITCODE){throw 'Firmware link failed'}
    & $objcopy -O binary "$output/viewer.elf" "$output/viewer.bin"
    if($LASTEXITCODE){throw 'Binary export failed'}
    & $objcopy -O ihex "$output/viewer.elf" "$output/viewer.hex"
    & $size "$output/viewer.elf"
    Write-Host "Built $output/viewer.elf and libdsp3d.a using only repository-local files."
} finally {Pop-Location}
