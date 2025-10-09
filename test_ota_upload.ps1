# OTA Firmware Upload Test Script
param(
    [string]$TargetHost = "192.168.1.225",
    [int]$Port = 80,
    [string]$FirmwarePath = "monitor\.pio\build\uno_r4_wifi\firmware.bin"
)

function Test-OTAUpload {
    param(
        [string]$Host,
        [string]$FilePath
    )
    
    try {
        Write-Host "🚀 OTA Upload Test" -ForegroundColor Green
        Write-Host "==================" -ForegroundColor Green
        
        # Check if firmware file exists
        if (-not (Test-Path $FilePath)) {
            Write-Host "❌ Firmware file not found: $FilePath" -ForegroundColor Red
            return $false
        }
        
        $fileInfo = Get-Item $FilePath
        Write-Host "📁 Firmware file: $($fileInfo.Name)" -ForegroundColor Cyan
        Write-Host "📊 File size: $($fileInfo.Length) bytes" -ForegroundColor Cyan
        Write-Host "🎯 Target: $Host" -ForegroundColor Cyan
        
        Write-Host "`n🔍 Testing OTA server accessibility..." -ForegroundColor Yellow
        
        # First test if the server is accessible
        $testResponse = Invoke-WebRequest -Uri "http://$Host/update" -Method GET -TimeoutSec 10 -UseBasicParsing
        
        if ($testResponse.StatusCode -ne 200) {
            Write-Host "❌ OTA server not accessible (Status: $($testResponse.StatusCode))" -ForegroundColor Red
            return $false
        }
        
        Write-Host "✅ OTA server accessible (Status: $($testResponse.StatusCode))" -ForegroundColor Green
        Write-Host "📄 Response length: $($testResponse.Content.Length) bytes" -ForegroundColor Gray
        
        Write-Host "`n📤 Starting firmware upload..." -ForegroundColor Yellow
        
        # Prepare multipart form data
        $boundary = [System.Guid]::NewGuid().ToString()
        $LF = "`r`n"
        
        # Read firmware file as bytes
        $firmwareBytes = [System.IO.File]::ReadAllBytes($FilePath)
        
        # Create multipart form data
        $bodyLines = @()
        $bodyLines += "--$boundary"
        $bodyLines += "Content-Disposition: form-data; name=`"firmware`"; filename=`"firmware.bin`""
        $bodyLines += "Content-Type: application/octet-stream"
        $bodyLines += ""
        
        # Convert text parts to bytes
        $textPart = ($bodyLines -join $LF) + $LF
        $textBytes = [System.Text.Encoding]::UTF8.GetBytes($textPart)
        
        # Create closing boundary
        $closingBoundary = $LF + "--$boundary--" + $LF
        $closingBytes = [System.Text.Encoding]::UTF8.GetBytes($closingBoundary)
        
        # Combine all parts
        $bodyBytes = $textBytes + $firmwareBytes + $closingBytes
        
        Write-Host "📦 Total upload size: $($bodyBytes.Length) bytes" -ForegroundColor Cyan
        
        # Create HTTP request
        $uri = "http://$Host/update"
        $headers = @{
            "Content-Type" = "multipart/form-data; boundary=$boundary"
        }
        
        Write-Host "🌐 Uploading to: $uri" -ForegroundColor Yellow
        Write-Host "⏳ This may take a moment..." -ForegroundColor Yellow
        
        # Upload firmware
        $uploadResponse = Invoke-RestMethod -Uri $uri -Method POST -Body $bodyBytes -Headers $headers -TimeoutSec 60
        
        Write-Host "✅ Upload completed!" -ForegroundColor Green
        Write-Host "📋 Server response:" -ForegroundColor Cyan
        Write-Host $uploadResponse -ForegroundColor White
        
        return $true
        
    } catch {
        Write-Host "❌ Upload failed: $($_.Exception.Message)" -ForegroundColor Red
        
        if ($_.Exception.Response) {
            Write-Host "📊 HTTP Status: $($_.Exception.Response.StatusCode)" -ForegroundColor Red
            Write-Host "📄 Response: $($_.Exception.Response.StatusDescription)" -ForegroundColor Red
        }
        
        return $false
    }
}

function Test-OTAProgress {
    param([string]$Host)
    
    try {
        Write-Host "`n🔍 Checking OTA progress..." -ForegroundColor Yellow
        $progressResponse = Invoke-WebRequest -Uri "http://$Host/progress" -TimeoutSec 5 -UseBasicParsing
        
        Write-Host "📊 Progress API Status: $($progressResponse.StatusCode)" -ForegroundColor Green
        Write-Host "📄 Progress Data: $($progressResponse.Content)" -ForegroundColor Cyan
        
    } catch {
        Write-Host "⚠️  Progress check failed: $($_.Exception.Message)" -ForegroundColor Yellow
    }
}

# Main execution
Write-Host "🧪 OTA Firmware Upload Test" -ForegroundColor Magenta
Write-Host "=============================" -ForegroundColor Magenta

# Test the upload
$success = Test-OTAUpload -Host $TargetHost -FilePath $FirmwarePath

if ($success) {
    # Check progress
    Test-OTAProgress -Host $TargetHost
    
    Write-Host "`n🎉 OTA upload test completed successfully!" -ForegroundColor Green
} else {
    Write-Host "`n💥 OTA upload test failed!" -ForegroundColor Red
}

Write-Host "`n📝 Note: This is a test upload. The device may need to be reset manually." -ForegroundColor Yellow