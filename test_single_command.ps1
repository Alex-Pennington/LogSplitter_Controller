# Test a single OTA command
param(
    [string]$TargetHost = "192.168.1.225",
    [int]$Port = 23,
    [string]$Command = "ota status"
)

function Send-SingleCommand {
    param(
        [string]$TargetHost,
        [int]$Port, 
        [string]$Command
    )
    
    try {
        Write-Host "Testing command: $Command" -ForegroundColor Cyan
        Write-Host "Connecting to $TargetHost`:$Port..." -ForegroundColor Yellow
        
        # Create TCP client with longer timeouts
        $tcpClient = New-Object System.Net.Sockets.TcpClient
        $tcpClient.ReceiveTimeout = 8000
        $tcpClient.SendTimeout = 5000
        
        # Connect to device
        $tcpClient.Connect($TargetHost, $Port)
        
        if ($tcpClient.Connected) {
            Write-Host "Connected!" -ForegroundColor Green
            
            # Get network stream
            $stream = $tcpClient.GetStream()
            $encoding = [System.Text.Encoding]::ASCII
            
            # Wait for connection to establish
            Start-Sleep -Milliseconds 500
            
            # Send command with newline
            Write-Host "Sending: '$Command'" -ForegroundColor Yellow
            $commandBytes = $encoding.GetBytes($Command + "`r`n")
            $stream.Write($commandBytes, 0, $commandBytes.Length)
            $stream.Flush()
            
            # Wait for response
            $response = ""
            $attempts = 0
            $maxAttempts = 25
            
            while ($attempts -lt $maxAttempts) {
                if ($stream.DataAvailable) {
                    $buffer = New-Object byte[] 1024
                    $bytesRead = $stream.Read($buffer, 0, 1024)
                    if ($bytesRead -gt 0) {
                        $chunk = $encoding.GetString($buffer, 0, $bytesRead)
                        $response += $chunk
                    }
                }
                Start-Sleep -Milliseconds 100
                $attempts++
            }
            
            if ($response.Trim()) {
                Write-Host "Response:" -ForegroundColor Green
                Write-Host "--------" -ForegroundColor Green
                Write-Host $response -ForegroundColor White
                Write-Host "--------" -ForegroundColor Green
            } else {
                Write-Host "No response received" -ForegroundColor Yellow
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
Write-Host "Single Command Test" -ForegroundColor Green
Write-Host "==================" -ForegroundColor Green

Send-SingleCommand -TargetHost $TargetHost -Port $Port -Command $Command