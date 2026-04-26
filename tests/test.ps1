$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$Exe = "out/windows-x64-debug/MyGrep.exe"
$ExePath = Join-Path $RepoRoot $Exe

if (-not (Test-Path -LiteralPath $ExePath)) {
    throw "Executable not found: $Exe"
}

function Assert-MyGrep {
    param(
        [string]$Name,
        [string]$Pattern,
        [string]$File,
        [string[]]$Options = @(),
        [string[]]$Expected
    )

    $FilePath = Join-Path $PSScriptRoot $File
    $Actual = @(& $ExePath @Options $Pattern $FilePath)
    if ($LASTEXITCODE -ne 0) {
        throw "$Name failed with exit code $LASTEXITCODE"
    }

    if ($Actual.Count -ne $Expected.Count) {
        throw "$Name expected $($Expected.Count) lines but got $($Actual.Count). Actual: $($Actual -join ', ')"
    }

    for ($Index = 0; $Index -lt $Expected.Count; $Index++) {
        if ($Actual[$Index] -ne $Expected[$Index]) {
            throw "$Name line $Index expected '$($Expected[$Index])' but got '$($Actual[$Index])'"
        }
    }

    Write-Host "PASS $Name"
}

Assert-MyGrep -Name "literal-ab-basic" -Pattern "ab" -File "ab_basic.txt" -Expected @("ab", "aba")
Assert-MyGrep -Name "alternation-basic" -Pattern "a|b" -File "ab_basic.txt" -Expected @("a", "b", "ab", "ba", "aba", "bbb", "baa", "aaa")
Assert-MyGrep -Name "star-basic" -Pattern "a*b" -File "ab_basic.txt" -Expected @("b", "ab", "ba", "aba", "bbb", "baa")
Assert-MyGrep -Name "grouped-abb" -Pattern "(a|b)*abb" -File "ab_blocks.txt" -Expected @("abb", "aabb", "babb", "abba")
Assert-MyGrep -Name "literal-none" -Pattern "aaa" -File "ab_blocks.txt" -Expected @()
Assert-MyGrep -Name "literal-ba-repeats" -Pattern "ba" -File "ab_repeats.txt" -Expected @("abab", "baba", "bbba")
Assert-MyGrep -Name "whole-literal-ab-basic" -Pattern "ab" -File "ab_basic.txt" -Options @("-x") -Expected @("ab")
Assert-MyGrep -Name "whole-alternation-basic" -Pattern "a|b" -File "ab_basic.txt" -Options @("-x") -Expected @("a", "b")
Assert-MyGrep -Name "whole-star-basic" -Pattern "a*b" -File "ab_basic.txt" -Options @("-x") -Expected @("b", "ab")
Assert-MyGrep -Name "whole-grouped-abb" -Pattern "(a|b)*abb" -File "ab_blocks.txt" -Options @("-x") -Expected @("abb", "aabb", "babb")
Assert-MyGrep -Name "whole-literal-substring-none" -Pattern "ab" -File "ab_repeats.txt" -Options @("-x") -Expected @()