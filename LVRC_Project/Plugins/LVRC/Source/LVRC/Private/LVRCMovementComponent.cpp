// Fill out your copyright notice in the Description page of Project Settings.


#include "LVRCMovementComponent.h"

#include "HeadMountedDisplayFunctionLibrary.h"
#include "LVRCStatics.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/KillZVolume.h"
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
	FVector TraceStartLocation, FVector TraceStartDirection, FVector& ValidatedGroundLocation,
	FVector& ArcEndLocation, TArray<FVector>& ValidatedArcLocations, TArray<FVector>& RemainingArcLocations,
	float& HeightAdjustmentRatio, TArray<FVector>& StepLocations, bool& bDropAfterArc, bool& bIsLethal) const
{
	check(bIsTeleporting);
	check(TraceStartDirection.IsNormalized());

	EDrawDebugTrace::Type DrawDebugType = EDrawDebugTrace::Type::ForOneFrame;

	// Limit the start direction to a maximum vertical angle
	const float MinThetaRadians = FMath::DegreesToRadians(90.0f - TeleportArcMaxVerticalAngle);
	FVector2D DirectionSpherical = TraceStartDirection.UnitCartesianToSpherical();
	DirectionSpherical.X = FMath::Max(DirectionSpherical.X, MinThetaRadians); // Theta goes from 0 (up) to pi (down)
	TraceStartDirection = DirectionSpherical.SphericalToUnitCartesian();

	// Use tracing in an arc from the hand to choose a desired teleport destination
	TArray<FVector> ArcTraceLocations;
	FHitResult ArcHit;
	ULVRCStatics::PredictProjectilePathPointDrag(
		ArcTraceLocations, ArcHit, this, TraceStartLocation, TraceStartDirection * TeleportArcInitialSpeed,
		LocomotionBlockingObjectTypes, {}, TeleportArcDampingFactor, -98, TeleportArcMaxSimTime, TeleportArcNumSubsteps,
		false,
		DrawDebugType);
	ArcEndLocation = ArcTraceLocations[ArcTraceLocations.Num() - 1];

	// Handle arc pointing at a kill volume. Don't return early, because we might avoid this by stopping early at the
	// ledge during step validation if we're far enough away from it. 
	bIsLethal = ArcHit.Actor.IsValid() && ArcHit.Actor->IsA(AKillZVolume::StaticClass());

	// Determine if the arc's destination is somewhere we can stand
	bDropAfterArc = !IsWalkable(ArcHit) && !bIsLethal;

	// 2D direction from player to desired destination
	FVector TargetDirection2D = ArcEndLocation - LVRCCharacterOwner->GetVRCamera()->GetComponentLocation();
	TargetDirection2D.Z = 0.0f;
	TargetDirection2D.Normalize();

	FVector DesiredGroundLocation;
	if (bDropAfterArc)
	{
		// Back up by a bit more than the capsule radius if we hit an unwalkable surface (wall) so we can fit there.
		if (ArcHit.IsValidBlockingHit())
		{
			// TODO fix this backing up. It should probably move away from the ArcHit surface by a bit more than the
			// capsule radius * the hit normal, then project that point parallel to the hit surface onto the teleport
			// arc to figure out how far along the teleport path is safe to go, then remove some of the elements from
			// the teleport arc and shorten the last one s.t. the ArcEndLocation is a safely teleportable distance from
			// the wall. Should write the projection-onto-arc stuff as a subroutine, as it's necessary for tele viz. 
			FVector BackUpDirection = -TargetDirection2D;
			ArcEndLocation += 1.01f * CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius() * BackUpDirection;
		}

		// Trace down from the arc end point and determine if it was a deadly drop
		FVector TraceEnd = ArcEndLocation + FVector::DownVector * (MaxDropDistance -
			(CharacterOwner->GetActorLocation().Z - ArcEndLocation.Z));
		FHitResult DropHit;
		UKismetSystemLibrary::LineTraceSingleForObjects(
			this, ArcEndLocation, TraceEnd, LocomotionBlockingObjectTypes, false,
			{}, DrawDebugType, DropHit, true);

		if (!DropHit.bBlockingHit || (DropHit.Actor.IsValid() && DropHit.Actor->IsA(AKillZVolume::StaticClass())))
		{
			// Drop after arc would lead to death. However, don't return early, because we might avoid this by stopping
			// early at the ledge during step validation if we're far enough away from it. 
			bIsLethal = true;
			DesiredGroundLocation = ArcEndLocation;
		}
		else
		{
			// Drop contacted the ground, will try to reach that point.
			DesiredGroundLocation = DropHit.Location;
		}
	}
	else
	{
		// Arc hit walkable ground 
		DesiredGroundLocation = ArcEndLocation;
	}

	// Sweep a sphere upwards from the desired destination to a max height of the capsule height to determine the height
	// the player would need to crouch to fit there
	// TODO
	// TODO write a function to get the player's height which is HMD local Z position + eye to top of head distance and refactor the Set Capsule Height function with it before using it here

	// Run validation to find a validated teleportation destination as well as the step path
	bool bStepsReachedDestination = false;
	USceneComponent* VRCamera = LVRCCharacterOwner->GetVRCamera();
	// Start steps at the position on the ground under the camera
	FVector CurrentStepPosition = VRCamera->GetComponentLocation() - FVector(0, 0, VRCamera->GetRelativeLocation().Z);
	FVector PreviousStepPosition = CurrentStepPosition;
	FHitResult StepForwardHit, StepUpHit, StepDownHit;
	float CapsuleRadius = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius() * 0.5f; // TODO 0.2 debug
	float CapsuleHalfHeight = 0.5f * TeleportStepCapsuleHeight * 0.5f; // TODO 0.2 debug
	FVector CapsuleFloorOffset = FVector(0, 0, CapsuleHalfHeight);

	// Take intermediate steps forward until we get stuck or pass the destination
	StepLocations.Reset();
	for (int i = 0; i < 30; i++)
	{
		// Limit step distance to the remaining distance to the destination
		float StepToDestinationDistance2D = (DesiredGroundLocation - CurrentStepPosition).Size2D();
		float StepForwardLengthRemaining = FMath::Min(TeleportStepLength, StepToDestinationDistance2D);

		// Trace position is the center of the capsule, not the ground
		FVector CapsuleCenterPosition = CurrentStepPosition + CapsuleFloorOffset;

		// Step forward
		FVector StartLocation = CapsuleCenterPosition;
		FVector EndLocation = StartLocation + TargetDirection2D * StepForwardLengthRemaining;
		UKismetSystemLibrary::CapsuleTraceSingleForObjects(
			this, StartLocation, EndLocation, CapsuleRadius, CapsuleHalfHeight, LocomotionBlockingObjectTypes,
			false, {}, DrawDebugType, StepForwardHit, true, FLinearColor(0, 1, 0), FLinearColor(0, 0.2f, 0));
		CapsuleCenterPosition = StepForwardHit.bBlockingHit ? StepForwardHit.Location : EndLocation;

		// Step up if didn't complete forward step
		if (StepForwardHit.bBlockingHit)
		{
			// Step up
			StartLocation = CapsuleCenterPosition - 1.0f * TargetDirection2D;
			// Back up a bit to not hit the forward barrier again
			EndLocation = StartLocation + FVector::UpVector * MaxStepHeight;
			UKismetSystemLibrary::CapsuleTraceSingleForObjects(
				this, StartLocation, EndLocation, CapsuleRadius, CapsuleHalfHeight, LocomotionBlockingObjectTypes,
				false, {}, DrawDebugType, StepUpHit, true, FLinearColor(0, 0, 1), FLinearColor(0, 0, 0.2f));
			CapsuleCenterPosition = StepUpHit.bBlockingHit ? StepUpHit.Location : EndLocation;

			// Step forward again by any remaining amount
			StartLocation = CapsuleCenterPosition;
			EndLocation = StartLocation + TargetDirection2D * StepForwardLengthRemaining * (1.0f - StepForwardHit.Time);
			UKismetSystemLibrary::CapsuleTraceSingleForObjects(
				this, StartLocation, EndLocation, CapsuleRadius, CapsuleHalfHeight, LocomotionBlockingObjectTypes,
				false, {}, DrawDebugType, StepForwardHit, true, FLinearColor(0, 1, 1),
				FLinearColor(0, 0.2f, 0.2f));
			CapsuleCenterPosition = StepForwardHit.bBlockingHit ? StepForwardHit.Location : EndLocation;
		}

		// Step down, including drops
		StartLocation = CapsuleCenterPosition;
		EndLocation = StartLocation + FVector::DownVector * MaxDropDistance + StepUpHit.Distance;
		UKismetSystemLibrary::CapsuleTraceSingleForObjects(
			this, StartLocation, EndLocation, CapsuleRadius, CapsuleHalfHeight, LocomotionBlockingObjectTypes,
			false, {}, DrawDebugType, StepDownHit, true, FLinearColor(1.0f, 0, 0), FLinearColor(0.2f, 0, 0));
		CapsuleCenterPosition = StepDownHit.bBlockingHit ? StepDownHit.Location : EndLocation;
		CapsuleCenterPosition.Z += (MIN_FLOOR_DIST + MAX_FLOOR_DIST) * 0.5f; // Float a little above the floor

		CurrentStepPosition = CapsuleCenterPosition - CapsuleFloorOffset;

		// If didn't move since last step, we're done (blocked or reached the destination)
		if ((CurrentStepPosition - PreviousStepPosition).IsNearlyZero(0.1f))
		{
			break;
		}

		StepLocations.Add(CurrentStepPosition);
		PreviousStepPosition = CurrentStepPosition;
	}

	// Check backwards to find the last intermediate step that's a valid destination
	// TODO
	// Keep track of last valid as an int, then remove all at once from the end instead of removing within the loop

	// Try to see if a jump is viable if steps didn't get us to the destination
	if (!bStepsReachedDestination)
	{
		// Get player HMD height and trace from current HMD position to HMD position at the destination
	}

	// ValidatedGroundLocation = StepLocations[StepLocations.Num() - 1]; // TODO
	ValidatedGroundLocation = DesiredGroundLocation; // TODO
	ValidatedArcLocations = ArcTraceLocations; // TODO

	// TODO Calculate other things needed for teleport UI
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
