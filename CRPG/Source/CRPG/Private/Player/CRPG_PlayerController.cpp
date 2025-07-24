// Copyright. © 2024. Spxcebxr Games.


#include "Player/CRPG_PlayerController.h"

// CRPG
#include "Player/CRPG_PlayerCamera.h"
#include "Player/Input/CRPG_TacticalInputDataAsset.h"

// Unreal
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY(LogCRPGPlayerController);

ACRPG_PlayerController::ACRPG_PlayerController()
{
	bReplicates = true;
	bAlwaysRelevant = true;

	// This is the default set up and would force classic CRPG movement.
	bUsingTactical = true;
	bIsLockedToTarget = false;
	bBlockingCameraInput = false;
}

void ACRPG_PlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACRPG_PlayerController, PlayerCamera);
	DOREPLIFETIME(ACRPG_PlayerController, bIsLockedToTarget);
}

/* ------------------------------------------------ BEGIN: Input ---------------------------------------------------- */

void ACRPG_PlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);

	if(!IsValid(EnhancedInputComponent))
	{
		return;
	}

	if(TacticalInputDataAsset)
	{		
		EnhancedInputComponent->BindAction(TacticalInputDataAsset->GetMoveCamera(), ETriggerEvent::Triggered, this, &ACRPG_PlayerController::CameraMoveInput);
		EnhancedInputComponent->BindAction(TacticalInputDataAsset->GetRotateCamera(), ETriggerEvent::Triggered, this, &ACRPG_PlayerController::CameraRotateInput);
		EnhancedInputComponent->BindAction(TacticalInputDataAsset->GetZoomCamera(), ETriggerEvent::Triggered, this, &ACRPG_PlayerController::CameraZoomInput);
		EnhancedInputComponent->BindAction(TacticalInputDataAsset->GetLockCameraToCharacter(), ETriggerEvent::Started, this, &ACRPG_PlayerController::CameraLockInput);
	}
}

void ACRPG_PlayerController::DisableInput(APlayerController* InPlayerController)
{
	Super::DisableInput(InPlayerController);
}

void ACRPG_PlayerController::RemapInput() const
{
	UEnhancedInputLocalPlayerSubsystem* EnhancedInputLocalPlayerSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());

	if(!IsValid(EnhancedInputLocalPlayerSubsystem))
	{
		return;
	}

	EnhancedInputLocalPlayerSubsystem->ClearAllMappings();
	
	if(bUsingTactical && TacticalInputDataAsset)
	{
		if(TacticalInputDataAsset->GetTacticalMovementInputMappingContext())
		{
			EnhancedInputLocalPlayerSubsystem->AddMappingContext(TacticalInputDataAsset->GetTacticalMovementInputMappingContext(), 0);
		}

		if(TacticalInputDataAsset->GetTacticalInteractionInputMappingContext())
		{
			EnhancedInputLocalPlayerSubsystem->AddMappingContext(TacticalInputDataAsset->GetTacticalInteractionInputMappingContext(), 0);
		}		
	}
	/*else if(ThirdPersonInputDataAsset)
	{
		// Set up movement and camera for a more third person approach to movement. 
	}*/
}

void ACRPG_PlayerController::SetTacticalMovement(bool bValue)
{
	bUsingTactical = bValue;	
}

/* ------------------------------------------------ END: Input ------------------------------------------------------ */

/* ------------------------------------------------ BEGIN: Camera Input --------------------------------------------- */

void ACRPG_PlayerController::CameraMoveInput(const FInputActionValue& Input)
{
	if(!bUsingTactical || bBlockingCameraInput)
	{
		return;
	}

	if(bIsLockedToTarget)
	{
		UnlockCamera();
	}
	
	if(IsValid(PlayerCamera))
	{
		PlayerCamera->MoveCamera(Input.Get<FVector2D>());		
	}
}

void ACRPG_PlayerController::CameraRotateInput(const FInputActionValue& Input)
{
	if(IsValid(PlayerCamera))
	{
		PlayerCamera->RotateCamera(Input.Get<float>());
	}
}

void ACRPG_PlayerController::CameraZoomInput(const FInputActionValue& Input)
{
	if(IsValid(PlayerCamera))
	{
		PlayerCamera->ZoomCamera(Input.Get<float>());
	}
}

void ACRPG_PlayerController::CameraLockInput(const FInputActionValue& Input)
{
	if(IsValid(GetPawn()))
	{
		LockCameraToTarget(GetPawn());
	}
}

/* ------------------------------------------------ END: Camera Input ----------------------------------------------- */

/* ------------------------------------------------ BEGIN: Camera Setup --------------------------------------------- */

void ACRPG_PlayerController::AutoManageActiveCameraTarget(AActor* SuggestedTarget)
{
	SetPlayerCamera();
}

void ACRPG_PlayerController::SetPlayerCamera()
{
	if(IsValid(GetWorld()) && IsValid(GetPawn()) && !IsValid(PlayerCamera) && PlayerCameraToSpawn)
	{
		if(HasAuthority())
		{
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.Owner = this;
			PlayerCamera = GetWorld()->SpawnActor<ACRPG_PlayerCamera>(PlayerCameraToSpawn, GetPawn()->GetTransform(), SpawnParameters);
		}
		else
		{
			SERVER_SpawnPlayerCamera();
		}
	}

	if(IsValid(PlayerCamera))
	{
		if(HasAuthority() && !IsLocalController())
		{
			CLIENT_SetPlayerCamera(PlayerCamera);
		}		

		if(IsLocalController())
		{
			SetViewTargetWithBlend(PlayerCamera, 0.5f);
			
			RemapInput();
		}
	}
}

void ACRPG_PlayerController::SERVER_SpawnPlayerCamera_Implementation()
{
	SetPlayerCamera();
}

bool ACRPG_PlayerController::SERVER_SpawnPlayerCamera_Validate()
{
	return true;
}

void ACRPG_PlayerController::CLIENT_SetPlayerCamera_Implementation(ACRPG_PlayerCamera* NewCamera)
{
	if(IsValid(NewCamera))
	{
		PlayerCamera = NewCamera;

		SetPlayerCamera();
	}
}

/* ------------------------------------------------ END: Camera Setup ----------------------------------------------- */

/* ------------------------------------------------ BEGIN: Camera Attachment ---------------------------------------- */

void ACRPG_PlayerController::LockCameraToTarget(AActor* ActorToTarget, bool bTargetBlocksCameraInput)
{
	if(!IsValid(ActorToTarget) || !IsValid(PlayerCamera))
	{
		return;
	}

	bIsLockedToTarget = true;
	bBlockingCameraInput = bTargetBlocksCameraInput;

	PlayerCamera->FollowTarget(ActorToTarget);
	
	if(!HasAuthority())
	{				
		SERVER_LockCameraToTarget(ActorToTarget, bTargetBlocksCameraInput);
	}
}

void ACRPG_PlayerController::UnlockCamera(bool bUnblockCameraInput)
{
	if(!IsValid(PlayerCamera))
	{
		return;
	}

	bBlockingCameraInput = !bUnblockCameraInput;
	bIsLockedToTarget = false;

	PlayerCamera->StopFollowTarget();
	PlayerCamera->StopMoveTo();
	
	if(!HasAuthority())
	{
		SERVER_UnlockCamera(bUnblockCameraInput);
	}
}

void ACRPG_PlayerController::SERVER_LockCameraToTarget_Implementation(AActor* ActorToTarget, bool bTargetBlocksCameraInput)
{
	LockCameraToTarget(ActorToTarget, bTargetBlocksCameraInput);
}

void ACRPG_PlayerController::SERVER_UnlockCamera_Implementation(bool bUnblockCameraInput)
{
	UnlockCamera(bUnblockCameraInput);
}

/* ------------------------------------------------ END: Camera Attachment ------------------------------------------ */
