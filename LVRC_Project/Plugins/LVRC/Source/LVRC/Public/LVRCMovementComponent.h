// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LVRCCharacter.h"
#include "Components/ActorComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "LVRCMovementComponent.generated.h"

class UCameraComponent;

/**
 * LVRCMovementComponent handles movement logic for the associated LVRCPawn owner.
 * It supports various movement modes including: walking, teleporting, falling.
 *
 * It is based on the Character Movement Component, but overrides some functionality for a VR Character.
 *
 * An important concept is virtual movement, or movement that only happens in the game world and does not reflect the
 * tracked device motions of the real player.
 *
 * Networking has not yet been tested for these VR-specific movement mechanics, but the networking support built-in to
 * the Character Movement Component should get most of the way there.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LVRC_API ULVRCMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	ULVRCMovementComponent(const FObjectInitializer& ObjectInitializer);

public:
	// Configurable Settings

	/** Offset from the HMD tracked position to use for the top of the capsule component. */
	UPROPERTY(EditDefaultsOnly)
	float CapsuleHeightOffset = 10.0f;

	/** The maximum angle upwards you can point a teleport arc. */
	UPROPERTY(EditDefaultsOnly, Category="Teleportation")
	float TeleportArcMaxVerticalAngle = 35.0f;
	
	/** Initial speed of the simulated projectile teleport arc. */
	UPROPERTY(EditDefaultsOnly, Category="Teleportation")
	float TeleportArcInitialSpeed = 100.0f;
	
	/** Damping factor used to fake drag on the simulated projectile teleport arc. */
	UPROPERTY(EditDefaultsOnly, Category="Teleportation", meta=(ClampMin=0.0f, ClampMax=1.0f))
	float TeleportArcDampingFactor = 2.0f;

	/** Maximum simulation time of the simulated projectile teleport arc. */
	UPROPERTY(EditDefaultsOnly, Category="Teleportation")
	float TeleportArcMaxSimTime = 4.0f;
	
	/** How many sub-steps to use in the simulation (chopping up TeleportArcMaxSimTime). */
	UPROPERTY(EditDefaultsOnly, Category="Teleportation")
	int TeleportArcNumSubsteps = 32;

	// BlueprintCallable Interface

	/** Updates the capsule component's position as well as the position of the HMD (camera) to be in sync. */
	UFUNCTION(BlueprintCallable)
	void UpdateCapsulePositionToHMD() const;

	/** Matches the capsule component's height to the HMD and offsets the VROrigin to be in sync. */
	UFUNCTION(BlueprintCallable)
	void UpdateCapsuleHeightToHMD() const;

	/**
	 * @brief Calculate parameters for teleportation functionality/visualization. Implements advanced features like
	 * path validation and partial movement for invalid arc destinations and drop-offs.
	 * @param TraceStartPosition Position from which to start tracing the teleport arc.
	 * @param TraceStartDirection Direction from which to start tracing the teleport arc.
	 * @param ValidatedDestination The destination that has been determined as valid for the player to teleport to.
	 * @param ProjectedDestination The destination on the ground that just the teleportation arc wanted to get to.
	 * @param ValidatedArcPositions Positions of the segmented arc leading up to and including the spot above the ValidatedDestination.
	 * @param RemainingArcPositions Any remaining positions of the arc past the ValidatedDestination to the ProjectedDestination.
	 * @param HeightAdjustmentRatio How much the player would have to crouch to fit vertically in the ProjectedDestination.
	 * 0 represents the player's height fitting underneath the ceiling. Just above 0 is just barely intersecting.
	 * 1 represents the current player height being double the ceiling height.
	 * @param StepPositions The sequence of validated step ground positions leading to the ValidatedDestination.
	 * @param IsLethal Whether this teleport would kill the play (i.e. from a fall or landing in a kill volume).
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure=false)
	void CalculateTeleportationParameters(
		FVector TraceStartPosition, FVector TraceStartDirection, FVector& ValidatedDestination,
		FVector& ProjectedDestination, TArray<FVector>& ValidatedArcPositions, TArray<FVector>& RemainingArcPositions,
		float& HeightAdjustmentRatio, TArray<FVector>& StepPositions, bool& IsLethal) const;

	// Interface Implementations

	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void PostLoad() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent Interface

protected:
	/** Whether movement due to continuous locomotion is happening this frame. */
	UPROPERTY(BlueprintReadOnly)
	bool bIsPerformingContinuousLocomotion = false;

	/**
	 * Whether we're in any part of the process of teleporting, from first pressing the button that visualizes
	 * the teleport arc to the last frame of blink/shift teleportation before the player regains control.
	 */
	UPROPERTY(BlueprintReadWrite)
	bool bIsTeleporting = false;

private:
	/**
	 * Handle the beginning of continuous locomotion when no previous locomotion was happening last tick. Does things
	 * like validating if the capsule would be unblocked at the current HMD location and sweeping the player capsule
	 * to the HMD location if it would be blocked to find a valid place to pop the player out.
	 */
	void BeginContinuousLocomotion() const;

	/** LVRC Character this movement component belongs to */
	UPROPERTY(Transient, DuplicateTransient)
	ALVRCCharacter* LVRCCharacterOwner;

	FVector PreviousTickInputVector;


	/**
	 * Objects that you aren't allowed to overlap with, such as static level geometry and large physics objects.
	 * Note: You may be able to "move" through these with virtual movement validation after real moving through them.
	 */
	UPROPERTY(EditDefaultsOnly)
	TArray<TEnumAsByte<EObjectTypeQuery>> LocomotionBlockingObjectTypes = {ObjectTypeQuery1};

	/** Objects that you should never be allowed to move through, like level bounds and gameplay barriers. */
	UPROPERTY(EditDefaultsOnly)
	TArray<TEnumAsByte<EObjectTypeQuery>> ImpassibleObjectTypes = {ObjectTypeQuery1};
};
