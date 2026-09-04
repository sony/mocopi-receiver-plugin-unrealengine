#!/bin/bash

###
# Copyright (C) 2026 Sony Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
###

# Script will Build and Package MocopiLiveLink plugin for the 3 UE versions defined below.

# Begin modifiable variables section ----------
UE_Version_5=5.5
UE_Version_6=5.6
UE_Version_7=5.7
UE_Version_8=5.8

# UE Installs directories
UE_Version5_Dir="/Users/Shared/Epic Games/UE_${UE_Version_5}"
UE_Version6_Dir="/Users/Shared/Epic Games/UE_${UE_Version_6}"
UE_Version7_Dir="/Users/Shared/Epic Games/UE_${UE_Version_7}"
UE_Version8_Dir="/Users/Shared/Epic Games/UE_${UE_Version_8}"


# Output Directory for Packaged builds
UserName="your_username_here"  # <-- Change this to your macOS username
PkgOutputDir="/Users/${UserName}/Documents/Development/MocopiLiveLink_Packaging/106"

# Plugin Version for Path-naming purposes ONLY. Does NOT modify version number of the plugin.
PluginVersion=1.0.7

# Xcode Paths.
PathToXcode="/Applications/Xcode.app"

# End modifiable variables section ----------

# Get Path to repository's .uplugin relative to script
ScriptDir="$(dirname "$(readlink -f "$0")")"
ProjectDir="$(dirname "$ScriptDir")"
PluginDir="${ProjectDir}/Plugins/MocopiLiveLink/MocopiLiveLink.uplugin"

# UAT build script
RunUAT_Dir="/Engine/Build/BatchFiles/RunUAT.sh"

function BuildPlugin {
	local UE_Dir=$1
	local UE_Version=$2
	"${UE_Dir}${RunUAT_Dir}" BuildPlugin -Plugin="${PluginDir}" -Package="${PkgOutputDir}/MocopiLiveLink_${PluginVersion}_UE${UE_Version}/MocopiLiveLink" -architecture=arm64+x64 -Rocket 
}

# Switch Xcode version
sudo xcode-select --switch ${PathToXcode}

# Build and Package for each UE version
BuildPlugin "${UE_Version5_Dir}" "${UE_Version_5}" 
BuildPlugin "${UE_Version6_Dir}" "${UE_Version_6}"
BuildPlugin "${UE_Version7_Dir}" "${UE_Version_7}"
BuildPlugin "${UE_Version8_Dir}" "${UE_Version_8}"

