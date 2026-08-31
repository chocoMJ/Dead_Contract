#include "Actors/Managers/MapGenerationManager.h"
#include "Actors/Rooms/Room.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
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

		BuildGridMapFromRooms();

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

		BuildRoomConnections();
		OpenRoomDoors();

		BuildCorridors();
		SpawnCorridors();
		DrawGridMap();
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

	const int32 MaxAttempts = numOfRoom * 10;
	int32 Attempts = 0;

	while (GeneratedRooms.Num() < numOfRoom && Attempts < MaxAttempts)
	{
		++Attempts;

		URoomTemplateDataAsset* Template = RoomTemplates[FMath::RandRange(0, RoomTemplates.Num() - 1)];

		if (!Template || !Template->RoomClass)
		{
			continue;
		}

		const ARoom* RoomCDO = Template->RoomClass->GetDefaultObject<ARoom>();
		if (!RoomCDO)
		{
			continue;
		}

		FGeneratedRoom NewRoom;
		NewRoom.Template = Template;

		NewRoom.RoomSizeInGrid = RoomCDO->RoomSizeInGrid;

		NewRoom.HalfExtent = FVector2D(
			NewRoom.RoomSizeInGrid.X * GridSize * 0.5f,
			NewRoom.RoomSizeInGrid.Y * GridSize * 0.5f
		);

		const FVector ManagerLocation = GetActorLocation();
		const FVector2D Offset = GetRandomPointInCircle(radius);
		NewRoom.Center = FVector2D(ManagerLocation.X, ManagerLocation.Y) + Offset;

		NewRoom.RoomClass = Template->RoomClass;

		GeneratedRooms.Add(NewRoom);
	}

	UE_LOG(LogTemp, Warning, TEXT("Requested Rooms: %d, Generated Rooms: %d, Attempts: %d"), numOfRoom, GeneratedRooms.Num(), Attempts);
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

	for (int iter = 0; iter < MaxIterations; iter++)
	{
		bool bOverlapping = false;

		for (int32 i = 0; i < GeneratedRooms.Num(); i++)
		{
			for (int32 j = i + 1; j < GeneratedRooms.Num(); j++)
			{
				FGeneratedRoom& RoomA = GeneratedRooms[i];
				FGeneratedRoom& RoomB = GeneratedRooms[j];

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

					Delta.Normalize();

					RoomA.Center += Delta * PushStrength;
					RoomB.Center -= Delta * PushStrength;

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

	for (FGeneratedRoom& Room : GeneratedRooms)
	{
		Room.Center = SnapToGrid(Room.Center);
	}
}

FVector2D AMapGenerationManager::SnapToGrid(const FVector2D& WorldPosition) const
{
	const FVector ManagerLocation = GetActorLocation();

	const float LocalX = WorldPosition.X - ManagerLocation.X;
	const float LocalY = WorldPosition.Y - ManagerLocation.Y;

	const float SnappedLocalX = FMath::RoundToFloat(LocalX / GridSize) * GridSize;
	const float SnappedLocalY = FMath::RoundToFloat(LocalY / GridSize) * GridSize;

	return FVector2D(
		ManagerLocation.X + SnappedLocalX,
		ManagerLocation.Y + SnappedLocalY
	);
}

bool AMapGenerationManager::AABBCollisionDetector(const FGeneratedRoom& A, const FGeneratedRoom& B)
{
	const bool bOverlapX = FMath::Abs(A.Center.X - B.Center.X) < (A.HalfExtent.X + GridSize + B.HalfExtent.X + GridSize);
	const bool bOverlapY = FMath::Abs(A.Center.Y - B.Center.Y) < (A.HalfExtent.Y + GridSize + B.HalfExtent.Y + GridSize);

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
	Rooms.Empty();

	for (const FGeneratedRoom& RoomData : GeneratedRooms)
	{
		if (!RoomData.Template)
		{
			UE_LOG(LogTemp, Warning, TEXT("RoomData.Template is null"));
			continue;
		}

		FVector SpawnLocation(RoomData.Center.X, RoomData.Center.Y, 0.f);
		FRotator SpawnRotation = FRotator::ZeroRotator;
		TSubclassOf<ARoom> RoomClass = RoomData.RoomClass;

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

		if (SpawnedRoom)
		{
			Rooms.Add(SpawnedRoom);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("Spawned Rooms: %d"), Rooms.Num());
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

void AMapGenerationManager::BuildGridMapFromRooms()
{
	GridMap.Empty();

	for (const FGeneratedRoom& Room : GeneratedRooms) {
		const FIntRect Rect = GetRoomGridRect(Room);

		for (int32 Y = Rect.Min.Y; Y < Rect.Max.Y; ++Y) {
			for (int32 X = Rect.Min.X; X < Rect.Max.X; ++X) {
				const bool bIsWall =
					X == Rect.Min.X ||
					X == Rect.Max.X - 1 ||
					Y == Rect.Min.Y ||
					Y == Rect.Max.Y - 1;

				GridMap.Add(
					FIntPoint(X, Y),
					bIsWall ? EGridCellType::Wall : EGridCellType::Room
				);
			}
		}
	}
}

TArray<FTriangle> AMapGenerationManager::DelaunayTriangulation(const TArray<FVector2D>& Nodes)
{
	if (Nodes.Num() < 3)
	{
		return TArray<FTriangle>();
	}

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

	TArray<FVector2D> AllPoints = Nodes;
	int32 SuperVertex0 = AllPoints.Add(FVector2D(MidX - 10.f * DMax, MidY - DMax));
	int32 SuperVertex1 = AllPoints.Add(FVector2D(MidX, MidY + 10.f * DMax));
	int32 SuperVertex2 = AllPoints.Add(FVector2D(MidX + 10.f * DMax, MidY - DMax));

	TArray<FTriangle> Triangles;
	FTriangle SuperTriangle(SuperVertex0, SuperVertex1, SuperVertex2);
	CalculateCircumcircle(SuperTriangle, AllPoints);
	Triangles.Add(SuperTriangle);

	for (int32 PointIdx = 0; PointIdx < Nodes.Num(); ++PointIdx)
	{
		FVector2D Point = Nodes[PointIdx];
		TArray<FRoomEdge> Polygon;
		TArray<int32> TrianglesToRemove;

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

		for (int32 i = TrianglesToRemove.Num() - 1; i >= 0; --i)
		{
			Triangles.RemoveAt(TrianglesToRemove[i]);
		}

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

		for (const FRoomEdge& Edge : UniqueEdges)
		{
			FTriangle NewTriangle(Edge.RoomIndexA, Edge.RoomIndexB, PointIdx);
			CalculateCircumcircle(NewTriangle, AllPoints);
			Triangles.Add(NewTriangle);
		}
	}

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
	FVector2D A = Points[Triangle.Vertex0];
	FVector2D B = Points[Triangle.Vertex1];
	FVector2D C = Points[Triangle.Vertex2];
	
	float a = B.X - A.X;
	float b = B.Y - A.Y;
	float c = C.X - A.X;
	float d = C.Y - A.Y;

	float ASq = A.X * A.X + A.Y * A.Y;
	float BSq = B.X * B.X + B.Y * B.Y;
	float CSq = C.X * C.X + C.Y * C.Y;

	float e = (BSq - ASq) * 0.5f;
	float f = (CSq - ASq) * 0.5f;

	float D = a * d - b * c;

	if (FMath::Abs(D) < KINDA_SMALL_NUMBER)
	{
		Triangle.CircumradiusSquared = MAX_flt;
		return;
	}

	float Ux = (d * e - b * f) / D;
	float Uy = (a * f - c * e) / D;

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
	TSet<FString> EdgeSet;

	for (const FTriangle& Triangle : Triangles)
	{
		auto AddEdge = [&](int32 A, int32 B)
			{
				int32 MinIdx = FMath::Min(A, B);
				int32 MaxIdx = FMath::Max(A, B);
				FString Key = FString::Printf(TEXT("%d-%d"), MinIdx, MaxIdx);

				if (!EdgeSet.Contains(Key))
				{
					EdgeSet.Add(Key);

					float Distance = FVector::Dist(
						Rooms[A]->GetActorLocation(),
						Rooms[B]->GetActorLocation()
					);

					Edges.Add(FRoomEdge(A, B, Distance));
				}
			};

		AddEdge(Triangle.Vertex0, Triangle.Vertex1);
		AddEdge(Triangle.Vertex1, Triangle.Vertex2);
		AddEdge(Triangle.Vertex2, Triangle.Vertex0);
	}

	return Edges;
}

TArray<FRoomEdge> AMapGenerationManager::ComputeMST(int32 StartIndex)
{
	int32 NumNodes = Rooms.Num();

	TArray<FPrimNode> Nodes;
	Nodes.SetNum(NumNodes);

	Nodes[StartIndex].MinDistance = 0.f;
	Nodes[StartIndex].DistFromStart = 0.f;

	TArray<TPair<float, int32>> Heap;
	Heap.Add(TPair<float, int32>(0.f, StartIndex));

	while (Heap.Num() > 0)
	{
		Heap.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) {
			return A.Key < B.Key;
			});

		TPair<float, int32> Top = Heap[0];
		Heap.RemoveAt(0);

		int32 CurrentIndex = Top.Value;

		if (Nodes[CurrentIndex].bInMST) continue;
		Nodes[CurrentIndex].bInMST = true;

		for (const FRoomEdge& Edge : DelaunayEdges)
		{
			int32 NeighborIndex = -1;

			if (Edge.RoomIndexA == CurrentIndex)
				NeighborIndex = Edge.RoomIndexB;
			else if (Edge.RoomIndexB == CurrentIndex)
				NeighborIndex = Edge.RoomIndexA;
			else
				continue;

			if (Nodes[NeighborIndex].bInMST) continue;

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

	TArray<int32> Degree;
	Degree.SetNum(NumNodes);

	for (const FRoomEdge& Edge : MSTEdges)
	{
		Degree[Edge.RoomIndexA]++;
		Degree[Edge.RoomIndexB]++;
	}

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

		if (FMath::FRand() < 0.2f)
		{
			Result.Add(Edge);
		}
	}

	return Result;
}

void AMapGenerationManager::BuildRoomConnections()
{
	for (FRoomEdge& Edge : FinalEdges) {
		if (!GeneratedRooms.IsValidIndex(Edge.RoomIndexA) ||
			!GeneratedRooms.IsValidIndex(Edge.RoomIndexB))
		{
			continue;
		}

		const FGeneratedRoom& RoomA = GeneratedRooms[Edge.RoomIndexA];
		const FGeneratedRoom& RoomB = GeneratedRooms[Edge.RoomIndexB];

		const FIntPoint Delta = RoomB.GridCenter - RoomA.GridCenter;

		const ERoomSocketDirection DirectionA = GetDirectionFromDelta(Delta);
		const ERoomSocketDirection DirectionB = GetOppositeDirection(DirectionA);

		FRoomSocket SocketA;
		FRoomSocket SocketB;

		if (!FindSocketForDirection(RoomA, DirectionA, SocketA) ||
			!FindSocketForDirection(RoomB, DirectionB, SocketB))
		{
			continue;
		}

		Edge.DoorA = RoomA.GridCenter + SocketA.GridOffset;
		Edge.DoorB = RoomB.GridCenter + SocketB.GridOffset;
		Edge.DoorDirectionA = SocketA.Direction;
		Edge.DoorDirectionB = SocketB.Direction;
		Edge.DoorComponentA = SocketA.ComponentName;
		Edge.DoorComponentB = SocketB.ComponentName;
		Edge.bHasDoors = true;

		GridMap.Add(Edge.DoorA, EGridCellType::Door);
		GridMap.Add(Edge.DoorB, EGridCellType::Door);
	}
}

void AMapGenerationManager::OpenRoomDoors()
{
	for (const FRoomEdge& Edge : FinalEdges)
	{
		if (!Edge.bHasDoors)
		{
			continue;
		}

		if (Rooms.IsValidIndex(Edge.RoomIndexA))
		{
			DisableDoorComponent(Rooms[Edge.RoomIndexA], Edge.DoorComponentA);
		}

		if (Rooms.IsValidIndex(Edge.RoomIndexB))
		{
			DisableDoorComponent(Rooms[Edge.RoomIndexB], Edge.DoorComponentB);
		}
	}
}

bool AMapGenerationManager::DisableDoorComponent(ARoom* Room, const FName& ComponentName) const
{
	if (!Room || ComponentName.IsNone())
	{
		return false;
	}

	TArray<UActorComponent*> Components;
	Room->GetComponents(Components);

	for (UActorComponent* Component : Components)
	{
		if (!Component || Component->GetFName() != ComponentName)
		{
			continue;
		}

		if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component))
		{
			PrimitiveComponent->SetVisibility(false, true);
			PrimitiveComponent->SetHiddenInGame(true, true);
			PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			return true;
		}

		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("Door component not found: %s"), *ComponentName.ToString());
	return false;
}

ERoomSocketDirection AMapGenerationManager::GetDirectionFromDelta(const FIntPoint& Delta) const
{
	if (FMath::Abs(Delta.X) > FMath::Abs(Delta.Y))
	{
		return Delta.X > 0
			? ERoomSocketDirection::East
			: ERoomSocketDirection::West;
	}

	return Delta.Y > 0
		? ERoomSocketDirection::North
		: ERoomSocketDirection::South;
}

ERoomSocketDirection AMapGenerationManager::GetOppositeDirection(ERoomSocketDirection Direction) const
{
	switch (Direction)
	{
	case ERoomSocketDirection::North:
		return ERoomSocketDirection::South;
	case ERoomSocketDirection::East:
		return ERoomSocketDirection::West;
	case ERoomSocketDirection::South:
		return ERoomSocketDirection::North;
	case ERoomSocketDirection::West:
		return ERoomSocketDirection::East;
	default:
		return ERoomSocketDirection::North;
	}
}

FIntPoint AMapGenerationManager::DirectionToGridOffset(ERoomSocketDirection Direction) const
{
	switch (Direction)
	{
	case ERoomSocketDirection::North:
		return FIntPoint(0, 1);
	case ERoomSocketDirection::East:
		return FIntPoint(1, 0);
	case ERoomSocketDirection::South:
		return FIntPoint(0, -1);
	case ERoomSocketDirection::West:
		return FIntPoint(-1, 0);
	default:
		return FIntPoint::ZeroValue;
	}
}

bool AMapGenerationManager::FindSocketForDirection(
	const FGeneratedRoom& Room,
	ERoomSocketDirection Direction,
	FRoomSocket& OutSocket
) const
{
	if (!Room.RoomClass)
	{
		return false;
	}

	const ARoom* RoomCDO = Room.RoomClass->GetDefaultObject<ARoom>();
	if (!RoomCDO)
	{
		return false;
	}

	for (const FRoomSocket& Socket : RoomCDO->Sockets)
	{
		if (Socket.Direction == Direction)
		{
			OutSocket = Socket;
			return true;
		}
	}

	return false;
}

void AMapGenerationManager::BuildCorridors()
{
	for (const FRoomEdge& Edge : FinalEdges)
	{
		if (!Edge.bHasDoors)
		{
			continue;
		}

		const FIntPoint Start = Edge.DoorA + DirectionToGridOffset(Edge.DoorDirectionA);
		const FIntPoint End = Edge.DoorB + DirectionToGridOffset(Edge.DoorDirectionB);
		const EGridCellType* StartType = GridMap.Find(Start);
		const EGridCellType* EndType = GridMap.Find(End);

		UE_LOG(LogTemp, Warning, TEXT("Corridor Edge %d-%d | DoorA(%d,%d) DirA=%d Start(%d,%d) StartType=%d | DoorB(%d,%d) DirB=%d End(%d,%d) EndType=%d"),
			Edge.RoomIndexA,
			Edge.RoomIndexB,
			Edge.DoorA.X,
			Edge.DoorA.Y,
			static_cast<int32>(Edge.DoorDirectionA),
			Start.X,
			Start.Y,
			StartType ? static_cast<int32>(*StartType) : -1,
			Edge.DoorB.X,
			Edge.DoorB.Y,
			static_cast<int32>(Edge.DoorDirectionB),
			End.X,
			End.Y,
			EndType ? static_cast<int32>(*EndType) : -1
		);

		const TArray<FIntPoint> HorizontalFirstPath = BuildLPath(Start, End, true);
		const TArray<FIntPoint> VerticalFirstPath = BuildLPath(Start, End, false);

		const int32 HorizontalFirstCost = CalculatePathCost(HorizontalFirstPath);
		const int32 VerticalFirstCost = CalculatePathCost(VerticalFirstPath);

		ApplyCorridorPath(HorizontalFirstCost <= VerticalFirstCost ? HorizontalFirstPath : VerticalFirstPath);
	}
}

TArray<FIntPoint> AMapGenerationManager::BuildLPath(const FIntPoint& Start, const FIntPoint& End, bool bHorizontalFirst) const
{
	TArray<FIntPoint> Path;
	FIntPoint Current = Start;
	Path.Add(Current);

	auto StepX = [&]()
		{
			const int32 Step = End.X > Current.X ? 1 : -1;
			while (Current.X != End.X)
			{
				Current.X += Step;
				Path.Add(Current);
			}
		};

	auto StepY = [&]()
		{
			const int32 Step = End.Y > Current.Y ? 1 : -1;
			while (Current.Y != End.Y)
			{
				Current.Y += Step;
				Path.Add(Current);
			}
		};

	if (bHorizontalFirst)
	{
		StepX();
		StepY();
	}
	else
	{
		StepY();
		StepX();
	}

	return Path;
}

int32 AMapGenerationManager::CalculatePathCost(const TArray<FIntPoint>& Path) const
{
	int32 Cost = 0;

	for (const FIntPoint& Cell : Path)
	{
		const EGridCellType* CellType = GridMap.Find(Cell);

		if (!CellType || *CellType == EGridCellType::Empty)
		{
			Cost += 1;
			continue;
		}

		switch (*CellType)
		{
		case EGridCellType::Corridor:
			Cost += 1;
			break;
		case EGridCellType::Door:
			Cost += 1;
			break;
		case EGridCellType::Wall:
			Cost += 50;
			break;
		case EGridCellType::Room:
			Cost += 100;
			break;
		default:
			Cost += 1;
			break;
		}
	}

	return Cost;
}

void AMapGenerationManager::ApplyCorridorPath(const TArray<FIntPoint>& Path)
{
	for (const FIntPoint& Cell : Path)
	{
		const EGridCellType* CellType = GridMap.Find(Cell);

		if (CellType && (*CellType == EGridCellType::Door || *CellType == EGridCellType::Room || *CellType == EGridCellType::Wall))
		{
			continue;
		}

		GridMap.Add(Cell, EGridCellType::Corridor);
	}
}

uint8 AMapGenerationManager::GetCellConnectionMask(const FIntPoint& Cell) const
{
	uint8 Mask = static_cast<uint8>(EGridConnectionMask::None);

	auto HasConnection = [this](const FIntPoint& TargetCell)
		{
			const EGridCellType* CellType = GridMap.Find(TargetCell);
			return CellType && (*CellType == EGridCellType::Corridor || *CellType == EGridCellType::Door);
		};

	if (HasConnection(Cell + FIntPoint(0, 1)))
	{
		Mask |= static_cast<uint8>(EGridConnectionMask::North);
	}

	if (HasConnection(Cell + FIntPoint(1, 0)))
	{
		Mask |= static_cast<uint8>(EGridConnectionMask::East);
	}

	if (HasConnection(Cell + FIntPoint(0, -1)))
	{
		Mask |= static_cast<uint8>(EGridConnectionMask::South);
	}

	if (HasConnection(Cell + FIntPoint(-1, 0)))
	{
		Mask |= static_cast<uint8>(EGridConnectionMask::West);
	}

	return Mask;
}

bool AMapGenerationManager::GetCorridorClassAndRotationFromMask(uint8 Mask, TSubclassOf<AActor>& OutClass, FRotator& OutRotation) const
{
	const uint8 North = static_cast<uint8>(EGridConnectionMask::North);
	const uint8 East = static_cast<uint8>(EGridConnectionMask::East);
	const uint8 South = static_cast<uint8>(EGridConnectionMask::South);
	const uint8 West = static_cast<uint8>(EGridConnectionMask::West);

	OutClass = nullptr;
	OutRotation = FRotator::ZeroRotator;

	if (Mask == (East | West))
	{
		OutClass = StraightCorridorClass;
		OutRotation = FRotator(0.f, 0.f, 0.f);
		return OutClass != nullptr;
	}

	if (Mask == (North | South))
	{
		OutClass = StraightCorridorClass;
		OutRotation = FRotator(0.f, 90.f, 0.f);
		return OutClass != nullptr;
	}

	if (Mask == (North | East))
	{
		OutClass = CornerCorridorClass;
		OutRotation = FRotator(0.f, 0.f, 0.f);
		return OutClass != nullptr;
	}

	if (Mask == (East | South))
	{
		OutClass = CornerCorridorClass;
		OutRotation = FRotator(0.f, -90.f, 0.f);
		return OutClass != nullptr;
	}

	if (Mask == (South | West))
	{
		OutClass = CornerCorridorClass;
		OutRotation = FRotator(0.f, 180.f, 0.f);
		return OutClass != nullptr;
	}

	if (Mask == (West | North))
	{
		OutClass = CornerCorridorClass;
		OutRotation = FRotator(0.f, 90.f, 0.f);
		return OutClass != nullptr;
	}

	if (Mask == (North | East | West))
	{
		OutClass = TJunctionCorridorClass;
		OutRotation = FRotator(0.f, 0.f, 0.f);
		return OutClass != nullptr;
	}

	if (Mask == (North | East | South))
	{
		OutClass = TJunctionCorridorClass;
		OutRotation = FRotator(0.f, -90.f, 0.f);
		return OutClass != nullptr;
	}

	if (Mask == (East | South | West))
	{
		OutClass = TJunctionCorridorClass;
		OutRotation = FRotator(0.f, 180.f, 0.f);
		return OutClass != nullptr;
	}

	if (Mask == (North | South | West))
	{
		OutClass = TJunctionCorridorClass;
		OutRotation = FRotator(0.f, 90.f, 0.f);
		return OutClass != nullptr;
	}

	if (Mask == (North | East | South | West))
	{
		OutClass = CrossCorridorClass;
		OutRotation = FRotator::ZeroRotator;
		return OutClass != nullptr;
	}

	if (Mask == North)
	{
		OutClass = DeadEndCorridorClass;
		OutRotation = FRotator(0.f, 0.f, 0.f);
		return OutClass != nullptr;
	}

	if (Mask == East)
	{
		OutClass = DeadEndCorridorClass;
		OutRotation = FRotator(0.f, -90.f, 0.f);
		return OutClass != nullptr;
	}

	if (Mask == South)
	{
		OutClass = DeadEndCorridorClass;
		OutRotation = FRotator(0.f, 180.f, 0.f);
		return OutClass != nullptr;
	}

	if (Mask == West)
	{
		OutClass = DeadEndCorridorClass;
		OutRotation = FRotator(0.f, 90.f, 0.f);
		return OutClass != nullptr;
	}

	return false;
}

void AMapGenerationManager::SpawnCorridors()
{
	if (!GetWorld())
	{
		return;
	}

	for (AActor* Corridor : SpawnedCorridors)
	{
		if (IsValid(Corridor))
		{
			Corridor->Destroy();
		}
	}
	SpawnedCorridors.Empty();

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (const TPair<FIntPoint, EGridCellType>& Pair : GridMap)
	{
		if (Pair.Value != EGridCellType::Corridor)
		{
			continue;
		}

		TSubclassOf<AActor> CorridorClass;
		FRotator CorridorRotation;
		const uint8 Mask = GetCellConnectionMask(Pair.Key);

		if (!GetCorridorClassAndRotationFromMask(Mask, CorridorClass, CorridorRotation))
		{
			continue;
		}

		const FVector2D CellWorld2D = GridToWorld2D(Pair.Key);
		const FVector SpawnLocation(CellWorld2D.X, CellWorld2D.Y, 0.f);

		AActor* SpawnedCorridor = GetWorld()->SpawnActor<AActor>(
			CorridorClass,
			SpawnLocation,
			CorridorRotation,
			Params
		);

		if (SpawnedCorridor)
		{
			SpawnedCorridors.Add(SpawnedCorridor);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("Spawned Corridors: %d"), SpawnedCorridors.Num());
}

void AMapGenerationManager::DrawGridMap() const
{
	if (!GetWorld())
	{
		return;
	}

	const FVector BoxExtent(GridSize * 0.45f, GridSize * 0.45f, 20.f);

	for (const TPair<FIntPoint, EGridCellType>& Pair : GridMap)
	{
		FColor CellColor = FColor::White;

		switch (Pair.Value)
		{
		case EGridCellType::Room:
			CellColor = FColor::Green;
			break;
		case EGridCellType::Wall:
			CellColor = FColor::Silver;
			break;
		case EGridCellType::Door:
			CellColor = FColor::Yellow;
			break;
		case EGridCellType::Corridor:
			CellColor = FColor::Blue;
			break;
		default:
			CellColor = FColor::White;
			break;
		}

		const FVector2D CellWorld2D = GridToWorld2D(Pair.Key);
		const FVector CellWorld(CellWorld2D.X, CellWorld2D.Y, 20.f);

		DrawDebugBox(
			GetWorld(),
			CellWorld,
			BoxExtent,
			CellColor,
			false,
			30.0f,
			0,
			8.0f
		);
	}
}
void AMapGenerationManager::DrawEdges(const TArray<FRoomEdge>& Edges)
{
	for (int32 i = 0; i < Edges.Num(); ++i)
	{
		const FRoomEdge& Edge = Edges[i];

		FVector StartPos = Rooms[Edge.RoomIndexA]->GetActorLocation();
		FVector EndPos = Rooms[Edge.RoomIndexB]->GetActorLocation();

		DrawDebugLine(
			GetWorld(),
			StartPos,
			EndPos,
			FColor::Cyan,
			false,
			30.0f,
			0,
			5.0f
		);

		FVector MidPoint = (StartPos + EndPos) / 2.0f;
		DrawDebugString(
			GetWorld(),
			MidPoint,
			FString::Printf(TEXT("E%d"), i),
			nullptr,
			FColor::White,
			30.0f,
			true,
			1.0f
		);
	}
}
