// Copyright Epic Games, Inc. All Rights Reserved.

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

	PhysicsConstraint = CreateDefaultSubobject<UPhysicsConstraintComponent>(TEXT("PhysicsConstraint"));
	PhysicsConstraint->SetupAttachment(RootComponent);

	GrappleConstraint = CreateDefaultSubobject<UPhysicsConstraintComponent>(TEXT("GrappleConstraint"));
	GrappleConstraint->SetupAttachment(RootComponent);

	// GrappleCable->SetupAttachment(GetMesh(), FName("middle_metacarpal_r"));
	// GrappleCable->bAttachEnd = true;

	GrappleEndPosition = CreateDefaultSubobject<USceneComponent>(TEXT("GrappleEndPosition"));
	GrappleEndPosition->SetupAttachment(RootComponent);
	
	GrappleCable = CreateDefaultSubobject<UCableComponent>(TEXT("GrappleCable"));
	GrappleCable->SetupAttachment(GetMesh(), FName(TEXT("middle_metacarpal_rSocket")));
	GrappleCable->bAttachEnd = true;
	GrappleCable->SetAttachEndToComponent(GrappleEndPosition);
	GrappleCable->SetVisibility(false); // Start hidden


	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void ADegreeProjectCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bIsSwinging)
	{
		FVector DirectionToAnchor = (CurrentGrapplePoint - GetActorLocation());
		float Distance = DirectionToAnchor.Size();
		FVector RopeDir = DirectionToAnchor / Distance;

		FVector Velocity = GetCharacterMovement()->Velocity;

		FVector Gravity = FVector(0.f, 0.f, -980.f);

		FVector TangentVelocity = Velocity - FVector::DotProduct(Velocity, RopeDir) * RopeDir;

		FVector TangentGravity = Gravity - FVector::DotProduct(Gravity, RopeDir) * RopeDir;

		FVector NewVelocity = TangentVelocity + TangentGravity * DeltaSeconds;

		GetCharacterMovement()->Velocity = NewVelocity;
	
		UE_LOG(LogTemp, Display, TEXT("New Velocity: %f"), NewVelocity.Size());
		
		GrappleEndPosition->SetWorldLocation(CurrentGrapplePoint);
	
		// Optional: update cable visuals
		// GrappleCable->SetWorldLocation(CurrentGrapplePoint);
		// GrappleCable->bAttachEnd = false;
		// GrappleCable->EndLocation = CurrentGrapplePoint;
		// GrappleCable->CableLength = FVector::Dist(GrappleEndPosition->GetComponentLocation(), CurrentGrapplePoint);

		
	
		// Velocity Direction
		DrawDebugLine(
		GetWorld(),
		GetActorLocation(),
		GetActorLocation() + GetCharacterMovement()->Velocity * 0.1f,
		FColor::Blue,
		false, -1.f, 0, 2.f);
	
		// Tangential Gravity
		DrawDebugLine(
		GetWorld(),
		GetActorLocation(),
		GetActorLocation() + TangentGravity * 0.1f,
		FColor::Red,
		false, -1.f, 0, 2.f);
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
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
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
		DrawDebugLine(GetWorld(), Start, HitResult.Location, FColor::Emerald, false, 1.0f, 0, 1.0f);
		Swing(HitResult.Location, HitResult.GetActor());
	}
	else
	{
		DrawDebugLine(GetWorld(), Start, End, FColor::Purple, false, 1.0f, 0, 1.0f);
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
	// Move the end position to the grapple point
	// GrappleEndPosition->SetWorldLocation(CurrentGrapplePoint);
	GrappleCable->SetVisibility(true);
	// GrappleCable->CableLength = FVector::Dist(GrappleCable->GetComponentLocation(), CurrentGrapplePoint);
	
	
	// GrappleCable->SetWorldLocation(HitLocation);
	// GrappleCable->CableLength = FVector::Dist(Muzzle->GetComponentLocation(), HitLocation);

	// Give it an initial lateral swing to the right
	// GetCharacterMovement()->Velocity += GetActorForwardVector() * 2000.f;
}


