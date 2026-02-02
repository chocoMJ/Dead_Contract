#include "Actors/Managers/MapGenerationManager.h"
#include "Actors/Rooms/Room.h"


// Sets default values
AMapGenerationManager::AMapGenerationManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AMapGenerationManager::BeginPlay()
{
	GenerateRandomMap();
}

FVector2D AMapGenerationManager::GetRandomPointInCircle(float _radius)
{
	float theta = 2.f * PI * FMath::FRand();
	float u = FMath::FRand() + FMath::FRand();
	float r = (u > 1.f) ? 2.f - u : u;

	return FVector2D(
		_radius * r * cos(theta),
		_radius * r * sin(theta));
}

void AMapGenerationManager::GenerateRandomMap()
{
	FVector actorLocation = GetActorLocation();
	FVector SpawnLocation;

	for (int i = 0; i < numOfRoom; i++)
	{
		FVector2D randomLocation = GetRandomPointInCircle(radius);
		SpawnLocation = actorLocation + FVector(randomLocation, 0.f);

		// SpawnActor È£Ãâ
		GetWorld()->SpawnActor<ARoom>(
			RoomClass,
			SpawnLocation,
			FRotator::ZeroRotator
		);

		UE_LOG(LogTemp, Warning, TEXT("sequence : %d"), i);
	}
}
