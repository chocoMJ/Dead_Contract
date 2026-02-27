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
	const float PushStrength = 500.0f;

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
	FVector ExtentA = RA->CollisionBox->GetScaledBoxExtent();
	FVector ExtentB = RB->CollisionBox->GetScaledBoxExtent();

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
	int32 SuperVertex0 = AllPoints.Add(FVector2D(MidX - 3.f * DMax, MidY - DMax));
	int32 SuperVertex1 = AllPoints.Add(FVector2D(MidX, MidY + 3.f * DMax));
	int32 SuperVertex2 = AllPoints.Add(FVector2D(MidX + 3.f * DMax, MidY - DMax));

	TArray<FTriangle> Triangles;
	FTriangle SuperTriangle(SuperVertex0, SuperVertex1, SuperVertex2);
	CalculateCircumcircle(SuperTriangle, AllPoints);
	Triangles.Add(SuperTriangle); // 초기 삼각형으로 추가!

	return Triangles;

	//각점 추가 로직 필요
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
