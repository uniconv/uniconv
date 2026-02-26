# uniconv installer for Windows
# Usage:
#   PowerShell: irm https://raw.githubusercontent.com/uniconv/uniconv/main/install.ps1 | iex
#   cmd:        powershell -c "irm https://raw.githubusercontent.com/uniconv/uniconv/main/install.ps1 | iex"

$ErrorActionPreference = "Stop"

$Repo = "uniconv/uniconv"
$InstallDir = "$env:USERPROFILE\.uniconv\bin"

function Write-Info($msg) { Write-Host $msg }
function Write-Err($msg) { Write-Host "Error: $msg" -ForegroundColor Red; exit 1 }

# Detect architecture
$arch = if ([Environment]::Is64BitOperatingSystem) { "x86_64" } else { Write-Err "Only 64-bit Windows is supported." }

# Fetch latest version from GitHub redirect
Write-Info "uniconv installer"
Write-Info ""
Write-Info "  Platform: windows/$arch"

try {
    $response = Invoke-WebRequest -Uri "https://github.com/$Repo/releases/latest" -MaximumRedirection 0 -ErrorAction SilentlyContinue -UseBasicParsing 2>$null
} catch {
    $response = $_.Exception.Response
}

$redirectUrl = if ($response.StatusCode -ge 300 -and $response.StatusCode -lt 400) {
    $response.Headers.Location
} elseif ($response.Headers -and $response.Headers["Location"]) {
    $response.Headers["Location"]
} else {
    $null
}

if (-not $redirectUrl) {
    # Fallback: use GitHub API
    try {
        $apiResponse = Invoke-RestMethod -Uri "https://api.github.com/repos/$Repo/releases/latest" -UseBasicParsing
        $version = $apiResponse.tag_name
    } catch {
        Write-Err "Failed to fetch latest release. Check your internet connection."
    }
} else {
    $version = ($redirectUrl -split '/')[-1]
}

if (-not $version -or $version -eq "latest") {
    Write-Err "Failed to determine latest version."
}

Write-Info "  Version:  $version"
Write-Info ""

# Download
$filename = "uniconv-$version-windows-x86_64.zip"
$url = "https://github.com/$Repo/releases/download/$version/$filename"
$tmpDir = Join-Path $env:TEMP "uniconv-install-$(Get-Random)"
New-Item -ItemType Directory -Path $tmpDir -Force | Out-Null
$zipPath = Join-Path $tmpDir $filename

Write-Info "Downloading uniconv $version..."
try {
    Invoke-WebRequest -Uri $url -OutFile $zipPath -UseBasicParsing
} catch {
    Write-Err "Failed to download $url"
}

# Verify checksum
$checksumUrl = "https://github.com/$Repo/releases/download/$version/checksums.txt"
$checksumFile = Join-Path $tmpDir "checksums.txt"
try {
    Invoke-WebRequest -Uri $checksumUrl -OutFile $checksumFile -UseBasicParsing
    $expectedLine = Get-Content $checksumFile | Where-Object { $_ -match $filename }
    if ($expectedLine) {
        $expected = ($expectedLine -split '\s+')[0]
        $actual = (Get-FileHash -Path $zipPath -Algorithm SHA256).Hash.ToLower()
        if ($expected -ne $actual) {
            Write-Err "Checksum mismatch! Expected $expected, got $actual."
        }
        Write-Info "Checksum verified."
    }
} catch {
    # Checksum verification is best-effort
}

# Install
Write-Info "Installing to $InstallDir..."
New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null
Expand-Archive -Path $zipPath -DestinationPath $InstallDir -Force

# Clean up
Remove-Item -Path $tmpDir -Recurse -Force -ErrorAction SilentlyContinue

# Add to PATH (user-level)
$userPath = [Environment]::GetEnvironmentVariable("Path", "User")
if ($userPath -notlike "*$InstallDir*") {
    [Environment]::SetEnvironmentVariable("Path", "$InstallDir;$userPath", "User")
    Write-Info "Added $InstallDir to user PATH."
}

Write-Info ""
Write-Info "uniconv $version installed successfully!"
Write-Info ""
Write-Info "Restart your terminal, then try:"
Write-Info "  uniconv --version"
