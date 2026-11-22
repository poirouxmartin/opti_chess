# Speed comparison of two or more opti_chess_cli binaries, runs interleaved to spread machine noise.
# Usage: perf_compare.ps1 -A old.exe -B new.exe [-Rounds 3] [-BenchIters 2000]
#        perf_compare.ps1 -Exes a.exe,b.exe,c.exe [-Rounds 7] [-Csv out.csv]
# With N binaries the order rotates every round (A B C, B C A, C A B...) so that no binary
# always runs first or last. Reports the median and min/max of each side, then per-round
# paired ratios of every side against A (the robust number on a noisy machine).
# -Pin 0x800 pins every run on one logical core at High priority, to cut noise on a busy machine (0 = no pinning).
param([string]$A, [string]$B, [string[]]$Exes, [int]$Rounds = 3, [int]$BenchIters = 2000, [string]$Csv, [long]$Pin = 0)

if (-not $Exes) { $Exes = @($A, $B) }
# powershell -File passes "a,b,c" as one string
$Exes = @($Exes | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$names = @()
for ($i = 0; $i -lt $Exes.Count; $i++) { $names += [string][char](65 + $i) }

function Run-Cli($exe, [string[]]$cliArgs) {
    if ($Pin -eq 0) { return & $exe @cliArgs }
    $tmp = [IO.Path]::GetTempFileName()
    $p = Start-Process -FilePath $exe -ArgumentList $cliArgs -NoNewWindow -PassThru -RedirectStandardOutput $tmp
    try { $p.ProcessorAffinity = [IntPtr]$Pin; $p.PriorityClass = 'High' } catch {}
    $p.WaitForExit()
    $lines = Get-Content $tmp
    Remove-Item $tmp
    return $lines
}
function Perft-Mnps($exe) {
    $out = Run-Cli $exe @('perft-suite','full')
    $total = $out | Where-Object { $_ -like 'TOTAL*' }
    $ok = $out | Where-Object { $_ -like 'RESULT*' }
    [double]([regex]::Match($total, 'mnps=([\d.]+)').Groups[1].Value), $ok
}
function Bench($exe) {
    $out = Run-Cli $exe @('bench', "$BenchIters")
    $line = $out | Where-Object { $_ -like 'total*' }
    $sig = ($out | Where-Object { $_ -like 'signature*' }) -replace 'signature=', ''
    [double]([regex]::Match($line, 'nodes/s=(\d+)').Groups[1].Value), $sig
}
function Median($xs) {
    $s = @($xs | Sort-Object)
    if ($s.Count % 2) { $s[[int][math]::Floor($s.Count / 2)] } else { ($s[$s.Count / 2 - 1] + $s[$s.Count / 2]) / 2 }
}

$rows = @()
for ($r = 1; $r -le $Rounds; $r++) {
    for ($k = 0; $k -lt $Exes.Count; $k++) {
        $i = ($k + $r - 1) % $Exes.Count
        $p = Perft-Mnps $Exes[$i]
        $bn = Bench $Exes[$i]
        $rows += [pscustomobject]@{ round = $r; side = $names[$i]; perft_mnps = $p[0]; perft = $p[1]; bench_nps = $bn[0]; signature = $bn[1] }
    }
}
if ($Csv) { $rows | Export-Csv -NoTypeInformation -Path $Csv }
$rows | Format-Table -AutoSize | Out-String
for ($i = 0; $i -lt $Exes.Count; $i++) {
    $s = @($rows | Where-Object { $_.side -eq $names[$i] })
    $pm = Median $s.perft_mnps; $bm = Median $s.bench_nps
    $pr = ($s.perft_mnps | Measure-Object -Minimum -Maximum); $br = ($s.bench_nps | Measure-Object -Minimum -Maximum)
    "median $($names[$i]) ($($Exes[$i])) : perft $pm Mnps [$($pr.Minimum)-$($pr.Maximum)], bench $bm nodes/s [$($br.Minimum)-$($br.Maximum)]"
}
for ($i = 1; $i -lt $Exes.Count; $i++) {
    $bRatios = @(); $pRatios = @()
    for ($r = 1; $r -le $Rounds; $r++) {
        $ref = $rows | Where-Object { $_.side -eq $names[0] -and $_.round -eq $r }
        $cur = $rows | Where-Object { $_.side -eq $names[$i] -and $_.round -eq $r }
        $bRatios += $cur.bench_nps / $ref.bench_nps
        $pRatios += $cur.perft_mnps / $ref.perft_mnps
    }
    $bm = [math]::Round((Median $bRatios), 3); $pm = [math]::Round((Median $pRatios), 3)
    $br = $bRatios | Measure-Object -Minimum -Maximum; $pr = $pRatios | Measure-Object -Minimum -Maximum
    "ratio $($names[$i])/A : bench median $bm [$([math]::Round($br.Minimum, 3))-$([math]::Round($br.Maximum, 3))], perft median $pm [$([math]::Round($pr.Minimum, 3))-$([math]::Round($pr.Maximum, 3))]"
}
