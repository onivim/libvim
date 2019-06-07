cd %OCAMLPATH%/../bin

set "err=0"
for %%i in (*.exe) do %%i || set "err=1"
exit /b %err%
