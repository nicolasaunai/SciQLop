pushd %~dp0
set script_dir=%CD%
popd
set PYTHONHOME=%script_dir%\python
%script_dir%\sciqlopapp.exe