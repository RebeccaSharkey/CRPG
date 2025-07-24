// Copyright. © 2024. Spxcebxr Games.


#include "Player/CRPG_PlayerCamera.h"

// UE
#include "Camera/CameraComponent.h"
#include "Components/SplineComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY(LogCRPGPlayerCamera);

ACRPG_PlayerCamera::ACRPG_PlayerCamera()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	bAlwaysRelevant = true;

	DefaultRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultRootComponent"));
	SetRootComponent(DefaultRootComponent);
	
	SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>("SpringArm");
	SpringArmComponent->SetupAttachment(RootComponent);

	SpringArmComponent->bEnableCameraLag = true;
	SpringArmComponent->bEnableCameraRotationLag = true;
	SpringArmComponent->bDoCollisionTest = false;

	CameraComponent = CreateDefaultSubobject<UCameraComponent>("Camera");
	CameraComponent->SetupAttachment(SpringArmComponent);

	SplineComponent = CreateDefaultSubobject<USplineComponent>("SplineComponent");
	SplineComponent->SetupAttachment(RootComponent);

	CorrectedIndex = INDEX_NONE;	
	bPositionCorrected = false;
	bRotationCorrected = false;
	
	bMovingToDestination = false;
	CameraDestination = FTransform::Identity;
	CameraStart = FTransform::Identity;
	TotalDuration = INDEX_NONE;
	CurrentTime = INDEX_NONE;

	bRotationBlocked = false;
}

void ACRPG_PlayerCamera::BeginPlay()
{
	Super::BeginPlay();
	
	SetCameraTransformAlongSpline(DefaultZoomPercent);
	ZoomPercent = DefaultZoomPercent;
}

void ACRPG_PlayerCamera::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if(!IsValid(GetOwner()) || !(Cast<APlayerController>(GetOwner())->IsLocalController() || HasAuthority()))
	{			
		return;
	}

	if(IsFollowingTarget())
	{
		TickFollowTarget(DeltaSeconds);
	}
	
	MoveToDestination(DeltaSeconds);
	
	NetworkSmoothing(DeltaSeconds);
}

/* ------------------------------------------------ BEGIN: Networking ----------------------------------------------- */

void ACRPG_PlayerCamera::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACRPG_PlayerCamera, bRotationBlocked);
}

void ACRPG_PlayerCamera::NetworkSmoothing(float DeltaSeconds)
{
	if(bMovingToDestination)
	{
		return;
	}
	
	// Smoothly interpolate between predicted and server-confirmed position
	if (bPositionCorrected)
	{		
		const FVector CurrentLocation = GetActorLocation();		
		SetActorLocation(FMath::VInterpTo(CurrentLocation, ServerConfirmedLocation, DeltaSeconds, CameraCorrectedMovementSpeed));		
		// Stop correcting if we're close enough to the server's position
		bPositionCorrected = FVector::Dist(CurrentLocation, ServerConfirmedLocation) > NetworkedMovementDifference;
	}

	if(bRotationCorrected)
	{
		const FRotator CurrentRotation = GetActorRotation();		
		SetActorRotation(FMath::RInterpTo(CurrentRotation, ServerConfirmedRotation, DeltaSeconds, CameraCorrectedRotationSpeed));

		FRotator DeltaRotator = (CurrentRotation - ServerConfirmedRotation).GetNormalized();

		float AngularDistance = FMath::Sqrt(
			FMath::Square(DeltaRotator.Pitch) +
			FMath::Square(DeltaRotator.Yaw) +
			FMath::Square(DeltaRotator.Roll));
				
		bRotationCorrected = AngularDistance > NetworkedRotationDifference;
	}

	// Delete old data.
	if(CorrectedIndex != INDEX_NONE)
	{
		MoveHistory.RemoveAt(0, CorrectedIndex + 1);
		CorrectedIndex = INDEX_NONE;
	}
}

/* --------------------------------------------- END: Networking ---------------------------------------------------- */

/* --------------------------------------------- BEGIN: Movement ---------------------------------------------------- */

void ACRPG_PlayerCamera::MoveCamera(FVector2D MoveToLocation)
{
	if(bMovingToDestination)
	{
		StopFollowTarget();
		StopMoveTo();
	}
	
	const FVector ForwardMovement = (GetActorForwardVector() * MoveToLocation.Y) * CameraMovementSpeed * GetWorld()->DeltaTimeSeconds;
	const FVector RightMovement = (GetActorRightVector() * MoveToLocation.X) * CameraMovementSpeed * GetWorld()->DeltaTimeSeconds;

	PredictedLocation  = GetActorLocation() + ForwardMovement + RightMovement;
	const float TimeStamp = GetWorld()->GetTimeSeconds();
	
	// Simulate the movement on the client side (prediction)
	SetActorLocation(PredictedLocation);        

	if(!HasAuthority())
	{
		bool bFoundMove = false;
		for (FCameraMoveData& Move : MoveHistory)
		{
			if(Move.TimeStamp == TimeStamp)
			{
				bFoundMove = true;
				Move.MoveToLocation = PredictedLocation;
			}
		}

		// If there was no move with that timestamp create it and add this move.
		if(!bFoundMove)
		{
			// Store the input data (for reconciliation)
			FCameraMoveData NewMove;
			NewMove.MoveToLocation = PredictedLocation;
			NewMove.TimeStamp = TimeStamp;
			
			MoveHistory.Add(NewMove);
		}
		
		SERVER_MoveCamera(MoveToLocation, TimeStamp);
	}
	else
	{
		MULTICAST_CorrectedMoveCamera(PredictedLocation, TimeStamp);
	}
}

void ACRPG_PlayerCamera::SERVER_MoveCamera_Implementation(FVector2D MoveToLocation, float TimeStamp)
{
	if(bMovingToDestination)
	{
		StopFollowTarget();
		StopMoveTo();
	}
	
	// Recalculate the position on the server based on input
	const FVector ForwardMovement = (GetActorForwardVector() * MoveToLocation.Y) * CameraMovementSpeed * GetWorld()->DeltaTimeSeconds;
	const FVector RightMovement = (GetActorRightVector() * MoveToLocation.X) * CameraMovementSpeed * GetWorld()->DeltaTimeSeconds;

	const FVector NewPosition = GetActorLocation() + ForwardMovement + RightMovement;	
	SetActorLocation(NewPosition);

	MULTICAST_CorrectedMoveCamera(NewPosition, TimeStamp);
}

void ACRPG_PlayerCamera::MULTICAST_CorrectedMoveCamera_Implementation(FVector CorrectPosition, float TimeStamp)
{
	if (bMovingToDestination || HasAuthority())
	{
		return;
	}
	
	/* TODO: Replicate Camera owner to late joining players */
	if(!IsValid(GetOwner()))
	{
		if(bMovingToDestination)
		{
			StopMoveTo();
		}
		
		SetActorLocation(CorrectPosition);
		return;
	}

	// If the owner of this actor isn't locally owned then update the position to the server position.
	if(!Cast<APlayerController>(GetOwner())->IsLocalController())
	{
		if(bMovingToDestination)
		{
			StopMoveTo();
		}
		
		SetActorLocation(CorrectPosition);
		return;
	}
			
	// Find the input data associated with this timestamp
	for (int32 i = 0; i < MoveHistory.Num(); i++)
	{
		if (MoveHistory[i].TimeStamp == TimeStamp)
		{				
			ServerConfirmedLocation = CorrectPosition;

			if(FVector::Dist(MoveHistory[i].MoveToLocation, ServerConfirmedLocation) > NetworkedMovementDifference)
			{
				bPositionCorrected = true;
			}
            
			CorrectedIndex = (CorrectedIndex == INDEX_NONE) ? i : FMath::Min(CorrectedIndex, i);
			break;
		}
	}	
}

/* --------------------------------------------- END: Movement ------------------------------------------------------ */

/* --------------------------------------------- BEGIN: Rotate ------------------------------------------------------ */

void ACRPG_PlayerCamera::RotateCamera(float MoveToRotation)
{
	PredictedRotation  = FRotator(GetActorRotation().Pitch, (MoveToRotation * CameraRotationSpeed  * GetWorld()->DeltaTimeSeconds) + GetActorRotation().Yaw, GetActorRotation().Roll);
	const float TimeStamp = GetWorld()->GetTimeSeconds();
	
	if (!HasAuthority())
	{
		// Simulate the rotation on the client side (prediction)
		SetActorRotation(PredictedRotation);        
		
		// Find if there is a move with this timestamp and add the rotation.
		bool bFoundMove = false;
		for (FCameraMoveData& Move : MoveHistory)
		{
			if(Move.TimeStamp == TimeStamp)
			{
				bFoundMove = true;
				Move.MoveToRotation = PredictedRotation;
			}
		}

		// If there was no move with that timestamp create it and add this move.
		if(!bFoundMove)
		{
			// Store the input data (for reconciliation)
			FCameraMoveData NewMove;
			NewMove.MoveToRotation = PredictedRotation;
			NewMove.TimeStamp = TimeStamp;
			
			MoveHistory.Add(NewMove);
		}		
	}

	SERVER_RotateCamera(MoveToRotation, TimeStamp);
}

void ACRPG_PlayerCamera::SERVER_RotateCamera_Implementation(float MoveToRotation, float TimeStamp)
{
	// Recalculate the rotation on the server based on input
	const FRotator NewRotation = FRotator(GetActorRotation().Pitch, (MoveToRotation * CameraRotationSpeed * GetWorld()->DeltaTimeSeconds) + GetActorRotation().Yaw, GetActorRotation().Roll);
	
	SetActorRotation(NewRotation);

	MULTICAST_CorrectedRotateCamera(NewRotation, TimeStamp);
}

void ACRPG_PlayerCamera::MULTICAST_CorrectedRotateCamera_Implementation(FRotator CorrectRotation, float TimeStamp)
{
	if (!HasAuthority())
	{
		/* TODO: Replicate Camera owner to late joining players */
		if(!IsValid(GetOwner()))
		{			
			SetActorRotation(CorrectRotation);
			return;
		}

		// If the owner of this actor isn't locally owned then update the rotation to the server rotation.
		if(!Cast<APlayerController>(GetOwner())->IsLocalController())
		{
			SetActorRotation(CorrectRotation);
			return;
		}
				
		// Find the input data associated with this timestamp
		for (int32 i = 0; i < MoveHistory.Num(); i++)
		{
			if (MoveHistory[i].TimeStamp == TimeStamp)
			{				
				ServerConfirmedRotation = CorrectRotation;

				FRotator DeltaRotator = (MoveHistory[i].MoveToRotation - ServerConfirmedRotation).GetNormalized();

				float AngularDistance = FMath::Sqrt(
					FMath::Square(DeltaRotator.Pitch) +
					FMath::Square(DeltaRotator.Yaw) +
					FMath::Square(DeltaRotator.Roll));
				
				if(AngularDistance > NetworkedRotationDifference)
				{
					bRotationCorrected = true;
				}

				CorrectedIndex = (CorrectedIndex == INDEX_NONE) ? i : FMath::Min(CorrectedIndex, i);
				break;
			}
		}
	}
}

/* --------------------------------------------- END: Rotate -------------------------------------------------------- */

/* --------------------------------------------- BEGIN: Zoom -------------------------------------------------------- */

void ACRPG_PlayerCamera::ZoomCamera(float InputZoom)
{
	ZoomPercent = FMath::Clamp((ZoomSpeed * InputZoom * GetWorld()->DeltaTimeSeconds) + ZoomPercent, 0.f, 1.f);
	SetCameraTransformAlongSpline(ZoomPercent);

	if(!HasAuthority())
	{
		SERVER_ZoomCamera(InputZoom);
	}
	else
	{
		MULTICAST_ZoomCamera(ZoomPercent);
	}
}

void ACRPG_PlayerCamera::SERVER_ZoomCamera_Implementation(float InputZoom)
{
	ZoomCamera(InputZoom);
}

void ACRPG_PlayerCamera::MULTICAST_ZoomCamera_Implementation(float NewZoomPercent)
{
	/* TODO: Replicate Camera owner to late joining players */
	if(!IsValid(GetOwner()) || !Cast<APlayerController>(GetOwner())->IsLocalController())
	{
		ZoomPercent = NewZoomPercent;
		SetCameraTransformAlongSpline(NewZoomPercent);
	}
}

void ACRPG_PlayerCamera::SetCameraTransformAlongSpline(float ZoomPercentage) const  
{  
	if(!SpringArmComponent || !SplineComponent)  
	{       return;  
	}

	SpringArmComponent->SetWorldLocation(SplineComponent->GetLocationAtTime(ZoomPercentage, ESplineCoordinateSpace::World));   ;  
	SpringArmComponent->SetWorldRotation(UKismetMathLibrary::FindLookAtRotation(SpringArmComponent->GetComponentLocation(), GetActorLocation()));  
}

/* --------------------------------------------- END: Zoom ---------------------------------------------------------- */

/* --------------------------------------------- BEGIN: Move To Location -------------------------------------------- */

void ACRPG_PlayerCamera::MoveTo(const FTransform Destination)
{
	// Clear off network smoothing for any previous moves.
	bPositionCorrected = false;
	bRotationCorrected = false;
	CorrectedIndex = INDEX_NONE;
	
	CameraStart = GetActorTransform();
	CameraDestination = Destination;
		
	float Distance = FVector::Distance(CameraStart.GetLocation(), CameraDestination.GetLocation());
	TotalDuration = FMath::GetMappedRangeValueClamped(FVector2D(0.0f, MaxMoveToDestinationBeforeMaxSpeed), FVector2D(MinMoveToDuration, MaxMoveToDuration), Distance);
	CurrentTime = 0.f;		
	
	bMovingToDestination = true;
	
	if(!HasAuthority())
	{
		SERVER_MoveTo(Destination);
	}
}

void ACRPG_PlayerCamera::SERVER_MoveTo_Implementation(const FTransform Destination)
{
	MoveTo(Destination);
}

void ACRPG_PlayerCamera::StopMoveTo()
{
	bMovingToDestination = false;
	
	if(!HasAuthority())
	{
		SERVER_StopMoveTo();
	}
}

void ACRPG_PlayerCamera::SERVER_StopMoveTo_Implementation()
{
	StopMoveTo();
}

void ACRPG_PlayerCamera::MoveToDestination(float DeltaSeconds)
{
	if(!bMovingToDestination)
	{
		return;
	}
	
	CurrentTime += DeltaSeconds;

	// Smooth the in and out of the move to.
	float Alpha = FMath::Clamp(CurrentTime / TotalDuration, 0.0f, 1.0f);
	Alpha = Alpha < 0.5f
		? 4.0f * Alpha * Alpha * Alpha
		: 1.0f - FMath::Pow(-2.0f * Alpha + 2.0f, 3.0f) / 2.0f;

	FTransform NewTransform = FTransform::Identity;
	NewTransform.SetLocation(FMath::Lerp(CameraStart.GetLocation(), CameraDestination.GetLocation(), Alpha));
	if(bRotationBlocked)
	{		
		NewTransform.SetRotation(FQuat::Slerp(CameraStart.GetRotation(), CameraDestination.GetRotation(), Alpha));
		SetActorTransform(NewTransform);
	}
	else
	{
		SetActorLocation(NewTransform.GetLocation());
	}

	if (Alpha >= 1.0f && !IsFollowingTarget())
	{
		StopMoveTo();
	}

	if(HasAuthority())
	{
		MULTICAST_MoveToDestination(NewTransform);
	}
}

void ACRPG_PlayerCamera::MULTICAST_MoveToDestination_Implementation(FTransform NewTransform)
{
	// If the owner of this actor isn't locally owned then update the transform to the server transform.
	if(!IsValid(Cast<APlayerController>(GetOwner())) || !Cast<APlayerController>(GetOwner())->IsLocalController())
	{
		if(bRotationBlocked)
		{		
			SetActorTransform(NewTransform);
		}
		else
		{
			SetActorLocation(NewTransform.GetLocation());
		}
	}
}

/* --------------------------------------------- END: Move To Location ---------------------------------------------- */

/* --------------------------------------------- BEGIN: Follow Target ----------------------------------------------- */

void ACRPG_PlayerCamera::FollowTarget(AActor* ActorToFollow, bool bLockRotation /* = false */)
{
	if(!IsValid(ActorToFollow) || IsFollowingTarget(ActorToFollow))
	{
		return;
	}

	if(TargetToFollow.IsValid())
	{
		StopFollowTarget();
	}

	bRotationBlocked = bLockRotation;
	TargetToFollow = ActorToFollow;
	bIsFollowingTarget = true;
	MoveTo(TargetToFollow->GetActorTransform());
}

void ACRPG_PlayerCamera::StopFollowTarget()
{
	bRotationBlocked = false;
	bIsFollowingTarget = false;
	TargetToFollow = nullptr;
}

void ACRPG_PlayerCamera::TickFollowTarget(float DeltaTime)
{
	if(TargetToFollow.IsValid())
	{
		CameraDestination = TargetToFollow->GetActorTransform();
	}
	else
	{
		bIsFollowingTarget = false;
	}
}

/* --------------------------------------------- END: Follow Target ------------------------------------------------- */