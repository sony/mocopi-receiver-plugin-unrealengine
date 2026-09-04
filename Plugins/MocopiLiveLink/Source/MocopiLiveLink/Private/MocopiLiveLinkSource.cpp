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

#include "MocopiLiveLinkSource.h"
#include "MocopiLiveLinkSourceSettings.h"
#include "MocopiLog.h"

#include "ILiveLinkClient.h"
#include "Async/Async.h"
#include "HAL/PlatformTime.h"
#include "HAL/RunnableThread.h"
#include "Common/UdpSocketBuilder.h"
#include "Roles/LiveLinkAnimationRole.h"
#include "Misc/MessageDialog.h"
#include "UObject/UnrealType.h"

#include "SMocopiLiveLinkSourceFactory.h"

#define LOCTEXT_NAMESPACE "MocopiLiveLinkModule"

const uint16 MOCOPI_MAX_PACKET_SIZE = 4096;
const int32 DEFAULT_SOCKET_RECEIVE_BUFFER_SIZE = 512 * 1024;
const double DEFAULT_CONNECTION_TIMEOUT_MS = 2000.0;
const FString DEFAULT_UDP_THREAD_NAME = "MocopiUdpThread";

namespace
{
bool IsPresetControlledProperty(FName PropertyName)
{
  return PropertyName == GET_MEMBER_NAME_CHECKED(ULiveLinkSourceSettings, Mode)
    || PropertyName == GET_MEMBER_NAME_CHECKED(FLiveLinkSourceBufferManagementSettings, EngineTimeOffset)
    || PropertyName == GET_MEMBER_NAME_CHECKED(FLiveLinkSourceBufferManagementSettings, MaxNumberOfFrameToBuffered)
    || PropertyName == GET_MEMBER_NAME_CHECKED(UMocopiLiveLinkSourceSettings, UdpReceiveBufferSizeKB)
    || PropertyName == GET_MEMBER_NAME_CHECKED(UMocopiLiveLinkSourceSettings, ConnectionTimeoutSeconds)
    || PropertyName == GET_MEMBER_NAME_CHECKED(UMocopiLiveLinkSourceSettings, bRejectDuplicateAndOutOfOrderFrames)
    || PropertyName == GET_MEMBER_NAME_CHECKED(UMocopiLiveLinkSourceSettings, bUsePacketTimestampRecovery)
    || PropertyName == GET_MEMBER_NAME_CHECKED(UMocopiLiveLinkSourceSettings, bEnablePoseSmoothing)
    || PropertyName == GET_MEMBER_NAME_CHECKED(UMocopiLiveLinkSourceSettings, RotationSmoothingStrength)
    || PropertyName == GET_MEMBER_NAME_CHECKED(UMocopiLiveLinkSourceSettings, TranslationSmoothingStrength);
}
}

FMocopiLiveLinkSource::FMocopiLiveLinkSource(uint16 inputPort, FName subjectName) :
  mUdpThreadName(""),
  mUdpThread(nullptr),
  mIsStopping(false),
  mIsThreadFinished(true),
  mSocket(nullptr),
  mSocketSubSystem(nullptr),
  mInputPort(inputPort),
  mClient(nullptr),
  mSourceGuid(),
  mSubjectName(subjectName),
  mConnectionTimeoutMs(DEFAULT_CONNECTION_TIMEOUT_MS),
  mRejectDuplicateAndOutOfOrderFrames(true),
  mUsePacketTimestampRecovery(true),
  mEnablePoseSmoothing(true),
  mRotationSmoothingStrength(0.20f),
  mTranslationSmoothingStrength(0.15f),
  mEnableNetworkSimulation(false),
  mSimulationSeed(1337),
  mRandomPacketLossPercent(0.0f),
  mBurstLossIntervalFrames(0),
  mBurstLossLengthFrames(3),
  mMaximumJitterMs(0.0f),
  mDuplicatePacketPercent(0.0f),
  mReorderPacketPercent(0.0f),
  mReorderExtraDelayMs(40.0f),
  mNextSimulatedPacketSequence(0),
  mWasNetworkSimulationEnabled(false),
  mHasPreviousSmoothedFrame(false),
  mHasPacketTimestampBase(false),
  mPacketTimestampBase(0.0),
  mEngineTimeBase(0.0),
  mLastFrameId(0),
  mHasLastFrameId(false),
  mPendingStreamReset(false),
  mTimedOut(false),
  mReceivedFrames(0),
  mEstimatedLostFrames(0),
  mRejectedFrames(0),
  mSimulatedDroppedPackets(0),
  mSimulatedDelayedPackets(0),
  mSimulatedDuplicatePackets(0),
  mPreviousFrameTimestamp_ms(0),
  mCurrentMocopiFPS(50),
  mNeedsToProcessSkelDef(true)
{
  SMocopiLiveLinkSourceFactory::AddSubject(mInputPort, mSubjectName);
}

FMocopiLiveLinkSource::~FMocopiLiveLinkSource()
{
  bool bIsReadyToShutdown = false;
  while (!bIsReadyToShutdown)
  {
    bIsReadyToShutdown = ShutdownThreadAndSocket();
  }

  SMocopiLiveLinkSourceFactory::RemoveSubject(mInputPort);
}

void FMocopiLiveLinkSource::OpenConnection()
{
  mSocket = FUdpSocketBuilder(TEXT("Mocopi UDP Socket"))
    .AsBlocking()
    .AsReusable()
    .BoundToAddress(FIPv4Address::Any)
    .BoundToPort(mInputPort)
    .WithReceiveBufferSize(DEFAULT_SOCKET_RECEIVE_BUFFER_SIZE);

  mRecvBuffer.SetNumUninitialized(MOCOPI_MAX_PACKET_SIZE);

  if ((mSocket != nullptr) && (mSocket->GetSocketType() == SOCKTYPE_Datagram))
  {
    StartUdpThread();
  }
  else
  {
    FText errorMsg = LOCTEXT("socketInitFailed", "Socket Initialization Failed");
    UE_LOG(LogMocopiLiveLink, Error, TEXT("%s"), *(errorMsg.ToString()));

    FText portInUseTitle = LOCTEXT("errorTitleUsedPort", "Error: Port in Use");
#ifdef USE_DEPRECATED_DEBUGF
    FMessageDialog::Debugf(errorMsg, &portInUseTitle);
#else
    FMessageDialog::Debugf(errorMsg, portInUseTitle);
#endif
  }
}

void FMocopiLiveLinkSource::ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid)
{
  mClient = InClient;
  mSourceGuid = InSourceGuid;

  OpenConnection();
}

void FMocopiLiveLinkSource::InitializeSettings(ULiveLinkSourceSettings* Settings)
{
  UMocopiLiveLinkSourceSettings* MocopiSettings = Cast<UMocopiLiveLinkSourceSettings>(Settings);
  mSettings = MocopiSettings;
  ApplySettings(MocopiSettings);
}

void FMocopiLiveLinkSource::Update()
{
  if (UMocopiLiveLinkSourceSettings* Settings = mSettings.Get())
  {
    Settings->ReceivedFrames = mReceivedFrames.load();
    Settings->EstimatedLostFrames = mEstimatedLostFrames.load();
    Settings->RejectedFrames = mRejectedFrames.load();
    Settings->SimulatedDroppedPackets = mSimulatedDroppedPackets.load();
    Settings->SimulatedDelayedPackets = mSimulatedDelayedPackets.load();
    Settings->SimulatedDuplicatePackets = mSimulatedDuplicatePackets.load();
  }
}

TSubclassOf<ULiveLinkSourceSettings> FMocopiLiveLinkSource::GetSettingsClass() const
{
  return UMocopiLiveLinkSourceSettings::StaticClass();
}

void FMocopiLiveLinkSource::OnSettingsChanged(ULiveLinkSourceSettings* Settings, const FPropertyChangedEvent& PropertyChangedEvent)
{
  UMocopiLiveLinkSourceSettings* MocopiSettings = Cast<UMocopiLiveLinkSourceSettings>(Settings);
  if (MocopiSettings == nullptr)
  {
    return;
  }

  const FName PropertyName = PropertyChangedEvent.GetPropertyName();
  if (PropertyName == GET_MEMBER_NAME_CHECKED(UMocopiLiveLinkSourceSettings, ReliabilityPreset))
  {
    MocopiSettings->ApplyReliabilityPreset();
  }
  else if (IsPresetControlledProperty(PropertyName))
  {
    MocopiSettings->ReliabilityPreset = EMocopiReliabilityPreset::Custom;
  }

  ApplySettings(MocopiSettings);
}

void FMocopiLiveLinkSource::ApplySettings(UMocopiLiveLinkSourceSettings* Settings)
{
  if (Settings == nullptr)
  {
    return;
  }

  mConnectionTimeoutMs.store(FMath::Max(400.0, static_cast<double>(Settings->ConnectionTimeoutSeconds) * 1000.0));
  mRejectDuplicateAndOutOfOrderFrames.store(Settings->bRejectDuplicateAndOutOfOrderFrames);
  mUsePacketTimestampRecovery.store(Settings->bUsePacketTimestampRecovery);
  mEnablePoseSmoothing.store(Settings->bEnablePoseSmoothing);
  mRotationSmoothingStrength.store(FMath::Clamp(Settings->RotationSmoothingStrength, 0.0f, 0.95f));
  mTranslationSmoothingStrength.store(FMath::Clamp(Settings->TranslationSmoothingStrength, 0.0f, 0.95f));

#if UE_BUILD_SHIPPING
  mEnableNetworkSimulation.store(false);
#else
  mEnableNetworkSimulation.store(Settings->bEnableNetworkSimulation);
#endif
  mSimulationSeed.store(Settings->SimulationSeed);
  mRandomPacketLossPercent.store(FMath::Clamp(Settings->RandomPacketLossPercent, 0.0f, 50.0f));
  mBurstLossIntervalFrames.store(FMath::Max(0, Settings->BurstLossIntervalFrames));
  mBurstLossLengthFrames.store(FMath::Clamp(Settings->BurstLossLengthFrames, 1, 50));
  mMaximumJitterMs.store(FMath::Clamp(Settings->MaximumJitterMs, 0.0f, 500.0f));
  mDuplicatePacketPercent.store(FMath::Clamp(Settings->DuplicatePacketPercent, 0.0f, 50.0f));
  mReorderPacketPercent.store(FMath::Clamp(Settings->ReorderPacketPercent, 0.0f, 50.0f));
  mReorderExtraDelayMs.store(FMath::Clamp(Settings->ReorderExtraDelayMs, 1.0f, 500.0f));

  if (mSocket != nullptr)
  {
    const int32 RequestedBufferSize = FMath::Clamp(Settings->UdpReceiveBufferSizeKB, 64, 4096) * 1024;
    int32 ActualBufferSize = 0;
    if (!mSocket->SetReceiveBufferSize(RequestedBufferSize, ActualBufferSize))
    {
      UE_LOG(LogMocopiLiveLink, Warning, TEXT("Unable to set UDP receive buffer to %d bytes"), RequestedBufferSize);
    }
    else
    {
      UE_LOG(LogMocopiLiveLink, Log, TEXT("UDP receive buffer set to %d bytes"), ActualBufferSize);
    }
  }
}

void FMocopiLiveLinkSource::StartUdpThread()
{
  if ((mSocket == nullptr) || (mSocket->GetSocketType() != SOCKTYPE_Datagram))
  {
    FText error = LOCTEXT("errorInvalidSocket", "Trying to start UDP thread with an invalid / uninitialized socket");
    UE_LOG(LogMocopiLiveLink, Error, TEXT("%s"), *(error.ToString()));
    return;
  }

  FString udpThreadName = DEFAULT_UDP_THREAD_NAME + mInputPort;

  mSocketSubSystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
  mPreviousFrameArrivalTime = std::chrono::steady_clock::now();
  mUdpThreadName = udpThreadName;
  mUdpThreadName.AppendInt(FAsyncThreadIndex::GetNext());

  uint32 threadStackSize = 128 * 1024; // Default taken from Unreal's JSONLiveLink Example
  mUdpThread = FRunnableThread::Create(this, *mUdpThreadName, threadStackSize, TPri_AboveNormal, FPlatformAffinity::GetPoolThreadMask());
}

void FMocopiLiveLinkSource::Stop()
{
  mIsStopping = true;
}

// FRunnable Interface - Runs the Udp thread
uint32 FMocopiLiveLinkSource::Run()
{
  using clock = std::chrono::steady_clock;

  TSharedRef<FInternetAddr> sender = mSocketSubSystem->CreateInternetAddr();

  mIsThreadFinished = false;

  while (!mIsStopping)
  {
    const bool bSimulationEnabled = mEnableNetworkSimulation.load();
    if (mWasNetworkSimulationEnabled && !bSimulationEnabled)
    {
      mNetworkSimulator.Configure(GetNetworkSimulationConfig());
    }
    mWasNetworkSimulationEnabled = bSimulationEnabled;
    ProcessPendingSimulatedPackets(!bSimulationEnabled);

    uint32 size;

    if (mSocket->HasPendingData(size))
    {
      int32 bytesRead = 0;

      if (mSocket->RecvFrom(mRecvBuffer.GetData(), mRecvBuffer.Num(), bytesRead, *sender))
      {
        if (bytesRead > 0)
        {
          TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> receivedData = MakeShareable(new TArray<uint8>());
          receivedData->SetNumUninitialized(bytesRead);
          memcpy(receivedData->GetData(), mRecvBuffer.GetData(), bytesRead);
          QueueOrProcessReceivedData(receivedData);
        }

        mPreviousFrameArrivalTime = clock::now();
        mTimedOut = false;
      }
    }
    else
    {
      std::chrono::duration<double, std::milli> duration_ms = clock::now() - mPreviousFrameArrivalTime;
      if (!mTimedOut && duration_ms.count() > mConnectionTimeoutMs.load())
      {
        // Reset variable for future data stream
        mNeedsToProcessSkelDef = true;
        ResetStreamState();
        mTimedOut = true;
      }
    }
  }

  mIsThreadFinished = true;

  return 0;
}

void FMocopiLiveLinkSource::UpdateFrameData(FLiveLinkAnimationFrameData& outFrame)
{
  const size_t numBones = mDataHandler.GetNumBones();

  outFrame.Transforms.SetNumUninitialized(numBones);
  if (mPreviousSmoothedTransforms.Num() != static_cast<int32>(numBones))
  {
    mPreviousSmoothedTransforms.SetNumUninitialized(static_cast<int32>(numBones));
    mHasPreviousSmoothedFrame = false;
  }

  // Set Root bone translation individually
  const MocopiBoneData root = mDataHandler.GetBoneInfoByIndex(0);
  FVector boneTranslation = FVector(root.translate[0], root.translate[1], root.translate[2]);
  FQuat boneRotation = FQuat(root.rotate[0], root.rotate[1], root.rotate[2], root.rotate[3]);
  outFrame.Transforms[0] = ApplyPoseSmoothing(0, FTransform(boneRotation, boneTranslation));

  // Then Set values for rest of the bones
  for (size_t i = 1; i < numBones; i++)
  {
    const MocopiBoneData bone = mDataHandler.GetBoneInfoByIndex(i);
    boneTranslation = FVector(bone.defaultTranslate[0], bone.defaultTranslate[1], bone.defaultTranslate[2]);
    boneRotation = FQuat(bone.rotate[0], bone.rotate[1], bone.rotate[2], bone.rotate[3]);

    outFrame.Transforms[i] = ApplyPoseSmoothing(static_cast<int32>(i), FTransform(boneRotation, boneTranslation));
  }

  mHasPreviousSmoothedFrame = true;

  // Set Timecode information on this frame
  MocopiFrameMetaData metaData = mDataHandler.GetFrameMetaData();

  // Convert mocopi packet timestamps to the engine-time domain using deltas.
  // The raw timestamp is not guaranteed to share Live Link's clock origin.
  if (mUsePacketTimestampRecovery.load() && FMath::IsFinite(metaData.timeStamp))
  {
    outFrame.WorldTime = FLiveLinkWorldTime(GetPacketTimestampWorldTime(static_cast<double>(metaData.timeStamp)), 0.0);
  }

  FQualifiedFrameTime mocopiFrameTime = GetQualifiedFrameTime(metaData);
  outFrame.MetaData.SceneTime = mocopiFrameTime;
  outFrame.FrameId = metaData.frameId;

}

FMocopiNetworkSimulationConfig FMocopiLiveLinkSource::GetNetworkSimulationConfig() const
{
  FMocopiNetworkSimulationConfig Config;
  Config.bEnabled = mEnableNetworkSimulation.load();
  Config.Seed = mSimulationSeed.load();
  Config.RandomLossPercent = mRandomPacketLossPercent.load();
  Config.BurstIntervalFrames = mBurstLossIntervalFrames.load();
  Config.BurstLengthFrames = mBurstLossLengthFrames.load();
  Config.MaxJitterMs = mMaximumJitterMs.load();
  Config.DuplicatePercent = mDuplicatePacketPercent.load();
  Config.ReorderPercent = mReorderPacketPercent.load();
  Config.ReorderExtraDelayMs = mReorderExtraDelayMs.load();
  return Config;
}

void FMocopiLiveLinkSource::QueueOrProcessReceivedData(TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> ReceivedData)
{
  std::byte* DataBuffer = reinterpret_cast<std::byte*>(ReceivedData->GetData());
  const int32 BufferSize = ReceivedData->Num();

  // Skeleton definitions are never impaired. The panel targets the motion
  // stream so an aggressive test cannot randomly prevent source startup.
  if (!mEnableNetworkSimulation.load() || !mDataHandler.IsFrameData(DataBuffer, BufferSize))
  {
    HandleReceivedData(ReceivedData);
    return;
  }

  mNetworkSimulator.Configure(GetNetworkSimulationConfig());
  const FMocopiNetworkSimulationDecision Decision = mNetworkSimulator.DecideForFrame();
  if (Decision.bDrop)
  {
    mSimulatedDroppedPackets.fetch_add(1);
    return;
  }

  const auto Now = std::chrono::steady_clock::now();
  const auto Delay = std::chrono::duration<double, std::milli>(Decision.DelayMs);
  mPendingSimulatedPackets.Add({ReceivedData, Now + std::chrono::duration_cast<std::chrono::steady_clock::duration>(Delay), mNextSimulatedPacketSequence++});

  if (Decision.DelayMs > 0.0)
  {
    mSimulatedDelayedPackets.fetch_add(1);
  }

  if (Decision.bDuplicate)
  {
    const auto DuplicateDelay = std::chrono::duration<double, std::milli>(Decision.DelayMs + 1.0);
    mPendingSimulatedPackets.Add({ReceivedData, Now + std::chrono::duration_cast<std::chrono::steady_clock::duration>(DuplicateDelay), mNextSimulatedPacketSequence++});
    mSimulatedDuplicatePackets.fetch_add(1);
  }

  ProcessPendingSimulatedPackets();
}

void FMocopiLiveLinkSource::ProcessPendingSimulatedPackets(bool bFlushAll)
{
  const auto Now = std::chrono::steady_clock::now();

  while (!mPendingSimulatedPackets.IsEmpty())
  {
    int32 EarliestIndex = 0;
    for (int32 Index = 1; Index < mPendingSimulatedPackets.Num(); ++Index)
    {
      const FPendingSimulatedPacket& Candidate = mPendingSimulatedPackets[Index];
      const FPendingSimulatedPacket& Earliest = mPendingSimulatedPackets[EarliestIndex];
      if (Candidate.ReleaseTime < Earliest.ReleaseTime
        || (Candidate.ReleaseTime == Earliest.ReleaseTime && Candidate.Sequence < Earliest.Sequence))
      {
        EarliestIndex = Index;
      }
    }

    if (!bFlushAll && mPendingSimulatedPackets[EarliestIndex].ReleaseTime > Now)
    {
      return;
    }

    TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> Data = mPendingSimulatedPackets[EarliestIndex].Data;
    mPendingSimulatedPackets.RemoveAtSwap(EarliestIndex, 1, EAllowShrinking::No);
    HandleReceivedData(Data);
  }
}

FTransform FMocopiLiveLinkSource::ApplyPoseSmoothing(int32 BoneIndex, const FTransform& RawTransform)
{
  FTransform SmoothedTransform = RawTransform;

  if (mEnablePoseSmoothing.load() && mHasPreviousSmoothedFrame && mPreviousSmoothedTransforms.IsValidIndex(BoneIndex))
  {
    const FTransform& PreviousTransform = mPreviousSmoothedTransforms[BoneIndex];
    const float RotationAlpha = 1.0f - mRotationSmoothingStrength.load();
    const float TranslationAlpha = 1.0f - mTranslationSmoothingStrength.load();

    const FQuat SmoothedRotation = FQuat::Slerp(PreviousTransform.GetRotation(), RawTransform.GetRotation(), RotationAlpha).GetNormalized();
    const FVector SmoothedTranslation = FMath::Lerp(PreviousTransform.GetTranslation(), RawTransform.GetTranslation(), TranslationAlpha);

    SmoothedTransform.SetRotation(SmoothedRotation);
    SmoothedTransform.SetTranslation(SmoothedTranslation);
  }

  mPreviousSmoothedTransforms[BoneIndex] = SmoothedTransform;
  return SmoothedTransform;
}

void FMocopiLiveLinkSource::ResetStreamState()
{
  mHasPreviousSmoothedFrame = false;
  mPreviousSmoothedTransforms.Reset();
  mHasPacketTimestampBase = false;
  mPacketTimestampBase = 0.0;
  mEngineTimeBase = 0.0;
  mHasLastFrameId = false;
  mPendingStreamReset = false;
  mPreviousFrameTimestamp_ms = 0;
}

double FMocopiLiveLinkSource::GetPacketTimestampWorldTime(double PacketTimestamp)
{
  if (!mHasPacketTimestampBase || PacketTimestamp < mPacketTimestampBase)
  {
    mHasPacketTimestampBase = true;
    mPacketTimestampBase = PacketTimestamp;
    mEngineTimeBase = FPlatformTime::Seconds();
  }

  return mEngineTimeBase + (PacketTimestamp - mPacketTimestampBase);
}

bool FMocopiLiveLinkSource::ShouldAcceptFrame(int32 FrameId, double PacketTimestamp)
{
  if (!mHasLastFrameId)
  {
    mLastFrameId = FrameId;
    mHasLastFrameId = true;
    mReceivedFrames.fetch_add(1);
    return true;
  }

  const int64 FrameDelta = static_cast<int64>(FrameId) - static_cast<int64>(mLastFrameId);
  if (FrameDelta < 0 && FMath::IsFinite(PacketTimestamp) && mHasPacketTimestampBase && PacketTimestamp <= mPacketTimestampBase)
  {
    ResetStreamState();
    mLastFrameId = FrameId;
    mHasLastFrameId = true;
    mReceivedFrames.fetch_add(1);
    return true;
  }

  if (FrameDelta == 0)
  {
    if (mRejectDuplicateAndOutOfOrderFrames.load())
    {
      mRejectedFrames.fetch_add(1);
      return false;
    }

    mReceivedFrames.fetch_add(1);
    return true;
  }

  if (FrameDelta < 0)
  {
    // A small negative delta is a late packet. A large jump is treated as a
    // new sender session whose frame counter restarted.
    if (FrameDelta > -1000)
    {
      mRejectedFrames.fetch_add(1);
      if (mRejectDuplicateAndOutOfOrderFrames.load())
      {
        return false;
      }

      mReceivedFrames.fetch_add(1);
      return true;
    }

    ResetStreamState();
    mLastFrameId = FrameId;
    mHasLastFrameId = true;
    mReceivedFrames.fetch_add(1);
    return true;
  }

  if (FrameDelta > 1 && FrameDelta < 10000)
  {
    mEstimatedLostFrames.fetch_add(static_cast<uint64>(FrameDelta - 1));
  }

  mLastFrameId = FrameId;
  mReceivedFrames.fetch_add(1);
  return true;
}

FQualifiedFrameTime FMocopiLiveLinkSource::GetQualifiedFrameTime(MocopiFrameMetaData& frameMetaData)
{
  int frameRate = mCurrentMocopiFPS;
  double utcTimeDouble = frameMetaData.utcTime;
  double ms = utcTimeDouble - (long)utcTimeDouble;

  // Temporary Fix for determining mocopi stream FPS.
  // TODO:: Delete this when implemented on App Side.
  double interval_ms = ms - mPreviousFrameTimestamp_ms;
  if (interval_ms > 0)
  {
    if (interval_ms >= .033)
    {
      frameRate = 30;
    }
    else if (interval_ms >= .019)
    {
      frameRate = 50;
    }
    else if (interval_ms >= .016)
    {
      frameRate = 60;
    }

    mCurrentMocopiFPS = frameRate;
  }

  mPreviousFrameTimestamp_ms = ms;

  FTimecode mocopiTimecode;

  if (frameMetaData.timecode.isValid())
  {
    // If any timecode exist use timecode value
    mocopiTimecode.Hours = (int32)frameMetaData.timecode.hour;
    mocopiTimecode.Minutes = (int32)frameMetaData.timecode.min;
    mocopiTimecode.Seconds = (int32)frameMetaData.timecode.sec;
    mocopiTimecode.Frames = (int32)frameMetaData.timecode.frame;
    mocopiTimecode.bDropFrameFormat = frameMetaData.timecode.dropFrame;
  }
  else 
  {

    // Else use UTC time as timecode. Convert to local time first.
    auto timePoint = std::chrono::system_clock::from_time_t(static_cast<std::time_t>(utcTimeDouble));
    time_t time = std::chrono::system_clock::to_time_t(timePoint);
    std::tm localTime;
#ifdef _WIN64
    localtime_s(&localTime, &time);
#elif __APPLE__
    localtime_r(&time, &localTime);
#endif

    mocopiTimecode.Hours = localTime.tm_hour;
    mocopiTimecode.Minutes = localTime.tm_min;
    mocopiTimecode.Seconds = localTime.tm_sec;

    int frame = round(ms * frameRate);
    mocopiTimecode.Frames = frame;

    mocopiTimecode.bDropFrameFormat = false;
  }

  FFrameRate mocopiFrameRate;
  mocopiFrameRate.Denominator = 1;

  if (frameMetaData.timecode.frameRate > 0) 
  {
    mocopiFrameRate.Numerator = (int32)frameMetaData.timecode.frameRate;
  }
  else 
  {
    mocopiFrameRate.Numerator = frameRate;
  }

#ifdef DEBUG
  UE_LOG(LogMocopiLiveLink, Log, TEXT("timecode  %s, frameRate: %f"), *(mocopiTimecode.ToString()), mocopiFrameRate.AsDecimal());
#endif 

  return FQualifiedFrameTime(mocopiTimecode, mocopiFrameRate);
}

void FMocopiLiveLinkSource::HandleReceivedData(TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> receivedData)
{
  std::byte* dataBuffer = reinterpret_cast<std::byte*>(receivedData->GetData());
  int bufferSize = receivedData->Num();

  if (mDataHandler.IsFrameData(dataBuffer, bufferSize))
  {
    if (mNeedsToProcessSkelDef)
    {
      // Don't process data if we still haven't received a SkelDef packet
      return;
    }

    if (mPendingStreamReset)
    {
      ResetStreamState();
    }

    mDataHandler.ProcessFrameData(dataBuffer, bufferSize);
    const MocopiFrameMetaData MetaData = mDataHandler.GetFrameMetaData();
    if (!ShouldAcceptFrame(MetaData.frameId, static_cast<double>(MetaData.timeStamp)))
    {
      return;
    }
  }
  else if (mDataHandler.IsSkeletonDefinition(dataBuffer, bufferSize))
  {
    if (!mNeedsToProcessSkelDef)
    {
      mPendingStreamReset = true;
      return;
    }

    mDataHandler.ProcessSkeletonDefinitionData(dataBuffer, bufferSize);

    DefineNewMocopiSubject();

    mNeedsToProcessSkelDef = false;
    ResetStreamState();
    return;
  }
  else
  {
    // Handle Unrecognized Data
    FText warningMessage = LOCTEXT("receivedUnrecognizedData", "MocopiLiveLink plugin received unrecognized data");
    UE_LOG(LogMocopiLiveLink, Warning, TEXT("%s"), *(warningMessage.ToString()));
    return;
  }

  FLiveLinkFrameDataStruct frameDataStruct = FLiveLinkFrameDataStruct(FLiveLinkAnimationFrameData::StaticStruct());
  FLiveLinkAnimationFrameData& frameData = *frameDataStruct.Cast<FLiveLinkAnimationFrameData>();

  UpdateFrameData(frameData);

  FLiveLinkSubjectKey subjectKey = { mSourceGuid, mSubjectName };
  mClient->PushSubjectFrameData_AnyThread(subjectKey, MoveTemp(frameDataStruct));
}

void FMocopiLiveLinkSource::DefineNewMocopiSubject()
{
  const int numBones = mDataHandler.GetNumBones();

  FLiveLinkStaticDataStruct staticDataStruct = FLiveLinkStaticDataStruct(FLiveLinkSkeletonStaticData::StaticStruct());
  FLiveLinkSkeletonStaticData& skeletonStaticData = *staticDataStruct.Cast<FLiveLinkSkeletonStaticData>();
  skeletonStaticData.BoneNames.SetNumUninitialized(numBones);
  skeletonStaticData.BoneParents.SetNumUninitialized(numBones);

  for (int boneIndex = 0; boneIndex < numBones; boneIndex++)
  {
    const MocopiBoneData boneData = mDataHandler.GetBoneInfoByIndex(boneIndex);
    skeletonStaticData.BoneNames[boneIndex] = FName(boneData.jointName);
    skeletonStaticData.BoneParents[boneIndex] = boneData.parentIndex;
  }

  mClient->PushSubjectStaticData_AnyThread({ mSourceGuid, mSubjectName }, ULiveLinkAnimationRole::StaticClass(), MoveTemp(staticDataStruct));
}

bool FMocopiLiveLinkSource::IsSourceStillValid() const
{
  // Source is valid if we have a valid thread and socket
  bool isSourceValid = !mIsStopping && (mUdpThread != nullptr) && (mSocket != nullptr);
  return isSourceValid;
}

bool FMocopiLiveLinkSource::RequestSourceShutdown()
{
  return ShutdownThreadAndSocket();
}

bool FMocopiLiveLinkSource::ShutdownThreadAndSocket()
{
  if (!mIsStopping)
  {
    Stop();
  }

  if (!mIsThreadFinished)
  {
    return false;
  }

  if (mUdpThread != nullptr)
  {
    delete mUdpThread;
    mUdpThread = nullptr;
  }
  if (mSocket != nullptr)
  {
    mSocket->Close();
    mSocketSubSystem->DestroySocket(mSocket);
    mSocket = nullptr;
  }

  return true;
}

FText FMocopiLiveLinkSource::GetSourceType() const
{
  return FText(LOCTEXT("sourceType", "Mocopi LiveLink"));
}

FText FMocopiLiveLinkSource::GetSourceMachineName() const
{
  return FText(LOCTEXT("sourceMachineName", "Mocopi LiveLink"));
}

FText FMocopiLiveLinkSource::GetSourceStatus() const
{
  FText portString = FText::FromString(FString::FromInt(mInputPort)); // Need to do this to not have a comma in port number 
  FText receivedString = FText::AsNumber(mReceivedFrames.load());
  FText lostString = FText::AsNumber(mEstimatedLostFrames.load());
  FText sourceStatus = FText::Format(
    LOCTEXT("SourceStatusReceiving", "Listening to port {0} | Frames {1} | Estimated lost {2}"),
    portString,
    receivedString,
    lostString);
  return sourceStatus;
}

#undef LOCTEXT_NAMESPACE
