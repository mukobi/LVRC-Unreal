// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
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

private:
	/** The VR origin, updated when syncing misalignments between the updated component and camera. */
	UPROPERTY()
	USceneComponent* VROriginComponent;
	
	/** The player camera, needed for some movement validation routines. */
	UPROPERTY()
	USceneComponent* CameraComponent;

public:
	// Sets default values for this component's properties
	ULVRCMovementComponent(const FObjectInitializer& ObjectInitializer);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	/** Whether movement due to continuous locomotion is happening this frame. */
	UPROPERTY(BlueprintReadOnly)
	bool bIsPerformingContinuousLocomotion = false;

public:
	void SetVROriginComponent(USceneComponent* InVROriginComponent)
	{
		VROriginComponent = InVROriginComponent;
	}
	
	void SetCameraComponent(USceneComponent* InCameraComponent)
	{
		CameraComponent = InCameraComponent;
	}

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** Updates the capsule components position and height as well as the position of the HMD (camera) to be in sync. */
	void UpdateCapsuleToFollowHMD() const;

	/**
	 * Handle the beginning of continuous locomotion when no previous locomotion was happening last tick. Does things
	 * like validating the current HMD location and sweeping the player capsule to the HMD location .
	 */
	void BeginContinuousLocomotion();

private:
	FVector PreviousTickInputVector;
};
