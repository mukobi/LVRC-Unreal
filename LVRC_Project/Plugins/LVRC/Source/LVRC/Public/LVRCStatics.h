// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/KismetSystemLibrary.h"
#include "LVRCStatics.generated.h"


/** Static class with useful utility functions called across LVRC code. */
UCLASS()
class LVRC_API ULVRCStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	/**
	 * @brief Similar to a simplified UGameplayStatics::PredictProjectilePath, but for a point (0 radius) with air resistance.
	 * Used for predicting the teleport arc in ULVRCMovementComponent::TODO
	 * 
	 * @param PathPositions Predicted projectile path. Ordered series of positions from StartPos to the end. Includes location at point of impact if it hit something.
	 * @param OutHit Predicted hit result, if the projectile will hit something.
	 * @param StartLocation First start trace location.
	 * @param LaunchVelocity Velocity the "virtual projectile" is launched at.
	 * @param ObjectTypes ObjectTypes to trace against.
	 * @param ActorsToIgnore Actors to exclude from the traces.
	 * @param DragCoefficient Coefficient multiplied by the square of velocity to generate the drag force.
	 * @param GravityZ Gravity force.
	 * @param MaxSimTime Maximum simulation time for the virtual projectile.
	 * @param NumSubsteps How many sub-steps to use in the simulation (chopping up MaxSimTime). Recommended between 10 to 30 depending on desired quality versus performance.
	 * @param bTraceComplex Use TraceComplex (trace against triangles not primitives.
	 * @param DrawDebugType Debug type (one-frame, duration, persistent).
	 * @param TraceColor Debug color for traces that complete without a hit.
	 * @param TraceHitColor Debug color for traces that hit.
	 * @param DrawDebugTime Duration of debug lines (only relevant for DrawDebugType::Duration).
	 * @return True if hit something along the path.
	 */
	static bool PredictProjectilePathPointDrag(
		TArray<FVector>& PathPositions, FHitResult& OutHit, const FVector StartLocation, const FVector LaunchVelocity,
		const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes, const TArray<AActor*>& ActorsToIgnore,
		float DragCoefficient = 0.0f, float GravityZ = 98.0f, const float MaxSimTime = 2.0f, uint8 NumSubsteps = 15,
		bool bTraceComplex = false, EDrawDebugTrace::Type DrawDebugType = EDrawDebugTrace::Type::None,
		FLinearColor TraceColor = FLinearColor::Red, FLinearColor TraceHitColor = FLinearColor::Green,
		float DrawDebugTime = 0.0f);
};
