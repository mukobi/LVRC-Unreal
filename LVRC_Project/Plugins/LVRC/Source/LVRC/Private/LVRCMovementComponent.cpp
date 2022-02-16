// Fill out your copyright notice in the Description page of Project Settings.


#include "LVRCMovementComponent.h"

#include "HeadMountedDisplayFunctionLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"


// Sets default values for this component's properties
ULVRCMovementComponent::ULVRCMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// Called when the game starts
void ULVRCMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	
	LVRCCharacterOwner = Cast<ALVRCCharacter>(PawnOwner);
}


// Called every frame
void ULVRCMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                           FActorComponentTickFunction* ThisTickFunction)
{
	// First, check the input vector to detect a start to continuous locomotion
	const FVector InputVector = GetPendingInputVector();
	bIsPerformingContinuousLocomotion = !FMath::IsNearlyZero(InputVector.SizeSquared());

	if (bIsPerformingContinuousLocomotion)
	{
		UpdateCapsuleHeightToHMD();

		if (FMath::IsNearlyZero(PreviousTickInputVector.SizeSquared()))
		{
			BeginContinuousLocomotion();
		}

		UpdateCapsulePositionToHMD();
	}

	PreviousTickInputVector = InputVector;


	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void ULVRCMovementComponent::UpdateCapsulePositionToHMD() const
{
	FVector CapsuleToHMD = LVRCCharacterOwner->GetVRCamera()->GetComponentLocation() - UpdatedComponent->
		GetComponentLocation();
	CapsuleToHMD.Z = 0.0f;

	GetPawnOwner()->AddActorWorldOffset(CapsuleToHMD);
	LVRCCharacterOwner->GetVROrigin()->AddWorldOffset(-CapsuleToHMD);
}

void ULVRCMovementComponent::UpdateCapsuleHeightToHMD() const
{
	FRotator HMDRotation;
	FVector HMDPosition;
	UHeadMountedDisplayFunctionLibrary::GetOrientationAndPosition(HMDRotation, HMDPosition);
	CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight((HMDPosition.Z + CapsuleHeightOffset) / 2.0f);
	LVRCCharacterOwner->MatchVROriginOffsetToCapsuleHalfHeight();
}

void ULVRCMovementComponent::PostLoad()
{
	Super::PostLoad();
}

void ULVRCMovementComponent::BeginContinuousLocomotion()
{
	GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Green, TEXT("BeginContinuousLocomotion"));

	// TODO: Do capsule tracing here
}
