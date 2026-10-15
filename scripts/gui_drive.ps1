<#
.SYNOPSIS
  Drives the raylib GUI (opti_chess.exe) with synthetic input and checks its debug log.
  Real window, real input (keybd_event / mouse_event), no test harness inside the GUI.

.USAGE
  powershell -ExecutionPolicy Bypass -File scripts\gui_drive.ps1 -Script scripts\gui_steps\smoke.txt
  powershell -ExecutionPolicy Bypass -File scripts\gui_drive.ps1 -Steps 'wait 500','key CTRL+G','expect_log iter=1,'
  Params: -Exe -Script -Steps -Width 1200 -Height 800 -OutDir -KeyHoldMs 120 -StepDelayMs 250 -KeepOpen
  If both -Script and -Steps are given, script lines run first, then -Steps.
  Exit code 0 if no FAIL, 1 if any FAIL, 2 on bad arguments.

.STEP GRAMMAR (one per line; blank and '#' lines ignored; tokens split on whitespace)
  wait <ms>
  key <combo>            combo: G, CTRL+G, SHIFT+P, CTRL+SHIFT+X, LEFT, RIGHT, UP, DOWN, F1..F12,
                         KP0..KP9, SPACE, ENTER, DEL, ESC, TAB, BACKSPACE, digit, letter
  hold <combo> <ms>      same key handling, held for <ms>
  click <sq>             left click on square (e.g. e2), uses [geom] mapping
  rclick <sq>            right click on square
  clickxy <x> <y>        raylib coords (unscaled), multiplied by client/geom scale
  drag <from> <to>       left drag, 8 steps over ~300 ms
  rdrag <from> <to>      right drag
  resize <w> <h>         client area resize as in setup, then wait 800
  minimize | restore     ShowWindow 6 / 9, then wait 800
  shot <name>            PNG of client area to OutDir\<name>.png
  clip <text>            Set-Clipboard, text kept verbatim
  expect_fen <fen>       last "[pos] <tag> fen=" line; first 4 FEN fields must match
  expect_log <regex>     PASS if any new log line matches
  expect_nolog <regex>   PASS if no new log line matches
  alive                  PASS if the process has not exited

.GUI REQUIREMENTS
  The GUI must log "[geom] w=<int> h=<int> pad_x=<n> pad_y=<n> tile=<n> orient=<0|1>"
  (from resize_GUI) for square/clickxy mapping, and "[pos] <tag> fen=<FEN>" lines for expect_fen.
  All log checks read only the lines written after the script started (opti_chess_debug.log).
#>
[CmdletBinding()]
param(
  [string]$Exe,
  [string]$Script,
  [string[]]$Steps,
  [int]$Width = 1200,
  [int]$Height = 800,
  [string]$OutDir,
  [int]$KeyHoldMs = 120,
  [int]$StepDelayMs = 250,
  [switch]$KeepOpen
)

$ErrorActionPreference = 'Continue'
# PS 5.1 leaves $PSScriptRoot empty inside param defaults: resolve here.
$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $Exe) { $Exe = Join-Path $repoRoot 'opti_chess\build\windows-release\Release\opti_chess.exe' }
if (-not $OutDir) { $OutDir = Join-Path $repoRoot 'build\gui_audit' }
# GetFullPath resolves against the process CWD, not the PS location
if (-not [System.IO.Path]::IsPathRooted($Exe)) { $Exe = Join-Path (Get-Location) $Exe }
if (-not [System.IO.Path]::IsPathRooted($OutDir)) { $OutDir = Join-Path (Get-Location) $OutDir }
$Exe = [System.IO.Path]::GetFullPath($Exe)
$OutDir = [System.IO.Path]::GetFullPath($OutDir)

# ---------- Argument checks and step loading ----------
if (-not $Script -and -not $Steps) { Write-Host 'Need -Script <file> or -Steps <lines>'; exit 2 }
$rawLines = New-Object System.Collections.Generic.List[string]
if ($Script) {
  if (-not (Test-Path -LiteralPath $Script)) { Write-Host "Script not found: $Script"; exit 2 }
  foreach ($l in (Get-Content -LiteralPath $Script -Encoding UTF8)) { $rawLines.Add($l) }
}
if ($Steps) {
  foreach ($s in $Steps) { foreach ($l in ($s -split '\r?\n')) { $rawLines.Add($l) } }
}
$stepList = New-Object System.Collections.Generic.List[string]
foreach ($l in $rawLines) {
  $t = $l.Trim()
  if ($t -eq '' -or $t.StartsWith('#')) { continue }
  $stepList.Add($l)
}
if (-not (Test-Path -LiteralPath $Exe)) { Write-Host "exe not found: $Exe"; exit 2 }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

# ---------- Win32 P/Invoke (single Add-Type) ----------
$src = @'
using System;
using System.Runtime.InteropServices;
namespace OptiGuiDrive {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
  public static class Native {
    [DllImport("user32.dll")] public static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, UIntPtr dwExtraInfo);
    [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint uCode, uint uMapType);
    [DllImport("user32.dll")] public static extern void mouse_event(uint dwFlags, uint dx, uint dy, uint dwData, UIntPtr dwExtraInfo);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int X, int Y);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hWnd, out RECT lpRect);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr hWnd, ref POINT lpPoint);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
    [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    public static int[] WindowRect(IntPtr h) { RECT r; GetWindowRect(h, out r); return new int[] { r.Left, r.Top, r.Right, r.Bottom }; }
    public static int[] ClientRect(IntPtr h) { RECT r; GetClientRect(h, out r); return new int[] { r.Left, r.Top, r.Right, r.Bottom }; }
    public static int[] ToScreen(IntPtr h, int x, int y) { POINT p; p.X = x; p.Y = y; ClientToScreen(h, ref p); return new int[] { p.X, p.Y }; }
  }
}
'@
Add-Type -AssemblyName System.Drawing
if (-not ('OptiGuiDrive.Native' -as [type])) { Add-Type -TypeDefinition $src -Language CSharp }
[void][OptiGuiDrive.Native]::SetProcessDPIAware()

$MOUSE_LDOWN = [uint32]2
$MOUSE_LUP = [uint32]4
$MOUSE_RDOWN = [uint32]8
$MOUSE_RUP = [uint32]16
$KEYUP = [uint32]2
$KEYEVENTF_EXTENDEDKEY = [uint32]1
# Without the extended flag Windows delivers the numpad keys for these VKs
$EXTENDED_VKS = @(0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x2D, 0x2E)
$SWP_NOZORDER_SHOW = [uint32](0x0004 -bor 0x0040)

# ---------- Setup: kill, log offset, start, window, size ----------
Get-Process -Name 'opti_chess' -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 300

$script:LogFile = Join-Path (Split-Path $Exe) 'opti_chess_debug.log'
$script:LogStart = 0
if (Test-Path -LiteralPath $script:LogFile) { $script:LogStart = (Get-Item -LiteralPath $script:LogFile).Length }

$proc = Start-Process -FilePath $Exe -WorkingDirectory (Split-Path $Exe) -PassThru
$deadline = (Get-Date).AddSeconds(20)
while ($true) {
  $proc.Refresh()
  if ($proc.HasExited) { Write-Host "process exited during startup (code $($proc.ExitCode))"; exit 1 }
  if ($proc.MainWindowHandle -ne [IntPtr]::Zero) { break }
  if ((Get-Date) -gt $deadline) { Write-Host 'no main window after 20 s'; Stop-Process -Id $proc.Id -Force; exit 1 }
  Start-Sleep -Milliseconds 200
}
$hwnd = $proc.MainWindowHandle

function Set-ClientSize([int]$w, [int]$h) {
  $wr = [OptiGuiDrive.Native]::WindowRect($hwnd)
  $cr = [OptiGuiDrive.Native]::ClientRect($hwnd)
  $bw = ($wr[2] - $wr[0]) - ($cr[2] - $cr[0])
  $bh = ($wr[3] - $wr[1]) - ($cr[3] - $cr[1])
  [void][OptiGuiDrive.Native]::SetWindowPos($hwnd, [IntPtr]::Zero, 50, 50, ($w + $bw), ($h + $bh), $SWP_NOZORDER_SHOW)
}

function Set-Foreground {
  [void][OptiGuiDrive.Native]::SetForegroundWindow($hwnd)
  Start-Sleep -Milliseconds 100
}

function Get-ClientWidth { $cr = [OptiGuiDrive.Native]::ClientRect($hwnd); return ($cr[2] - $cr[0]) }

Set-ClientSize $Width $Height
[void][OptiGuiDrive.Native]::SetForegroundWindow($hwnd)
Start-Sleep -Milliseconds 1500

# ---------- Log helpers ----------
function Get-NewLog {
  if (-not (Test-Path -LiteralPath $script:LogFile)) { return @() }
  $fs = [System.IO.File]::Open($script:LogFile, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
  try {
    $from = $script:LogStart
    if ($fs.Length -lt $from) { $from = 0 }
    [void]$fs.Seek($from, [System.IO.SeekOrigin]::Begin)
    $sr = New-Object System.IO.StreamReader($fs, [System.Text.Encoding]::UTF8)
    $text = $sr.ReadToEnd()
  } finally { $fs.Close() }
  return @($text -split '\r?\n' | Where-Object { $_ -ne '' })
}

function Find-Line([string]$pattern) {
  foreach ($l in (Get-NewLog)) { if ([regex]::IsMatch($l, $pattern)) { return $l } }
  return $null
}

function Get-Geom {
  $rx = '\[geom\] w=(\d+) h=(\d+) pad_x=([\d.]+) pad_y=([\d.]+) tile=([\d.]+) orient=(\d)'
  $m = $null
  foreach ($l in (Get-NewLog)) { $x = [regex]::Match($l, $rx); if ($x.Success) { $m = $x } }
  if ($null -eq $m) { return $null }
  $inv = [Globalization.CultureInfo]::InvariantCulture
  return @{
    w = [double]::Parse($m.Groups[1].Value, $inv)
    h = [double]::Parse($m.Groups[2].Value, $inv)
    px = [double]::Parse($m.Groups[3].Value, $inv)
    py = [double]::Parse($m.Groups[4].Value, $inv)
    tile = [double]::Parse($m.Groups[5].Value, $inv)
    orient = [int]$m.Groups[6].Value
  }
}

# ---------- Coordinate mapping ----------
function Convert-Square([string]$sq, $g) {
  $s = $sq.ToLower()
  if ($s -notmatch '^[a-h][1-8]$') { throw "bad square '$sq'" }
  $c = [int][char]$s.Substring(0, 1) - 97
  $r = [int]$s.Substring(1, 1) - 1
  if ($g.orient -eq 1) { $dc = $c; $dr = 7 - $r } else { $dc = 7 - $c; $dr = $r }
  return @(($g.px + $g.tile * ($dc + 0.5)), ($g.py + $g.tile * ($dr + 0.5)))
}

function Convert-ToScreen([double]$x, [double]$y, $g) {
  $scale = (Get-ClientWidth) / $g.w
  return [OptiGuiDrive.Native]::ToScreen($hwnd, [int][Math]::Round($x * $scale), [int][Math]::Round($y * $scale))
}

function Get-SquareScreen([string]$sq) {
  $g = Get-Geom
  if ($null -eq $g) { throw 'no [geom] line in new log yet' }
  $a = Convert-Square $sq $g
  return Convert-ToScreen $a[0] $a[1] $g
}

function Get-ClickXYScreen([double]$x, [double]$y) {
  $g = Get-Geom
  if ($null -eq $g) { throw 'no [geom] line in new log yet' }
  return Convert-ToScreen $x $y $g
}

# ---------- Input ----------
function Get-VKCode([string]$name) {
  $named = @{ CTRL = 0xA2; SHIFT = 0xA0; ALT = 0xA4; LEFT = 0x25; UP = 0x26; RIGHT = 0x27; DOWN = 0x28;
              DEL = 0x2E; ESC = 0x1B; TAB = 0x09; ENTER = 0x0D; SPACE = 0x20; BACKSPACE = 0x08 }
  if ($named.ContainsKey($name)) { return [int]$named[$name] }
  if ($name -match '^KP([0-9])$') { return (0x60 + [int]$Matches[1]) }
  if ($name -match '^F([1-9]|1[0-2])$') { return (0x6F + [int]$Matches[1]) }
  if ($name -match '^[A-Z]$') { return [int][char]$name }
  if ($name -match '^[0-9]$') { return (0x30 + [int]$name) }
  throw "unknown key '$name'"
}

function Send-Combo([string]$combo, [int]$holdMs) {
  $codes = @($combo.ToUpper().Split('+') | ForEach-Object { Get-VKCode $_ })
  $key = $codes[$codes.Count - 1]
  $mods = @()
  for ($i = 0; $i -lt $codes.Count - 1; $i++) { $mods += $codes[$i] }
  # GLFW maps keys by scan code: send the real one (scan 0 + extended flag
  # is unmapped, scan 0 alone turns arrows into keypad keys)
  foreach ($m in $mods) { [OptiGuiDrive.Native]::keybd_event([byte]$m, [byte][OptiGuiDrive.Native]::MapVirtualKey([uint32]$m, 0), 0, [UIntPtr]::Zero) }
  $ext = $EXTENDED_VKS -contains $key
  $scan = [byte][OptiGuiDrive.Native]::MapVirtualKey([uint32]$key, 0)
  [OptiGuiDrive.Native]::keybd_event([byte]$key, $scan, $(if ($ext) { $KEYEVENTF_EXTENDEDKEY } else { [uint32]0 }), [UIntPtr]::Zero)
  Start-Sleep -Milliseconds $holdMs
  [OptiGuiDrive.Native]::keybd_event([byte]$key, $scan, $(if ($ext) { $KEYEVENTF_EXTENDEDKEY -bor $KEYUP } else { $KEYUP }), [UIntPtr]::Zero)
  for ($i = $mods.Count - 1; $i -ge 0; $i--) { [OptiGuiDrive.Native]::keybd_event([byte]$mods[$i], [byte][OptiGuiDrive.Native]::MapVirtualKey([uint32]$mods[$i], 0), $KEYUP, [UIntPtr]::Zero) }
}

function Invoke-ClickAt([int[]]$p, [uint32]$down, [uint32]$up) {
  [void][OptiGuiDrive.Native]::SetCursorPos($p[0], $p[1])
  Start-Sleep -Milliseconds 80
  [OptiGuiDrive.Native]::mouse_event($down, 0, 0, 0, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 100
  [OptiGuiDrive.Native]::mouse_event($up, 0, 0, 0, [UIntPtr]::Zero)
}

function Invoke-Drag([string]$from, [string]$to, [uint32]$down, [uint32]$up) {
  $p1 = Get-SquareScreen $from
  $p2 = Get-SquareScreen $to
  [void][OptiGuiDrive.Native]::SetCursorPos([int]$p1[0], [int]$p1[1])
  Start-Sleep -Milliseconds 80
  [OptiGuiDrive.Native]::mouse_event($down, 0, 0, 0, [UIntPtr]::Zero)
  for ($i = 1; $i -le 8; $i++) {
    $x = [int][Math]::Round($p1[0] + ($p2[0] - $p1[0]) * $i / 8)
    $y = [int][Math]::Round($p1[1] + ($p2[1] - $p1[1]) * $i / 8)
    [void][OptiGuiDrive.Native]::SetCursorPos($x, $y)
    Start-Sleep -Milliseconds 37
  }
  [OptiGuiDrive.Native]::mouse_event($up, 0, 0, 0, [UIntPtr]::Zero)
}

function Save-Shot([string]$name) {
  Set-Foreground
  Start-Sleep -Milliseconds 200
  $cr = [OptiGuiDrive.Native]::ClientRect($hwnd)
  $cw = $cr[2] - $cr[0]
  $ch = $cr[3] - $cr[1]
  $org = [OptiGuiDrive.Native]::ToScreen($hwnd, 0, 0)
  $bmp = New-Object System.Drawing.Bitmap $cw, $ch
  $gr = [System.Drawing.Graphics]::FromImage($bmp)
  $sz = New-Object System.Drawing.Size $cw, $ch
  $gr.CopyFromScreen([int]$org[0], [int]$org[1], 0, 0, $sz)
  $fname = $name
  if (-not $fname.EndsWith('.png')) { $fname = "$fname.png" }
  $bmp.Save((Join-Path $OutDir $fname), [System.Drawing.Imaging.ImageFormat]::Png)
  $gr.Dispose()
  $bmp.Dispose()
}

function Get-Arg($tok, [int]$i) {
  if ($tok.Count -le $i) { throw "missing argument $i" }
  return $tok[$i]
}

function Short([string]$s) {
  if ($s.Length -gt 160) { return $s.Substring(0, 160) + '...' }
  return $s
}

# ---------- Step dispatcher ----------
function Invoke-Step([string]$raw) {
  $t = $raw.Trim()
  $tok = @($t -split '\s+')
  $cmd = $tok[0].ToLower()
  $rest = $t.Substring($tok[0].Length).Trim()
  switch ($cmd) {
    'wait' { Start-Sleep -Milliseconds ([int](Get-Arg $tok 1)); return 'ok' }
    'key' { Set-Foreground; Send-Combo (Get-Arg $tok 1) $KeyHoldMs; return 'ok' }
    'hold' { Set-Foreground; Send-Combo (Get-Arg $tok 1) ([int](Get-Arg $tok 2)); return 'ok' }
    'click' { Set-Foreground; Invoke-ClickAt ([int[]](Get-SquareScreen (Get-Arg $tok 1))) $MOUSE_LDOWN $MOUSE_LUP; return 'ok' }
    'rclick' { Set-Foreground; Invoke-ClickAt ([int[]](Get-SquareScreen (Get-Arg $tok 1))) $MOUSE_RDOWN $MOUSE_RUP; return 'ok' }
    'clickxy' {
      Set-Foreground
      $p = Get-ClickXYScreen ([double](Get-Arg $tok 1)) ([double](Get-Arg $tok 2))
      Invoke-ClickAt ([int[]]$p) $MOUSE_LDOWN $MOUSE_LUP
      return 'ok'
    }
    'drag' { Set-Foreground; Invoke-Drag (Get-Arg $tok 1) (Get-Arg $tok 2) $MOUSE_LDOWN $MOUSE_LUP; return 'ok' }
    'rdrag' { Set-Foreground; Invoke-Drag (Get-Arg $tok 1) (Get-Arg $tok 2) $MOUSE_RDOWN $MOUSE_RUP; return 'ok' }
    'resize' {
      Set-ClientSize ([int](Get-Arg $tok 1)) ([int](Get-Arg $tok 2))
      Start-Sleep -Milliseconds 800
      return 'ok'
    }
    'minimize' { [void][OptiGuiDrive.Native]::ShowWindow($hwnd, 6); Start-Sleep -Milliseconds 800; return 'ok' }
    'restore' { [void][OptiGuiDrive.Native]::ShowWindow($hwnd, 9); Start-Sleep -Milliseconds 800; return 'ok' }
    'shot' { Save-Shot (Get-Arg $tok 1); return 'ok' }
    'clip' {
      $m = [regex]::Match($raw, '^\s*clip(?: (.*))?$')
      Set-Clipboard -Value $m.Groups[1].Value
      return 'ok'
    }
    'expect_fen' {
      $want = @($rest -split '\s+' | Where-Object { $_ -ne '' } | Select-Object -First 4) -join ' '
      $found = $null
      foreach ($l in (Get-NewLog)) { $x = [regex]::Match($l, '\[pos\] (\S+) fen=(.*)$'); if ($x.Success) { $found = $x } }
      if ($null -eq $found) { return 'FAIL no [pos] line in new log' }
      $got = @($found.Groups[2].Value.Trim() -split '\s+' | Select-Object -First 4) -join ' '
      if ($got -ceq $want) { return "PASS fen=$got (tag $($found.Groups[1].Value))" }
      return "FAIL expected '$want' got '$got' (tag $($found.Groups[1].Value))"
    }
    'expect_log' {
      if ($rest -eq '') { throw 'empty regex' }
      $hit = Find-Line $rest
      if ($null -ne $hit) { return ('PASS matched: ' + (Short $hit)) }
      return "FAIL no new log line matches /$rest/"
    }
    'expect_nolog' {
      if ($rest -eq '') { throw 'empty regex' }
      $hit = Find-Line $rest
      if ($null -eq $hit) { return "PASS no new log line matches /$rest/" }
      return ('FAIL matched: ' + (Short $hit))
    }
    'alive' {
      $proc.Refresh()
      if ($proc.HasExited) { return "FAIL process exited (code $($proc.ExitCode))" }
      return 'PASS alive'
    }
    default { return "FAIL unknown step '$cmd'" }
  }
}

# ---------- Main loop ----------
$pass = 0
$fail = 0
$n = 0
foreach ($raw in $stepList) {
  $n++
  $t = $raw.Trim()
  $proc.Refresh()
  if ($proc.HasExited) {
    $fail++
    Write-Host ("[{0}] {1} -> FAIL process died (exit code {2})" -f $n, $t, $proc.ExitCode)
    break
  }
  try {
    $result = Invoke-Step $raw
  } catch {
    $result = 'FAIL ' + $_.Exception.Message
  }
  if ($result.StartsWith('PASS')) { $pass++ }
  if ($result.StartsWith('FAIL')) { $fail++ }
  Write-Host ("[{0}] {1} -> {2}" -f $n, $t, $result)
  Start-Sleep -Milliseconds $StepDelayMs
}

# ---------- End ----------
Write-Host ("PASS={0} FAIL={1}" -f $pass, $fail)
$slice = @(Get-NewLog)
Set-Content -LiteralPath (Join-Path $OutDir 'debug_slice.log') -Value $slice -Encoding UTF8

if ($KeepOpen) {
  Write-Host "left open, pid $($proc.Id)"
} else {
  $proc.Refresh()
  if (-not $proc.HasExited) {
    [void]$proc.CloseMainWindow()
    if (-not $proc.WaitForExit(3000)) { Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue }
  }
}

if ($fail -gt 0) { exit 1 } else { exit 0 }
