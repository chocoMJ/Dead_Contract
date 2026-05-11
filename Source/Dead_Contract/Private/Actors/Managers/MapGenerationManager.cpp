#include "Actors/Managers/MapGenerationManager.h"
#include "Actors/Rooms/Room.h"
#include "Components/BoxComponent.h"
#include "DataAssets/RoomTemplateDataAsset.h"
#include "DebugHelper.h"


// Sets default values
AMapGenerationManager::AMapGenerationManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AMapGenerationManager::BeginPlay()
{
	if (HasAuthority())
	{
		CreateGeneratedRoomsFromTemplates();

		SeperateRooms();

		UpdateRoomGridCenters();

		TArray<FVector2D> RoomPositions;
		for (FGeneratedRoom Room : GeneratedRooms)
		{
			FVector2D Pos = Room.Center;
			RoomPositions.Add(Pos);
		}

		SpawnRooms();

		TArray<FTriangle> Triangles = DelaunayTriangulation(RoomPositions);

		DelaunayEdges = TrianglesToEdges(Triangles);

		StartRoomIndex = FindStartRoomIndex();

		MSTEdges = ComputeMST(StartRoomIndex);

		FinalEdges = AddRandomEdges();

		DrawEdges(FinalEdges);
	}
}

void AMapGenerationManager::CreateGeneratedRoomsFromTemplates()
{
	GeneratedRooms.Empty();

	if (RoomTemplates.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("RoomTemplates is empty"));
		return;
	}

	for (int32 i = 0; i < numOfRoom; ++i)
	{
		URoomTemplateDataAsset* Template = RoomTemplates[FMath::RandRange(0, RoomTemplates.Num() - 1)];

		if (!Template)
		{
			continue;
		}

		FGeneratedRoom NewRoom;
		NewRoom.Template = Template;
		NewRoom.HalfExtent = Template->HalfExtent;
		NewRoom.Center = GetRandomPointInCircle(radius);

		GeneratedRooms.Add(NewRoom);
		UE_LOG(LogTemp, Warning, TEXT("Add!!"));
	}
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

void AMapGenerationManager::SeperateRooms()
{
	const int32 MaxIterations = 100;
	const float PushStrength = GridSize;

	//iter만큼 충돌감지 및 밀어내기 작업
	for (int iter = 0; iter < MaxIterations; iter++)
	{
		bool bOverlapping = false;

		for (FGeneratedRoom& Room : GeneratedRooms)
		{
			Room.Center = SnapToGrid(Room.Center);
		}
		
		for (int32 i = 0; i < GeneratedRooms.Num(); i++)
		{
			for (int32 j = i + 1; j < GeneratedRooms.Num(); j++)
			{
				FGeneratedRoom& RoomA = GeneratedRooms[i];
				FGeneratedRoom& RoomB = GeneratedRooms[j];

				// 겹치는지 확인
				if (AABBCollisionDetector(RoomA, RoomB))
				{
					
					FVector2D Delta = RoomA.Center - RoomB.Center;

					if (Delta.IsNearlyZero())
					{
						Delta = FVector2D(
							FMath::RandBool() ? 1.f : -1.f,
							FMath::RandBool() ? 1.f : -1.f
						);
					}

					if (FMath::Abs(Delta.X) > FMath::Abs(Delta.Y))
					{
						const float DirX = Delta.X > 0.f ? 1.f : -1.f;

						RoomA.Center.X += DirX * PushStrength;
						RoomB.Center.X -= DirX * PushStrength;
					}
					else
					{
						const float DirY = Delta.Y > 0.f ? 1.f : -1.f;

						RoomA.Center.Y += DirY * PushStrength;
						RoomB.Center.Y -= DirY * PushStrength;
					}

					bOverlapping = true;
				}
			}
		}

		if (!bOverlapping)
		{
			UE_LOG(LogTemp, Warning, TEXT("Separation finished at iteration %d"), iter);
			break;
		}

	}
}

FVector2D AMapGenerationManager::SnapToGrid(const FVector2D& WorldPosition) const
{
	const FVector ManagerLocation = GetActorLocation();

	// Manager를 원점으로 본 로컬 좌표
	const float LocalX = WorldPosition.X - ManagerLocation.X;
	const float LocalY = WorldPosition.Y - ManagerLocation.Y;

	// GridSize 단위로 반올림
	const float SnappedLocalX = FMath::RoundToFloat(LocalX / GridSize) * GridSize;
	const float SnappedLocalY = FMath::RoundToFloat(LocalY / GridSize) * GridSize;

	// 다시 월드 좌표로 변환
	return FVector2D(
		ManagerLocation.X + SnappedLocalX,
		ManagerLocation.Y + SnappedLocalY
	);
}

bool AMapGenerationManager::AABBCollisionDetector(const FGeneratedRoom& A, const FGeneratedRoom& B)
{
	const bool bOverlapX = FMath::Abs(A.Center.X - B.Center.X) < (A.HalfExtent.X + GridSize + B.HalfExtent.X + 100.f);
	const bool bOverlapY = FMath::Abs(A.Center.Y - B.Center.Y) < (A.HalfExtent.Y + GridSize + B.HalfExtent.Y + 100.f);

	return bOverlapX && bOverlapY;
}

int32 AMapGenerationManager::FindStartRoomIndex()
{
	float MaxDist = 0.f;
	int32 StartIndex = 0;
	FVector ManagerLocation = GetActorLocation();

	for (int32 i = 0; i < Rooms.Num(); i++)
	{
		FVector Pos = Rooms[i]->GetActorLocation();
		float Dist = 
			FVector2D(Pos.X - ManagerLocation.X, Pos.Y - ManagerLocation.Y).SizeSquared();

		if (Dist > MaxDist)
		{
			MaxDist = Dist;
			StartIndex = i;
		}
	}

	return StartIndex;
}

void AMapGenerationManager::SpawnRooms()
{
	for (const FGeneratedRoom& RoomData : GeneratedRooms)
	{
		if (!RoomData.Template)
		{
			UE_LOG(LogTemp, Warning, TEXT("RoomData.Template is null"));
			continue;
		}

		FVector SpawnLocation(RoomData.Center.X, RoomData.Center.Y, 0.f);
		FRotator SpawnRotation = FRotator::ZeroRotator;
		TSubclassOf<ARoom> RoomClass = RoomData.Template->RoomClass;

		if (!RoomClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("RoomClass is null in Template"));
			continue;
		}

		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ARoom* SpawnedRoom = GetWorld()->SpawnActor<ARoom>(
			RoomClass,
			SpawnLocation,
			SpawnRotation,
			Params
		);
		Rooms.Add(SpawnedRoom);
	}
}

FIntPoint AMapGenerationManager::WorldToGrid(const FVector2D& WorldPosition) const
{
	const FVector ManagerLocation = GetActorLocation();

	const float LocalX = WorldPosition.X - ManagerLocation.X;
	const float LocalY = WorldPosition.Y - ManagerLocation.Y;

	const int32 GridX = FMath::RoundToInt(LocalX / GridSize);
	const int32 GridY = FMath::RoundToInt(LocalY / GridSize);

	return FIntPoint(GridX, GridY);
}

FVector2D AMapGenerationManager::GridToWorld2D(const FIntPoint& GridPosition) const
{
	const FVector ManagerLocation = GetActorLocation();

	const float WorldX = ManagerLocation.X + GridPosition.X * GridSize;
	const float WorldY = ManagerLocation.Y + GridPosition.Y * GridSize;

	return FVector2D(WorldX, WorldY);
}

void AMapGenerationManager::UpdateRoomGridCenters()
{
	for (FGeneratedRoom& Room : GeneratedRooms)
	{
		Room.GridCenter = WorldToGrid(Room.Center);
	}
}

FIntRect AMapGenerationManager::GetRoomGridRect(const FGeneratedRoom& Room) const
{
	const int32 Width = Room.RoomSizeInGrid.X;
	const int32 Height = Room.RoomSizeInGrid.Y;

	const int32 MinX = Room.GridCenter.X - Width / 2;
	const int32 MinY = Room.GridCenter.Y - Height / 2;

	const int32 MaxX = MinX + Width;
	const int32 MaxY = MinY + Height;

	return FIntRect(
		FIntPoint(MinX, MinY),
		FIntPoint(MaxX, MaxY)
	);
}

TArray<FTriangle> AMapGenerationManager::DelaunayTriangulation(const TArray<FVector2D>& Nodes)
{
	if (Nodes.Num() < 3)
	{
		return TArray<FTriangle>();
	}

	//step1. super triangle 구하기
	//모든 점을 포함하는 바운딩 박스 구하기
	float MinX = Nodes[0].X, MinY = Nodes[0].Y;
	float MaxX = Nodes[0].X, MaxY = Nodes[0].Y;

	for (const FVector2D& Node : Nodes)
	{
		MinX = FMath::Min(MinX, Node.X);
		MinY = FMath::Min(MinY, Node.Y);
		MaxX = FMath::Max(MaxX, Node.X);
		MaxY = FMath::Max(MaxY, Node.Y);
	}

	float DX = MaxX - MinX;
	float DY = MaxY - MinY;
	float DMax = FMath::Max(DX, DY);
	float MidX = (MinX + MaxX) / 2.f;
	float MidY = (MinY + MaxY) / 2.f;

	//바운딩박스보다 훨씬 큰 삼각형 생성
	TArray<FVector2D> AllPoints = Nodes;
	int32 SuperVertex0 = AllPoints.Add(FVector2D(MidX - 10.f * DMax, MidY - DMax));
	int32 SuperVertex1 = AllPoints.Add(FVector2D(MidX, MidY + 10.f * DMax));
	int32 SuperVertex2 = AllPoints.Add(FVector2D(MidX + 10.f * DMax, MidY - DMax));

	TArray<FTriangle> Triangles;
	FTriangle SuperTriangle(SuperVertex0, SuperVertex1, SuperVertex2);
	CalculateCircumcircle(SuperTriangle, AllPoints);
	Triangles.Add(SuperTriangle); // 초기 삼각형으로 추가!

	//step 2. 각점을 하나씩 추가함.
	for (int32 PointIdx = 0; PointIdx < Nodes.Num(); ++PointIdx)
	{
		FVector2D Point = Nodes[PointIdx];
		TArray<FRoomEdge> Polygon;
		TArray<int32> TrianglesToRemove;

		//2.1 Node를 포함하는 삼각형 찾기
		for (int32 i = 0; i < Triangles.Num(); ++i)
		{
			if (IsPointInCircumcircle(Triangles[i], Point))
			{
				FRoomEdge Edge0(Triangles[i].Vertex0, Triangles[i].Vertex1, 0.f);
				FRoomEdge Edge1(Triangles[i].Vertex1, Triangles[i].Vertex2, 0.f);
				FRoomEdge Edge2(Triangles[i].Vertex2, Triangles[i].Vertex0, 0.f);

				Polygon.Add(Edge0);
				Polygon.Add(Edge1);
				Polygon.Add(Edge2);

				TrianglesToRemove.Add(i);
			}
		}

		// 2.2. Bad triangles 제거
		for (int32 i = TrianglesToRemove.Num() - 1; i >= 0; --i)
		{
			Triangles.RemoveAt(TrianglesToRemove[i]);
		}

		// 2.3. 공유간선 제거
		TArray<FRoomEdge> UniqueEdges;
		for (const FRoomEdge& Edge : Polygon)
		{
			int32 DuplicateCount = 0;
			for (const FRoomEdge& Other : Polygon)
			{
				if (Edge == Other)
				{
					DuplicateCount++;
				}
			}

			// 한 번만 등장하는 간선만 고려
			if (DuplicateCount == 1)
			{
				bool AlreadyAdded = false;
				for (const FRoomEdge& Unique : UniqueEdges)
				{
					if (Edge == Unique)
					{
						AlreadyAdded = true;
						break;
					}
				}

				if (!AlreadyAdded)
				{
					UniqueEdges.Add(Edge);
				}
			}
		}

		// 2.4. 새로운 삼각형 생성
		for (const FRoomEdge& Edge : UniqueEdges)
		{
			FTriangle NewTriangle(Edge.RoomIndexA, Edge.RoomIndexB, PointIdx);
			CalculateCircumcircle(NewTriangle, AllPoints);
			Triangles.Add(NewTriangle);
		}
	}

	// 3. Super Triangle 정점을 포함하는 삼각형 제거
	TArray<FTriangle> FinalTriangles;
	for (const FTriangle& Triangle : Triangles)
	{
		if (!Triangle.ContainsVertex(SuperVertex0) &&
			!Triangle.ContainsVertex(SuperVertex1) &&
			!Triangle.ContainsVertex(SuperVertex2))
		{
			FinalTriangles.Add(Triangle);
		}
	}

	return FinalTriangles;
}

void AMapGenerationManager::CalculateCircumcircle(FTriangle& Triangle, const TArray<FVector2D>& Points)
{
	//삼각형의 세 꼭짓점 가져오기
	FVector2D A = Points[Triangle.Vertex0];
	FVector2D B = Points[Triangle.Vertex1];
	FVector2D C = Points[Triangle.Vertex2];
	
	//변수 정의
	float a = B.X - A.X;
	float b = B.Y - A.Y;
	float c = C.X - A.X;
	float d = C.Y - A.Y;

	float ASq = A.X * A.X + A.Y * A.Y;
	float BSq = B.X * B.X + B.Y * B.Y;
	float CSq = C.X * C.X + C.Y * C.Y;

	float e = (BSq - ASq) * 0.5f;
	float f = (CSq - ASq) * 0.5f;

	//행렬식 구하기
	float D = a * d - b * c;

	//예외 계산
	if (FMath::Abs(D) < KINDA_SMALL_NUMBER)
	{
		Triangle.CircumradiusSquared = MAX_flt;
		return;
	}

	//크래머 공식
	float Ux = (d * e - b * f) / D;
	float Uy = (a * f - c * e) / D;

	//중심과 반지름 구하기
	Triangle.Circumcenter = FVector(Ux, Uy, 0.f);

	FVector2D center(Ux, Uy);
	Triangle.CircumradiusSquared =
		FVector2D::DistSquared(center, A);
}

bool AMapGenerationManager::IsPointInCircumcircle(const FTriangle& Triangle, const FVector2D& Point)
{
	FVector2D center(Triangle.Circumcenter.X, Triangle.Circumcenter.Y);
	float DistSquared = FVector2D::DistSquared(center, Point);
	return DistSquared < Triangle.CircumradiusSquared;
}

TArray<FRoomEdge> AMapGenerationManager::TrianglesToEdges(const TArray<FTriangle>& Triangles)
{
	TArray<FRoomEdge> Edges;
	TSet<FString> EdgeSet;  // 중복 방지용

	// 모든 삼각형 순회
	for (const FTriangle& Triangle : Triangles)
	{
		// 각 삼각형의 3개 간선 추출
		auto AddEdge = [&](int32 A, int32 B)
			{
				int32 MinIdx = FMath::Min(A, B);
				int32 MaxIdx = FMath::Max(A, B);
				FString Key = FString::Printf(TEXT("%d-%d"), MinIdx, MaxIdx);

				// 중복 체크
				if (!EdgeSet.Contains(Key))
				{
					EdgeSet.Add(Key);

					// 거리 계산
					float Distance = FVector::Dist(
						Rooms[A]->GetActorLocation(),
						Rooms[B]->GetActorLocation()
					);

					Edges.Add(FRoomEdge(A, B, Distance));
				}
			};

		// 삼각형의 3개 간선 추가
		AddEdge(Triangle.Vertex0, Triangle.Vertex1);  // AB
		AddEdge(Triangle.Vertex1, Triangle.Vertex2);  // BC
		AddEdge(Triangle.Vertex2, Triangle.Vertex0);  // CA
	}

	return Edges;
}

TArray<FRoomEdge> AMapGenerationManager::ComputeMST(int32 StartIndex)
{
	int32 NumNodes = Rooms.Num();

	TArray<FPrimNode> Nodes;
	Nodes.SetNum(NumNodes);

	// 시작 노드 설정
	Nodes[StartIndex].MinDistance = 0.f;
	Nodes[StartIndex].DistFromStart = 0.f;

	TArray<TPair<float, int32>> Heap; //mindistance와 NodeIndex를 넣어줄 힙
	Heap.Add(TPair<float, int32>(0.f, StartIndex));

	while (Heap.Num() > 0)
	{
		// 힙에서 가장 가까운 노드 꺼내기
		Heap.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) {
			return A.Key < B.Key;
			});

		TPair<float, int32> Top = Heap[0];
		Heap.RemoveAt(0);

		int32 CurrentIndex = Top.Value;

		// 중복 처리
		if (Nodes[CurrentIndex].bInMST) continue;
		Nodes[CurrentIndex].bInMST = true;

		// 현재 노드의 인접 간선 탐색
		for (const FRoomEdge& Edge : DelaunayEdges)
		{
			int32 NeighborIndex = -1;

			if (Edge.RoomIndexA == CurrentIndex)
				NeighborIndex = Edge.RoomIndexB;
			else if (Edge.RoomIndexB == CurrentIndex)
				NeighborIndex = Edge.RoomIndexA;
			else
				continue;

			// 이미 MST에 있으면 스킵
			if (Nodes[NeighborIndex].bInMST) continue;

			// 노드와 트리간 거리 갱신
			if (Edge.Distance < Nodes[NeighborIndex].MinDistance)
			{
				Nodes[NeighborIndex].MinDistance = Edge.Distance;
				Nodes[NeighborIndex].ParentIndex = CurrentIndex;
				Nodes[NeighborIndex].DistFromStart = Nodes[CurrentIndex].DistFromStart + Edge.Distance;

				Heap.Add(TPair<float, int32>(Edge.Distance, NeighborIndex));
			}
		}

		if (Nodes[CurrentIndex].ParentIndex != -1)
		{
			FRoomEdge NewEdge(
				Nodes[CurrentIndex].ParentIndex,
				CurrentIndex,
				Nodes[CurrentIndex].MinDistance
			);
			MSTEdges.Add(NewEdge);
		}
	}

	// MST 완성 후 leaf 노드 중 가장 먼 노드 = 보스방
	// leaf 노드를 찾기 위한 degree 계산
	TArray<int32> Degree;
	Degree.SetNum(NumNodes);

	for (const FRoomEdge& Edge : MSTEdges)
	{
		Degree[Edge.RoomIndexA]++;
		Degree[Edge.RoomIndexB]++;
	}

	// leaf 노드 중 DistFromStart 최대값 = 보스방
	float MaxDist = 0.f;
	BossRoomIndex = -1;

	for (int32 i = 0; i < NumNodes; i++)
	{
		if (Degree[i] == 1 && Nodes[i].DistFromStart > MaxDist)
		{
			MaxDist = Nodes[i].DistFromStart;
			BossRoomIndex = i;
		}
	}

	return MSTEdges;
}

TArray<FRoomEdge> AMapGenerationManager::AddRandomEdges()
{
	TArray<FRoomEdge> Result = MSTEdges;

	for (const FRoomEdge& Edge : DelaunayEdges)
	{
		// MST에 이미 있는 엣지면 스킵
		bool bAlreadyInMST = false;
		for (const FRoomEdge& MSTEdge : MSTEdges)
		{
			if (Edge == MSTEdge)
			{
				bAlreadyInMST = true;
				break;
			}
		}

		if (bAlreadyInMST) continue;

		// 20% 확률로 추가
		if (FMath::FRand() < 0.2f)
		{
			Result.Add(Edge);
		}
	}

	return Result;
}

void AMapGenerationManager::DrawEdges(const TArray<FRoomEdge>& Edges)
{
	for (int32 i = 0; i < Edges.Num(); ++i)
	{
		const FRoomEdge& Edge = Edges[i];

		// 두 방의 위치
		FVector StartPos = Rooms[Edge.RoomIndexA]->GetActorLocation();
		FVector EndPos = Rooms[Edge.RoomIndexB]->GetActorLocation();

		// 선 그리기
		DrawDebugLine(
			GetWorld(),
			StartPos,
			EndPos,
			FColor::Cyan,        // 하늘색
			false,               // Persistent (false = 시간제한 있음)
			30.0f,               // 30초간 표시
			0,                   // Depth priority
			5.0f                 // 두께
		);

		// 중점에 번호 표시 (옵션)
		FVector MidPoint = (StartPos + EndPos) / 2.0f;
		DrawDebugString(
			GetWorld(),
			MidPoint,
			FString::Printf(TEXT("E%d"), i),  // Edge 번호
			nullptr,
			FColor::White,
			30.0f,
			true,
			1.0f
		);
	}
}


//void AMapGenerationManager::GenerateRandomMap()
//{
//	FVector actorLocation = GetActorLocation();
//	FVector SpawnLocation;
//
//	for (int i = 0; i < numOfRoom; i++)
//	{
//		//Spawn할 location 지정
//		FVector2D randomLocation = GetRandomPointInCircle(radius);
//		SpawnLocation = actorLocation + FVector(randomLocation, 0.f);
//
//		// SpawnActor 호출
//		ARoom* room = GetWorld()->SpawnActor<ARoom>(
//			RoomClass,
//			SpawnLocation,
//			FRotator::ZeroRotator
//		);
//
//		//Room 배열에 추가
//		Rooms.Add(room);
//	}
//}

//bool AMapGenerationManager::AABBCollisionDetector(ARoom* RA, ARoom* RB)
//{
//	// Null 체크
//	if (!RA || !RB || !RA->CollisionBox || !RB->CollisionBox)
//	{
//		return false;
//	}
//
//	// 두 방의 중심 위치
//	FVector CenterA = RA->GetActorLocation();
//	FVector CenterB = RB->GetActorLocation();
//
//	// 두 방의 박스 크기
//	FVector ExtentA = RA->CollisionBox->GetScaledBoxExtent() + 100.f;
//	FVector ExtentB = RB->CollisionBox->GetScaledBoxExtent() + 100.f;
//
//	// AABB 충돌 검사 (2축 모두 겹쳐야 충돌)
//	bool bOverlapX = FMath::Abs(CenterA.X - CenterB.X) < (ExtentA.X + ExtentB.X);
//	bool bOverlapY = FMath::Abs(CenterA.Y - CenterB.Y) < (ExtentA.Y + ExtentB.Y);
//
//	return bOverlapX && bOverlapY;
//}