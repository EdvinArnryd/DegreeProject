// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "DegreeProjectCharacter.generated.h"

class UPhysicsConstraintComponent;
class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UCableComponent;
class UNiagaraComponent;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UCLASS(config=Game)
class ADegreeProjectCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* FireGunAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* SwingBoostAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	USceneComponent* Muzzle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics",  meta = (AllowPrivateAccess = "true"))
	UPhysicsConstraintComponent* PhysicsConstraint;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics",  meta = (AllowPrivateAccess = "true"))
	UPhysicsConstraintComponent* GrappleConstraint;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics",  meta = (AllowPrivateAccess = "true"))
	UCableComponent* GrappleCable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics",  meta = (AllowPrivateAccess = "true"))
	USceneComponent* GrappleEndPosition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swing",  meta = (AllowPrivateAccess = "true"))
	float SpeedBoostMultiplier = 1.3;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swing",  meta = (AllowPrivateAccess = "true"))
	UNiagaraComponent* SwingSpeedEffect;
	
	ADegreeProjectCharacter();
	
	virtual void Tick(float DeltaSeconds) override;
	
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swing",  meta = (AllowPrivateAccess = "true"))
	float launchSpeed = 1500;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swing",  meta = (AllowPrivateAccess = "true"))
	float PublicSwingSpeed = 1.01f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swing",  meta = (AllowPrivateAccess = "true"))
	float BoostAmount = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swing",  meta = (AllowPrivateAccess = "true"))
	float MaxSwingSpeed = 1500.0f;

public:
	
	UPROPERTY(BlueprintReadOnly, Category = "Swing")
	bool bIsSwinging;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	void SpeedBoost();

	virtual void NotifyControllerChanged() override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void FireHook();

	void ReleaseHook();

	void Swing(FVector HitLocation, AActor* HitActor);

	void StartBoosting();
	void StopBoosting();

	
	bool bIsGrappling;

	// Make this into Blueprintable
	UPROPERTY(BlueprintReadOnly)
	bool bIsBoosting;

	UPROPERTY()
	FVector CurrentGrapplePoint;
};

