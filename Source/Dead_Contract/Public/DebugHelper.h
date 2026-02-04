// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 
 */
class DEAD_CONTRACT_API DebugHelper
{
public:
#define BENCHMARK(FuncName) \
    { \
        double Start = FPlatformTime::Seconds(); \
        this->FuncName(); \
        double End = FPlatformTime::Seconds(); \
        UE_LOG(LogTemp, Warning, TEXT(#FuncName ": %.3f ms"), (End - Start) * 1000.0); \
    }
};
