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
class UAnimMontage;
class USoundBase;

UCLASS(config=Game)
class ALVRCCharacter : public ACharacter
{
	GENERATED_BODY()

	/** The offset from the center of the capsule used to position the VR camera. Z-loc should equal -1 8 capsule half-height. */
	UPROPERTY(Category=Character, VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess = "true"))
	USceneComponent* VROrigin;
	
	/** The VR camera, automatically tracked with OpenXR */
	UPROPERTY(Category=Character, VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess = "true"))
	UCameraComponent* VRCamera;

public:
	ALVRCCharacter(const FObjectInitializer& ObjectInitializer);

protected:
	/** Resets HMD orientation and position in VR. */
	void OnResetVR();

protected:
	// APawn interface
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	// End of APawn interface
};
