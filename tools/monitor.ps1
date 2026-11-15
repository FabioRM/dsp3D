# SPDX-License-Identifier: BSD-3-Clause
param([string]$Port='COM14',[int]$Seconds=10,[string]$Command='')
$ErrorActionPreference='Stop'
$serial=[System.IO.Ports.SerialPort]::new($Port,115200,'None',8,'One')
$serial.ReadTimeout=200
try {
    $serial.Open()
    if($Command){$serial.Write($Command)}
    $deadline=[DateTime]::UtcNow.AddSeconds($Seconds)
    while([DateTime]::UtcNow -lt $deadline){
        try {$serial.ReadLine()} catch [TimeoutException] {}
    }
} finally {if($serial.IsOpen){$serial.Close()};$serial.Dispose()}
