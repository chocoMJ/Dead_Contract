#include "Actors/Managers/MapGenerationManager.h"
#include "Actors/Rooms/Room.h"
#include "Components/BoxComponent.h"
#include "DebugHelper.h"


// Sets default values
AMapGenerationManager::AMapGenerationManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AMapGenerationManager::BeginPlay()
{
	GenerateRandomMap();
	
	SeperateRooms();

	TArray<FVector2D> RoomPositions;
	for (ARoom* Room : Rooms)
	{
		FVector Pos = Room->GetActorLocation();
		RoomPositions.Add(FVector2D(Pos.X, Pos.Y));
	}

	TArray<FTriangle> Triangles = DelaunayTriangulation(RoomPositions);

	TArray<FRoomEdge> DelaunayEdges = TrianglesToEdges(Triangles);

	DrawEdges(DelaunayEdges);
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
		//Spawn할 location 지정
		FVector2D randomLocation = GetRandomPointInCircle(radius);
		SpawnLocation = actorLocation + FVector(randomLocation, 0.f);

		// SpawnActor 호출
		ARoom* room = GetWorld()->SpawnActor<ARoom>(
			RoomClass,
			SpawnLocation,
			FRotator::ZeroRotator
		);

		//Room 배열에 추가
		Rooms.Add(room);
	}
}



void AMapGenerationManager::SeperateRooms()
{
	const int32 MaxIterations = 100;
	const float PushStrength = 100.0f;

	//iter만큼 충돌감지 및 밀어내기 작업
	for (int iter = 0; iter < MaxIterations; iter++)
	{
		bool bOverlapping = false;
		
		for (int32 i = 0; i < Rooms.Num(); i++)
		{
			for (int32 j = i + 1; j < Rooms.Num(); j++)
			{
				ARoom* RoomA = Rooms[i];
				ARoom* RoomB = Rooms[j];
				// 겹치는지 확인
				if (AABBCollisionDetector(RoomA, RoomB))
				{
					
					// 방향 계산
					FVector Direction = RoomA->GetActorLocation() - RoomB->GetActorLocation();
					Direction.Z = 0; // Z축 무시

					// 같은 위치면 랜덤
					if (Direction.IsNearlyZero())
					{
						Direction = FVector(FMath::RandRange(-1.f, 1.f), FMath::RandRange(-1.f, 1.f), 0);
					}

					Direction.Normalize();

					// 서로 밀어내기
					RoomA->AddActorWorldOffset(Direction * PushStrength);
					RoomB->AddActorWorldOffset(-Direction * PushStrength);

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

bool AMapGenerationManager::AABBCollisionDetector(ARoom* RA, ARoom* RB)
{
	// Null 체크
	if (!RA || !RB || !RA->CollisionBox || !RB->CollisionBox)
	{
		return false;
	}

	// 두 방의 중심 위치
	FVector CenterA = RA->GetActorLocation();
	FVector CenterB = RB->GetActorLocation();

	// 두 방의 박스 크기
	FVector ExtentA = RA->CollisionBox->GetScaledBoxExtent() + 100.f;
	FVector ExtentB = RB->CollisionBox->GetScaledBoxExtent() + 100.f;

	// AABB 충돌 검사 (2축 모두 겹쳐야 충돌)
	bool bOverlapX = FMath::Abs(CenterA.X - CenterB.X) < (ExtentA.X + ExtentB.X);
	bool bOverlapY = FMath::Abs(CenterA.Y - CenterB.Y) < (ExtentA.Y + ExtentB.Y);

	return bOverlapX && bOverlapY;
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
