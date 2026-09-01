# Copyright 2026 Summon Software Labs
# Licensed under the Apache License, Version 2.0 (the "License"); see LICENSE.
# Multiprocess distributed-health proof: a real coordinator plus two worker OS
# processes over framed TCP, exercising stale-authority fencing and recovery.
param(
  [string]$BuildDir = "build-cuda",
  [int]$Port = 7521
)
$ErrorActionPreference = "Stop"
$coord = Join-Path $BuildDir "apps\ah-coordinator.exe"
$worker = Join-Path $BuildDir "apps\ah-worker.exe"
if (-not (Test-Path $coord)) { Write-Output "coordinator not built"; exit 2 }
$state = Join-Path (Get-Location) "dist_state.bin"
Get-Process ah-coordinator,ah-worker -ErrorAction SilentlyContinue | Stop-Process -Force
Remove-Item $state -ErrorAction SilentlyContinue

$failures = 0
function Check($name, $cond) {
  if ($cond) { Write-Output "[ OK  ] $name" } else { Write-Output "[FAIL ] $name"; $script:failures++ }
}
function RunWorker([string[]]$workerArgs) {
  $out = & $worker @workerArgs 2>&1
  return ($out -join [char]10)
}
function StartCoord($epoch, $stateFile, $load = $false) {
  $argsList = @("--port", "$Port", "--epoch", "$epoch", "--state", $stateFile)
  if ($load) { $argsList += "--load" }
  $p = Start-Process -FilePath $coord -ArgumentList $argsList -RedirectStandardOutput "dcoord.log" -RedirectStandardError "dcoord.err" -PassThru -WindowStyle Hidden
  Start-Sleep -Milliseconds 900
  return $p
}

# Steps 1-4: coordinator starts, Worker A and Worker B register/publish/validate.
$c = StartCoord 1 $state
$wa = Start-Process -FilePath $worker -ArgumentList @("--port","$Port","--worker-id","1","--boot-id","1","--device","100","--mode","live") -PassThru -WindowStyle Hidden -RedirectStandardOutput "wa.txt"
$wb = Start-Process -FilePath $worker -ArgumentList @("--port","$Port","--worker-id","2","--boot-id","1","--device","200","--mode","live") -PassThru -WindowStyle Hidden -RedirectStandardOutput "wb.txt"
Start-Sleep -Milliseconds 1500
$qa = RunWorker @("--port","$Port","--worker-id","3","--boot-id","1","--device","100","--mode","query")
$qb = RunWorker @("--port","$Port","--worker-id","4","--boot-id","1","--device","200","--mode","query")
Check "device A healthy/ready" ($qa -match "state=HEALTHY" -and $qa -match "readiness=READY")
Check "device B healthy/ready" ($qb -match "state=HEALTHY" -and $qb -match "readiness=READY")
Check "two workers live" ((Get-Process -Id $wa.Id -ErrorAction SilentlyContinue) -and (Get-Process -Id $wb.Id -ErrorAction SilentlyContinue))

# Steps 6-7: kill Worker A (real OS process) -> mark its device stale.
Stop-Process -Id $wa.Id -Force
Start-Sleep -Milliseconds 700
$qa2 = RunWorker @("--port","$Port","--worker-id","3","--boot-id","1","--device","100","--mode","query")
$qb2 = RunWorker @("--port","$Port","--worker-id","4","--boot-id","1","--device","200","--mode","query")
Check "device A stale after kill" ($qa2 -match "readiness=REVALIDATION_REQUIRED" -or $qa2 -match "readiness=NOT_READY")
Check "device B unaffected" ($qb2 -match "state=HEALTHY")

# Step 8: roll CoordinatorEpoch (restart coordinator at epoch 2).
Stop-Process -Id $wb.Id -Force -ErrorAction SilentlyContinue
Stop-Process -Id $c.Id -Force
Start-Sleep -Milliseconds 400
$c2 = StartCoord 2 $state

# Steps 10-11: restart Worker A with fresh boot, replay stale authority -> rejected.
$a_epoch = RunWorker @("--port","$Port","--worker-id","1","--boot-id","2","--device","100","--epoch","1","--mode","stale-epoch")
$a_boot  = RunWorker @("--port","$Port","--worker-id","1","--boot-id","2","--device","100","--mode","stale-boot")
$a_obs   = RunWorker @("--port","$Port","--worker-id","1","--boot-id","3","--device","100","--mode","stale-obsgen")
$a_dev   = RunWorker @("--port","$Port","--worker-id","1","--boot-id","2","--device","100","--mode","stale-devgen")
Check "stale coordinator epoch rejected" ($a_epoch -match "accept=0")
Check "stale worker boot rejected" ($a_boot -match "accept=0")
Check "stale observation generation rejected" ($a_obs -match "second=0")
Check "stale device generation rejected" ($a_dev -match "accept=0")

# Steps 13-15: Worker A publishes fresh evidence + revalidation -> HEALTHY/READY.
$wa2 = Start-Process -FilePath $worker -ArgumentList @("--port","$Port","--worker-id","1","--boot-id","5","--epoch","2","--device","100","--mode","live","--hold","10") -PassThru -WindowStyle Hidden -RedirectStandardOutput "wa2.txt"
Start-Sleep -Milliseconds 700
$af = "live"
$qa3 = RunWorker @("--port","$Port","--worker-id","5","--boot-id","1","--device","100","--mode","query")
Check "device A healthy again" ($qa3 -match "state=HEALTHY" -and $qa3 -match "readiness=READY")

# Steps 16-17: save coordinator state, restart with --load.
RunWorker @("--port","$Port","--worker-id","6","--boot-id","1","--device","100","--mode","query") | Out-Null
Stop-Process -Id $c2.Id -Force
Start-Sleep -Milliseconds 400
$c3 = StartCoord 2 $state $true
# Step 18: recovered dynamic evidence is NOT falsely CURRENT.
$qr = RunWorker @("--port","$Port","--worker-id","7","--boot-id","1","--device","100","--mode","query")
Check "recovered evidence not CURRENT" (($qr -notmatch "readiness=READY") -and ($qr -match "readiness=REVALIDATION_REQUIRED"))

# Steps 19-20: refresh and reproduce a stable digest.
$wa3 = Start-Process -FilePath $worker -ArgumentList @("--port","$Port","--worker-id","1","--boot-id","6","--epoch","2","--device","100","--mode","live","--hold","8") -PassThru -WindowStyle Hidden -RedirectStandardOutput "wa3.txt"
Start-Sleep -Milliseconds 700
$af2 = "live"
$q1 = RunWorker @("--port","$Port","--worker-id","8","--boot-id","1","--device","100","--mode","query")
$q2 = RunWorker @("--port","$Port","--worker-id","9","--boot-id","1","--device","100","--mode","query")
$d1 = ""; $d2 = ""
if ($q1 -match "digest=([0-9a-f]+)") { $d1 = $Matches[1] }
if ($q2 -match "digest=([0-9a-f]+)") { $d2 = $Matches[1] }
Check "stable digest reproduced" ($d1 -ne "" -and $d1 -eq $d2)

Stop-Process -Id $wa2.Id,$wa3.Id -Force -ErrorAction SilentlyContinue
Stop-Process -Id $c3.Id -Force -ErrorAction SilentlyContinue
if ($failures -eq 0) { Write-Output "DISTRIBUTED-PROOF PASS"; exit 0 } else { Write-Output "DISTRIBUTED-PROOF FAIL"; exit 1 }