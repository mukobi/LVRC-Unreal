// Fill out your copyright notice in the Description page of Project Settings.


#include "LVRCStatics.h"

bool ULVRCStatics::PredictProjectilePathPointDrag(
	TArray<FVector>& PathPositions, FHitResult& OutHit,
	const UObject* WorldContextObject, const FVector StartLocation, const FVector LaunchVelocity,
	const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes, const TArray<AActor*>& ActorsToIgnore,
	float DragCoefficient, float GravityZ,
	const float MaxSimTime, uint8 NumSubsteps, bool bTraceComplex, EDrawDebugTrace::Type DrawDebugType,
	const FLinearColor TraceColor, const FLinearColor TraceHitColor, const float DrawDebugTime)
{
	PathPositions.Reset(NumSubsteps + 1);

	UWorld const* const World = GEngine->GetWorldFromContextObject(
		WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);

	if (World)
	{
		PathPositions.Add(StartLocation);
		FVector CurrentVel = LaunchVelocity;
		FVector TraceStart = StartLocation;
		FVector TraceEnd = TraceStart;
		float CurrentTime = 0.f;
		const float SubstepDeltaTime = MaxSimTime / NumSubsteps;

		while (CurrentTime < MaxSimTime)
		{
			// Limit step to not go further than total time.
			const float ActualStepDeltaTime = FMath::Min(MaxSimTime - CurrentTime, SubstepDeltaTime);

			// Integrate (modified Velocity Verlet method, see https://web.physics.wustl.edu/~wimd/topic01.pdf)
			TraceStart = TraceEnd;
			FVector Acceleration = FVector(0.f, 0.f, GravityZ)
				+ DragCoefficient * -CurrentVel.GetUnsafeNormal();
			CurrentVel = CurrentVel + Acceleration * ActualStepDeltaTime;
			TraceEnd = TraceStart + (CurrentVel * ActualStepDeltaTime)
				+ (0.5 * Acceleration * ActualStepDeltaTime * ActualStepDeltaTime);

			// Trace this segment
			if (UKismetSystemLibrary::LineTraceSingleForObjects(
				WorldContextObject, TraceStart, TraceEnd, ObjectTypes, bTraceComplex, ActorsToIgnore, DrawDebugType,
				OutHit, true, TraceColor, TraceHitColor, DrawDebugTime))
			{
				// Hit! We are done. Choose trace with earliest hit time.
				PathPositions.Add(OutHit.Location);
				return true;
			}

			PathPositions.Add(TraceEnd);

			// Advance time
			CurrentTime += ActualStepDeltaTime;
		}
	}
	return false;
}
