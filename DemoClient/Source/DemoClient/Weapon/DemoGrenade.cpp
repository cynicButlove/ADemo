#include "Weapon/DemoGrenade.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"

ADemoGrenade::ADemoGrenade()
{
	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	CollisionComponent->InitSphereRadius(8.f);
	CollisionComponent->SetCollisionProfileName(TEXT("Projectile"));
	CollisionComponent->SetSimulatePhysics(false);
	RootComponent = CollisionComponent;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(CollisionComponent);
	MeshComponent->SetRelativeScale3D(FVector(0.15f));
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	MovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("MovementComponent"));
	MovementComponent->UpdatedComponent = CollisionComponent;
	MovementComponent->InitialSpeed = 0.f;
	MovementComponent->MaxSpeed = 3000.f;
	MovementComponent->bRotationFollowsVelocity = true;
	MovementComponent->bShouldBounce = true;
	MovementComponent->Bounciness = 0.3f;
	MovementComponent->Friction = 0.5f;
	MovementComponent->ProjectileGravityScale = 1.0f;

	MovementComponent->OnProjectileBounce.AddDynamic(this, &ADemoGrenade::OnBounce);
}

void ADemoGrenade::BeginPlay()
{
	Super::BeginPlay();

	GetWorldTimerManager().SetTimer(
		FuseTimerHandle, this, &ADemoGrenade::Explode, FuseTime, false);
}

void ADemoGrenade::Launch(FVector Velocity)
{
	MovementComponent->Velocity = Velocity;
}

void ADemoGrenade::Explode()
{
	if (bHasExploded) return;
	bHasExploded = true;

	FVector Location = GetActorLocation();

	if (ExplosionEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(), ExplosionEffect, Location, FRotator::ZeroRotator, FVector(1.f), true);
	}

	if (ExplosionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), ExplosionSound, Location);
	}

	TArray<AActor*> IgnoredActors;
	UGameplayStatics::ApplyRadialDamageWithFalloff(
		GetWorld(),
		ExplosionDamage,
		MinDamage,
		Location,
		ExplosionRadius * 0.3f,
		ExplosionRadius,
		1.f,
		nullptr,
		IgnoredActors,
		GetOwner(),
		GetInstigatorController(),
		ECC_Visibility);

	Destroy();
}

void ADemoGrenade::OnBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity)
{
	if (BounceSound)
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), BounceSound, GetActorLocation(), 0.5f);
	}
}
