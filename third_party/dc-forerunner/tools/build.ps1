# Build the title-screen image: assets from the owner's disc (tools/build_assets.py), ELF with KOS, disc image with mkdcdisc.
# Reads config\config.local. Every child hidden, output to build\build.log. Image: $DC_OUT\dc-<PORT_NAME>-test.cdi (numbered images are the lead's job).
param([switch]$SkipAssets, [string]$Out = "")
$ErrorActionPreference = "Continue"   # native tools write progress to stderr; failures are caught by exit code
$root = Split-Path -Parent $PSScriptRoot
$cfg = @{}
foreach ($l in Get-Content (Join-Path $root "config\config.local")) { if ($l -match '^([A-Z0-9_]+)=(.+)$') { $cfg[$Matches[1]] = $Matches[2] } }
foreach ($k in @("PORT_NAME", "DC_TITLE", "DC_VOLUME_ID", "MSYS2_ROOT", "KOS_BASE", "KOS_CC_BASE", "MKDCDISC", "DC_OUT")) {
    if (-not $cfg[$k]) { throw "config key $k missing: see config/config.example.cfg" }
}
$name = $cfg["PORT_NAME"]
$log = Join-Path $root "build\build.log"
New-Item -ItemType Directory -Force (Join-Path $root "build") | Out-Null
if (-not $SkipAssets) { & python -I (Join-Path $root "tools\build_assets.py") 2>&1 | Tee-Object -FilePath $log; if ($LASTEXITCODE) { throw "assets failed" } }
$msys = $cfg["MSYS2_ROOT"]
function U($p) { $p = $p.Replace([string][char]92, '/'); if ($p -match '^([A-Za-z]):(.*)') { return '/' + $Matches[1].ToLower() + $Matches[2] } return $p }
$kos = U $cfg["KOS_BASE"]; $cc = U $cfg["KOS_CC_BASE"]; $proj = U $root
$sh = Join-Path $msys "usr\bin\bash.exe"
$cmd = "export PATH=/mingw64/bin:$cc/bin:`$PATH; export KOS_CC_BASE=$cc; source $kos/environ.sh; cd $proj && make -j2 PROJECT=$name"
& $sh -lc $cmd *>> $log
if ($LASTEXITCODE) { throw "make failed, see build\build.log" }
$stage = Join-Path $root "build\disc"
$dcout = $cfg["DC_OUT"]; New-Item -ItemType Directory -Force $dcout | Out-Null
if (-not $Out) { $Out = Join-Path $dcout "dc-$name-test.cdi" }
& $cfg["MKDCDISC"] -e (Join-Path $root "build\$name.elf") -D $stage -o $Out -n $cfg["DC_TITLE"] -s $cfg["DC_VOLUME_ID"] --allow-overwrite *>> $log
if ($LASTEXITCODE) { throw "mkdcdisc failed, see build\build.log" }
Write-Host "image: $Out"
