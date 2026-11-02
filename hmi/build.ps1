param([Parameter(Mandatory=$true)][string]$MibHmi,[string]$JdkBin)
$ErrorActionPreference='Stop'
if(-not $JdkBin){$JdkBin=Split-Path (Get-Command javac).Source}
$root=Split-Path -Parent $MyInvocation.MyCommand.Path
$out=Join-Path $root 'build'
if(Test-Path -LiteralPath $out){Remove-Item -LiteralPath $out -Recurse -Force}
New-Item -ItemType Directory -Path (Join-Path $out 'classes') | Out-Null
$sources=Get-ChildItem -LiteralPath (Join-Path $root 'src\mst2\nar\cluster') -Filter '*.java' | ForEach-Object FullName
& (Join-Path $JdkBin 'javac.exe') -source 1.3 -target 1.3 -nowarn -encoding UTF-8 -cp $MibHmi -d (Join-Path $out 'classes') $sources
if($LASTEXITCODE){throw 'javac failed'}
& (Join-Path $JdkBin 'jar.exe') cfm (Join-Path $out 'mst2-nar-rgi-hmi.jar') (Join-Path $root 'manifest.mf') -C (Join-Path $out 'classes') .
if($LASTEXITCODE){throw 'jar failed'}
Write-Output (Join-Path $out 'mst2-nar-rgi-hmi.jar')
