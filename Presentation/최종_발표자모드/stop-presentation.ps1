[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$DisplaySwitchPath = Join-Path $env:SystemRoot 'System32\DisplaySwitch.exe'
$ServerStatePath = Join-Path ([System.IO.Path]::GetTempPath()) 'AIRE_Presentation_Server.pid'

if ($env:AIRE_PRESENTATION_VALIDATE_ONLY -eq '1')
{
    if (-not (Test-Path -LiteralPath $DisplaySwitchPath))
    {
        throw "Windows 화면 전환 도구를 찾지 못했습니다: $DisplaySwitchPath"
    }
    Write-Output 'AIRE_PRESENTATION_STOP_VALIDATION_OK'
    return
}

if (Test-Path -LiteralPath $ServerStatePath)
{
    $ServerProcessIdText = (Get-Content -LiteralPath $ServerStatePath -Raw).Trim()
    $ServerProcessId = 0
    if ([int]::TryParse($ServerProcessIdText, [ref]$ServerProcessId))
    {
        $ServerProcess = Get-CimInstance -ClassName Win32_Process -Filter "ProcessId = $ServerProcessId" -ErrorAction SilentlyContinue
        $IsExpectedNodeProcess = $ServerProcess `
            -and $ServerProcess.Name -eq 'node.exe' `
            -and $ServerProcess.CommandLine -match 'server\.mjs'
        if ($IsExpectedNodeProcess)
        {
            Stop-Process -Id $ServerProcessId -Force -ErrorAction SilentlyContinue
            Write-Host '발표 서버를 종료했습니다.' -ForegroundColor Green
        }
    }
    Remove-Item -LiteralPath $ServerStatePath -Force -ErrorAction SilentlyContinue
}

if (-not (Test-Path -LiteralPath $DisplaySwitchPath))
{
    throw "Windows 화면 전환 도구를 찾지 못했습니다: $DisplaySwitchPath"
}

Write-Host 'Windows 화면을 복제 모드로 복구합니다.' -ForegroundColor Cyan
Start-Process -FilePath $DisplaySwitchPath -ArgumentList '/clone' -Wait -WindowStyle Hidden
Write-Host '복구가 완료되었습니다.' -ForegroundColor Green
