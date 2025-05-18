// Copyright Epic Games, Inc. All Rights Reserved.

#include "DegreeProjectCharacter.h"
#include "DegreeProjectCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "CableComponent.h"
#include "MovieSceneTracksComponentTypes.h"
#include "Storage/Nodes/FileEntry.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// ADegreeProjectCharacter

ADegreeProjectCharacter::ADegreeProjectCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	Muzzle = CreateDefaultSubobject<USceneComponent>(TEXT("Muzzle"));
	Muzzle->SetupAttachment(RootComponent);

	GrappleEndPosition = CreateDefaultSubobject<USceneComponent>(TEXT("GrappleEndPosition"));
	GrappleEndPosition->SetupAttachment(RootComponent);

	// Cable
	GrappleCable = CreateDefaultSubobject<UCableComponent>(TEXT("GrappleCable"));
	GrappleCable->SetupAttachment(GetMesh(), FName(TEXT("middle_metacarpal_rSocket")));
	GrappleCable->bAttachEnd = true;
	GrappleCable->SetAttachEndToComponent(GrappleEndPosition);
	GrappleCable->SetVisibility(false);
}

void ADegreeProjectCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Dynamically adjust FOV based on velocity
	float CurrentSpeed = GetVelocity().Size();
	float TargetFOV = FMath::GetMappedRangeValueClamped(
		FVector2D(0.0f, MaxSwingSpeed),
		FVector2D(BaseFOV, MaxFOV),
		CurrentSpeed
	);
	
	if (bIsSwinging)
	{
		FVector DirectionToAnchor = (CurrentGrapplePoint - GetActorLocation());
		float Distance = DirectionToAnchor.Size();
		FVector RopeDir = DirectionToAnchor / Distance;

		FVector Velocity = GetCharacterMovement()->Velocity;

		FVector Gravity = FVector(0.f, 0.f, -980.f);

		FVector TangentVelocity = Velocity - FVector::DotProduct(Velocity, RopeDir) * RopeDir;

		FVector NewVelocity = TangentVelocity + Gravity * DeltaSeconds;
		
		GetCharacterMovement()->Velocity = NewVelocity;
		
		if (bIsBoosting)
		{
			if (GetCharacterMovement()->Velocity.Size() < MaxSwingSpeed)
			{
				GetCharacterMovement()->Velocity += GetCharacterMovement()->GetForwardVector() * 20;
			}
		}

		// Rotate to rope
		FVector RopeDirection = (CurrentGrapplePoint - GetActorLocation()).GetSafeNormal();
		TangentVelocity.Normalize();

		FVector Forward = TangentVelocity;
		FVector Up = RopeDirection;
		FVector Right = FVector::CrossProduct(Up, Forward);
		Forward = FVector::CrossProduct(Right, Up);
		
		FMatrix RotationMatrix(Forward, Right, Up, FVector::ZeroVector);
		FRotator NewRotation = RotationMatrix.Rotator();
		SetActorRotation(NewRotation);

		// Interpolate FOV
		float NewFOV = FMath::FInterpTo(FollowCamera->FieldOfView, TargetFOV, DeltaSeconds, FOVInterpSpeed);
		FollowCamera->SetFieldOfView(NewFOV);
		
	}
	else
	{
		float NewFOV = FMath::FInterpTo(FollowCamera->FieldOfView, BaseFOV, DeltaSeconds, FOVInterpSpeed);
		FollowCamera->SetFieldOfView(NewFOV);
	}
}

void ADegreeProjectCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	// Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void ADegreeProjectCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADegreeProjectCharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ADegreeProjectCharacter::Look);

		// Fire Gun
		EnhancedInputComponent->BindAction(FireGunAction, ETriggerEvent::Started, this, &ADegreeProjectCharacter::FireHook);
		EnhancedInputComponent->BindAction(FireGunAction, ETriggerEvent::Completed, this, &ADegreeProjectCharacter::ReleaseHook);

		EnhancedInputComponent->BindAction(SwingBoostAction, ETriggerEvent::Started, this, &ADegreeProjectCharacter::StartBoosting);
		EnhancedInputComponent->BindAction(SwingBoostAction, ETriggerEvent::Completed, this, &ADegreeProjectCharacter::StopBoosting);
	}
	else
	{
	}
}

void ADegreeProjectCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	
		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void ADegreeProjectCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void ADegreeProjectCharacter::FireHook()
{
	FVector Start = Muzzle->GetComponentLocation();
	FVector ForwardVector = FollowCamera->GetForwardVector();
	FVector End = Start + (ForwardVector * 10000.0f);

	FHitResult HitResult;
	FCollisionQueryParams TraceParams;
	TraceParams.AddIgnoredActor(this);

	bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		Start,
		End,
		ECC_Visibility,
		TraceParams
	);

	if (bHit)
	{
		Swing(HitResult.Location, HitResult.GetActor());
	}
	else
	{
	}
}

void ADegreeProjectCharacter::ReleaseHook()
{
	if (bIsSwinging)
	{
		GrappleCable->SetVisibility(false);
		bIsSwinging = false;

		GetCharacterMovement()->BrakingDecelerationFalling = 1.0f;
		GetCharacterMovement()->FallingLateralFriction = 1.0f;
		GetCharacterMovement()->AirControl = 1.0f;
	}
		
}

void ADegreeProjectCharacter::Swing(FVector HitLocation, AActor* HitActor)
{
	bIsSwinging = true;
	
	CurrentGrapplePoint = HitLocation;
	
	GetCharacterMovement()->BrakingDecelerationFalling = 0.0f;
	GetCharacterMovement()->FallingLateralFriction = 0.0f;
	GetCharacterMovement()->AirControl = 0.0f;

	// Cable
	GrappleCable->SetVisibility(true);
	GrappleEndPosition->SetWorldLocation(CurrentGrapplePoint);
	GrappleCable->EndLocation = GrappleEndPosition->GetComponentLocation();
}

void ADegreeProjectCharacter::StartBoosting()
{
	bIsBoosting = true;
}

void ADegreeProjectCharacter::StopBoosting()
{
	bIsBoosting = false;
}


