$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$Exe = "out/windows-x64-debug/MyGrep.exe"
$ExePath = Join-Path $RepoRoot $Exe
$DotExe = "C:\Program Files (x86)\Graphviz\bin\dot.exe"
$OutDir = Join-Path $PSScriptRoot "visualization-out"

if (-not (Test-Path -LiteralPath $ExePath)) {
    throw "Executable not found: $Exe"
}

if (-not (Test-Path -LiteralPath $DotExe)) {
    throw "Graphviz dot.exe not found: $DotExe"
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

function Invoke-VisualizationTest {
    param(
        [string]$Name,
        [string]$Pattern,
        [string]$Mode
    )

    $BaseName = "$Name-$Mode"
    $DotFile = Join-Path $OutDir "$BaseName.dot"
    $PngFile = Join-Path $OutDir "$BaseName.png"

    & $ExePath "-v" $Mode $Pattern $DotFile
    if ($LASTEXITCODE -ne 0) {
        throw "$BaseName failed to generate dot with exit code $LASTEXITCODE"
    }

    & $DotExe "-Tpng" $DotFile "-o" $PngFile
    if ($LASTEXITCODE -ne 0) {
        throw "$BaseName failed to render png with exit code $LASTEXITCODE"
    }

    Write-Host "PASS $BaseName"
}

$Cases = @(
    @{ Name = "literal"; Pattern = "ab" },
    @{ Name = "alternation"; Pattern = "a|b" },
    @{ Name = "star"; Pattern = "a*b" },
    @{ Name = "grouped"; Pattern = "(a|b)*abb" },
    @{ Name = "nested"; Pattern = "((ab)*b)*abbb*(a|b)*" }
)

foreach ($Case in $Cases) {
    Invoke-VisualizationTest -Name $Case.Name -Pattern $Case.Pattern -Mode "NFA"
    Invoke-VisualizationTest -Name $Case.Name -Pattern $Case.Pattern -Mode "DFA"
    Invoke-VisualizationTest -Name $Case.Name -Pattern $Case.Pattern -Mode "MINDFA"
}