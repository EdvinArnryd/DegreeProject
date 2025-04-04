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

	GrappleCable = CreateDefaultSubobject<UCableComponent>(TEXT("GrappleCable"));
	GrappleCable->SetupAttachment(Muzzle);
	GrappleCable->bAttachEnd = true;


	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void ADegreeProjectCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// if (bIsGrappling)
	// {
	// 	FVector SwingPoint = GrappleConstraint->GetComponentLocation();
	// 	FVector PlayerLocation = GetActorLocation();
	//
	// 	// 🚀 **Spring Force Towards Swing Point**
	// 	FVector DirectionToSwing = (SwingPoint - PlayerLocation).GetSafeNormal();
	// 	float Distance = FVector::Dist(SwingPoint, PlayerLocation);
	// 	float SpringForceMagnitude = FMath::Clamp(Distance * 20.0f, 0.0f, 5000.0f);
	//
	// 	FVector SwingForce = DirectionToSwing * SpringForceMagnitude;
	//
	// 	GetCharacterMovement()->AddForce(SwingForce);
	//
	// 	// 🚀 **Gravity Effect**
	// 	FVector GravityForce = FVector(0, 0, -980.0f);
	// 	GetCharacterMovement()->AddForce(GravityForce * GetCharacterMovement()->Mass);
	// }
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
		EnhancedInputComponent->BindAction(FireGunAction, ETriggerEvent::Started, this, &ADegreeProjectCharacter::FireGun);
		EnhancedInputComponent->BindAction(FireGunAction, ETriggerEvent::Completed, this, &ADegreeProjectCharacter::ReleaseGun);
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

void ADegreeProjectCharacter::PullPlayer(FVector HitLocation)
{
	FVector PlayerLocation = GetActorLocation();
	FVector PullDirection = HitLocation - PlayerLocation;
	PullDirection.Normalize();

	float PullStrength = 1500.f;

	FVector LaunchVelocity = PullDirection * PullStrength;
	
	LaunchCharacter(LaunchVelocity,true, true);
}

void ADegreeProjectCharacter::FireGun()
{
	UE_LOG(LogTemp, Warning, TEXT("Fired the gun!"));
	FVector Start = Muzzle->GetComponentLocation();
	FVector ForwardVector = FollowCamera->GetForwardVector();
	FVector End = Start + (ForwardVector * 10000.0f); // Trace 10,000 units forward

	FHitResult HitResult;
	FCollisionQueryParams TraceParams;
	TraceParams.AddIgnoredActor(this); // Ignore self

	bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		Start,
		End,
		ECC_Visibility,
		TraceParams
	);

	if (bHit)
	{
		DrawDebugLine(GetWorld(), Start, HitResult.Location, FColor::Red, false, 1.0f, 0, 1.0f);
		PullPlayer(HitResult.Location);
		AttachGrapple(HitResult.Location, HitResult.GetActor());
	}
	else
	{
		DrawDebugLine(GetWorld(), Start, End, FColor::Blue, false, 1.0f, 0, 1.0f);
	}
}

void ADegreeProjectCharacter::ReleaseGun()
{
	GetCapsuleComponent()->SetSimulatePhysics(false);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	GrappleCable->SetVisibility(false);

	// FVector launchForward = GetCharacterMovement()->GetForwardVector() * launchSpeed;
	
	// FVector CapsuleVelocity = GetCapsuleComponent()->GetComponentVelocity();
	// FVector CapsuleDirection = -CapsuleVelocity.GetSafeNormal();
	//
	// FVector LaunchForward = CapsuleDirection * launchSpeed;

	FVector CharacterVelocity = GetCharacterMovement()->Velocity;
	FVector CharacterDirection = CharacterVelocity.GetSafeNormal();

	FVector LaunchForward = CharacterDirection * launchSpeed;

	// FVector LaunchForward = GetFollowCamera()->GetForwardVector() * launchSpeed;

	LaunchCharacter(LaunchForward, true, true);
}

void ADegreeProjectCharacter::AttachGrapple(FVector HitLocation, AActor* HitActor)
{
    if (!HitActor) return;

    UPrimitiveComponent* HitComponent = Cast<UPrimitiveComponent>(HitActor->GetRootComponent());
    if (!HitComponent) return;

    // Move constraint to the hit location
    GrappleConstraint->SetWorldLocation(HitLocation);

    // Attach player capsule to the grapple point
    GrappleConstraint->SetConstrainedComponents(
        Cast<UPrimitiveComponent>(GetCapsuleComponent()), NAME_None,
        HitComponent, NAME_None
    );

    // Set swinging constraints
    GrappleConstraint->SetLinearXLimit(ELinearConstraintMotion::LCM_Limited, 500.0f);
    GrappleConstraint->SetLinearYLimit(ELinearConstraintMotion::LCM_Limited, 500.0f);
    GrappleConstraint->SetLinearZLimit(ELinearConstraintMotion::LCM_Limited, 500.0f);

    GrappleConstraint->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Limited, 360.0f);
    GrappleConstraint->SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Limited, 360.0f);
    GrappleConstraint->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Limited, 360.0f);

    // Enable the cable component
    GrappleCable->SetVisibility(true);
    GrappleCable->SetWorldLocation(HitLocation);
    GrappleCable->CableLength = FVector::Dist(Muzzle->GetComponentLocation(), HitLocation) / 10;

    // 🚀 **Hybrid Physics Fix**
    // GetCharacterMovement()->SetMovementMode(MOVE_Flying); // Keep controlled movement
    // GetCharacterMovement()->Velocity = FVector::ZeroVector;
    //
    // // 🚀 **Apply an initial impulse for the swing**
    FVector SwingDirection = (HitLocation - GetActorLocation()).GetSafeNormal();
    GetCapsuleComponent()->AddImpulse(SwingDirection * 50000.0f, NAME_None);

    // 🚀 **Enable physics interactions without overriding movement**
    GetCapsuleComponent()->SetSimulatePhysics(true); // Do NOT fully simulate physics
    GetCapsuleComponent()->SetEnableGravity(true); // Keep gravity enabled

    UE_LOG(LogTemp, Warning, TEXT("Grapple attached! Swinging started."));
}





void ADegreeProjectCharacter::SwingForward()
{
	FVector ForwardForce = GetActorForwardVector() * 1000.0f;
	GetCharacterMovement()->AddImpulse(ForwardForce, true);
}

void ADegreeProjectCharacter::SwingBackward()
{
	FVector BackwardForce = -GetActorForwardVector() * 1000.0f;
	GetCharacterMovement()->AddImpulse(BackwardForce, true);
}



