// Copyright Epic Games, Inc. All Rights Reserved.

#include "LVRCPawn.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/InputSettings.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "LVRCMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "MotionControllerComponent.h"
#include "XRMotionControllerBase.h" // for FXRMotionControllerBase::RightHandSourceId

DEFINE_LOG_CATEGORY_STATIC(LogFPChar, Warning, All);

//////////////////////////////////////////////////////////////////////////
// ALVRCPawn

ALVRCPawn::ALVRCPawn()
{
	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>("CapsuleCollision");
	CapsuleComponent->InitCapsuleSize(34.0f, 88.0f);
	CapsuleComponent->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);

	CapsuleComponent->CanCharacterStepUpOn = ECB_No;
	CapsuleComponent->SetShouldUpdatePhysicsVolume(true);
	CapsuleComponent->SetCanEverAffectNavigation(false);
	CapsuleComponent->bDynamicObstacle = true;
	RootComponent = CapsuleComponent;

	MovementComponent = CreateDefaultSubobject<ULVRCMovementComponent>(TEXT("LVRCMovementComponent"));
	MovementComponent->UpdatedComponent = CapsuleComponent;
}

//////////////////////////////////////////////////////////////////////////
// Input

void ALVRCPawn::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// set up gameplay key bindings
	check(PlayerInputComponent);

	// PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &ALVRCPawn::OnResetVR);
}

void ALVRCPawn::OnResetVR()
{
	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
}
