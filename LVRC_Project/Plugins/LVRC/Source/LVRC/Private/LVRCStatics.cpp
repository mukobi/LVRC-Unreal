// Fill out your copyright notice in the Description page of Project Settings.


#include "LVRCStatics.h"

bool ULVRCStatics::PredictProjectilePathPointDrag(TArray<FVector>& PathPositions, FHitResult& OutHit,
	const FVector StartLocation, const FVector LaunchVelocity, const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes,
	const TArray<AActor*>& ActorsToIgnore, float DragCoefficient, float GravityZ, const float MaxSimTime,
	uint8 NumSubsteps, bool bTraceComplex, EDrawDebugTrace::Type DrawDebugType, FLinearColor TraceColor,
	FLinearColor TraceHitColor, float DrawDebugTime)
{

	
	return false;
}
