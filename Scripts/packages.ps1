# Requred for downloading from GitHub
[System.Net.ServicePointManager]::SecurityProtocol = [System.Net.SecurityProtocolType]::Tls12;

# URL Configuration
$packageDir = $args[0] # Directory to save packages to
$release = $args[1] # Package release to download from

Write-Output "Scanning packages..."

# Array of all dependent packages that are in progress by other instances
$inProgress = New-Object System.Collections.ArrayList

# Enumerate the list of packages
foreach ($package in $args[2..$args.Length])
{
    $packagePath = $packageDir + $package
    $inProgressPath = $packagePath + ".in_progress"

    # Check if The package is currently in progress
    if (Test-Path $inProgressPath)
    {
        Write-Output ("Waiting on in-progress package '{0}'." -f $package)
        $inProgress.Add($inProgressPath) | Out-Null
        continue
    }
    # Check if we've already installed the package
    elseif (Test-Path $packagePath)
    {
        Write-Output ("Package '{0}' is up to date." -f $package)
        continue
    }

    # Create the in-progress file
    New-Item -Force $inProgressPath | Out-Null

    # Get a temp directory to download to
    $temp = [System.IO.Path]::GetTempFileName() + ".zip"

    # Download the package
    Write-Output ("Downloading package '{0}'..." -f $package)
    $url = "https://github.com/willcassella/SinGE/releases/download/{0}/{1}.zip" -f $release, $package
    $web = New-Object net.webclient
    $web.DownloadFile($url, $temp)

    # Extract the package
    Write-Output ("Extracting package '{0}'..." -f $package)
    Expand-Archive $temp -dest $packagePath

    # Delete the temporary file and the in-progress file
    Remove-Item $temp
    Remove-Item $inProgressPath
}

# Wait for dependent packages to complete
while ($inProgress.Count -gt 0)
{
    for ($i = 0; $i -lt $inProgress.Count; $i++)
    {
        # If the item is no longer downloading
        if (-Not (Test-Path $inProgress[$i]))
        {
            $inProgress.RemoveAt($i)
        }
    }
}
