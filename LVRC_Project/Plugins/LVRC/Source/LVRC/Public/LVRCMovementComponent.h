// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PawnMovementComponent.h"
#include "UObject/Object.h"
#include "LVRCMovementComponent.generated.h"

/**
 * LVRCMovementComponent handles movement logic for the associated LVRCPawn owner.
 * It supports various movement modes including: walking, teleporting, falling.
 *
 * Movement is affected primarily by current Velocity and Acceleration. Acceleration is updated each frame
 * based on the input vector accumulated thus far (see UPawnMovementComponent::GetPendingInputVector()).
 *
 * An important concept is virtual movement, or movement that only happens in the game world and does not reflect the
 * tracked device motions of the real player.
 *
 * Networking is not yet implemented.
 */
UCLASS()
class LVRC_API ULVRCMovementComponent : public UPawnMovementComponent
{
	GENERATED_BODY()

public:
	/** Try to smoothly walk in the specified direction. Performs movement validation and correction. */
	void TryContinuousLocomotion(FVector2D Direction);

	/** Whether the pawn is currently virtual moving (i.e. continuous locomotion or teleportation) this frame. */
	bool IsVirtuallyMoving() const
	{
		return bIsPerformingContinuousLocomotion || bIsPerformingTeleportation;
	}

	/** Sets the internal walking speed based on whether the pawn should be moving fast (e.g. in combat) or not. */
	void SetCurrentWalkingSpeed(const bool bIsFast)
	{
		CurrentWalkingSpeed = bIsFast ? WalkSpeedFast : WalkSpeedDefault;
	}

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

protected:
	/** Walking speed when out of combat. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float WalkSpeedDefault = 50.0f;

	/** Walking speed when in combat or otherwise hurried */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float WalkSpeedFast = 75.0f;

private:
	/** Current walking speed, updated using SetCurrentWalkingSpeed. */
	float CurrentWalkingSpeed = 0.0f;

	/** Whether movement due to continuous locomotion is happening this frame. */
	bool bIsPerformingContinuousLocomotion = false;

	/** Whether movement due to teleportation is happening this frame. */
	bool bIsPerformingTeleportation = false;
};
