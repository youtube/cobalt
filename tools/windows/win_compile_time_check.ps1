# Script to test if a small target builds quickly on windows as expected.

# Define the timeout value in seconds
$timeout = 120

$folderPath = ".\out\win32_test\"

# Remove folder if present from out directory
if (Test-Path $folderPath) {
    # Delete the folder
    Remove-Item $folderPath -Recurse -Force
}

# Start a timer
$start = Get-Date

# Run the command
$command = "python .\cobalt\build\gn.py .\out\win32_test -p win-win32 -C devel ; ninja -C $folderpath eztime_test"
Invoke-Expression $command

# Get the end time
$end = Get-Date

# Calculate the execution time
$executionTime = $end - $start
$executionTimeinSecs = $executionTime.seconds
Write-Output "Execution time in secs is $executionTimeinSecs"

# Check if the execution time is greater than the timeout value
if ($executionTime.seconds -gt $timeout) {
    # Check b/291665088 for more discussion.
    throw "The command took more than $timeout seconds to run."
}

# The command ran successfully
Write-Output "The command ran successfully."
