// Copyright. © 2024. Spxcebxr Games.

#pragma once

#include "CoreMinimal.h"
#include "InputActionValue.h"
#include "GameFramework/PlayerController.h"
#include "CRPG_PlayerController.generated.h"

class ACRPG_PlayerCamera;
class UCRPG_TacticalInputDataAsset;

DECLARE_LOG_CATEGORY_EXTERN(LogCRPGPlayerController, Log, All);

/**
 * 
 */
UCLASS()
class CRPG_API ACRPG_PlayerController : public APlayerController
{
	GENERATED_BODY()

	/* TODO:
	 *	4. Set up movement of Camera.
	 *	5. Set up movement of player if they choose 3rd Person Mode.
	 */
	
public:
	ACRPG_PlayerController();

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	/* --- BEGIN: Input --- */
	
protected:		
	// If the player should be using movement to look around the map or control as ThirdPerson
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input")
	bool bUsingTactical;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Tactical")
	TObjectPtr<UCRPG_TacticalInputDataAsset> TacticalInputDataAsset;

	virtual void SetupInputComponent() override;
	virtual void DisableInput(APlayerController* InPlayerController) override;
	void RemapInput() const;	

public:
	void SetTacticalMovement(bool bValue);

	/* --- END: Input --- */

	/* --- BEGIN: Camera Input --- */

protected:
	void CameraMoveInput(const FInputActionValue& Input);
	void CameraRotateInput(const FInputActionValue& Input);
	void CameraZoomInput(const FInputActionValue& Input);
	void CameraLockInput(const FInputActionValue& Input);
	
	/* --- END: Camera Input --- */

	/* --- BEGIN: Camera Setup --- */

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera|Setup")
	TSubclassOf<ACRPG_PlayerCamera> PlayerCameraToSpawn;

private:
	UPROPERTY(Replicated)
	TObjectPtr<ACRPG_PlayerCamera> PlayerCamera;

public:
	virtual void AutoManageActiveCameraTarget(AActor* SuggestedTarget) override;

private:
	void SetPlayerCamera();

	UFUNCTION(Server, Reliable, WithValidation)
	void SERVER_SpawnPlayerCamera();

	UFUNCTION(Client, Reliable)
	void CLIENT_SetPlayerCamera(ACRPG_PlayerCamera* NewCamera);
	
	/* --- END: Camera Setup --- */

	/* --- BEGIN: Camera Attachment --- */
	
protected:
	UPROPERTY(Replicated)
	bool bIsLockedToTarget;
	
	bool bBlockingCameraInput;
	
public:
	void LockCameraToTarget(AActor* ActorToTarget, bool bTargetBlocksCameraInput = false);
	void UnlockCamera(bool bUnblockCameraInput = true);

	UFUNCTION(Server, Reliable)
	void SERVER_LockCameraToTarget(AActor* ActorToTarget, bool bTargetBlocksCameraInput = false);

	UFUNCTION(Server, Reliable)
	void SERVER_UnlockCamera(bool bUnblockCameraInput = true);
			
	/* --- END: Camera Attachment --- */
};
