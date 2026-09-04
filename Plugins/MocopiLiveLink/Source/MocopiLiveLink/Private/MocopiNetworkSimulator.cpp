////
// Copyright (C) 2026 Sony Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
////

#include "MocopiNetworkSimulator.h"

bool FMocopiNetworkSimulationConfig::operator==(const FMocopiNetworkSimulationConfig& Other) const
{
  return bEnabled == Other.bEnabled
    && Seed == Other.Seed
    && FMath::IsNearlyEqual(RandomLossPercent, Other.RandomLossPercent)
    && BurstIntervalFrames == Other.BurstIntervalFrames
    && BurstLengthFrames == Other.BurstLengthFrames
    && FMath::IsNearlyEqual(MaxJitterMs, Other.MaxJitterMs)
    && FMath::IsNearlyEqual(DuplicatePercent, Other.DuplicatePercent)
    && FMath::IsNearlyEqual(ReorderPercent, Other.ReorderPercent)
    && FMath::IsNearlyEqual(ReorderExtraDelayMs, Other.ReorderExtraDelayMs);
}

FMocopiNetworkSimulator::FMocopiNetworkSimulator()
  : RandomStream(Config.Seed)
{
}

void FMocopiNetworkSimulator::Configure(const FMocopiNetworkSimulationConfig& InConfig)
{
  if (Config == InConfig)
  {
    return;
  }

  Config = InConfig;
  Config.RandomLossPercent = FMath::Clamp(Config.RandomLossPercent, 0.0f, 100.0f);
  Config.DuplicatePercent = FMath::Clamp(Config.DuplicatePercent, 0.0f, 100.0f);
  Config.ReorderPercent = FMath::Clamp(Config.ReorderPercent, 0.0f, 100.0f);
  Config.MaxJitterMs = FMath::Max(0.0f, Config.MaxJitterMs);
  Config.ReorderExtraDelayMs = FMath::Max(0.0f, Config.ReorderExtraDelayMs);
  Config.BurstIntervalFrames = FMath::Max(0, Config.BurstIntervalFrames);
  Config.BurstLengthFrames = FMath::Max(0, Config.BurstLengthFrames);

  RandomStream.Initialize(Config.Seed);
  FrameCounter = 0;
  RemainingBurstDrops = 0;
}

FMocopiNetworkSimulationDecision FMocopiNetworkSimulator::DecideForFrame()
{
  FMocopiNetworkSimulationDecision Decision;
  if (!Config.bEnabled)
  {
    return Decision;
  }

  ++FrameCounter;

  if (RemainingBurstDrops > 0)
  {
    --RemainingBurstDrops;
    Decision.bDrop = true;
    return Decision;
  }

  if (Config.BurstIntervalFrames > 0
    && Config.BurstLengthFrames > 0
    && FrameCounter % static_cast<uint64>(Config.BurstIntervalFrames) == 0)
  {
    RemainingBurstDrops = Config.BurstLengthFrames - 1;
    Decision.bDrop = true;
    return Decision;
  }

  if (Roll(Config.RandomLossPercent))
  {
    Decision.bDrop = true;
    return Decision;
  }

  if (Config.MaxJitterMs > 0.0f)
  {
    Decision.DelayMs = RandomStream.FRandRange(0.0f, Config.MaxJitterMs);
  }

  Decision.bReorder = Roll(Config.ReorderPercent);
  if (Decision.bReorder)
  {
    Decision.DelayMs += Config.ReorderExtraDelayMs;
  }

  Decision.bDuplicate = Roll(Config.DuplicatePercent);
  return Decision;
}

bool FMocopiNetworkSimulator::Roll(float Percent)
{
  return Percent > 0.0f && RandomStream.FRandRange(0.0f, 100.0f) < Percent;
}
