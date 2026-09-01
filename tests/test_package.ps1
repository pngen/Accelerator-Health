# Copyright 2026 Summon Software Labs
# Licensed under the Apache License, Version 2.0 (the "License"); see LICENSE.
# Installs the package and validates an independent downstream consumer.
param([string]$BuildDir = "build-rel")
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$prefix = Join-Path $root "ah_install_prefix"
$consumerDir = Join-Path $root "consumer"
$bdir = Join-Path $root "consumer-build"
Remove-Item $prefix -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item $bdir -Recurse -Force -ErrorAction SilentlyContinue
$installOut = cmake --install $BuildDir --prefix $prefix 2>&1
if ($LASTEXITCODE -ne 0) { Write-Output "INSTALL FAIL: $installOut"; exit 1 }
New-Item -ItemType Directory -Path $bdir | Out-Null
$cfgOut = cmake -S $consumerDir -B $bdir -G Ninja -DCMAKE_PREFIX_PATH="$prefix" -DCMAKE_BUILD_TYPE=Release 2>&1
if ($LASTEXITCODE -ne 0) { Write-Output "CONFIGURE FAIL: $cfgOut"; exit 1 }
$buildOut = cmake --build $bdir 2>&1
if ($LASTEXITCODE -ne 0) { Write-Output "BUILD FAIL: $buildOut"; exit 1 }
$out = & (Join-Path $bdir "ahc-consumer.exe") 2>&1
if ($out -match "consumer state=HEALTHY") { Write-Output "DOWNSTREAM-CONSUMER PASS"; exit 0 }
Write-Output "DOWNSTREAM-CONSUMER FAIL: $out"; exit 1
