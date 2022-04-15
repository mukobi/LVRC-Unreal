// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "LVRCCharacter.generated.h"

class ULVRCMovementComponent;
class UInputComponent;
class USkeletalMeshComponent;
class USceneComponent;
class UCameraComponent;
class UMotionControllerComponent;

UCLASS(config=Game)
class ALVRCCharacter : public ACharacter
{
	GENERATED_BODY()

private:
	// Components //
	
	/** The offset from the center of the capsule used to position the VR camera. Z-loc should equal -1 8 capsule half-height. */
	UPROPERTY(Category=Character, VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess = "true"))
	USceneComponent* VROrigin;
	
	/** The VR camera, automatically tracked with OpenXR */
	UPROPERTY(Category=Character, VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess = "true"))
	UCameraComponent* VRCamera;

public:
	// Settings //
	
	/** Offset from the HMD tracked position to use for the top of the capsule component. */
	UPROPERTY(EditDefaultsOnly)
	float CapsuleHeightOffset = 10.0f;

public:
	ALVRCCharacter(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable)
	USceneComponent* GetVROrigin() const { return VROrigin; }
	
	UFUNCTION(BlueprintCallable)
	UCameraComponent* GetVRCamera() const { return VRCamera; }
	
	UFUNCTION(BlueprintCallable)
	ULVRCMovementComponent* GetLVRCMovementComponent() const;

	/** Gets the height of the top of the player's head in tracking space (a little more than the HMD position). */
	UFUNCTION(BlueprintPure)
	float GetPlayerTopOfHeadHeight() const;

	/** Updates the Z component of the VR origin to equal the negative capsule half height so it's on the ground. */
	UFUNCTION(BlueprintCallable)
	void MatchVROriginOffsetToCapsuleHalfHeight();

protected:
	/** Resets HMD orientation and position in VR. */
	void OnResetVR();

protected:
	// APawn interface
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	// End of APawn interface
};

