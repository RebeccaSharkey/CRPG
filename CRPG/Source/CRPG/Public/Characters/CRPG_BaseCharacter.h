// Copyright. © 2024. Spxcebxr Games.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CRPG_BaseCharacter.generated.h"

UCLASS(Abstract)
class CRPG_API ACRPG_BaseCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ACRPG_BaseCharacter();

protected:
	virtual void BeginPlay() override; 
};
