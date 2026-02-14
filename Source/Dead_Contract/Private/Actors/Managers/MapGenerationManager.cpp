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
	
	NaiveSeperateRooms();
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



void AMapGenerationManager::NaiveSeperateRooms()
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