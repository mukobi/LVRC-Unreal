// Fill out your copyright notice in the Description page of Project Settings.


#include "LVRCMovementComponent.h"

void ULVRCMovementComponent::TryContinuousLocomotion(FVector2D Direction)
{
	Velocity = FVector(Direction.X, Direction.Y, 0) * CurrentWalkingSpeed;
}

void ULVRCMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	const FVector InputVector = ConsumeInputVector();

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TryContinuousLocomotion(FVector2D(InputVector.X, InputVector.Y));
}
