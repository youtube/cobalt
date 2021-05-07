if "%1" == "" set "PLATFORM=win-win32"
if "%2" == "" set "CONFIG=devel"
if "%3" == "" set "TEST=eztime_test"

cd c:\code\out\%PLATFORM%_%CONFIG%\
unzip app_launcher.zip -d c:\app_launcher_out\
%python2% c:\app_launcher_out\starboard\tools\testing\test_runner.py --run --platform %PLATFORM% --config %CONFIG% -o c:\code\out\%PLATFORM%_%CONFIG%
