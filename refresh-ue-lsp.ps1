param(
    [string]$ProjectRoot = (Get-Location).Path,
    [string]$UnrealEngine = "D:\Program Files\Epic Games\UE_5.4"
)

$ErrorActionPreference = "Stop"

$UBT = Join-Path $UnrealEngine "Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll"

$uprojectFile = Get-ChildItem -Path $ProjectRoot -Filter *.uproject -ErrorAction Stop | Select-Object -First 1
if (-not $uprojectFile) { throw "No .uproject file found under: $ProjectRoot" }

$UProject= $uprojectFile.FullName
$ProjectName = [IO.Path]::GetFileNameWithoutExtension($UProject)

$Target = "$ProjectName" + "Editor Win64 Development"

& dotnet "$UBT" `
    -Mode=GenerateClangDatabase `
    -Project="$UProject" `
    -Target="$Target" `
    -OutputDir="$ProjectRoot"
