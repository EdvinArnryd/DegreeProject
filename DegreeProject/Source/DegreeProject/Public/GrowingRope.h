// GrowingRope.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CableComponent.h"
#include "GrowingRope.generated.h"

UCLASS()
class DEGREEPROJECT_API AGrowingRope : public AActor
{
	GENERATED_BODY()
	
public:	
	AGrowingRope();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	USceneComponent* RopeStart;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	USceneComponent* RopeEnd;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UCableComponent* Cable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TravelSpeed;

private:
	FVector TargetLocation;
	bool bIsGrowing;
};

