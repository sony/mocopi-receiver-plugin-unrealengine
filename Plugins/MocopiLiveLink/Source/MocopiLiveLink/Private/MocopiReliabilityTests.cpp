////
// Copyright (C) 2026 Sony Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
////

#if WITH_DEV_AUTOMATION_TESTS

#include "MocopiLiveLinkSourceSettings.h"
#include "MocopiNetworkSimulator.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
  FMocopiNetworkSimulationDeterminismTest,
  "Mocopi.Reliability.NetworkSimulation.Deterministic",
  EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMocopiNetworkSimulationDeterminismTest::RunTest(const FString& Parameters)
{
  (void)Parameters;

  FMocopiNetworkSimulationConfig Config;
  Config.bEnabled = true;
  Config.Seed = 8675309;
  Config.RandomLossPercent = 12.0f;
  Config.MaxJitterMs = 80.0f;
  Config.DuplicatePercent = 5.0f;
  Config.ReorderPercent = 4.0f;
  Config.ReorderExtraDelayMs = 40.0f;

  FMocopiNetworkSimulator First;
  FMocopiNetworkSimulator Second;
  First.Configure(Config);
  Second.Configure(Config);

  for (int32 Index = 0; Index < 500; ++Index)
  {
    const FMocopiNetworkSimulationDecision A = First.DecideForFrame();
    const FMocopiNetworkSimulationDecision B = Second.DecideForFrame();
    TestEqual(TEXT("Drop decision is reproducible"), A.bDrop, B.bDrop);
    TestEqual(TEXT("Duplicate decision is reproducible"), A.bDuplicate, B.bDuplicate);
    TestEqual(TEXT("Reorder decision is reproducible"), A.bReorder, B.bReorder);
    TestTrue(TEXT("Delay is reproducible"), FMath::IsNearlyEqual(A.DelayMs, B.DelayMs));
  }

  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
  FMocopiNetworkSimulationBurstTest,
  "Mocopi.Reliability.NetworkSimulation.BurstLoss",
  EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMocopiNetworkSimulationBurstTest::RunTest(const FString& Parameters)
{
  (void)Parameters;

  FMocopiNetworkSimulationConfig Config;
  Config.bEnabled = true;
  Config.BurstIntervalFrames = 10;
  Config.BurstLengthFrames = 3;

  FMocopiNetworkSimulator Simulator;
  Simulator.Configure(Config);

  for (int32 Frame = 1; Frame <= 25; ++Frame)
  {
    const bool bExpectedDrop = (Frame >= 10 && Frame <= 12) || (Frame >= 20 && Frame <= 22);
    TestEqual(*FString::Printf(TEXT("Burst decision for frame %d"), Frame), Simulator.DecideForFrame().bDrop, bExpectedDrop);
  }

  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
  FMocopiReliabilityPresetTest,
  "Mocopi.Reliability.Presets",
  EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMocopiReliabilityPresetTest::RunTest(const FString& Parameters)
{
  (void)Parameters;

  UMocopiLiveLinkSourceSettings* Settings = NewObject<UMocopiLiveLinkSourceSettings>();
  TestEqual(TEXT("Smooth is the default preset"), Settings->ReliabilityPreset, EMocopiReliabilityPreset::Smooth);
  TestEqual(TEXT("Smooth offset"), Settings->BufferSettings.EngineTimeOffset, 0.12f);
  TestEqual(TEXT("Smooth buffer"), Settings->BufferSettings.MaxNumberOfFrameToBuffered, 120);

  Settings->ReliabilityPreset = EMocopiReliabilityPreset::Realtime;
  Settings->ApplyReliabilityPreset();
  TestEqual(TEXT("Realtime offset"), Settings->BufferSettings.EngineTimeOffset, 0.04f);
  TestFalse(TEXT("Realtime pose smoothing"), Settings->bEnablePoseSmoothing);

  Settings->ReliabilityPreset = EMocopiReliabilityPreset::Reliable;
  Settings->ApplyReliabilityPreset();
  TestEqual(TEXT("Reliable offset"), Settings->BufferSettings.EngineTimeOffset, 0.20f);
  TestEqual(TEXT("Reliable buffer"), Settings->BufferSettings.MaxNumberOfFrameToBuffered, 160);
  TestEqual(TEXT("Reliable UDP buffer"), Settings->UdpReceiveBufferSizeKB, 1024);
  TestEqual(TEXT("Reliable timeout"), Settings->ConnectionTimeoutSeconds, 3.0f);

  return true;
}

#endif
