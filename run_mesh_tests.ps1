# PowerShell script to run mesh integration tests
# Ensures LCARS server is running, then executes test suite

Write-Host "[CHECK] Checking if LCARS server is running..." -ForegroundColor Cyan

# Test if server is reachable
try {
    $response = Invoke-WebRequest -Uri "http://localhost:3000" -TimeoutSec 2 -ErrorAction Stop
    Write-Host "[OK] LCARS server is already running" -ForegroundColor Green
} catch {
    Write-Host "[WARN] LCARS server not running, starting it now..." -ForegroundColor Yellow
    Start-Process -FilePath ".\.venv\Scripts\python.exe" -ArgumentList "lcars_docs_server.py"
    Write-Host "[WAIT] Waiting 5 seconds for server to initialize..." -ForegroundColor Yellow
    Start-Sleep -Seconds 5
}

Write-Host "`n[TEST] Running mesh integration test suite..." -ForegroundColor Cyan
Write-Host "=" * 80 -ForegroundColor Cyan

# Run the test suite
& .\.venv\Scripts\python.exe test_mesh_lcars_integration.py --url http://localhost:3000

Write-Host "`n[OK] Test suite complete!" -ForegroundColor Green
