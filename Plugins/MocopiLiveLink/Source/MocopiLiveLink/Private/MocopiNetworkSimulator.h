////
// Copyright (C) 2026 Sony Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
////

#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"

struct FMocopiNetworkSimulationConfig
{
  bool bEnabled = false;
  int32 Seed = 1337;
  float RandomLossPercent = 0.0f;
  int32 BurstIntervalFrames = 0;
  int32 BurstLengthFrames = 0;
  float MaxJitterMs = 0.0f;
  float DuplicatePercent = 0.0f;
  float ReorderPercent = 0.0f;
  float ReorderExtraDelayMs = 40.0f;

  bool operator==(const FMocopiNetworkSimulationConfig& Other) const;
  bool operator!=(const FMocopiNetworkSimulationConfig& Other) const { return !(*this == Other); }
};

struct FMocopiNetworkSimulationDecision
{
  bool bDrop = false;
  bool bDuplicate = false;
  bool bReorder = false;
  double DelayMs = 0.0;
};

/** Deterministic packet impairment model used by the mocopi debug panel. */
class FMocopiNetworkSimulator
{
public:
  FMocopiNetworkSimulator();

  void Configure(const FMocopiNetworkSimulationConfig& InConfig);
  FMocopiNetworkSimulationDecision DecideForFrame();

private:
  bool Roll(float Percent);

  FMocopiNetworkSimulationConfig Config;
  FRandomStream RandomStream;
  uint64 FrameCounter = 0;
  int32 RemainingBurstDrops = 0;
};
