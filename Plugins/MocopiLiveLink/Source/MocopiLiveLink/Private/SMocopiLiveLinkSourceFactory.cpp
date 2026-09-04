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

#include "SMocopiLiveLinkSourceFactory.h"

#include "Misc/MessageDialog.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Input/SButton.h"

#include "MocopiMotionFormat.h"

#define LOCTEXT_NAMESPACE "MocopiLiveLinkModule"

std::unordered_map<uint16, FName> SMocopiLiveLinkSourceFactory::mPortToSubjectNameMap;

SMocopiLiveLinkSourceFactory::~SMocopiLiveLinkSourceFactory()
{
}

void SMocopiLiveLinkSourceFactory::Construct(const FArguments& Args)
{
  CreateClicked = Args._OnCreateClicked;

  ChildSlot
  [
    SNew(SVerticalBox)
    + SVerticalBox::Slot()
      .Padding(5)
      .FillHeight(1.1)
      [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
          .HAlign(HAlign_Left)
          .FillWidth(0.6f)
          .VAlign(VAlign_Center)
        [
          SNew(STextBlock)
          .Text(LOCTEXT("InputPort", "Port Number"))
        ]
        + SHorizontalBox::Slot()
          .HAlign(HAlign_Right)
          .VAlign(VAlign_Center)
          .FillWidth(0.5f)
        [
          SAssignNew(mInputPortField, SEditableTextBox)
          .Text(FText::FromString(GetSuggestedPort()))
        ]
      ]

    + SVerticalBox::Slot()
      .FillHeight(1.4)
      [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
          .HAlign(HAlign_Left)
          .Padding(5)
          .AutoWidth()
          .VAlign(VAlign_Center)
          [
            SNew(STextBlock)
            .Text(LOCTEXT("SubjectName", "Subject Name"))
          ]

        + SHorizontalBox::Slot()
          .Padding(5,0)
          .HAlign(HAlign_Right)
          .AutoWidth()
          .VAlign(VAlign_Center)
        [
          SAssignNew(mSubjectNameField, SEditableTextBox)
          .Text(FText::FromString(GetSuggestedSkeletonName()))
        ]

      ]
      
    + SVerticalBox::Slot()
      .FillHeight(1.1)
      .Padding(5)
      .HAlign(HAlign_Center)
      [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
          [
            SNew(SButton)
            .OnClicked(this, &SMocopiLiveLinkSourceFactory::OnCreateClicked)
            [
              SNew(STextBlock)
              .Text(LOCTEXT("CreateSourceButtonLabel", "Create Mocopi Source"))
            ]
          ]
      ]
  ];
}

FReply SMocopiLiveLinkSourceFactory::OnCreateClicked()
{

  if (!MocopiMotionFormat::IsValid())
  {
    FText invalidMMFTitle = LOCTEXT("errorTitleInvalidMMF", "Error: mocopiMotionFormat Library");
#ifdef USE_DEPRECATED_DEBUGF
    FMessageDialog::Debugf(LOCTEXT("errorInvalidMMF", "Error loading the mocopiMotionFormat Library. Please reinstall the plugin to repair the files."), &invalidMMFTitle);
#else
    FMessageDialog::Debugf(LOCTEXT("errorInvalidMMF", "Error loading the mocopiMotionFormat Library. Please reinstall the plugin to repair the files."), invalidMMFTitle);
#endif
    return FReply::Unhandled();
  }

  TSharedPtr<SEditableTextBox> portEditableTextPin = mInputPortField.Pin();
  TSharedPtr<SEditableTextBox> subjectNameTextPin = mSubjectNameField.Pin();

  if (portEditableTextPin && subjectNameTextPin)
  {
    FText invalidPortTitle = LOCTEXT("errorTitleInvalidPort", "Error: Invalid Port");
    
    FString portString = portEditableTextPin->GetText().ToString();
    if (!portString.IsNumeric())
    {
#ifdef USE_DEPRECATED_DEBUGF
      FMessageDialog::Debugf(LOCTEXT("errorInvalidPort", "Invalid port.\nPlease enter a port number between 0 and 65535"), &invalidPortTitle);
#else
      FMessageDialog::Debugf(LOCTEXT("errorInvalidPort", "Invalid port.\nPlease enter a port number between 0 and 65535"), invalidPortTitle);
#endif
      return FReply::Unhandled();
    }

    uint64 portOverflowCheck = FCString::Atoi(*portString);
    if (portOverflowCheck > 65535)
    {
#ifdef USE_DEPRECATED_DEBUGF
      FMessageDialog::Debugf(LOCTEXT("errorInvalidPort", "Invalid port.\nPlease enter a port number between 0 and 65535"), &invalidPortTitle);
#else
      FMessageDialog::Debugf(LOCTEXT("errorInvalidPort", "Invalid port.\nPlease enter a port number between 0 and 65535"), invalidPortTitle);
#endif
      return FReply::Unhandled();
    }

    uint16 port = uint16(portOverflowCheck);
    if (mPortToSubjectNameMap.contains(port))
    {
      FText portInUseTitle = LOCTEXT("errorTitleUsedPort", "Error: Port in Use");
#ifdef USE_DEPRECATED_DEBUGF
      FMessageDialog::Debugf(LOCTEXT("errorUsedPort", "This port is already being used by another Mocopi LiveLink Source"), &portInUseTitle);
#else
      FMessageDialog::Debugf(LOCTEXT("errorUsedPort", "This port is already being used by another Mocopi LiveLink Source"), portInUseTitle);
#endif
      return FReply::Unhandled();
    }
   
    FString subjectNameStr = subjectNameTextPin->GetText().ToString();
    if (subjectNameStr.IsEmpty())
    {
      FText emptySubjectTitle = LOCTEXT("errorTitleNoSubjectName", "Error: Empty Subject Name");
#ifdef USE_DEPRECATED_DEBUGF
      FMessageDialog::Debugf(LOCTEXT("errorNoSubjectName", "Enter a subject name to create a Mocopi LiveLink Source"), &emptySubjectTitle);
#else
      FMessageDialog::Debugf(LOCTEXT("errorNoSubjectName", "Enter a subject name to create a Mocopi LiveLink Source"), emptySubjectTitle);
#endif

      return FReply::Unhandled();
    }

    FName subjectName = FName(*subjectNameStr);

    CreateClicked.ExecuteIfBound(port, subjectName);
    return FReply::Handled();
  }

  return FReply::Unhandled();
}

void SMocopiLiveLinkSourceFactory::RemoveSubject(uint16 inputPort)
{
  mPortToSubjectNameMap.erase(inputPort);
}

void SMocopiLiveLinkSourceFactory::AddSubject(uint16 inputPort, FName subjectName)
{
  mPortToSubjectNameMap[inputPort] = subjectName;
}

FString SMocopiLiveLinkSourceFactory::GetSuggestedSkeletonName()
{
  FString suggestedName = DEFAULT_SKELETON_NAME;

  int count = mPortToSubjectNameMap.size();
  if (count)
  {
    int humanReadableCount = count + 1;
    suggestedName += FString::FromInt(humanReadableCount);
  }

  return suggestedName;
}

FString SMocopiLiveLinkSourceFactory::GetSuggestedPort()
{
  int port = DEFAULT_MOCOPI_PORT;

  int count = mPortToSubjectNameMap.size();
  if (count)
  {
    port += count;
  }

  return FString::FromInt(port);
}

#undef LOCTEXT_NAMESPACE
