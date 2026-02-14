#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MapGenerationManager.generated.h"

class ARoom;

UCLASS()
class DEAD_CONTRACT_API AMapGenerationManager : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AMapGenerationManager();

protected:
	virtual void BeginPlay() override;

private:
	FVector Center;

	TMap<FIntPoint, TArray<ARoom*>> Grid;

	UPROPERTY(EditAnywhere)
	float radius;

	UPROPERTY(EditAnywhere)
	int32 numOfRoom;

	UPROPERTY(EditAnywhere)
	TSubclassOf<ARoom> RoomClass;

	UPROPERTY()
	TArray<ARoom*> Rooms;

	FVector2D GetRandomPointInCircle(float radius);

	void GenerateRandomMap();

	void NaiveSeperateRooms();

	bool AABBCollisionDetector(ARoom* RA, ARoom* RB);
};

