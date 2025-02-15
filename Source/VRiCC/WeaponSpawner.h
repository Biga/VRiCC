// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeaponSpawner.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPickUpWeapon);

UCLASS()
class VRICC_API AWeaponSpawner : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AWeaponSpawner();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TSubclassOf<class AActor> WeaponClass;

	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnPickUpWeapon OnPickUpWeapon;


protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
