// Copyright Epic Games, Inc. All Rights Reserved.


#include "TP_WeaponComponent.h"
#include "VRiCCCharacter.h"
#include "VRiCCProjectile.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "DrawDebugHelpers.h"
#include "Camera/CameraComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/DamageEvents.h"

// Sets default values for this component's properties
UTP_WeaponComponent::UTP_WeaponComponent()
{
	// Default offset from the character location for projectiles to spawn
	MuzzleOffset = FVector(100.0f, 0.0f, 10.0f);
}


void UTP_WeaponComponent::Fire()
{
	if (Character == nullptr || Character->GetController() == nullptr)
	{
		return;
	}

	// Reloading state (1sec)
	if (_Reloading)
	{
		return;
	}

	// Need to reload
	if (Character->VRiCC_ShotsLeft == 0)
	{
		if (Character->VRiCC_AmmoRacks > 0)
		{
			Reload();
			return;
		}

		// out of ammo
		UGameplayStatics::PlaySoundAtLocation(this, EmptySound, Character->GetActorLocation());
		return;
	}

	// fire
	Character->VRiCC_ShotsLeft--;
	Character->ShowAmmoInfo();

	FHitResult OutHit;
	const FRotator SpawnRotation = Character->GetControlRotation();
	//const FVector Start = GetComponentLocation() + SpawnRotation.RotateVector(MuzzleOffset);
	const FVector MuzzlePos = GetSocketLocation("Muzzle");
	FVector ForwardVector = GetSocketRotation("GripPoint").Vector() * -1.0f;
	const FVector Start = MuzzlePos + (ForwardVector * 10.0f);

	FVector End = ((ForwardVector * 1000.f) + Start);
	FCollisionQueryParams CollisionParams;
	FCollisionObjectQueryParams CollObjects;
	CollisionParams.AddIgnoredActor(Character);

	CollObjects.AddObjectTypesToQuery(ECollisionChannel::ECC_Pawn);
	CollObjects.AddObjectTypesToQuery(ECollisionChannel::ECC_PhysicsBody);

	// TC_Weapon trace channel check - ECC_GameTraceChannel1
	if (GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, ECollisionChannel::ECC_GameTraceChannel1, CollisionParams))
	//if (GetWorld()->LineTraceSingleByObjectType(OutHit, Start, End, CollObjects, CollisionParams))
	{
		if (OutHit.bBlockingHit)
		{
			End = OutHit.Location;
			DrawDebugLine(GetWorld(), Start, End, FColor::Red, false, 1.0f, 0, 0.5f);

			FString n1 = OutHit.GetActor()->GetName();
			bool b1 = OutHit.Component->IsSimulatingPhysics();
			
			if ((OutHit.GetActor() != nullptr) && (OutHit.GetActor() != Character))
			{
				// physical actors
				if (OutHit.Component != nullptr && OutHit.Component->IsSimulatingPhysics())
				{
					FString s1 = OutHit.Component.Get()->GetName();
					OutHit.Component->AddImpulseAtLocation(OutHit.ImpactNormal * -100000.0f, OutHit.ImpactPoint);
				}

				// enemy players
				if (Cast<ACharacter>(OutHit.GetActor()))
				{
					TSubclassOf<UDamageType> DmgTypeClass = UDamageType::StaticClass();

					OutHit.GetActor()->TakeDamage(0.1f, FDamageEvent(DmgTypeClass), GetOwner()->GetInstigatorController(), Character);

				}
			}
			
		}
		else
		{
			DrawDebugLine(GetWorld(), Start, End, FColor::Yellow , true, -1.0f, 0, 0.5f);
		}
	}
	else
	{
		DrawDebugLine(GetWorld(), Start, End, FColor::Green, true, -1.0f, 0, 0.5f);
	}
	

	// Try and play the sound if specified
	if (FireSound != nullptr)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, Character->GetActorLocation());
	}
	
	// Try and play a firing animation if specified
	if (FireAnimation != nullptr)
	{
		// Get the animation object for the arms mesh
		UAnimInstance* AnimInstance = Character->GetMesh1P()->GetAnimInstance();
		if (AnimInstance != nullptr)
		{
			AnimInstance->Montage_Play(FireAnimation, 1.f);
		}
	}
}

// Fill the ammorack and decrease racks number
void UTP_WeaponComponent::Reload()
{
	if (Character && Character->VRiCC_ShotsLeft < Character->VRiCC_ShotsPerRack && Character->VRiCC_AmmoRacks > 0)
	{
		_Reloading = true;

		UGameplayStatics::PlaySoundAtLocation(this, ReloadSound, Character->GetActorLocation());
		GetWorld()->GetTimerManager().SetTimer(ReloadTimerHandle, this, &UTP_WeaponComponent::ReloadAmmoReset, 1.0, false);

		Character->VRiCC_ShotsLeft = Character->VRiCC_ShotsPerRack;
		Character->VRiCC_AmmoRacks--;
		Character->ShowAmmoInfo();
	}
}


void UTP_WeaponComponent::AttachWeapon(AVRiCCCharacter* TargetCharacter)
{
	Character = TargetCharacter;

	// Check that the character is valid, and has no rifle yet
	if (Character == nullptr || Character->GetHasRifle())
	{
		return;
	}

	// Attach the weapon to the First Person Character
	FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
	AttachToComponent(Character->GetMesh1P(), AttachmentRules, FName(TEXT("GripPoint")));
	
	// switch bHasRifle so the animation blueprint can switch to another animation set
	Character->SetHasRifle(true);
	Character->AttachWeaponHUD(Character->GetMesh1P(), FName(TEXT("GripPoint")));
	Character->ShowAmmoInfo();

	// Set up action bindings
	if (APlayerController* PlayerController = Cast<APlayerController>(Character->GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			// Set the priority of the mapping to 1, so that it overrides the Jump action with the Fire action when using touch input
			Subsystem->AddMappingContext(FireMappingContext, 1);
		}

		if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerController->InputComponent))
		{
			// Fire
			EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Triggered, this, &UTP_WeaponComponent::Fire);
			// Reload
			EnhancedInputComponent->BindAction(ReloadAction, ETriggerEvent::Triggered, this, &UTP_WeaponComponent::Reload);
		}
	}
}

void UTP_WeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Character == nullptr)
	{
		return;
	}

	if (APlayerController* PlayerController = Cast<APlayerController>(Character->GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->RemoveMappingContext(FireMappingContext);
		}
	}
}