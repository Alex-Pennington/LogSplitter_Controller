# TFTP Firmware Upload Test Script
param(
    [string]$TargetHost = "192.168.1.225",
    [string]$FirmwarePath = "monitor\.pio\build\uno_r4_wifi\firmware.bin"
)

function Test-TFTPUpload {
    param(
        [string]$TargetIP,
        [string]$FilePath
    )
    
    try {
        Write-Host "TFTP Upload Test" -ForegroundColor Green
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
        
        Write-Host "`nChecking TFTP availability..." -ForegroundColor Yellow
        
        # Check if TFTP client is available
        $tftpCmd = Get-Command tftp -ErrorAction SilentlyContinue
        if (-not $tftpCmd) {
            Write-Host "ERROR: TFTP client not found. Please install TFTP client:" -ForegroundColor Red
            Write-Host "  Windows: Enable 'TFTP Client' feature in Windows Features" -ForegroundColor Yellow
            Write-Host "  Or use: dism /online /Enable-Feature /FeatureName:TFTP" -ForegroundColor Yellow
            return $false
        }
        
        Write-Host "TFTP client found: $($tftpCmd.Source)" -ForegroundColor Green
        
        Write-Host "`nStarting TFTP upload..." -ForegroundColor Yellow
        Write-Host "Command: tftp $TargetIP PUT `"$FilePath`" firmware.bin" -ForegroundColor Gray
        
        # Create TFTP script
        $tftpScript = @"
connect $TargetIP
binary
put "$FilePath" firmware.bin
quit
"@
        
        $scriptFile = "tftp_upload.txt"
        $tftpScript | Out-File -FilePath $scriptFile -Encoding ASCII
        
        try {
            # Execute TFTP upload
            $result = & tftp -i $TargetIP -s $scriptFile 2>&1
            
            if ($LASTEXITCODE -eq 0) {
                Write-Host "SUCCESS: TFTP upload completed!" -ForegroundColor Green
                Write-Host "Upload result: $result" -ForegroundColor White
                return $true
            } else {
                Write-Host "ERROR: TFTP upload failed (Exit code: $LASTEXITCODE)" -ForegroundColor Red
                Write-Host "Error output: $result" -ForegroundColor Red
                return $false
            }
        }
        finally {
            # Clean up script file
            if (Test-Path $scriptFile) {
                Remove-Item $scriptFile -Force
            }
        }
        
    } catch {
        Write-Host "ERROR: Upload failed: $($_.Exception.Message)" -ForegroundColor Red
        return $false
    }
}

# Main execution
Write-Host "TFTP Firmware Upload Test" -ForegroundColor Magenta
Write-Host "========================" -ForegroundColor Magenta

# Test the upload
$success = Test-TFTPUpload -TargetIP $TargetHost -FilePath $FirmwarePath

if ($success) {
    Write-Host "`nTFTP upload test completed successfully!" -ForegroundColor Green
    Write-Host "Note: Check device telnet for transfer status" -ForegroundColor Yellow
} else {
    Write-Host "`nTFTP upload test failed!" -ForegroundColor Red
}

Write-Host "`nAlternative manual commands:" -ForegroundColor Cyan
Write-Host "tftp -i $TargetHost put `"$FirmwarePath`" firmware.bin" -ForegroundColor Gray
Write-Host "Or using Linux/macOS:" -ForegroundColor Cyan  
Write-Host "tftp $TargetHost -m binary -c put firmware.bin" -ForegroundColor Gray