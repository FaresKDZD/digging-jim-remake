@echo off
if /I "%~1"=="--version" (
  echo git version 2.45.0.windows.1
  exit /b 0
)
if /I "%~1"=="version" (
  echo git version 2.45.0.windows.1
  exit /b 0
)
echo git stub: unsupported %* 1>&2
exit /b 1
