////
// Copyright (C) 2026 Sony Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
////

#include "MocopiLiveLinkSourceSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MocopiLiveLinkSourceSettings)

UMocopiLiveLinkSourceSettings::UMocopiLiveLinkSourceSettings()
{
	ApplyReliabilityPreset();
}

void UMocopiLiveLinkSourceSettings::ApplyReliabilityPreset()
{
	Mode = ELiveLinkSourceMode::EngineTime;
	BufferSettings.bValidEngineTimeEnabled = true;
	BufferSettings.bKeepAtLeastOneFrame = true;
	bRejectDuplicateAndOutOfOrderFrames = true;
	bUsePacketTimestampRecovery = true;

	switch (ReliabilityPreset)
	{
	case EMocopiReliabilityPreset::Realtime:
		BufferSettings.EngineTimeOffset = 0.04f;
		BufferSettings.MaxNumberOfFrameToBuffered = 60;
		BufferSettings.ValidEngineTime = 1.0f;
		UdpReceiveBufferSizeKB = 256;
		ConnectionTimeoutSeconds = 1.0f;
		bEnablePoseSmoothing = false;
		RotationSmoothingStrength = 0.05f;
		TranslationSmoothingStrength = 0.03f;
		break;

	case EMocopiReliabilityPreset::Reliable:
		BufferSettings.EngineTimeOffset = 0.20f;
		BufferSettings.MaxNumberOfFrameToBuffered = 160;
		BufferSettings.ValidEngineTime = 4.0f;
		UdpReceiveBufferSizeKB = 1024;
		ConnectionTimeoutSeconds = 3.0f;
		bEnablePoseSmoothing = true;
		RotationSmoothingStrength = 0.30f;
		TranslationSmoothingStrength = 0.25f;
		break;

	case EMocopiReliabilityPreset::Smooth:
		BufferSettings.EngineTimeOffset = 0.12f;
		BufferSettings.MaxNumberOfFrameToBuffered = 120;
		BufferSettings.ValidEngineTime = 2.0f;
		UdpReceiveBufferSizeKB = 512;
		ConnectionTimeoutSeconds = 2.0f;
		bEnablePoseSmoothing = true;
		RotationSmoothingStrength = 0.20f;
		TranslationSmoothingStrength = 0.15f;
		break;

	case EMocopiReliabilityPreset::Custom:
	default:
		break;
	}
}
