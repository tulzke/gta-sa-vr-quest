# Fast local updates of an already installed personal build. No uninstall path.
[CmdletBinding()]
param(
    [string]$ToolsFile = 'D:\development\SAVRBuild\tools.json',
    [string]$BuildRoot,
    [string]$SeedBuild,
    [string]$Keystore,
    [string]$Serial,
    [string]$GamePackage,
    [string]$AudioSource,
    [switch]$BuildOnly,
    [switch]$NoBackup,
    [switch]$Developer,
    [switch]$Full
)
$ErrorActionPreference = 'Stop'
$refreshSeed = $PSBoundParameters.ContainsKey('SeedBuild')
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (!$BuildRoot) { $BuildRoot = Join-Path $repo 'build\quest-deploy' }
if (!$SeedBuild) { $SeedBuild = Join-Path $repo 'build\vehicle-test-area' }
if (!$Keystore) {
    $Keystore = Join-Path ([Environment]::GetFolderPath('UserProfile')) 'AppData\Local\GTASAVRBuilder\signing\savr.keystore'
}
$BuildRoot = [IO.Path]::GetFullPath($BuildRoot)
$cfg = Get-Content -Raw -LiteralPath $ToolsFile | ConvertFrom-Json
New-Item -ItemType Directory -Force -Path $BuildRoot | Out-Null
$log = Join-Path $BuildRoot 'deploy.log'
Set-Content -LiteralPath $log -Value "Deploy started: $([DateTime]::Now.ToString('s'))" -Encoding UTF8
$timer = [Diagnostics.Stopwatch]::StartNew()
$adbArgs = @()
if ($Serial) { $adbArgs = @('-s', $Serial) }
$package = 'com.rockstargames.gtasa'

function Run {
    param([string]$Exe, [string[]]$Arguments)
    & $Exe @Arguments >> $log 2>&1
    if ($LASTEXITCODE -ne 0) { throw "$([IO.Path]::GetFileName($Exe)) failed (exit $LASTEXITCODE). Log: $log" }
}
function Backup-UserFiles {
    Run $cfg.Adb ($adbArgs + @('shell','am','force-stop',$package))
    if ($NoBackup) { return }
    $remote = "/sdcard/Android/data/$package/files"
    $names = @(& $cfg.Adb @adbArgs shell ls -1 $remote 2>> $log)
    if ($LASTEXITCODE -ne 0) { throw "Cannot list saves/settings. Log: $log" }
    $script:backup = Join-Path $BuildRoot ('backups\' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
    New-Item -ItemType Directory -Force -Path $script:backup | Out-Null
    foreach ($name in $names) {
        $name = ([string]$name).Trim()
        if ($name -match '^[A-Za-z0-9_.-]+\.(ini|b|set)$') {
            Run $cfg.Adb ($adbArgs + @('pull', "$remote/$name", $script:backup))
        }
    }
}

try {
    if ($Full) {
        # Explicit full mode uses the established validating builder/installer.
        if (!$GamePackage) { $GamePackage = Join-Path $SeedBuild 'out' }
        if (!$AudioSource) { $AudioSource = Join-Path $repo '.cache\package-source\audio' }
        if (!$BuildOnly) { Backup-UserFiles }
        $fullArgs = @{
            GamePackage=$GamePackage; AudioSource=$AudioSource
            WorkDir=(Split-Path -Parent $ToolsFile); AndroidSdk=$cfg.AndroidSdk
            JavaHome=$cfg.JavaHome; Keystore=$Keystore
            NonInteractive=$true; PersonalUpdate=$true; BuildOnly=$BuildOnly.IsPresent
        }
        if ($Serial) { $fullArgs.Serial=$Serial }
        & (Join-Path $PSScriptRoot 'build-and-install.ps1') @fullArgs *> $log
        # Carry forward new manifests/loader/HLOD after a full build, so the
        # next fast update cannot restore templates from an older full build.
        $outputMatch = [regex]::Matches((Get-Content -Raw -LiteralPath $log), '(?m)^APK output:\s*([^\r\n]+)')
        if ($outputMatch.Count) {
            $fullRoot = Split-Path -Parent $outputMatch[$outputMatch.Count-1].Groups[1].Value.Trim()
            $template = Join-Path $BuildRoot 'template'
            $hlodDir = Join-Path $BuildRoot 'aircraft-hlod-global'
            New-Item -ItemType Directory -Force -Path $template,$hlodDir | Out-Null
            foreach ($name in @('base.apk','split_config.arm64_v8a.apk')) {
                Copy-Item -LiteralPath (Join-Path $fullRoot "out\$name") -Destination (Join-Path $template $name) -Force
            }
            Copy-Item -LiteralPath (Join-Path $fullRoot 'aircraft-hlod-global\aircraft.hlod') -Destination $hlodDir -Force
        }
        Write-Host "Full deployment complete. Log: $log"
        return
    }

    $template = Join-Path $BuildRoot 'template'
    $out = Join-Path $BuildRoot 'out'
    New-Item -ItemType Directory -Force -Path $template,$out | Out-Null
    foreach ($name in @('base.apk','split_config.arm64_v8a.apk')) {
        $cached = Join-Path $template $name
        if ($refreshSeed -or !(Test-Path -LiteralPath $cached)) {
            Copy-Item -LiteralPath (Join-Path (Join-Path $SeedBuild 'out') $name) -Destination $cached
        }
    }
    $hlod = Join-Path $BuildRoot 'aircraft-hlod-global\aircraft.hlod'
    if ($refreshSeed -or !(Test-Path -LiteralPath $hlod)) {
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $hlod) | Out-Null
        Copy-Item -LiteralPath (Join-Path $SeedBuild 'aircraft-hlod-global\aircraft.hlod') -Destination $hlod
    }
    if (!(Test-Path -LiteralPath $Keystore)) { throw "Existing signing key not found: $Keystore" }

    # Keep all compiler temporaries inside the writable build directory.
    $oldTemp=$env:TEMP; $oldTmp=$env:TMP
    $temp = Join-Path $BuildRoot 'tmp'
    New-Item -ItemType Directory -Force -Path $temp | Out-Null
    try {
        $env:TEMP=$temp; $env:TMP=$temp
        Write-Host 'Building code and loader...'
        & (Join-Path $PSScriptRoot 'build.ps1') -AndroidSdk $cfg.AndroidSdk -JavaHome $cfg.JavaHome `
            -OpenXrLoader $cfg.OpenXrLoader -BuildRoot $BuildRoot -Developer:$Developer *> $log
    } finally { $env:TEMP=$oldTemp; $env:TMP=$oldTmp }

    # Reuse the installed-compatible manifests/engine. Rewrite only our code;
    # zipalign/sign below replace the old APK signing blocks and alignment.
    $rewrite = @'
from pathlib import Path
import sys, zipfile
root=Path(sys.argv[1])
sys.path.insert(0,sys.argv[2])
from assemble import existing_savr_loader_dex
loader=existing_savr_loader_dex(root/'template/base.apk')
for name,changes in [
    ('base.apk', {loader:root/'classes.dex'}),
    ('split_config.arm64_v8a.apk', {'lib/arm64-v8a/libsavr.so':root/'native/libsavr.so'})
]:
    with zipfile.ZipFile(root/'template'/name) as src, zipfile.ZipFile(root/'out'/('raw-'+name),'w') as dst:
        missing=set(changes)-set(src.namelist())
        if missing: raise RuntimeError(f'Incompatible cached APK {name}: missing {missing}')
        for item in src.infolist():
            upper=item.filename.upper()
            if upper.startswith('META-INF/') and (upper.endswith(('.SF','.RSA','.DSA','.EC')) or upper=='META-INF/MANIFEST.MF'):
                continue
            payload=changes[item.filename].read_bytes() if item.filename in changes else src.read(item.filename)
            dst.writestr(item,payload)
'@
    Write-Host 'Packaging two update APKs...'
    Run $cfg.Python @('-c', $rewrite, $BuildRoot, $PSScriptRoot)
    $buildTools = Join-Path $cfg.AndroidSdk 'build-tools\35.0.0'
    $java = Join-Path $cfg.JavaHome 'bin\java.exe'
    foreach ($name in @('base.apk','split_config.arm64_v8a.apk')) {
        $apk = Join-Path $out $name
        Run (Join-Path $buildTools 'zipalign.exe') @('-P','16','-f','4',(Join-Path $out ('raw-'+$name)),$apk)
        Run $java @('-jar',(Join-Path $buildTools 'lib\apksigner.jar'),'sign',
            '--ks',$Keystore,'--ks-key-alias','savr','--ks-pass','pass:savrsavr','--key-pass','pass:savrsavr',
            '--v1-signing-enabled','true','--v2-signing-enabled','true','--v3-signing-enabled','true',
            '--v4-signing-enabled','false',$apk)
    }
    if (!$BuildOnly) {
        Write-Host 'Backing up saves/settings and installing update...'
        Backup-UserFiles
        # Inherit the installed data/locale/density splits instead of sending GBs.
        # Android rejects a signature mismatch atomically; never uninstall/retry.
        Run $cfg.Adb ($adbArgs + @('install-multiple','-r','-p',$package,(Join-Path $out 'base.apk'),(Join-Path $out 'split_config.arm64_v8a.apk')))
    }
    $verb = if ($BuildOnly) { 'Built' } else { 'Installed' }
    Write-Host "$verb in $([math]::Round($timer.Elapsed.TotalSeconds))s. Log: $log"
    if ($script:backup) { Write-Host "Backup: $script:backup" }
} catch {
    Write-Error "Deploy stopped: $($_.Exception.Message)"
    exit 1
}
