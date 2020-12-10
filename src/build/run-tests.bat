cd %OCAMLPATH%/../bin

:: Override SHELL since esy may change it
set SHELL="cmd.exe"

set "err=0"
for %%i in (*.exe) do %%i || set "err=1"
exit /b %err%
