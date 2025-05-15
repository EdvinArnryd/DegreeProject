#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MovingBox.generated.h"

UCLASS()
class DEGREEPROJECT_API AMovingBox : public AActor
{
	GENERATED_BODY()
	
public:
	AMovingBox();
	
	virtual void Tick(float DeltaTime) override;

	virtual void BeginPlay() override;
	
	UPROPERTY(EditAnywhere)
	float TravelSpeed;

private:
	UPROPERTY()
	USceneComponent* Root;


};
