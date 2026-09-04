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

#pragma once

#include "CoreMinimal.h"
#include "LiveLinkSourceSettings.h"

#include "MocopiLiveLinkSourceSettings.generated.h"

UENUM()
enum class EMocopiReliabilityPreset : uint8
{
	Realtime UMETA(DisplayName = "Realtime"),
	Smooth UMETA(DisplayName = "Smooth (Recommended)"),
	Reliable UMETA(DisplayName = "Reliable"),
	Custom UMETA(DisplayName = "Custom")
};

/** Reliability and smoothing controls for a mocopi Live Link source. */
UCLASS()
class MOCOPILIVELINK_API UMocopiLiveLinkSourceSettings : public ULiveLinkSourceSettings
{
	GENERATED_BODY()

public:
	UMocopiLiveLinkSourceSettings();
	void ApplyReliabilityPreset();

	/** One-click latency versus resilience profile. Editing a controlled value changes this to Custom. */
	UPROPERTY(EditAnywhere, Category = "Mocopi Preset")
	EMocopiReliabilityPreset ReliabilityPreset = EMocopiReliabilityPreset::Smooth;

	/** Operating-system UDP receive buffer. Larger values tolerate short CPU and network bursts. */
	UPROPERTY(EditAnywhere, Category = "Mocopi Reliability", meta = (ClampMin = "64", ClampMax = "4096", UIMin = "64", UIMax = "2048", ForceUnits = "KB"))
	int32 UdpReceiveBufferSizeKB = 512;

	/** Time without packets before the source requests a fresh skeleton definition. */
	UPROPERTY(EditAnywhere, Category = "Mocopi Reliability", meta = (ClampMin = "0.4", ClampMax = "10.0", UIMin = "0.4", UIMax = "5.0", ForceUnits = "s"))
	float ConnectionTimeoutSeconds = 2.0f;

	/** Ignore duplicate and late out-of-order packets that can make a pose jump backwards. */
	UPROPERTY(EditAnywhere, Category = "Mocopi Reliability")
	bool bRejectDuplicateAndOutOfOrderFrames = true;

	/** Use the timestamp embedded in each mocopi packet so Live Link can interpolate across missing packets. */
	UPROPERTY(EditAnywhere, Category = "Mocopi Reliability")
	bool bUsePacketTimestampRecovery = true;

	/** Apply a lightweight low-pass filter before frames enter Live Link's delayed interpolation buffer. */
	UPROPERTY(EditAnywhere, Category = "Mocopi Smoothing")
	bool bEnablePoseSmoothing = true;

	/** Higher values reduce rotational jitter but respond more slowly. Zero disables rotation filtering. */
	UPROPERTY(EditAnywhere, Category = "Mocopi Smoothing", meta = (EditCondition = "bEnablePoseSmoothing", ClampMin = "0.0", ClampMax = "0.95", UIMin = "0.0", UIMax = "0.8"))
	float RotationSmoothingStrength = 0.20f;

	/** Higher values reduce root-position jitter but respond more slowly. Zero disables translation filtering. */
	UPROPERTY(EditAnywhere, Category = "Mocopi Smoothing", meta = (EditCondition = "bEnablePoseSmoothing", ClampMin = "0.0", ClampMax = "0.95", UIMin = "0.0", UIMax = "0.8"))
	float TranslationSmoothingStrength = 0.15f;

	/** Enable deterministic motion-packet impairment for testing. Disabled and ignored in Shipping builds. */
	UPROPERTY(EditAnywhere, Category = "Mocopi Network Simulation")
	bool bEnableNetworkSimulation = false;

	/** Reusing the same seed reproduces the same random impairment sequence. */
	UPROPERTY(EditAnywhere, Category = "Mocopi Network Simulation", meta = (EditCondition = "bEnableNetworkSimulation"))
	int32 SimulationSeed = 1337;

	/** Independently drop this percentage of motion packets. */
	UPROPERTY(EditAnywhere, Category = "Mocopi Network Simulation", meta = (EditCondition = "bEnableNetworkSimulation", ClampMin = "0.0", ClampMax = "50.0", UIMin = "0.0", UIMax = "20.0", ForceUnits = "%"))
	float RandomPacketLossPercent = 0.0f;

	/** Start a deterministic loss burst every N received motion frames. Zero disables burst loss. */
	UPROPERTY(EditAnywhere, Category = "Mocopi Network Simulation", meta = (EditCondition = "bEnableNetworkSimulation", ClampMin = "0", ClampMax = "10000", UIMin = "0", UIMax = "500"))
	int32 BurstLossIntervalFrames = 0;

	/** Number of consecutive frames dropped in each burst. */
	UPROPERTY(EditAnywhere, Category = "Mocopi Network Simulation", meta = (EditCondition = "bEnableNetworkSimulation && BurstLossIntervalFrames > 0", ClampMin = "1", ClampMax = "50", UIMin = "1", UIMax = "10"))
	int32 BurstLossLengthFrames = 3;

	/** Add a random delay from zero to this value to each motion packet. */
	UPROPERTY(EditAnywhere, Category = "Mocopi Network Simulation", meta = (EditCondition = "bEnableNetworkSimulation", ClampMin = "0.0", ClampMax = "500.0", UIMin = "0.0", UIMax = "150.0", ForceUnits = "ms"))
	float MaximumJitterMs = 0.0f;

	/** Duplicate this percentage of motion packets. */
	UPROPERTY(EditAnywhere, Category = "Mocopi Network Simulation", meta = (EditCondition = "bEnableNetworkSimulation", ClampMin = "0.0", ClampMax = "50.0", UIMin = "0.0", UIMax = "10.0", ForceUnits = "%"))
	float DuplicatePacketPercent = 0.0f;

	/** Add extra delay to this percentage of packets so later packets can arrive first. */
	UPROPERTY(EditAnywhere, Category = "Mocopi Network Simulation", meta = (EditCondition = "bEnableNetworkSimulation", ClampMin = "0.0", ClampMax = "50.0", UIMin = "0.0", UIMax = "10.0", ForceUnits = "%"))
	float ReorderPacketPercent = 0.0f;

	/** Extra delay applied to packets selected for reordering. */
	UPROPERTY(EditAnywhere, Category = "Mocopi Network Simulation", meta = (EditCondition = "bEnableNetworkSimulation && ReorderPacketPercent > 0", ClampMin = "1.0", ClampMax = "500.0", UIMin = "1.0", UIMax = "100.0", ForceUnits = "ms"))
	float ReorderExtraDelayMs = 40.0f;

	/** Frames accepted from mocopi since this source was created. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Mocopi Diagnostics")
	uint64 ReceivedFrames = 0;

	/** Missing frame IDs observed in the mocopi stream. Live Link interpolation bridges short gaps within the configured delay. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Mocopi Diagnostics")
	uint64 EstimatedLostFrames = 0;

	/** Duplicate or late packets rejected to prevent backwards pose jumps. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Mocopi Diagnostics")
	uint64 RejectedFrames = 0;

	/** Motion packets intentionally dropped by the test simulator. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Mocopi Diagnostics")
	uint64 SimulatedDroppedPackets = 0;

	/** Motion packets intentionally delayed by jitter or reordering. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Mocopi Diagnostics")
	uint64 SimulatedDelayedPackets = 0;

	/** Extra duplicate packets generated by the test simulator. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Mocopi Diagnostics")
	uint64 SimulatedDuplicatePackets = 0;
};
