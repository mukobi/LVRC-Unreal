// Fill out your copyright notice in the Description page of Project Settings.


#include "LVRCMovementComponent.h"

#include "HeadMountedDisplayFunctionLibrary.h"
#include "LVRCStatics.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/KismetSystemLibrary.h"


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

void ULVRCMovementComponent::CalculateTeleportationParameters(
	FVector TraceStartPosition, FVector TraceStartDirection, FVector& ValidatedDestination,
	FVector& ProjectedDestination, TArray<FVector>& ValidatedArcPositions, TArray<FVector>& RemainingArcPositions,
	float& HeightAdjustmentRatio, TArray<FVector>& StepPositions, bool& IsLethal) const
{
	checkSlow(bIsTeleporting);
	checkSlow(TraceStartDirection.IsNormalized());

	// Limit the start direction to a maximum vertical angle
	const float MinThetaRadians = FMath::DegreesToRadians(90.0f - TeleportArcMaxVerticalAngle);
	FVector2D DirectionSpherical = TraceStartDirection.UnitCartesianToSpherical();
	DirectionSpherical.X = FMath::Max(DirectionSpherical.X, MinThetaRadians); // Limit theta which goes from 0 (up) to pi (down)
	TraceStartDirection = DirectionSpherical.SphericalToUnitCartesian();

	// Use tracing from the hand to choose a desired teleport destination
	TArray<FVector> ArcTracePositions;
	FHitResult ArcHit;
	ULVRCStatics::PredictProjectilePathPointDrag(
		ArcTracePositions, ArcHit, this, TraceStartPosition, TraceStartDirection * TeleportArcInitialSpeed,
		LocomotionBlockingObjectTypes, {}, TeleportArcDampingFactor, -98, TeleportArcMaxSimTime, TeleportArcNumSubsteps, false,
		EDrawDebugTrace::Type::ForOneFrame);

	ValidatedArcPositions = ArcTracePositions;

	// Sweep a sphere upwards from the desired destination to a max height of the capsule height to determine the height
	// the player would need to crouch to fit there
	// TODO write a function to get the player's height which is HMD local Z position + eye to top of head distance and refactor the Set Capsule Height function with it before using it here

	// Run validation to find a validated teleportation destination as well as other things used for UI like the step path and maybe whether we're trying to jump into deadly fall or kill volume

	// Calculate other things needed for teleport UI 
}

void ULVRCMovementComponent::PostLoad()
{
	Super::PostLoad();
}

void ULVRCMovementComponent::BeginContinuousLocomotion() const
{
	// Constants
	constexpr EDrawDebugTrace::Type DrawDebugType = EDrawDebugTrace::Type::ForDuration;
	GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Green, TEXT("BeginContinuousLocomotion"));

	// Figure out where the capsule would be if it were teleported to the HMD (UpdateCapsuleHeightToHMD was just called)
	const UCapsuleComponent* CapsuleComponent = Cast<UCapsuleComponent>(UpdatedComponent);
	const float CapsuleHalfHeight = CapsuleComponent->GetScaledCapsuleHalfHeight();
	const float CapsuleRadius = CapsuleComponent->GetScaledCapsuleRadius();
	const FVector DesiredCapsuleLocation = LVRCCharacterOwner->GetVRCamera()->GetComponentLocation()
		+ FVector::UpVector * (CapsuleHeightOffset - CapsuleHalfHeight);

	// Sweep capsule from position it was left to this new position
	FHitResult ImpassibleSweepHit;
	UKismetSystemLibrary::CapsuleTraceSingleForObjects(
		this, UpdatedComponent->GetComponentLocation(), DesiredCapsuleLocation, CapsuleRadius,
		CapsuleHalfHeight, ImpassibleObjectTypes, false, {}, DrawDebugType,
		ImpassibleSweepHit, true, FLinearColor::Blue, FLinearColor::Yellow, 3.0f);

	if (ImpassibleSweepHit.bBlockingHit)
	{
		// Don't let player go through impassible objects (e.g. level boundaries)
		UpdateCapsulePositionToHMD();
		UpdatedComponent->SetWorldLocation(ImpassibleSweepHit.Location);
		return;
	}

	// We aren't trying to go through impassible objects, but we need to check if the destination is valid (sweep in place)
	FHitResult DestinationCheckHit;
	UKismetSystemLibrary::CapsuleTraceSingleForObjects(
		this, DesiredCapsuleLocation, DesiredCapsuleLocation, CapsuleRadius, CapsuleHalfHeight,
		LocomotionBlockingObjectTypes, false, {}, DrawDebugType, DestinationCheckHit,
		true, FLinearColor::Green, FLinearColor::Red, 3.0f);

	if (!DestinationCheckHit.bStartPenetrating)
	{
		// Destination is valid, let the player continue from there
		UpdateCapsulePositionToHMD();
		return;
	}

	// Destination is an invalid space for locomotion, so sweep for the first thing blocking us
	GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Orange, TEXT("bStartPenetrating"));
	FHitResult LocomotionBlockingSweepHit;
	ensureAlways(UKismetSystemLibrary::CapsuleTraceSingleForObjects(
		this, UpdatedComponent->GetComponentLocation(), DesiredCapsuleLocation, CapsuleRadius,
		CapsuleHalfHeight, LocomotionBlockingObjectTypes, false, {}, DrawDebugType,
		LocomotionBlockingSweepHit, true, FLinearColor::Blue, FLinearColor::Yellow, 3.0f));

	// Teleport the player to the hit location
	UpdateCapsulePositionToHMD();
	UpdatedComponent->SetWorldLocation(LocomotionBlockingSweepHit.Location);
}
