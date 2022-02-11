// Copyright Epic Games, Inc. All Rights Reserved.

#include "LVRCCharacter.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/InputSettings.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "MotionControllerComponent.h"
#include "XRMotionControllerBase.h" // for FXRMotionControllerBase::RightHandSourceId

DEFINE_LOG_CATEGORY_STATIC(LogFPChar, Warning, All);

//////////////////////////////////////////////////////////////////////////
// ALVRCCharacter

ALVRCCharacter::ALVRCCharacter(const FObjectInitializer& ObjectInitializer)
	: ACharacter(ObjectInitializer)
{
	VROrigin = CreateDefaultSubobject<USceneComponent>(TEXT("VR Origin"));
	VROrigin->SetupAttachment(RootComponent);
	VROrigin->AddLocalOffset(FVector(0, 0, -GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
	
	VRCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("VR Camera"));
	VRCamera->SetupAttachment(VROrigin);
}

//////////////////////////////////////////////////////////////////////////
// Input

void ALVRCCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// set up gameplay key bindings
	check(PlayerInputComponent);

	// PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &ALVRCCharacter::OnResetVR);
}

void ALVRCCharacter::OnResetVR()
{
	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
}
