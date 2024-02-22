@echo off

set /p DiscoveryUrl=URL for discovery:
set /p EngineUrl=URL for engine updates:

cd %~dp0\..\..

if exist bin if exist repos goto ThcrapDirFound
echo Error: failed to find thcrap directory
pause
exit 1

:ThcrapDirFound
del /s /q repos\thpatch
rd repos\thpatch

echo Generating config/config.js...
md config
(
	echo {
	echo 	"discovery_start_url": "%DiscoveryUrl%",
	echo 	"engine_update_url": "%EngineUrl%"
	echo }
) > config\config.js

echo Done.
pause
