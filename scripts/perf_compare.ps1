# A/B speed comparison of two opti_chess_cli binaries, runs interleaved (A B A B ...)
# to spread machine noise. Usage: perf_compare.ps1 -A old.exe -B new.exe [-Rounds 3] [-BenchIters 2000]
param([string]$A, [string]$B, [int]$Rounds = 3, [int]$BenchIters = 2000)

function Perft-Mnps($exe) {
    $out = & $exe perft-suite full
    $total = $out | Where-Object { $_ -like 'TOTAL*' }
    $ok = $out | Where-Object { $_ -like 'RESULT*' }
    [double]([regex]::Match($total, 'mnps=([\d.]+)').Groups[1].Value), $ok
}
function Bench($exe) {
    $out = & $exe bench $BenchIters
    $line = $out | Where-Object { $_ -like 'total*' }
    $sig = ($out | Where-Object { $_ -like 'signature*' }) -replace 'signature=', ''
    [double]([regex]::Match($line, 'nodes/s=(\d+)').Groups[1].Value), $sig
}

$rows = @()
for ($r = 1; $r -le $Rounds; $r++) {
    foreach ($side in @('A', 'B')) {
        $exe = if ($side -eq 'A') { $A } else { $B }
        $p = Perft-Mnps $exe
        $bn = Bench $exe
        $rows += [pscustomobject]@{ round = $r; side = $side; perft_mnps = $p[0]; perft = $p[1]; bench_nps = $bn[0]; signature = $bn[1] }
    }
}
$rows | Format-Table -AutoSize | Out-String
foreach ($side in @('A', 'B')) {
    $s = $rows | Where-Object { $_.side -eq $side }
    $pm = ($s.perft_mnps | Sort-Object)[[int][math]::Floor($s.Count / 2)]
    $bm = ($s.bench_nps | Sort-Object)[[int][math]::Floor($s.Count / 2)]
    "median $side : perft $pm Mnps, bench $bm nodes/s"
}
