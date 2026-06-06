$env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS='--remote-debugging-port=9222 --remote-allow-origins=*'
$appPath = 'D:\desarrollos\ABDSynths\ABDJUNiO601\build_j106\ABDSimpleJuno106_artefacts\Release\Standalone\ABD JUNiO 601.exe'
$proc = Start-Process -FilePath $appPath -PassThru -WindowStyle Hidden
Write-Host ('PID: ' + $proc.Id)
Start-Sleep -Seconds 15
Write-Host 'WAIT_DONE'
