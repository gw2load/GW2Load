Push-Location $PSScriptRoot

$dirty = $false
$uncommittedChanges = (git status -s).Length -gt 0
$latestTag = git describe --tags --abbrev=0
if($LASTEXITCODE -ne 0) {
    $latestTag = "0.0.0"
    $dirty = $true
} else {
    $tagHash = git rev-list -n 1 "$latestTag"
    $currentHash = git rev-parse HEAD
    $dirty = $tagHash -ne $currentHash
    $dirty = $dirty -or $uncommittedChanges
}

$gitVer = $latestTag.Replace('.', ',')
$gitVer += "," + ($dirty ? "1" : "0")

$dirtySuffix = ""
if($dirty)
{
    $dirtySuffix = " (pre " + (git rev-parse --short HEAD)
    if($uncommittedChanges)
    {
        $dirtySuffix += "+"
    }
    $dirtySuffix += ")"
}

$gitVerStr = $latestTag + $dirtySuffix

Write-Output "#define GIT_VER $gitVer
#define GIT_VER_STR ""$gitVerStr\0""" > version.h