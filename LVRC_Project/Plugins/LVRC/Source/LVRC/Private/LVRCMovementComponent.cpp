// Fill out your copyright notice in the Description page of Project Settings.


#include "LVRCMovementComponent.h"

void ULVRCMovementComponent::TryContinuousLocomotion(FVector2D Direction, float DeltaTime)
{
	Velocity = FVector(Direction.X, Direction.Y, 0) * CurrentWalkingSpeed;

	const FVector delta = Velocity * DeltaTime;

	// Try moving along the current floor
	FHitResult FloorHit(1.f);
	SafeMoveUpdatedComponent(delta, UpdatedComponent->GetComponentRotation(), true, FloorHit);
}

void ULVRCMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	const FVector InputVector = ConsumeInputVector();

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TryContinuousLocomotion(FVector2D(InputVector.X, InputVector.Y), DeltaTime);
}
