Remove-Item -Recurse -Force -Path .cache
Remove-Item -Recurse -Force -Path .idea
Remove-Item -Recurse -Force -Path .vs
Remove-Item -Recurse -Force -Path .vsconfig
Remove-Item -Recurse -Force -Path *.uproject.DotSettings.user
Remove-Item -Recurse -Force -Path compile_commands.json
Remove-Item -Recurse -Force -Path CursorZoomOrbit.sln
Remove-Item -Recurse -Force -Path DerivedDataCache
Remove-Item -Recurse -Force -Path Saved

$paths = @(".\Binaries",".\Intermediate",".\Plugins\*\Binaries",".\Plugins\*\Intermediate")
Remove-Item -Recurse -Force -Path $paths -ErrorAction SilentlyContinue
