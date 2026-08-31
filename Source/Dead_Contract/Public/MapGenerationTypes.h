#pragma once

#include "CoreMinimal.h"
#include "MapGenerationTypes.generated.h"

class URoomTemplateDataAsset;
class ARoom;
enum class ERoomSocketDirection : uint8;

UENUM(BlueprintType)
enum class EGridCellType : uint8
{
    Empty,
    Room,
    Corridor,
    Door,
    Wall
};

enum class EGridConnectionMask : uint8
{
	None = 0,
	North = 1 << 0,
	East = 1 << 1,
	South = 1 << 2,
	West = 1 << 3
};

USTRUCT()
struct FRoomEdge
{
	GENERATED_BODY()

	int32 RoomIndexA;
	int32 RoomIndexB;
	float Distance;
	FIntPoint DoorA = FIntPoint::ZeroValue;
	FIntPoint DoorB = FIntPoint::ZeroValue;
	ERoomSocketDirection DoorDirectionA{};
	ERoomSocketDirection DoorDirectionB{};
	FName DoorComponentA = NAME_None;
	FName DoorComponentB = NAME_None;
	bool bHasDoors = false;

	FRoomEdge() : RoomIndexA(-1), RoomIndexB(-1), Distance(0.f) {}

	FRoomEdge(int32 A, int32 B, float Dist)
		: RoomIndexA(A), RoomIndexB(B), Distance(Dist) {
	}

	bool operator==(const FRoomEdge& Other) const
	{
		return (RoomIndexA == Other.RoomIndexA && RoomIndexB == Other.RoomIndexB) ||
			(RoomIndexA == Other.RoomIndexB && RoomIndexB == Other.RoomIndexA);
	}
};

USTRUCT()
struct FTriangle
{
	GENERATED_BODY()

	int32 Vertex0;
	int32 Vertex1;
	int32 Vertex2;
	FVector Circumcenter;
	float CircumradiusSquared;

	FTriangle() : Vertex0(-1), Vertex1(-1), Vertex2(-1), CircumradiusSquared(0.f) {}

	FTriangle(int32 V0, int32 V1, int32 V2)
		: Vertex0(V0), Vertex1(V1), Vertex2(V2)
	{
		CircumradiusSquared = 0.f;
	}

	bool ContainsVertex(int32 VertexIndex) const
	{
		return Vertex0 == VertexIndex || Vertex1 == VertexIndex || Vertex2 == VertexIndex;
	}
};

USTRUCT()
struct FPrimNode
{
	GENERATED_BODY()

	float MinDistance;
	float DistFromStart;
	int32 ParentIndex;
	bool bInMST;

	FPrimNode() :
		MinDistance(MAX_flt), DistFromStart(MAX_flt), ParentIndex(-1), bInMST(false) {
	}
};

USTRUCT()
struct FGeneratedRoom
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<class URoomTemplateDataAsset> Template = nullptr;

	UPROPERTY()
	FVector2D Center = FVector2D::ZeroVector;
 
	UPROPERTY()
	FVector2D HalfExtent = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere)
	FIntPoint GridCenter = FIntPoint::ZeroValue;

	UPROPERTY(EditAnywhere)
	FIntPoint RoomSizeInGrid = FIntPoint(5, 5);

	UPROPERTY(EditAnywhere)
	TSubclassOf<ARoom> RoomClass;

	UPROPERTY()
	AActor* SpawnedRoom = nullptr;
};
