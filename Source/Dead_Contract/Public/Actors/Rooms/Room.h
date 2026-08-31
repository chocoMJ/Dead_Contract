// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Room.generated.h"

class UBoxComponent;

UENUM(BlueprintType)
enum class ERoomSocketDirection : uint8
{
    North,
    East,
    South,
    West
};

USTRUCT(BlueprintType)
struct FRoomSocket
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName Id;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    ERoomSocketDirection Direction = ERoomSocketDirection::North;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FIntPoint GridOffset = FIntPoint::ZeroValue;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName ComponentName;

};

UCLASS()
class DEAD_CONTRACT_API ARoom : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ARoom();

	UPROPERTY(VisibleAnywhere)
	UBoxComponent* CollisionBox;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
	FIntPoint RoomSizeInGrid = FIntPoint(5,5);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Sockets")
    TArray<FRoomSocket> Sockets;
};
