// Copyright. © 2024. Spxcebxr Games.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CRPG_TacticalInputDataAsset.generated.h"

class UInputMappingContext;
class UInputAction;

/**
 * 
 */
UCLASS()
class CRPG_API UCRPG_TacticalInputDataAsset : public UDataAsset
{
	GENERATED_BODY()


	/* --- BEGIN: Movement --- */
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement")
	TObjectPtr<UInputMappingContext> TacticalMovementInputMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement")
	TObjectPtr<UInputAction> MoveCamera;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement")
	TObjectPtr<UInputAction> RotateCamera;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement")
	TObjectPtr<UInputAction> ZoomCamera;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement")
	TObjectPtr<UInputAction> LockCameraToCharacter;

public:
	TObjectPtr<UInputMappingContext> GetTacticalMovementInputMappingContext() const
	{
		return TacticalMovementInputMappingContext;
	}

	TObjectPtr<UInputAction> GetMoveCamera() const
	{
		return MoveCamera;
	}

	TObjectPtr<UInputAction> GetRotateCamera() const
	{
		return RotateCamera;
	}

	TObjectPtr<UInputAction> GetZoomCamera() const
	{
		return ZoomCamera;
	}

	TObjectPtr<UInputAction> GetLockCameraToCharacter() const
	{
		return LockCameraToCharacter;
	}

	/* --- END: Movement --- */

	/* --- BEGIN: Interaction --- */
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Interaction")
	TObjectPtr<UInputMappingContext> TacticalInteractionInputMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Interaction")
	TObjectPtr<UInputAction> Select;

public:
	TObjectPtr<UInputMappingContext> GetTacticalInteractionInputMappingContext() const
	{
		return TacticalInteractionInputMappingContext;
	}

	TObjectPtr<UInputAction> GetSelect() const
	{
		return Select;
	}

	/* --- END: Movement --- */
};
