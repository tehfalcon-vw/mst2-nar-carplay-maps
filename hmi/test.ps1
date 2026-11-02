param([string]$JdkBin)
$ErrorActionPreference='Stop'
if(-not $JdkBin){$JdkBin=Split-Path (Get-Command javac).Source}
$root=Split-Path -Parent $MyInvocation.MyCommand.Path
$out=Join-Path $root 'build\test-classes'
if(Test-Path -LiteralPath $out){Remove-Item -LiteralPath $out -Recurse -Force}
New-Item -ItemType Directory -Path $out | Out-Null
$sources=@(
  (Join-Path $root 'test\de\vw\mib\asl\api\navbap\INavLaneGuidanceDataNavBap.java'),
  (Join-Path $root 'src\mst2\nar\cluster\Snapshot.java'),
  (Join-Path $root 'src\mst2\nar\cluster\LaneData.java'),
  (Join-Path $root 'src\mst2\nar\cluster\Maneuver.java'),
  (Join-Path $root 'test\mst2\nar\cluster\Check.java')
)
& (Join-Path $JdkBin 'javac.exe') -source 1.3 -target 1.3 -nowarn -encoding UTF-8 -d $out $sources
if($LASTEXITCODE){throw 'javac failed'}
& (Join-Path $JdkBin 'java.exe') -cp $out mst2.nar.cluster.Check
if($LASTEXITCODE){throw 'HMI model checks failed'}
