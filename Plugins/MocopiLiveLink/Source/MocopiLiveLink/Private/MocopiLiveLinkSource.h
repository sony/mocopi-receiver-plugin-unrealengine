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

#include <ILiveLinkSource.h>
#include "Roles/LiveLinkAnimationTypes.h"
#include "HAL/Runnable.h"
#include "MocopiDataHandler.h"
#include "MocopiNetworkSimulator.h"
#include "Modules/ModuleManager.h"
#include "Sockets.h"

#include <chrono>

class UMocopiLiveLinkSourceSettings;

class FMocopiLiveLinkSource : public ILiveLinkSource, public FRunnable
{
public:

  FMocopiLiveLinkSource(uint16 inputPort, FName subjectName);
  virtual ~FMocopiLiveLinkSource();

  // ILiveLinkSource Interface
  virtual void ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid) override;
  virtual void InitializeSettings(ULiveLinkSourceSettings* Settings) override;
  virtual void Update() override;
  virtual TSubclassOf<ULiveLinkSourceSettings> GetSettingsClass() const override;
  virtual void OnSettingsChanged(ULiveLinkSourceSettings* Settings, const FPropertyChangedEvent& PropertyChangedEvent) override;
  virtual bool IsSourceStillValid() const;
  virtual bool RequestSourceShutdown();

  virtual FText GetSourceType() const;
  virtual FText GetSourceMachineName() const;
  virtual FText GetSourceStatus() const;

  // FRunnable Interface
  virtual bool Init() override { return true; }
  virtual uint32 Run() override;
  virtual void Stop() override;
  virtual void Exit() override { }


private:

  MocopiDataHandler mDataHandler;

  // Udp Thread variables
  FString mUdpThreadName;
  FRunnableThread* mUdpThread;
  std::atomic_bool mIsStopping;
  std::atomic_bool mIsThreadFinished;

  // Udp Socket & buffer
  FSocket* mSocket;
  TArray<uint8> mRecvBuffer;
  ISocketSubsystem* mSocketSubSystem;
  uint16 mInputPort;

  // LiveLink Client & its ID
  ILiveLinkClient* mClient;
  FGuid mSourceGuid;

  // Helpers
  void UpdateFrameData(FLiveLinkAnimationFrameData& outFrame);

  void HandleReceivedData(TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> receivedData);

  void QueueOrProcessReceivedData(TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> ReceivedData);

  void ProcessPendingSimulatedPackets(bool bFlushAll = false);

  FMocopiNetworkSimulationConfig GetNetworkSimulationConfig() const;

  void OpenConnection();

  void StartUdpThread();

  bool ShutdownThreadAndSocket();

  void SetUpNewMocopiSubject();

  FName GetNewSubjectName();

  void DefineNewMocopiSubject();

  FQualifiedFrameTime GetQualifiedFrameTime(MocopiFrameMetaData& frameMetaData);

  void ApplySettings(UMocopiLiveLinkSourceSettings* Settings);

  void ResetStreamState();

  bool ShouldAcceptFrame(int32 FrameId, double PacketTimestamp);

  double GetPacketTimestampWorldTime(double PacketTimestamp);

  FTransform ApplyPoseSmoothing(int32 BoneIndex, const FTransform& RawTransform);

  FName mSubjectName; // This instance's LiveLink subject

  std::chrono::steady_clock::time_point mPreviousFrameArrivalTime;
  std::atomic<double> mConnectionTimeoutMs;

  std::atomic<bool> mRejectDuplicateAndOutOfOrderFrames;
  std::atomic<bool> mUsePacketTimestampRecovery;

  std::atomic<bool> mEnablePoseSmoothing;
  std::atomic<float> mRotationSmoothingStrength;
  std::atomic<float> mTranslationSmoothingStrength;

  std::atomic<bool> mEnableNetworkSimulation;
  std::atomic<int32> mSimulationSeed;
  std::atomic<float> mRandomPacketLossPercent;
  std::atomic<int32> mBurstLossIntervalFrames;
  std::atomic<int32> mBurstLossLengthFrames;
  std::atomic<float> mMaximumJitterMs;
  std::atomic<float> mDuplicatePacketPercent;
  std::atomic<float> mReorderPacketPercent;
  std::atomic<float> mReorderExtraDelayMs;

  struct FPendingSimulatedPacket
  {
    TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> Data;
    std::chrono::steady_clock::time_point ReleaseTime;
    uint64 Sequence = 0;
  };

  FMocopiNetworkSimulator mNetworkSimulator;
  TArray<FPendingSimulatedPacket> mPendingSimulatedPackets;
  uint64 mNextSimulatedPacketSequence;
  bool mWasNetworkSimulationEnabled;

  TArray<FTransform> mPreviousSmoothedTransforms;
  bool mHasPreviousSmoothedFrame;

  bool mHasPacketTimestampBase;
  double mPacketTimestampBase;
  double mEngineTimeBase;

  int32 mLastFrameId;
  bool mHasLastFrameId;
  bool mPendingStreamReset;
  bool mTimedOut;

  std::atomic<uint64> mReceivedFrames;
  std::atomic<uint64> mEstimatedLostFrames;
  std::atomic<uint64> mRejectedFrames;
  std::atomic<uint64> mSimulatedDroppedPackets;
  std::atomic<uint64> mSimulatedDelayedPackets;
  std::atomic<uint64> mSimulatedDuplicatePackets;

  TWeakObjectPtr<UMocopiLiveLinkSourceSettings> mSettings;

  double mPreviousFrameTimestamp_ms; // Helps determine mocopi FPS. Temporary Fix. Will Receive FPS from the App in the future.
  int mCurrentMocopiFPS;

  bool mNeedsToProcessSkelDef;

};

