[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$PresentationRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$DisplaySwitchPath = Join-Path $env:SystemRoot 'System32\DisplaySwitch.exe'
$ServerStatePath = Join-Path ([System.IO.Path]::GetTempPath()) 'AIRE_Presentation_Server.pid'
$ServerPort = 4173
$ServerProcess = $null
$DisplayWasExtended = $false

function Get-PresentationBrowserPath
{
    $Candidates = @(
        (Join-Path ${env:ProgramFiles(x86)} 'Microsoft\Edge\Application\msedge.exe'),
        (Join-Path $env:ProgramFiles 'Microsoft\Edge\Application\msedge.exe'),
        (Join-Path $env:ProgramFiles 'Google\Chrome\Application\chrome.exe'),
        (Join-Path ${env:ProgramFiles(x86)} 'Google\Chrome\Application\chrome.exe'),
        (Join-Path $env:LOCALAPPDATA 'Google\Chrome\Application\chrome.exe')
    )

    return $Candidates | Where-Object { $_ -and (Test-Path -LiteralPath $_) } | Select-Object -First 1
}

function Assert-ServerPortAvailable
{
    $Listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, $ServerPort)
    try
    {
        $Listener.Start()
    }
    catch
    {
        throw "포트 ${ServerPort}가 이미 사용 중입니다. 기존 발표 서버를 종료한 뒤 다시 실행하세요."
    }
    finally
    {
        $Listener.Stop()
    }
}

function Wait-ForPresentationServer
{
    $HealthUrl = "http://127.0.0.1:$ServerPort/"
    for ($Attempt = 0; $Attempt -lt 30; ++$Attempt)
    {
        if ($ServerProcess.HasExited)
        {
            throw '발표 서버가 시작 직후 종료되었습니다.'
        }

        try
        {
            $Response = Invoke-WebRequest -Uri $HealthUrl -Method Head -TimeoutSec 1 -UseBasicParsing
            if ($Response.StatusCode -eq 200)
            {
                return
            }
        }
        catch
        {
            Start-Sleep -Milliseconds 200
        }
    }

    throw '발표 서버가 제한 시간 안에 응답하지 않았습니다.'
}

$NodeCommand = Get-Command 'node.exe' -ErrorAction SilentlyContinue
if (-not $NodeCommand)
{
    throw 'Node.js를 찾지 못했습니다. Node.js 설치 또는 PATH 설정을 확인하세요.'
}
if (-not (Test-Path -LiteralPath $DisplaySwitchPath))
{
    throw "Windows 화면 전환 도구를 찾지 못했습니다: $DisplaySwitchPath"
}

$BrowserPath = Get-PresentationBrowserPath
if (-not $BrowserPath)
{
    throw 'Microsoft Edge 또는 Google Chrome을 찾지 못했습니다.'
}

Assert-ServerPortAvailable
if ($env:AIRE_PRESENTATION_VALIDATE_ONLY -eq '1')
{
    Write-Output 'AIRE_PRESENTATION_START_VALIDATION_OK'
    return
}

$SessionId = [guid]::NewGuid().ToString('N')
$PresenterUrl = "http://127.0.0.1:$ServerPort/?presenter=1&session=$SessionId"

try
{
    Write-Host 'Windows 화면을 발표자용 확장 모드로 전환합니다.' -ForegroundColor Cyan
    Start-Process -FilePath $DisplaySwitchPath -ArgumentList '/extend' -Wait -WindowStyle Hidden
    $DisplayWasExtended = $true
    Start-Sleep -Seconds 2

    $ServerProcess = Start-Process `
        -FilePath $NodeCommand.Source `
        -ArgumentList 'server.mjs' `
        -WorkingDirectory $PresentationRoot `
        -PassThru `
        -WindowStyle Hidden
    Set-Content -LiteralPath $ServerStatePath -Value $ServerProcess.Id -Encoding ascii
    Wait-ForPresentationServer

    Start-Process -FilePath $BrowserPath -ArgumentList @('--new-window', $PresenterUrl)

    Write-Host ''
    Write-Host 'AI : RE 발표자 모드를 시작했습니다.' -ForegroundColor Green
    Write-Host '청중 화면 열기 버튼을 누르고 두 번째 모니터에서 전체화면을 시작하세요.'
    Write-Host '이 창을 닫지 말고, 발표가 끝나면 아래에서 Enter를 누르세요.'
    Read-Host '발표 종료 및 복제 모드 복구'
}
finally
{
    if ($ServerProcess -and -not $ServerProcess.HasExited)
    {
        Stop-Process -Id $ServerProcess.Id -Force -ErrorAction SilentlyContinue
        $ServerProcess.WaitForExit(3000)
    }
    Remove-Item -LiteralPath $ServerStatePath -Force -ErrorAction SilentlyContinue

    if ($DisplayWasExtended)
    {
        Write-Host 'Windows 화면을 복제 모드로 복구합니다.' -ForegroundColor Cyan
        Start-Process -FilePath $DisplaySwitchPath -ArgumentList '/clone' -Wait -WindowStyle Hidden
    }
}
