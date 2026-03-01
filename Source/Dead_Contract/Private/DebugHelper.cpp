// Fill out your copyright notice in the Description page of Project Settings.


#include "DebugHelper.h"

void DrawTriangle(UWorld* World, const FVector2D& A, const FVector2D& B, const FVector2D& C, float Z)
{
    FVector VA(A.X, A.Y, Z);
    FVector VB(B.X, B.Y, Z);
    FVector VC(C.X, C.Y, Z);

    DrawDebugLine(World, VA, VB, FColor::Red, true, -1.f, 0, 2.f);
    DrawDebugLine(World, VB, VC, FColor::Red, true, -1.f, 0, 2.f);
    DrawDebugLine(World, VC, VA, FColor::Red, true, -1.f, 0, 2.f);
}