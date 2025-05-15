#include "MovingBox.h"



AMovingBox::AMovingBox()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	TravelSpeed = 1.f;
}

void AMovingBox::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FVector Direction = GetActorForwardVector(); // Normalized direction
	FVector NewLocation = GetActorLocation() + Direction * TravelSpeed * DeltaTime;

	SetActorLocation(NewLocation);
}


void AMovingBox::BeginPlay()
{
	Super::BeginPlay();
}
