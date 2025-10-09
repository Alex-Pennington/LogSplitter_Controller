# OTA Firmware Upload Test Script
param(
    [string]$TargetHost = "192.168.1.225",
    [string]$FirmwarePath = "monitor\.pio\build\uno_r4_wifi\firmware.bin"
)

function Test-OTAUpload {
    param(
        [string]$TargetIP,
        [string]$FilePath
    )
    
    try {
        Write-Host "OTA Upload Test" -ForegroundColor Green
        Write-Host "===============" -ForegroundColor Green
        
        # Check if firmware file exists
        if (-not (Test-Path $FilePath)) {
            Write-Host "ERROR: Firmware file not found: $FilePath" -ForegroundColor Red
            return $false
        }
        
        $fileInfo = Get-Item $FilePath
        Write-Host "Firmware file: $($fileInfo.Name)" -ForegroundColor Cyan
        Write-Host "File size: $($fileInfo.Length) bytes" -ForegroundColor Cyan
        Write-Host "Target: $TargetIP" -ForegroundColor Cyan
        
        Write-Host "`nTesting OTA server accessibility..." -ForegroundColor Yellow
        
        # First test if the server is accessible
        $testResponse = Invoke-WebRequest -Uri "http://$TargetIP/update" -Method GET -TimeoutSec 10 -UseBasicParsing
        
        if ($testResponse.StatusCode -ne 200) {
            Write-Host "ERROR: OTA server not accessible (Status: $($testResponse.StatusCode))" -ForegroundColor Red
            return $false
        }
        
        Write-Host "SUCCESS: OTA server accessible (Status: $($testResponse.StatusCode))" -ForegroundColor Green
        Write-Host "Response length: $($testResponse.Content.Length) bytes" -ForegroundColor Gray
        
        Write-Host "`nStarting firmware upload..." -ForegroundColor Yellow
        
        # Read firmware file
        $firmwareBytes = [System.IO.File]::ReadAllBytes((Resolve-Path $FilePath).Path)
        
        # Create a simple POST request with the binary data
        $uri = "http://$TargetIP/update"
        
        Write-Host "Uploading to: $uri" -ForegroundColor Yellow
        Write-Host "Upload size: $($firmwareBytes.Length) bytes" -ForegroundColor Cyan
        Write-Host "This may take a moment..." -ForegroundColor Yellow
        
        # Try the upload with different content types
        try {
            # First try with multipart form data
            $boundary = [System.Guid]::NewGuid().ToString()
            $headers = @{
                "Content-Type" = "multipart/form-data; boundary=$boundary"
            }
            
            # Create multipart body
            $LF = "`r`n"
            $bodyText = "--$boundary$LF" +
                       "Content-Disposition: form-data; name=`"firmware`"; filename=`"firmware.bin`"$LF" +
                       "Content-Type: application/octet-stream$LF$LF"
            $bodyEnd = "$LF--$boundary--$LF"
            
            $bodyTextBytes = [System.Text.Encoding]::UTF8.GetBytes($bodyText)
            $bodyEndBytes = [System.Text.Encoding]::UTF8.GetBytes($bodyEnd)
            
            $bodyBytes = $bodyTextBytes + $firmwareBytes + $bodyEndBytes
            
            $uploadResponse = Invoke-RestMethod -Uri $uri -Method POST -Body $bodyBytes -Headers $headers -TimeoutSec 120
            
            Write-Host "SUCCESS: Upload completed!" -ForegroundColor Green
            Write-Host "Server response: $uploadResponse" -ForegroundColor White
            
            return $true
            
        } catch {
            Write-Host "Multipart upload failed, trying simple binary upload..." -ForegroundColor Yellow
            
            # Fallback to simple binary upload
            $headers = @{
                "Content-Type" = "application/octet-stream"
            }
            
            $uploadResponse = Invoke-RestMethod -Uri $uri -Method POST -Body $firmwareBytes -Headers $headers -TimeoutSec 120
            
            Write-Host "SUCCESS: Binary upload completed!" -ForegroundColor Green
            Write-Host "Server response: $uploadResponse" -ForegroundColor White
            
            return $true
        }
        
    } catch {
        Write-Host "ERROR: Upload failed: $($_.Exception.Message)" -ForegroundColor Red
        
        if ($_.Exception.Response) {
            try {
                $reader = New-Object System.IO.StreamReader($_.Exception.Response.GetResponseStream())
                $responseBody = $reader.ReadToEnd()
                Write-Host "Server response: $responseBody" -ForegroundColor Red
            } catch {
                Write-Host "Could not read server response" -ForegroundColor Red
            }
        }
        
        return $false
    }
}

# Main execution
Write-Host "OTA Firmware Upload Test" -ForegroundColor Magenta
Write-Host "========================" -ForegroundColor Magenta

# Test the upload
$success = Test-OTAUpload -TargetIP $TargetHost -FilePath $FirmwarePath

if ($success) {
    Write-Host "`nOTA upload test completed successfully!" -ForegroundColor Green
} else {
    Write-Host "`nOTA upload test failed!" -ForegroundColor Red
}

Write-Host "`nNote: This is a test upload. Check the device status with telnet." -ForegroundColor Yellow