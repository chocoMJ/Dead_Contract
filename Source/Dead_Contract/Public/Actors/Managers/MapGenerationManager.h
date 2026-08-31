#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MapGenerationTypes.h"
#include "MapGenerationManager.generated.h"

class ARoom;
enum class ERoomSocketDirection : uint8;
struct FRoomSocket;


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
	TMap<FIntPoint, EGridCellType> GridMap;

	FVector Center;

	int32 StartRoomIndex;
	int32 BossRoomIndex;

	UPROPERTY(EditAnywhere)
	float radius;

	UPROPERTY(EditAnywhere)
	int32 numOfRoom;

	UPROPERTY()
	TArray<ARoom*> Rooms;

	UPROPERTY()
	TArray<FRoomEdge> DelaunayEdges;

	UPROPERTY()
	TArray<FRoomEdge> MSTEdges;

	UPROPERTY()
	TArray<FRoomEdge> FinalEdges;

	UPROPERTY(EditAnywhere, Category = "Generation")
	TArray<TObjectPtr<class URoomTemplateDataAsset>> RoomTemplates;

	UPROPERTY()
	TArray<FGeneratedRoom> GeneratedRooms;

	UPROPERTY(EditAnywhere, Category = "Map Generation|Grid")
	float GridSize = 400.0f;

	UPROPERTY(EditAnywhere, Category = "Map Generation|Corridor")
	TSubclassOf<AActor> StraightCorridorClass;

	UPROPERTY(EditAnywhere, Category = "Map Generation|Corridor")
	TSubclassOf<AActor> CornerCorridorClass;

	UPROPERTY(EditAnywhere, Category = "Map Generation|Corridor")
	TSubclassOf<AActor> TJunctionCorridorClass;

	UPROPERTY(EditAnywhere, Category = "Map Generation|Corridor")
	TSubclassOf<AActor> CrossCorridorClass;

	UPROPERTY(EditAnywhere, Category = "Map Generation|Corridor")
	TSubclassOf<AActor> DeadEndCorridorClass;

	UPROPERTY()
	TArray<AActor*> SpawnedCorridors;

	// Room object random placement and separation
	void CreateGeneratedRoomsFromTemplates();
	FVector2D GetRandomPointInCircle(float radius);
	void SeperateRooms();
	FVector2D SnapToGrid(const FVector2D& WorldPosition) const;
	bool AABBCollisionDetector(const FGeneratedRoom& A, const FGeneratedRoom& B);
	int32 FindStartRoomIndex();
	void SpawnRooms();

	// Grid based placement
	FIntPoint WorldToGrid(const FVector2D& WorldPosition) const;
	FVector2D GridToWorld2D(const FIntPoint& GridPosition) const;
	void UpdateRoomGridCenters();
	FIntRect GetRoomGridRect(const FGeneratedRoom& Room) const;
	void BuildGridMapFromRooms();

	// Delaunay triangulation
	TArray<FTriangle> DelaunayTriangulation(const TArray<FVector2D>& Nodes);
	void CalculateCircumcircle(FTriangle& Triangle, const TArray<FVector2D>& Points);
	bool IsPointInCircumcircle(const FTriangle& Triangle, const FVector2D& Point);
	TArray<FRoomEdge> TrianglesToEdges(const TArray<FTriangle>& Triangles);

	// MST
	TArray<FRoomEdge> ComputeMST(int32 StartIndex);
	TArray<FRoomEdge> AddRandomEdges();

	void BuildRoomConnections();
	ERoomSocketDirection GetDirectionFromDelta(const FIntPoint& Delta) const;
	ERoomSocketDirection GetOppositeDirection(ERoomSocketDirection Direction) const;
	FIntPoint DirectionToGridOffset(ERoomSocketDirection Direction) const;
	bool FindSocketForDirection(const FGeneratedRoom& Room, ERoomSocketDirection Direction, FRoomSocket& OutSocket) const;
	void OpenRoomDoors();
	bool DisableDoorComponent(ARoom* Room, const FName& ComponentName) const;
	void BuildCorridors();
	TArray<FIntPoint> BuildLPath(const FIntPoint& Start, const FIntPoint& End, bool bHorizontalFirst) const;
	int32 CalculatePathCost(const TArray<FIntPoint>& Path) const;
	void ApplyCorridorPath(const TArray<FIntPoint>& Path);
	uint8 GetCellConnectionMask(const FIntPoint& Cell) const;
	bool GetCorridorClassAndRotationFromMask(uint8 Mask, TSubclassOf<AActor>& OutClass, FRotator& OutRotation) const;
	void SpawnCorridors();

	// Debug
	void DrawEdges(const TArray<FRoomEdge>& Edges);
	void DrawGridMap() const;
};
