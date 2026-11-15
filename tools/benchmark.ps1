# SPDX-License-Identifier: BSD-3-Clause
param([string]$Port='COM14',[string]$Output='build/benchmark.csv',[int]$TimeoutSeconds=45)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$targetPath=[IO.Path]::GetFullPath((Join-Path $projectRoot $Output))
New-Item -ItemType Directory -Force (Split-Path $targetPath) | Out-Null
$serial=[System.IO.Ports.SerialPort]::new($Port,115200,'None',8,'One')
$serial.ReadTimeout=200
$rows=[Collections.Generic.List[string]]::new()
$raw=[Collections.Generic.List[string]]::new()
try {
    $serial.Open();$serial.DiscardInBuffer();$serial.Write('b')
    $deadline=[DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while($rows.Count -lt 8 -and [DateTime]::UtcNow -lt $deadline){
        try {
            $line=$serial.ReadLine().Trim();$raw.Add($line)
            if($line -match '(BENCH,(?:cmsis-dsp|scalar),[^\r\n]+)'){$rows.Add($Matches[1]);Write-Host $Matches[1]}
        } catch [TimeoutException] {}
    }
} finally {if($serial.IsOpen){$serial.Close()};$serial.Dispose()}
$raw | Set-Content ($targetPath+'.log')
if($rows.Count -ne 8){throw "Incomplete benchmark: $($rows.Count)/8 rows received"}
$header='record,backend,board,width,height,clock_hz,model,mode,frames,mean_render_us,min_render_us,max_render_us,mean_clear_us'
@($header)+$rows.ToArray() | Set-Content $targetPath
Write-Host "Saved $targetPath"
