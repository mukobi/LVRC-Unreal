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

	// BlueprintCallable Interface

	/** Updates the capsule component's position as well as the position of the HMD (camera) to be in sync. */
	UFUNCTION(BlueprintCallable)
	void UpdateCapsulePositionToHMD() const;

	/** Matches the capsule component's height to the HMD and offsets the VROrigin to be in sync. */
	UFUNCTION(BlueprintCallable)
	void UpdateCapsuleHeightToHMD() const;

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

