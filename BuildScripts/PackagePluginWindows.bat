REM ###
REM # Copyright (C) 2026 Sony Corporation
REM #
REM # Licensed under the Apache License, Version 2.0 (the "License");
REM # you may not use this file except in compliance with the License.
REM # You may obtain a copy of the License at
REM #
REM #      http://www.apache.org/licenses/LICENSE-2.0
REM #
REM # Unless required by applicable law or agreed to in writing, software
REM # distributed under the License is distributed on an "AS IS" BASIS,
REM # WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
REM # See the License for the specific language governing permissions and
REM # limitations under the License.
REM ###

setlocal enabledelayedexpansion

@echo off

REM Script to Build and Package MocopiLiveLink plugin.
REM Steps:
REM 1.Set the Plugin_Version for output naming purposes. It should be the same as the version defined in the .uplugin file.
REM 2.Add/Modify your UE Versions below
REM 3.Add/Modify corresponding Engine installation paths
REM 4.Add/Modify calls to the BuildAndPackage function 

set Plugin_Version=1.0.7

REM UE versions to package plugin for
set "UE_V5=5.5"
set "UE_V6=5.6"
set "UE_V7=5.7"
set "UE_V8=5.8"

REM Corresponding Engine paths
set "UE_V5_Dir=C:\Program Files\Epic Games\UE_%UE_V5%"
set "UE_V6_Dir=C:\Program Files\Epic Games\UE_%UE_V6%"
set "UE_V7_Dir=C:\Program Files\Epic Games\UE_%UE_V7%"
set "UE_V8_Dir=C:\Program Files\Epic Games\UE_%UE_V8%"

REM Output Directory for Packaged builds
set "Pkg_Output_Dir=C:\mocopi_temp\MocopiLiveLink_Packaging\%Plugin_Version%"

REM UAT build script 
set "RunUAT_Dir=\Engine\Build\BatchFiles\RunUAT.bat"

REM Getting path to repository's .uplugin relative to this script
set "Script_Path=%~dp0"
set "Project_Path=%Script_Path:~0,-1%"
for /f "delims=\ tokens=*" %%i in ("%Project_Path%") do set "Project_Path=%%~dpi"
set "Plugin_Dir=%Project_Path%\Plugins\MocopiLiveLink\MocopiLiveLink.uplugin"

REM  Comment these out as needed for testing
call :BuildAndPackage %UE_V5% "%UE_V5_Dir%"
call :BuildAndPackage %UE_V6% "%UE_V6_Dir%"
call :BuildAndPackage %UE_V7% "%UE_V7_Dir%"
call :BuildAndPackage %UE_V8% "%UE_V8_Dir%"

goto :eof

REM Function to build and package for a given UE version
:BuildAndPackage
set "UE_Version=%~1"
set "UE_Dir=%~2"
set "Package_Final_Path=%Pkg_Output_Dir%\MocopiLiveLink_%Plugin_Version%_UE%UE_Version%"
echo Building and packaging for Unreal Engine %UE_Version%...
echo "!UE_Dir!!RunUAT_Dir!" BuildPlugin -Plugin="%Plugin_Dir%" -Package="%Package_Final_Path%\MocopiLiveLink" -Rocket
"!UE_Dir!!RunUAT_Dir!" BuildPlugin -Plugin="%Plugin_Dir%" -Package="%Package_Final_Path%\MocopiLiveLink" -Rocket



