# Windows test runner for filc-cosmopolitan APE binaries.
# Same output markers as run_tests.sh so summarize.sh can parse it.
# Results are written to C:\filc\results.txt and copied to COM1 (QEMU serial
# file on the host), then the VM shuts down.
param([string]$Src = "D:")
$ErrorActionPreference = "Continue"
$work = "C:\filc"
New-Item -ItemType Directory -Force -Path $work | Out-Null
Copy-Item "$Src\*.com" $work -Force
Copy-Item "$Src\*.ape" $work -Force
Set-Location $work
$out = "$work\results.txt"
$ver = (Get-CimInstance Win32_OperatingSystem)
"### OS: Windows $($ver.Caption) $($ver.Version)" | Out-File $out -Encoding ascii

function Run-Test([string]$name, [string]$exe, [string]$argline) {
  "=== BEGIN $name" | Out-File $out -Append -Encoding ascii
  $so = "$work\so.txt"; $se = "$work\se.txt"
  Remove-Item $so, $se -ErrorAction SilentlyContinue
  try {
    $sp = @{ FilePath = $exe; RedirectStandardOutput = $so; RedirectStandardError = $se;
             NoNewWindow = $true; PassThru = $true }
    if ($argline) { $sp.ArgumentList = $argline }
    $p = Start-Process @sp
    if (-not $p.WaitForExit(60000)) {
      $p.Kill(); "TIMEOUT after 60s" | Out-File $out -Append -Encoding ascii
    }
    $p.WaitForExit()
    $rc = $p.ExitCode
  } catch {
    "LAUNCH ERROR: $($_.Exception.Message)" | Out-File $out -Append -Encoding ascii
    $rc = -1
  }
  foreach ($f in $so, $se) { if (Test-Path $f) { Get-Content $f | Out-File $out -Append -Encoding ascii } }
  "=== END $name rc=$rc" | Out-File $out -Append -Encoding ascii
}

# APEs are PE files; Windows runs them by content, but give them .exe names
# so nothing in the shell second-guesses them.
Get-ChildItem *.com, *.ape | ForEach-Object { Copy-Item $_.FullName ($_.BaseName + ".exe") -Force }

# Control: upstream Cosmopolitan APE with no Fil-C.
"=== BEGIN control" | Out-File $out -Append -Encoding ascii
Remove-Item "$work\ctl_touched" -ErrorAction SilentlyContinue
$p = Start-Process -FilePath "$work\ctl_touch.exe" -ArgumentList "ctl_touched" -NoNewWindow -PassThru -Wait
if (Test-Path "$work\ctl_touched") { "CONTROL_OK" | Out-File $out -Append -Encoding ascii }
"=== END control rc=$($p.ExitCode)" | Out-File $out -Append -Encoding ascii

Run-Test "smoke"        "$work\ape_portable_smoke.exe"   ""
Run-Test "stress1"      "$work\ape_stress_test.exe"      "1"
Run-Test "stress2"      "$work\ape_stress_test.exe"      "2"
Run-Test "stress3"      "$work\ape_stress_test.exe"      "3"
Run-Test "stress4"      "$work\ape_stress_test.exe"      "4"
Run-Test "malloc_align" "$work\ape_malloc_align.exe"     ""
Run-Test "ifconf_ok"    "$work\ape_ioctl_ifconf_ok.exe"  ""
Run-Test "ifconf_bad"   "$work\ape_ioctl_nested_ptr.exe" ""
"### DONE" | Out-File $out -Append -Encoding ascii

# Ship results to the host over the serial port, framed so the host can
# extract them cleanly from the boot noise.
$port = New-Object System.IO.Ports.SerialPort COM1, 115200
try {
  $port.Open()
  $port.WriteLine("@@@FILC_RESULTS_BEGIN@@@")
  Get-Content $out | ForEach-Object { $port.WriteLine($_) }
  $port.WriteLine("@@@FILC_RESULTS_END@@@")
  Start-Sleep -Seconds 2
  $port.Close()
} catch {
  cmd /c "type $out > COM1"
}
Stop-Computer -Force
