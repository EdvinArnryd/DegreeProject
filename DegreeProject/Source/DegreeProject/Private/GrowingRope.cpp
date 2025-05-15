// GrowingRope.cpp

#include "GrowingRope.h"
#include "Engine/Engine.h"

AGrowingRope::AGrowingRope()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	RopeStart = CreateDefaultSubobject<USceneComponent>(TEXT("RopeStart"));
	RopeStart->SetupAttachment(RootComponent);

	RopeEnd = CreateDefaultSubobject<USceneComponent>(TEXT("RopeEnd"));
	RopeEnd->SetupAttachment(RootComponent);

	Cable = CreateDefaultSubobject<UCableComponent>(TEXT("Cable"));
	Cable->SetupAttachment(RopeStart);
	Cable->SetAttachEndToComponent(nullptr); // No attachment for manual control

	// Initialize cable parameters
	Cable->CableLength = 0.f;
	Cable->NumSegments = 50;
	Cable->SubstepTime = 0.01f;
	Cable->SolverIterations = 8;
	Cable->bEnableStiffness = true;

	TravelSpeed = 300.f;
	bIsGrowing = false;
}

void AGrowingRope::BeginPlay()
{
	Super::BeginPlay();

	// Initialize start and target positions
	FVector Start = RopeStart->GetComponentLocation();
	FVector End = Start + GetActorForwardVector() * 2000.f;

	RopeStart->SetWorldLocation(Start);
	// Start cable end at start location (relative to RopeStart)
	Cable->EndLocation = GetActorLocation();

	TargetLocation = End;
	bIsGrowing = true;

	Cable->SetVisibility(true);
}

void AGrowingRope::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bIsGrowing)
		return;

	// Current world-space position of cable end
	FVector CurrentEndWorld = RopeStart->GetComponentLocation() + Cable->EndLocation;
	FVector Direction = (TargetLocation - CurrentEndWorld).GetSafeNormal();
	FVector NewEndWorld = CurrentEndWorld + Direction * TravelSpeed * DeltaTime;

	// Clamp to target
	if (FVector::Dist(NewEndWorld, TargetLocation) < 10.f)
	{
		NewEndWorld = TargetLocation;
		bIsGrowing = false;
	}

	// Update Cable->EndLocation relative to RopeStart
	Cable->EndLocation = NewEndWorld - RopeStart->GetComponentLocation();

	// Optional debug info
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Green, FString::Printf(TEXT("Cable End: %s"), *NewEndWorld.ToString()));
	}
}
