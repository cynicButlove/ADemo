#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DemoGrenade.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UNiagaraSystem;

UCLASS()
class DEMOCLIENT_API ADemoGrenade : public AActor
{
	GENERATED_BODY()

public:
	ADemoGrenade();

	void Launch(FVector Velocity);

protected:
	virtual void BeginPlay() override;

private:
	void Explode();

	UFUNCTION()
	void OnBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity);

	UPROPERTY(VisibleAnywhere, Category = "Components")
	USphereComponent* CollisionComponent;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UStaticMeshComponent* MeshComponent;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UProjectileMovementComponent* MovementComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Grenade")
	float FuseTime = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Grenade")
	float ExplosionRadius = 500.f;

	UPROPERTY(EditDefaultsOnly, Category = "Grenade")
	float ExplosionDamage = 150.f;

	UPROPERTY(EditDefaultsOnly, Category = "Grenade")
	float MinDamage = 30.f;

	UPROPERTY(EditDefaultsOnly, Category = "Grenade|Effects")
	UNiagaraSystem* ExplosionEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Grenade|Effects")
	USoundBase* ExplosionSound;

	UPROPERTY(EditDefaultsOnly, Category = "Grenade|Effects")
	USoundBase* BounceSound;

	FTimerHandle FuseTimerHandle;
	bool bHasExploded = false;
};
