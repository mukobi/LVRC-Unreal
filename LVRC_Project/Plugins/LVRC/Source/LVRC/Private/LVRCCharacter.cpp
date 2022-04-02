// Copyright Epic Games, Inc. All Rights Reserved.

#include "LVRCCharacter.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/InputSettings.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "LVRCMovementComponent.h"
#include "MotionControllerComponent.h"
#include "XRMotionControllerBase.h" // for FXRMotionControllerBase::RightHandSourceId

DEFINE_LOG_CATEGORY_STATIC(LogFPChar, Warning, All);

//////////////////////////////////////////////////////////////////////////
// ALVRCCharacter

ALVRCCharacter::ALVRCCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<ULVRCMovementComponent>(CharacterMovementComponentName))
{
	VROrigin = CreateDefaultSubobject<USceneComponent>(TEXT("VR Origin"));
	VROrigin->SetupAttachment(RootComponent);
	MatchVROriginOffsetToCapsuleHalfHeight();

	VRCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("VR Camera"));
	VRCamera->SetupAttachment(VROrigin);
}

ULVRCMovementComponent* ALVRCCharacter::GetLVRCMovementComponent() const
{
	return Cast<ULVRCMovementComponent>(GetMovementComponent());
}

void ALVRCCharacter::MatchVROriginOffsetToCapsuleHalfHeight()
{
	const FVector RelativeLocation = VROrigin->GetRelativeLocation();
	VROrigin->SetRelativeLocation(FVector(RelativeLocation.X, RelativeLocation.Y,
	                                      -GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
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
