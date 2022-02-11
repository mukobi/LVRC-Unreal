// Fill out your copyright notice in the Description page of Project Settings.


#include "LVRCMovementComponent.h"


// Sets default values for this component's properties
ULVRCMovementComponent::ULVRCMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


// Called when the game starts
void ULVRCMovementComponent::BeginPlay()
{
	Super::BeginPlay();
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
		UpdateCapsuleToFollowHMD();

		if (FMath::IsNearlyZero(PreviousTickInputVector.SizeSquared()))
		{
			BeginContinuousLocomotion();
		}
	}

	PreviousTickInputVector = InputVector;


	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void ULVRCMovementComponent::UpdateCapsuleToFollowHMD() const
{
	FVector CapsuleToHMD = CameraComponent->GetComponentLocation() - UpdatedComponent->GetComponentLocation();
	CapsuleToHMD.Z = 0.0f;

	GetPawnOwner()->AddActorWorldOffset(CapsuleToHMD);
	VROriginComponent->AddWorldOffset(-CapsuleToHMD);
}

void ULVRCMovementComponent::BeginContinuousLocomotion()
{
	GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Green, TEXT("BeginContinuousLocomotion"));
}
