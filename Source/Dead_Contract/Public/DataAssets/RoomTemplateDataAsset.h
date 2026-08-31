// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RoomTemplateDataAsset.generated.h"

class ARoom;

/**
 * 
 */
UCLASS()
class DEAD_CONTRACT_API URoomTemplateDataAsset : public UDataAsset
{
	GENERATED_BODY()
	
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
    TSubclassOf<ARoom> RoomClass;
};
