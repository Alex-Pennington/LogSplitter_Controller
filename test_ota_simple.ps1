# PowerShell Telnet Client for Testing LogMonitor OTA Commands
# Usage: .\test_ota_simple.ps1

param(
    [string]$TargetHost = "192.168.1.155",
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
        
        # Create TCP client
        $tcpClient = New-Object System.Net.Sockets.TcpClient
        $tcpClient.ReceiveTimeout = 3000
        $tcpClient.SendTimeout = 3000
        
        # Connect to device
        $tcpClient.Connect($TargetHost, $Port)
        
        if ($tcpClient.Connected) {
            Write-Host "Connected!" -ForegroundColor Green
            
            # Get network stream
            $stream = $tcpClient.GetStream()
            $writer = New-Object System.IO.StreamWriter($stream)
            $reader = New-Object System.IO.StreamReader($stream)
            
            # Wait for connection to establish
            Start-Sleep -Milliseconds 200
            
            # Send command
            Write-Host "Sending: $Command" -ForegroundColor Yellow
            $writer.WriteLine($Command)
            $writer.Flush()
            
            # Wait for response
            Start-Sleep -Milliseconds 1500
            
            # Read response
            $response = ""
            $attempts = 0
            while ($attempts -lt 10) {
                if ($stream.DataAvailable) {
                    $line = $reader.ReadLine()
                    if ($line) {
                        $response += $line + "`n"
                    }
                }
                Start-Sleep -Milliseconds 100
                $attempts++
            }
            
            if ($response.Trim()) {
                Write-Host "Response:" -ForegroundColor Green
                Write-Host $response.Trim() -ForegroundColor White
            } else {
                Write-Host "No response received" -ForegroundColor Yellow
            }
            
            # Clean up
            $reader.Close()
            $writer.Close()
            $stream.Close()
            $tcpClient.Close()
            
            return $true
        }
        
    } catch {
        Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
        return $false
    }
}

# Main execution
Write-Host "LogMonitor OTA Test Script" -ForegroundColor Green
Write-Host "=========================" -ForegroundColor Green

# Test connectivity first
Write-Host "`nTesting connectivity to $TargetHost`:$Port..." -ForegroundColor Yellow
if (Test-NetConnection -ComputerName $TargetHost -Port $Port -InformationLevel Quiet) {
    Write-Host "Device is reachable!" -ForegroundColor Green
    
    # Test basic commands
    $commands = @("help", "show", "ota status", "ota start", "ota url")
    
    foreach ($cmd in $commands) {
        Write-Host "`n--- Testing command: $cmd ---" -ForegroundColor Magenta
        $success = Send-TelnetCommand -TargetHost $TargetHost -Port $Port -Command $cmd
        if (-not $success) {
            Write-Host "Command failed, stopping tests" -ForegroundColor Red
            break
        }
        Start-Sleep -Seconds 1
    }
    
    # Test web interface
    Write-Host "`n--- Testing OTA Web Interface ---" -ForegroundColor Magenta
    $otaUrl = "http://$TargetHost/update"
    
    try {
        Write-Host "Testing: $otaUrl" -ForegroundColor Yellow
        $response = Invoke-WebRequest -Uri $otaUrl -TimeoutSec 5 -UseBasicParsing
        Write-Host "Web interface accessible! Status: $($response.StatusCode)" -ForegroundColor Green
    } catch {
        Write-Host "Web interface not accessible: $($_.Exception.Message)" -ForegroundColor Red
        Write-Host "Make sure OTA server is started with 'ota start' command" -ForegroundColor Yellow
    }
    
} else {
    Write-Host "Device not reachable at $TargetHost`:$Port" -ForegroundColor Red
    Write-Host "Check device power, WiFi connection, and IP address" -ForegroundColor Yellow
}

Write-Host "`nTest completed!" -ForegroundColor Green