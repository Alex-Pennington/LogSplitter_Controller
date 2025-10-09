# PowerShell Telnet Client for Testing LogMonitor OTA Commands
# Based on the existing Python telnet_test.py script
# Usage: .\test_ota_telnet.ps1

param(
    [string]$Host = "192.168.1.155",
    [int]$Port = 23,
    [string]$Command = ""
)

function Send-TelnetCommand {
    param(
        [string]$TargetHost,
        [int]$TargetPort, 
        [string]$Command
    )
    
    try {
        Write-Host "Connecting to ${TargetHost}:${TargetPort}..." -ForegroundColor Cyan
        
        # Create TCP client
        $tcpClient = New-Object System.Net.Sockets.TcpClient
        $tcpClient.ReceiveTimeout = 5000
        $tcpClient.SendTimeout = 5000
        
        # Connect to device
        $tcpClient.Connect($TargetHost, $TargetPort)
        
        if ($tcpClient.Connected) {
            Write-Host "✅ Connected successfully!" -ForegroundColor Green
            
            # Get network stream
            $stream = $tcpClient.GetStream()
            $writer = New-Object System.IO.StreamWriter($stream)
            $reader = New-Object System.IO.StreamReader($stream)
            
            # Wait for connection to establish
            Start-Sleep -Milliseconds 100
            
            # Send command
            Write-Host "📤 Sending command: $Command" -ForegroundColor Yellow
            $writer.WriteLine($Command)
            $writer.Flush()
            
            # Wait for response
            Start-Sleep -Milliseconds 1000  
            
            # Read response
            $response = ""
            $timeout = 0
            while ($stream.DataAvailable -or $timeout -lt 20) {
                if ($stream.DataAvailable) {
                    $response += $reader.ReadLine() + "`n"
                    $timeout = 0
                } else {
                    Start-Sleep -Milliseconds 100
                    $timeout++
                }
            }
            
            if ($response.Trim()) {
                Write-Host "📥 Response:" -ForegroundColor Green
                Write-Host $response.Trim() -ForegroundColor White
            } else {
                Write-Host "⚠️  No response received" -ForegroundColor Yellow
            }
            
            # Clean up
            $reader.Close()
            $writer.Close()
            $stream.Close()
            $tcpClient.Close()
            
            return $true
        } else {
            Write-Host "❌ Failed to connect" -ForegroundColor Red
            return $false
        }
        
    } catch {
        Write-Host "❌ Error: $($_.Exception.Message)" -ForegroundColor Red
        return $false
    }
}

function Test-OTACommands {
    param([string]$TargetHost)
    
    Write-Host "`n🧪 Testing OTA Commands on $TargetHost" -ForegroundColor Cyan
    Write-Host "=" * 50 -ForegroundColor Cyan
    
    $commands = @(
        @{cmd="help"; desc="Show help (verify OTA commands listed)"},
        @{cmd="show"; desc="Show system status"},
        @{cmd="ota status"; desc="Check OTA server status"},
        @{cmd="ota start"; desc="Start OTA server"},
        @{cmd="ota url"; desc="Get OTA update URLs"},
        @{cmd="ota status"; desc="Check OTA status after start"}
    )
    
    foreach ($test in $commands) {
        Write-Host "`n🔍 Test: $($test.desc)" -ForegroundColor Magenta
        Write-Host "Command: $($test.cmd)" -ForegroundColor Gray
        Write-Host "-" * 40 -ForegroundColor Gray
        
        $success = Send-TelnetCommand -TargetHost $TargetHost -TargetPort 23 -Command $test.cmd
        
        if (-not $success) {
            Write-Host "⚠️  Command failed, stopping tests" -ForegroundColor Red
            break
        }
        
        Start-Sleep -Milliseconds 500
    }
}

function Test-WebInterface {
    param([string]$TargetHost)
    
    Write-Host "`n🌐 Testing OTA Web Interface" -ForegroundColor Cyan
    Write-Host "=" * 50 -ForegroundColor Cyan
    
    $otaUrl = "http://$TargetHost/update"
    $progressUrl = "http://$TargetHost/progress"
    
    try {
        Write-Host "🔍 Testing main OTA page: $otaUrl" -ForegroundColor Yellow
        $response = Invoke-WebRequest -Uri $otaUrl -TimeoutSec 5 -UseBasicParsing
        
        if ($response.StatusCode -eq 200) {
            Write-Host "✅ OTA upload page accessible (Status: $($response.StatusCode))" -ForegroundColor Green
            
            # Check for key elements
            if ($response.Content -match "LogMonitor.*Update") {
                Write-Host "✅ OTA page contains expected title" -ForegroundColor Green
            }
            if ($response.Content -match "firmware.*file") {
                Write-Host "✅ OTA page contains file upload form" -ForegroundColor Green  
            }
        } else {
            Write-Host "⚠️  OTA page returned status: $($response.StatusCode)" -ForegroundColor Yellow
        }
        
        Write-Host "`n🔍 Testing progress API: $progressUrl" -ForegroundColor Yellow
        $progressResponse = Invoke-WebRequest -Uri $progressUrl -TimeoutSec 5 -UseBasicParsing
        
        if ($progressResponse.StatusCode -eq 200) {
            Write-Host "✅ Progress API accessible (Status: $($progressResponse.StatusCode))" -ForegroundColor Green
            Write-Host "📊 Progress data: $($progressResponse.Content)" -ForegroundColor Cyan
        }
        
    } catch {
        Write-Host "❌ Web interface test failed: $($_.Exception.Message)" -ForegroundColor Red
        Write-Host "💡 Make sure OTA server is started with 'ota start' command" -ForegroundColor Yellow
    }
}

# Main execution
Write-Host "🚀 LogMonitor OTA System Test Script" -ForegroundColor Green
Write-Host "====================================" -ForegroundColor Green

if ($Command) {
    # Single command mode
    Write-Host "`n📡 Single Command Mode" -ForegroundColor Cyan
    Send-TelnetCommand -TargetHost $Host -TargetPort $Port -Command $Command
} else {
    # Full test suite mode
    Write-Host "`n🧪 Full Test Suite Mode" -ForegroundColor Cyan
    Write-Host "Target Device: $Host" -ForegroundColor White
    
    # Test connectivity first
    Write-Host "`n🔌 Testing connectivity..." -ForegroundColor Yellow
    if (Test-NetConnection -ComputerName $Host -Port $Port -InformationLevel Quiet) {
        Write-Host "✅ Device is reachable on port $Port" -ForegroundColor Green
        
        # Run telnet command tests
        Test-OTACommands -TargetHost $Host
        
        # Test web interface (only if OTA might be started)
        Write-Host "`n⏳ Waiting 2 seconds for OTA server to initialize..." -ForegroundColor Yellow
        Start-Sleep -Seconds 2
        Test-WebInterface -TargetHost $Host
        
    } else {
        Write-Host "❌ Device not reachable at $Host:$Port" -ForegroundColor Red
        Write-Host "💡 Check device power, WiFi connection, and IP address" -ForegroundColor Yellow
    }
}

Write-Host "`n🏁 Test completed!" -ForegroundColor Green