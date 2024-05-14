// Copyright Epic Games, Inc. All Rights Reserved.


#include "TP_WeaponComponent.h"
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
	_FiringMode = FiringMode::FiringMode_Single;
}

// press/hold fire main method
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
		GetWorld()->GetTimerManager().ClearTimer(AutoFireTimerHandle);
		UGameplayStatics::PlaySoundAtLocation(this, EmptySound, Character->GetActorLocation());
		return;
	}

	// single fire when pressed

	if (_FiringMode == FiringMode::FiringMode_Single)
	{
		FireAndHit();
	}

}

// call timer for auto fire
void UTP_WeaponComponent::AutoFire()
{
	if (_FiringMode == FiringMode::FiringMode_Auto)
	{
		GetWorld()->GetTimerManager().SetTimer(AutoFireTimerHandle, this, &UTP_WeaponComponent::FireAndHit, 0.5, true);

	}
}

// released fire input, stop auto fire
void UTP_WeaponComponent::FireStop()
{
	GetWorld()->GetTimerManager().ClearTimer(AutoFireTimerHandle);
}

void UTP_WeaponComponent::FireAndHit()
{
	// if called by autofire, return when weapon is out, player can reload with click
	if (_FiringMode == FiringMode::FiringMode_Auto && Character->VRiCC_ShotsLeft == 0)
	{
		FireStop();
		return;
	}
	
	
	Character->VRiCC_ShotsLeft--;
	Character->ShowAmmoInfo(_FiringMode);

	// line trace: TC_Weapon trace channel check - ECC_GameTraceChannel1
	FHitResult OutHit;
	const FRotator SpawnRotation = Character->GetControlRotation();
	const FVector MuzzlePos = GetSocketLocation("Muzzle");
	FVector ForwardVector = GetSocketRotation("GripPoint").Vector() * -1.0f;
	const FVector Start = MuzzlePos + (ForwardVector * 5.0f);

	FVector End = ((ForwardVector * 1000.f) + Start);
	FCollisionQueryParams CollisionParams;
	FCollisionObjectQueryParams CollObjects;
	CollisionParams.AddIgnoredActor(Character);

	// debug trace line: red: hit, green: no hit
	if (GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, ECollisionChannel::ECC_GameTraceChannel1, CollisionParams))
	{
		if (OutHit.bBlockingHit)
		{
			End = OutHit.Location;
			DrawDebugLine(GetWorld(), Start, End, FColor::Red, false, 1.0f, 0, 0.5f);

			FString n1 = OutHit.GetActor()->GetName();
			bool b1 = OutHit.Component->IsSimulatingPhysics();

			if ((OutHit.GetActor() != nullptr) && (OutHit.GetActor() != Character))
			{
				// add force to physical actors
				if (OutHit.Component != nullptr && OutHit.Component->IsSimulatingPhysics())
				{
					FString s1 = OutHit.Component.Get()->GetName();
					OutHit.Component->AddImpulseAtLocation(OutHit.ImpactNormal * -100000.0f, OutHit.ImpactPoint);
				}

				// add damage to enemy players
				if (Cast<ACharacter>(OutHit.GetActor()))
				{
					TSubclassOf<UDamageType> DmgTypeClass = UDamageType::StaticClass();

					OutHit.GetActor()->TakeDamage(0.1f, FDamageEvent(DmgTypeClass), GetOwner()->GetInstigatorController(), Character);
				}
			}
		}
		else
		{
			DrawDebugLine(GetWorld(), Start, End, FColor::Yellow, false, 1.0f, 0, 0.5f);
		}
	}
	else
	{
		DrawDebugLine(GetWorld(), Start, End, FColor::Green, false, 1.0f, 0, 0.5f);
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
		GetWorld()->GetTimerManager().SetTimer(ReloadTimerHandle, this, &UTP_WeaponComponent::ReloadAmmoReset, 1.0f, false);

		Character->VRiCC_ShotsLeft = Character->VRiCC_ShotsPerRack;
		Character->VRiCC_AmmoRacks--;
		Character->ShowAmmoInfo(_FiringMode);
	}
}

// simple switch for firing mode
// single = single shot
// auto = firing continously per 0.5 seconds until needs to reload/holding fire
void UTP_WeaponComponent::ChangeFireMode()
{
	if (_FiringMode == FiringMode::FiringMode_Single)
		_FiringMode = FiringMode::FiringMode_Auto;
	else
		_FiringMode = FiringMode::FiringMode_Single;
	Character->ShowAmmoInfo(_FiringMode);
}

// added check hold states of Fire input for auto firing mode
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
	Character->ShowAmmoInfo(_FiringMode);

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
			EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Started, this, &UTP_WeaponComponent::AutoFire);
			EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Completed, this, &UTP_WeaponComponent::FireStop);
			// Reload
			EnhancedInputComponent->BindAction(ReloadAction, ETriggerEvent::Triggered, this, &UTP_WeaponComponent::Reload);
			// Change fire mode
			EnhancedInputComponent->BindAction(FireModeAction, ETriggerEvent::Triggered, this, &UTP_WeaponComponent::ChangeFireMode);
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