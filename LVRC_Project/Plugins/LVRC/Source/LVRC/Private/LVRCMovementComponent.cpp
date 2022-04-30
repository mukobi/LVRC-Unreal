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
	CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(LVRCCharacterOwner->GetPlayerTopOfHeadHeight() / 2.0f);
	LVRCCharacterOwner->MatchVROriginOffsetToCapsuleHalfHeight();
}

void ULVRCMovementComponent::CalculateTeleportationParameters(
	FVector TraceStartLocation, FVector TraceStartDirection, FVector& ValidatedGroundLocation,
	FVector& ArcEndLocation, TArray<FVector>& ValidatedArcLocations, TArray<FVector>& RemainingArcLocations,
	float& HeightAdjustmentRatio, TArray<FVector>& StepLocations, bool& bDropAfterArc, bool& bIsLethal) const
{
	check(bIsTeleporting);
	check(TraceStartDirection.IsNormalized());

	constexpr EDrawDebugTrace::Type ArcDrawDebugType = EDrawDebugTrace::Type::ForOneFrame;
	constexpr EDrawDebugTrace::Type StepCapsuleDrawDebugType = EDrawDebugTrace::Type::None;
	constexpr EDrawDebugTrace::Type DestinationValidationDrawDebugType = EDrawDebugTrace::Type::None;
	constexpr EDrawDebugTrace::Type LOSDrawDebugType = EDrawDebugTrace::Type::None;

	// Precompute some things
	FVector EyeWorldLocation = LVRCCharacterOwner->GetPlayerEyeWorldLocation();
	float PlayerEyeHeight = LVRCCharacterOwner->GetPlayerEyeHeight();
	float PlayerTopOfHeadHeight = LVRCCharacterOwner->GetPlayerTopOfHeadHeight();
	float PlayerTopOfHeadHalfHeight = 0.5f * PlayerTopOfHeadHeight;
	float PlayerCapsuleRadius = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius();
	float StepCapsuleHalfHeight = 0.5f * TeleportStepCapsuleHeight;
	FVector StepCapsuleFloorOffset = FVector(0, 0, StepCapsuleHalfHeight);
	float CapsuleFloatHeight = (MIN_FLOOR_DIST + MAX_FLOOR_DIST) * 0.5f;

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
		ArcDrawDebugType);
	ArcEndLocation = ArcTraceLocations[ArcTraceLocations.Num() - 1];

	// Handle arc pointing at a kill volume. Don't return early, because we might avoid this by stopping early at the
	// ledge during step validation if we're far enough away from it. 
	bIsLethal = ArcHit.GetActor() && ArcHit.GetActor()->IsA(AKillZVolume::StaticClass());

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
			// UPDATE: I'm pretty sure HLA doesn't back up the end teleport hit spot because it's too hard, it just
			// tries to get to the ground location and says we've reached the destination if we get to within some
			// decently large tolerance of it. If this stays true, can delete this block.
			
			// // TODO fix this backing up. It should probably move away from the ArcHit surface by a bit more than the
			// // capsule radius * the hit normal, then project that point parallel to the hit surface onto the teleport
			// // arc to figure out how far along the teleport path is safe to go, then remove some of the elements from
			// // the teleport arc and shorten the last one s.t. the ArcEndLocation is a safely teleportable distance from
			// // the wall. Should write the projection-onto-arc stuff as a subroutine, as it's necessary for tele viz. 
			// FVector BackUpDirection = -TargetDirection2D;
			// ArcEndLocation += 1.01f * CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius() * BackUpDirection;
		}

		// Trace down from the arc end point and determine if it was a deadly drop
		FVector TraceEnd = ArcEndLocation + FVector::DownVector * (MaxDropDistance -
			(CharacterOwner->GetActorLocation().Z - ArcEndLocation.Z));
		FHitResult DropHit;
		UKismetSystemLibrary::LineTraceSingleForObjects(
			this, ArcEndLocation, TraceEnd, LocomotionBlockingObjectTypes, false,
			{}, ArcDrawDebugType, DropHit, true);

		if (!DropHit.bBlockingHit || (DropHit.GetActor() && DropHit.GetActor()->IsA(AKillZVolume::StaticClass())))
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
	// TODO implement this for teleport UI

	// Run validation to find a validated teleportation destination as well as the step path
	USceneComponent* VRCamera = LVRCCharacterOwner->GetVRCamera();
	FVector CameraGroundLocation = VRCamera->GetComponentLocation() - FVector(0, 0, VRCamera->GetRelativeLocation().Z);
	// Start steps at the position on the ground under the camera
	FVector CurrentStepPosition = CameraGroundLocation;
	FVector PreviousStepPosition = CurrentStepPosition;
	FHitResult GroundStepForwardHit, StepUpHit, AirStepForwardHit, StepDownHit;

	// Take intermediate steps forward until we get stuck or pass the destination
	bool bStepsIncludeDrop = false;
	StepLocations.Reset();
	for (int i = 0; i < 30; i++)
	{
		// Limit step distance to the remaining distance to the destination
		float StepToDestinationDistance2D = (DesiredGroundLocation - CurrentStepPosition).Size2D();
		float StepForwardLengthRemaining = FMath::Min(TeleportStepLength, StepToDestinationDistance2D);

		// TODO refactor the following loop body out into some stepping routine taking the height, forward amount, and
		// downward amount. Try a normal step, but if the normal step ends up in the same location, try a mantle which
		// is a higher vertical step with much smaller forward step. Might refactor the mantel part out into its own function.

		// Trace position is the center of the capsule, not the ground
		FVector CapsuleCenterLocation = CurrentStepPosition + StepCapsuleFloorOffset;

		// Step forward
		FVector StartLocation = CapsuleCenterLocation;
		FVector EndLocation = StartLocation + TargetDirection2D * StepForwardLengthRemaining;
		UKismetSystemLibrary::CapsuleTraceSingleForObjects(
			this, StartLocation, EndLocation, PlayerCapsuleRadius, StepCapsuleHalfHeight,
			LocomotionBlockingObjectTypes, false, {}, StepCapsuleDrawDebugType, GroundStepForwardHit,
			true, FLinearColor(0, 1, 0), FLinearColor(0, 0.2f, 0));
		CapsuleCenterLocation = GroundStepForwardHit.bBlockingHit ? GroundStepForwardHit.Location : EndLocation;

		// Step up if didn't complete forward step
		if (GroundStepForwardHit.bBlockingHit)
		{
			// Step up
			StartLocation = CapsuleCenterLocation - 1.0f * TargetDirection2D;
			// Back up a bit to not hit the forward barrier again
			EndLocation = StartLocation + FVector::UpVector * MaxStepHeight;
			UKismetSystemLibrary::CapsuleTraceSingleForObjects(
				this, StartLocation, EndLocation, PlayerCapsuleRadius, StepCapsuleHalfHeight,
				LocomotionBlockingObjectTypes,
				false, {}, StepCapsuleDrawDebugType, StepUpHit, true, FLinearColor(0, 0, 1), FLinearColor(0, 0, 0.2f));
			CapsuleCenterLocation = StepUpHit.bBlockingHit ? StepUpHit.Location : EndLocation;

			// Step forward again by any remaining amount
			StartLocation = CapsuleCenterLocation;
			EndLocation = StartLocation + TargetDirection2D * StepForwardLengthRemaining * (1.0f - GroundStepForwardHit.Time);
			UKismetSystemLibrary::CapsuleTraceSingleForObjects(
				this, StartLocation, EndLocation, PlayerCapsuleRadius, StepCapsuleHalfHeight,
				LocomotionBlockingObjectTypes,
				false, {}, StepCapsuleDrawDebugType, AirStepForwardHit, true, FLinearColor(0, 1, 1),
				FLinearColor(0, 0.2f, 0.2f));
			CapsuleCenterLocation = AirStepForwardHit.bBlockingHit ? AirStepForwardHit.Location : EndLocation;
		}

		// Step down, including drops
		StartLocation = CapsuleCenterLocation;
		EndLocation = StartLocation + FVector::DownVector * MaxDropDistance + StepUpHit.Distance;
		UKismetSystemLibrary::CapsuleTraceSingleForObjects(
			this, StartLocation, EndLocation, PlayerCapsuleRadius, StepCapsuleHalfHeight,
			LocomotionBlockingObjectTypes,
			false, {}, StepCapsuleDrawDebugType, StepDownHit, true, FLinearColor(1.0f, 0, 0), FLinearColor(0.2f, 0, 0));
		if (!StepDownHit.bBlockingHit)
		{
			// This step would have been a lethal fall, don't include it
			bStepsIncludeDrop = true;
			break;
		}

		if (IsWalkable(StepDownHit))
		{
			CapsuleCenterLocation = StepDownHit.Location;
			CapsuleCenterLocation.Z += CapsuleFloatHeight; // Float a little above the floor
		}
		else
		{
			// We landed on a non-walkable surface, so defer to the grounded forward movement
			CapsuleCenterLocation = GroundStepForwardHit.Location;
		}

		// Distance check to fix incremental climbing when stuck in front of a vertical surface
		// if (FVector::DistSquared2D(CapsuleCenterLocation, StartingCapsuleCenterLocation) > KINDA_SMALL_NUMBER)
		// {
		// }

		CurrentStepPosition = CapsuleCenterLocation - StepCapsuleFloorOffset;

		// If didn't move since last step, we're done (blocked or reached the destination)
		if ((CurrentStepPosition - PreviousStepPosition).IsNearlyZero(0.1f))
		{
			break;
		}

		// Detect if we've dropped (stepped down more than the mantel height)
		if (PreviousStepPosition.Z - CurrentStepPosition.Z > MaxMantleHeight)
		{
			bStepsIncludeDrop = true;
		}

		StepLocations.Add(CurrentStepPosition);
		PreviousStepPosition = CurrentStepPosition;
	}

	// Check backwards to find the last intermediate step that's a valid destination
	int LastValidStepIndex;
	bool bPartialMovementLedge = bIsLethal;
	for (LastValidStepIndex = StepLocations.Num() - 1; LastValidStepIndex >= 0; LastValidStepIndex--)
	{
		FVector StepLocation = StepLocations[LastValidStepIndex];
		
		// Partial movement at ledges: If you can't see where your body would be after a drop, don't go there
		if (bStepsIncludeDrop)
		{
			FVector TraceStart = EyeWorldLocation;
			// TODO maybe try tracing to the feet or feet and head instead of the center of the body
			FVector TraceEnd = StepLocation + FVector::UpVector * PlayerTopOfHeadHalfHeight;
			FHitResult LOSTraceHit;
			UKismetSystemLibrary::LineTraceSingleForObjects(this, TraceStart, TraceEnd, LocomotionBlockingObjectTypes, false, {}, LOSDrawDebugType, LOSTraceHit, true, FLinearColor(0,0.8f,1.0f), FLinearColor::Red);
			if (LOSTraceHit.bBlockingHit)
			{
				// Can't see destination, this is not a viable destination
				bPartialMovementLedge = true;
				continue;
			}
		}
		
		// If the full player capsule can't fit here, the step is invalid
		FVector CapsuleCenterLocation = StepLocation + FVector::UpVector * PlayerTopOfHeadHalfHeight;
		FHitResult FullPlayerHit;
		UKismetSystemLibrary::CapsuleTraceSingleForObjects(
			this, CapsuleCenterLocation, CapsuleCenterLocation, PlayerCapsuleRadius, PlayerTopOfHeadHalfHeight,
			LocomotionBlockingObjectTypes, false, {}, DestinationValidationDrawDebugType, FullPlayerHit,
			true, FLinearColor(0.4f, 0.4f, 0.4f), FLinearColor(0.4f, 0, 0));
		if (!FullPlayerHit.bBlockingHit)
		{
			// Player capsule fits here, so this is a valid location
			// TODO maybe also try a line trace from the center of the bottom of the capsule down to make sure this
			// capsule center is on the ground.
			// EDIT: I think this is probably unnecessary. EDIT 2: Maybe this is necessary if we do the incremental
			// stepping up to the ledge in the below if block
			break;
		}
	}

	// Remove invalid steps all at once from the end
	StepLocations.RemoveAt(LastValidStepIndex + 1, StepLocations.Num() - LastValidStepIndex - 1, false);

	// TODO If partially moved due to ledges, nudge this step up to the edge by a series of TouchingGround line traces
	// (maybe binary search?) then capsule body tracing to check the body fits there again 
	if(bPartialMovementLedge)
	{
		
	}

	// Determine if the steps got us (close enough) to our desired destination
	bool bStepsReachedDestination;
	if (StepLocations.Num() == 0)
	{
		bStepsReachedDestination = false;
		ValidatedGroundLocation = CameraGroundLocation;
	}
	else {
		// At least one valid intermediate step location, treat the last step as our candidate destination
		ValidatedGroundLocation = StepLocations.Last();
	
		bStepsReachedDestination = (ValidatedGroundLocation - DesiredGroundLocation).SizeSquared()
			< TeleportStepLength * TeleportStepLength * 1.25f;

		if (!bStepsReachedDestination)
		{
			// Sweep forward with the full player capsule to get us right up to the wall
			FHitResult FullPlayerHit;
			FVector CapsuleCenterStartLocation = ValidatedGroundLocation + FVector::UpVector * PlayerTopOfHeadHalfHeight;
			FVector CapsuleCenterEndLocation = CapsuleCenterStartLocation + TargetDirection2D * TeleportStepLength;
			UKismetSystemLibrary::CapsuleTraceSingleForObjects(
				this, CapsuleCenterStartLocation, CapsuleCenterEndLocation, PlayerCapsuleRadius, PlayerTopOfHeadHalfHeight,
				LocomotionBlockingObjectTypes, false, {}, DestinationValidationDrawDebugType, FullPlayerHit,
				true, FLinearColor(0.9f, 0.9f, 0.9f), FLinearColor(0.9f, 0, 0));

			if (FullPlayerHit.IsValidBlockingHit())
			{
				// Line trace to check if this swept destination is on the same ground
				// NOTE maybe we binary search or linearly test for the edge with many traces?
				float TouchingGroundTraceLength = 5.0f; // TODO might need to be higher because of floating height
				FVector TraceStart = ValidatedGroundLocation + FVector::UpVector * 0.5f * TouchingGroundTraceLength;
				FVector TraceEnd = TraceStart + FVector::DownVector * TouchingGroundTraceLength;
				FHitResult GroundTraceHit;
				UKismetSystemLibrary::LineTraceSingleForObjects(this, TraceStart, TraceEnd, LocomotionBlockingObjectTypes, false, {}, DestinationValidationDrawDebugType, FullPlayerHit, true);
				if (GroundTraceHit.IsValidBlockingHit())
				{
					ValidatedGroundLocation = FullPlayerHit.Location;
				}
			}
		}
	}

	// TODO don't try a jump if steps got us close enough (maybe increase destination reached threshold to a bit more than step distance?)
	
	// TODO maybe we only need to jump if some of the steps fell down (since we can mantel up to jumpable areas without gaps)
	
	// Try to see if a jump is viable if steps didn't get us to the destination
	// Don't jump if a lethal fall unless we didn't do partial movement because you're close enough to the ledge
	bool bCloseToLedge = FVector::DistSquared2D(ValidatedGroundLocation, EyeWorldLocation)
		< TeleportLedgeClosenessThreshold * TeleportLedgeClosenessThreshold;
	if (!bStepsReachedDestination && (!bIsLethal || bCloseToLedge))
	{
		// TODO also check LOS for head position at jump destination (add another trace)
		// If player doesn't have LOS to destination location, jump is invalid
		FVector TraceStart = EyeWorldLocation;
		FVector TraceEnd = DesiredGroundLocation + FVector::UpVector * CapsuleFloatHeight;
		FHitResult LOSTraceHit;
		UKismetSystemLibrary::LineTraceSingleForObjects(this, TraceStart, TraceEnd, LocomotionBlockingObjectTypes, false, {}, LOSDrawDebugType, LOSTraceHit, true);
		if (!LOSTraceHit.bBlockingHit)
		{
			// TODO maybe nudge location by normal to hit so the jump destination is more regularly chosen
			// If the full player capsule doesn't fit in destination, jump is invalid. Use FindTeleportSpot to
			// nudge capsule out of blocking geometry (e.g. if DesiredGroundLocation is too close to a wall).
			FVector CapsuleCenterDestinationLocation = DesiredGroundLocation
				+ FVector::UpVector * (PlayerTopOfHeadHalfHeight + CapsuleFloatHeight);
			if (GetWorld()->FindTeleportSpot(CharacterOwner, CapsuleCenterDestinationLocation, CharacterOwner->GetActorRotation()))
			{
				// Player capsule fits here, so this is a valid jump location
				ValidatedGroundLocation = CapsuleCenterDestinationLocation + FVector::DownVector * PlayerTopOfHeadHalfHeight;
				StepLocations.Empty();
			}
		}
	}

	// TODO Calculate other things needed for teleport UI
	
	// TODO project ValidatedGroundLocation onto the arc based on 2D distance from camera
	
	// TODO split ArcTraceLocations into validated and invalidated segments
	ValidatedArcLocations = ArcTraceLocations; // TODO
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
		+ FVector::UpVector * (LVRCCharacterOwner->CapsuleHeightOffset - CapsuleHalfHeight);

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
