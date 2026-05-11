// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

void DrawTriangle(
	UWorld* World, 
	const FVector2D& A, 
	const FVector2D& B, 
	const FVector2D& C, 
	float Z = 0.f);

namespace Debug
{
	static void Print(const FString& Msg, const FColor& Color = FColor::MakeRandomColor(), int32 InKey = -1)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(InKey, 7.f, Color, Msg);

			UE_LOG(LogTemp, Warning, TEXT("%s"), *Msg);
		}
	}
}