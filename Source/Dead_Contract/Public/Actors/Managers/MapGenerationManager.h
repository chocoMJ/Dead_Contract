#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MapGenerationManager.generated.h"

class ARoom;

USTRUCT()
struct FRoomEdge
{
	GENERATED_BODY()

	int32 RoomIndexA;
	int32 RoomIndexB;
	float Distance;

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
		MinDistance(MAX_flt), DistFromStart(MAX_flt), ParentIndex(-1), bInMST(false) {}
};

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

	int32 StartRoomIndex;
	int32 BossRoomIndex;

	UPROPERTY(EditAnywhere)
	float radius;

	UPROPERTY(EditAnywhere)
	int32 numOfRoom;

	UPROPERTY(EditAnywhere)
	TSubclassOf<ARoom> RoomClass;

	UPROPERTY()
	TArray<ARoom*> Rooms;

	UPROPERTY()
	TArray<FRoomEdge> DelaunayEdges;

	UPROPERTY()
	TArray<FRoomEdge> MSTEdges;

	UPROPERTY()
	TArray<FRoomEdge> FinalEdges;

	//Room 객체 랜덤 배치 및 분할
	FVector2D GetRandomPointInCircle(float radius);
	void GenerateRandomMap();
	void SeperateRooms();
	bool AABBCollisionDetector(ARoom* RA, ARoom* RB);
	int32 FindStartRoomIndex();

	//들로네 공간분할
	TArray<FTriangle> DelaunayTriangulation(const TArray<FVector2D>& Nodes);
	void CalculateCircumcircle(FTriangle& Triangle, const TArray<FVector2D>& Points);
	bool IsPointInCircumcircle(const FTriangle& Triangle, const FVector2D& Point);
	TArray<FRoomEdge> TrianglesToEdges(const TArray<FTriangle>& Triangles);

	//MST 계산
	TArray<FRoomEdge> ComputeMST(int32 StartIndex);
	TArray<FRoomEdge> AddRandomEdges();

	void DrawEdges(const TArray<FRoomEdge>& Edges);
};

