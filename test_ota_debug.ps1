# Debug version of OTA test script with better response handling
param(
    [string]$TargetHost = "192.168.1.225",
    [int]$Port = 23
)

function Send-TelnetCommand {
    param(
        [string]$TargetHost,
        [int]$Port, 
        [string]$Command
    )
    
    try {
        Write-Host "Connecting to $TargetHost`:$Port..." -ForegroundColor Cyan
        
        # Create TCP client with longer timeouts
        $tcpClient = New-Object System.Net.Sockets.TcpClient
        $tcpClient.ReceiveTimeout = 10000  # 10 seconds
        $tcpClient.SendTimeout = 5000      # 5 seconds
        
        # Connect to device
        $tcpClient.Connect($TargetHost, $Port)
        
        if ($tcpClient.Connected) {
            Write-Host "Connected!" -ForegroundColor Green
            
            # Get network stream
            $stream = $tcpClient.GetStream()
            $encoding = [System.Text.Encoding]::ASCII
            
            # Wait longer for connection to establish
            Start-Sleep -Milliseconds 500
            
            # Check for any initial data
            if ($stream.DataAvailable) {
                $buffer = New-Object byte[] 1024
                $bytesRead = $stream.Read($buffer, 0, 1024)
                $initialData = $encoding.GetString($buffer, 0, $bytesRead)
                Write-Host "Initial data received: $initialData" -ForegroundColor Gray
            }
            
            # Send command with newline
            Write-Host "Sending: '$Command'" -ForegroundColor Yellow
            $commandBytes = $encoding.GetBytes($Command + "`r`n")
            $stream.Write($commandBytes, 0, $commandBytes.Length)
            $stream.Flush()
            
            # Wait for response with multiple attempts
            Write-Host "Waiting for response..." -ForegroundColor Gray
            $response = ""
            $attempts = 0
            $maxAttempts = 30  # 3 seconds total
            
            while ($attempts -lt $maxAttempts) {
                if ($stream.DataAvailable) {
                    $buffer = New-Object byte[] 1024
                    $bytesRead = $stream.Read($buffer, 0, 1024)
                    if ($bytesRead -gt 0) {
                        $chunk = $encoding.GetString($buffer, 0, $bytesRead)
                        $response += $chunk
                        Write-Host "Received chunk: '$chunk'" -ForegroundColor Gray
                    }
                }
                Start-Sleep -Milliseconds 100
                $attempts++
            }
            
            if ($response.Trim()) {
                Write-Host "Full Response:" -ForegroundColor Green
                Write-Host "=============" -ForegroundColor Green
                Write-Host $response -ForegroundColor White
                Write-Host "=============" -ForegroundColor Green
            } else {
                Write-Host "No response received after $maxAttempts attempts" -ForegroundColor Yellow
            }
            
            # Clean up
            $stream.Close()
            $tcpClient.Close()
            
            return $true
        }
        
    } catch {
        Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
        if ($tcpClient) { $tcpClient.Close() }
        return $false
    }
}

# Main execution
Write-Host "LogMonitor OTA Debug Test" -ForegroundColor Green
Write-Host "========================" -ForegroundColor Green

# Test connectivity first  
Write-Host "`nTesting connectivity to $TargetHost`:$Port..." -ForegroundColor Yellow
if (Test-NetConnection -ComputerName $TargetHost -Port $Port -InformationLevel Quiet) {
    Write-Host "Device is reachable!" -ForegroundColor Green
    
    # Test one command at a time with detailed debugging
    $commands = @("help", "show", "ota status")
    
    foreach ($cmd in $commands) {
        Write-Host "`n" + "="*50 -ForegroundColor Magenta
        Write-Host "Testing command: $cmd" -ForegroundColor Magenta
        Write-Host "="*50 -ForegroundColor Magenta
        
        $success = Send-TelnetCommand -TargetHost $TargetHost -Port $Port -Command $cmd
        if (-not $success) {
            Write-Host "Command failed, stopping tests" -ForegroundColor Red
            break
        }
        
        Write-Host "Waiting 2 seconds before next command..." -ForegroundColor Gray
        Start-Sleep -Seconds 2
    }
    
} else {
    Write-Host "Device not reachable at $TargetHost`:$Port" -ForegroundColor Red
}

Write-Host "`nDebug test completed!" -ForegroundColor Green