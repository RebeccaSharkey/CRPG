// Copyright. © 2024. Spxcebxr Games.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CRPG_PlayerCamera.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogCRPGPlayerCamera, Log, All);

class UCameraComponent;
class USplineComponent;
class USpringArmComponent;

USTRUCT()
struct FCameraMoveData
{
	GENERATED_BODY()

public:
	float TimeStamp;	
	FVector MoveToLocation;
	FRotator MoveToRotation;
};

UCLASS()
class CRPG_API ACRPG_PlayerCamera : public AActor
{
	GENERATED_BODY()

public:
	ACRPG_PlayerCamera();

protected:
	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	/* --- BEGIN: Components --- */
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Components")
	TObjectPtr<USceneComponent> DefaultRootComponent;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Components")
	TObjectPtr<USpringArmComponent> SpringArmComponent;	

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Components")
	TObjectPtr<UCameraComponent> CameraComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Components")
	TObjectPtr<USplineComponent> SplineComponent;
	
	/* --- END: Components --- */

	/* --- BEGIN: Networking --- */

public:
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
private:
	
	// Movement history to reconcile client-side prediction with server authority.
	UPROPERTY()
	TArray<FCameraMoveData> MoveHistory;
	
	// The index of the lowest MoveData the new corrections have come from.
	int32 CorrectedIndex;

	void NetworkSmoothing(float DeltaSeconds);

	/* --- END: Networking --- */
	
	/* --- BEGIN: Movement | Location --- */

	protected:
	// Movement speed of the camera.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Camera Movement|Location")
	float CameraMovementSpeed {10.f};
	
	// Movement speed of the camera when being corrected over the network.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Camera Movement|Location")
	float CameraCorrectedMovementSpeed {10.f};
	
	// Movement speed of the camera when being corrected over the network.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Camera Movement|Location")
	float NetworkedMovementDifference {1.f};
	
public:
	// Move the camera based on player input (client prediction).
	void MoveCamera(FVector2D MoveToLocation);

protected:
	// Server RPC to handle camera movement.
	UFUNCTION(Server, Unreliable)
	void SERVER_MoveCamera(FVector2D MoveToLocation, float TimeStamp);
	
	// Multicast RPC to update the camera position on all clients.
	UFUNCTION(NetMulticast, Unreliable)
	void MULTICAST_CorrectedMoveCamera(FVector CorrectPosition, float TimeStamp);

private:
	// Track whether the position is being corrected.
	bool bPositionCorrected;
	
	// Client-predicted position for the camera.
	FVector PredictedLocation;

	// Server-authoritative position for the camera.
	FVector ServerConfirmedLocation;
	
	/* --- END: Movement | Location --- */

	/* --- BEGIN: Movement | Rotation --- */

protected:
	// Rotation speed of the camera.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Camera Movement|Rotation")
	float CameraRotationSpeed {10.f};
	
	// Rotation speed of the camera when being corrected over the network.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Camera Movement|Rotation")
	float CameraCorrectedRotationSpeed {10.f};
	
	// Rotation speed of the camera when being corrected over the network.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Camera Movement|Rotation")
	float NetworkedRotationDifference {1.f};
	
public:
	// Rotate the camera based on player input (client prediction).
	void RotateCamera(float MoveToRotation);

protected:
	// Server RPC to handle camera rotation.
	UFUNCTION(Server, Unreliable)
	void SERVER_RotateCamera(float MoveToRotation, float TimeStamp);
	
	// Multicast RPC to update the camera rotation on all clients.
	UFUNCTION(NetMulticast, Unreliable)
	void MULTICAST_CorrectedRotateCamera(FRotator CorrectRotation, float TimeStamp);

private:
	// Track whether the rotation is being corrected.
	bool bRotationCorrected;
	
	// Client-predicted rotation for the camera.
	FRotator PredictedRotation;

	// Server-authoritative rotation for the camera.
	FRotator ServerConfirmedRotation;

	// Whether rotation is blocked when moving to or following target.
	UPROPERTY(Replicated)
	bool bRotationBlocked;
	
	/* --- END: Movement | Rotation --- */

	/* --- BEGIN: Movement | Zoom --- */
	
protected:  
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Camera Movement|Zoom")  
	float DefaultZoomPercent{0.f};
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Camera Movement|Zoom")  
	float ZoomSpeed{0.1f};
	
public:
	void ZoomCamera(float InputZoom);
  
protected:
	UFUNCTION(Server, Unreliable)
	void SERVER_ZoomCamera(float InputZoom);

	UFUNCTION(NetMulticast, Unreliable)
	void MULTICAST_ZoomCamera(float NewZoomPercent);
	
	void SetCameraTransformAlongSpline(float ZoomPercentage) const;  
  
private:
	float ZoomPercent;
	
	/* --- END: Movement | Zoom --- */

	/* --- BEGIN: Movement | Move To --- */

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Camera Movement|Move To")
	float MinMoveToDuration{0.5f};
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Camera Movement|Move To")
	float MaxMoveToDuration{5.f};

	// The Maximum amount of distance the player can travel before anything higher just defaults to max speed.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Camera Movement|Move To")
	float MaxMoveToDestinationBeforeMaxSpeed{1000.f};
	
public:
	// Move to a specified location.
	void MoveTo(const FTransform Destination);
	
	// Stop move to a specified location.
	void StopMoveTo();

protected:
	UFUNCTION(Server, Reliable)
	void SERVER_MoveTo(const FTransform Destination);
	
	UFUNCTION(Server, Reliable)
	void SERVER_StopMoveTo();
		
	// Tick the movement over time.
	void MoveToDestination(float DeltaSeconds);
	
	// Tick the movement over time for clients
	UFUNCTION(NetMulticast, Unreliable)
	void MULTICAST_MoveToDestination(FTransform NewTransform);	
	
private:
	bool bMovingToDestination;
	FTransform CameraDestination;	

	FTransform CameraStart;
	float TotalDuration;
	float CurrentTime;
	
	/* --- END: Movement | Move To --- */

	/* --- BEGIN: Movement | Follow Target --- */

public:
	void FollowTarget(AActor* ActorToFollow, bool bLockRotation = false);
	void StopFollowTarget();
	bool IsFollowingTarget() const { return bIsFollowingTarget; };
	bool IsFollowingTarget(const AActor* ActorToFollow) const { return bIsFollowingTarget && ActorToFollow == TargetToFollow; };

protected:
	void TickFollowTarget(float DeltaTime);
	
private:
	bool bIsFollowingTarget;
	TWeakObjectPtr<AActor> TargetToFollow;
	
	
	/* --- END: Movement | Follow Target --- */
};

