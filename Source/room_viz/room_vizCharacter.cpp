﻿// Copyright Epic Games, Inc. All Rights Reserved.

#include "room_vizCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "GameFramework/Pawn.h"
#include "NavigationSystem.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "CollisionQueryParams.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "ui/UIUserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Framework/Application/SlateApplication.h" // at top
#include "Components/PrimitiveComponent.h"
#include "Engine/StaticMeshActor.h" // ✅ Add this at top of .cpp

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// Aroom_vizCharacter

Aroom_vizCharacter::Aroom_vizCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void Aroom_vizCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Enable mouse cursor & click/hover events on the PC
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = true;
		PC->bEnableClickEvents = true;
		PC->bEnableMouseOverEvents = true;
	}

	// Create & add our UI widget
	if (UIWidgetClass)
	{
		UIWidgetInstance = CreateWidget<UUIUserWidget>(GetWorld(), UIWidgetClass);
		if (UIWidgetInstance)
		{
			UIWidgetInstance->AddToViewport();
			UIWidgetInstance->SetVisibility(ESlateVisibility::Visible);

			// ─── KEY: route mouse events to the UI first ───
			if (APlayerController* PC2 = Cast<APlayerController>(GetController()))
			{
				FInputModeGameAndUI InputMode;
				InputMode.SetWidgetToFocus(UIWidgetInstance->TakeWidget());
				InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
				PC2->SetInputMode(InputMode);
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to create UIWidgetInstance"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("UIWidgetClass is not set!"));
	}
}

//////////////////////////////////////////////////////////////////////////
// Input

void Aroom_vizCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
	
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &Aroom_vizCharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &Aroom_vizCharacter::Look);

		// Touch movement
		EnhancedInputComponent->BindAction(TouchMoveAction, ETriggerEvent::Triggered, this, &Aroom_vizCharacter::TouchMove);

		// Drag Operation
		EnhancedInputComponent->BindAction(DragAction, ETriggerEvent::Triggered, this, &Aroom_vizCharacter::DragMove);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void Aroom_vizCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	
		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void Aroom_vizCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void Aroom_vizCharacter::TouchMove(const FInputActionValue& Value)
{
	// Perform a line trace from the mouse to the world
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		FVector WorldLoc, WorldDir;
		if (!PC->DeprojectMousePositionToWorld(WorldLoc, WorldDir))
			return;

		FHitResult Hit;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);

		const FVector Start = WorldLoc;
		const FVector End = WorldLoc + (WorldDir * 10000.f);

		if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_GameTraceChannel1, Params))
		{
			// Debug
			DrawDebugSphere(GetWorld(), Hit.ImpactPoint, 20.f, 12, FColor::Green, false, 2.f);

			MoveCharacterToLocation(Hit.ImpactPoint);
		}
	}
}

void Aroom_vizCharacter::DragMove(const FInputActionValue& Value)
{
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("DragMove called!"));
}

void Aroom_vizCharacter::MoveCharacterToLocation(const FVector& Destination)
{
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		// Issue a navigation request via the AI helper
		UAIBlueprintHelperLibrary::SimpleMoveToLocation(PC, Destination);
	}


}

void Aroom_vizCharacter::OnMaterialDropped(const FFloorMaterialData& DroppedData, const FVector2D& ScreenPosition)
{
	UE_LOG(LogTemp, Warning, TEXT("🧩 OnMaterialDropped called with material: %s"), *DroppedData.Name);
	UE_LOG(LogTemp, Log, TEXT("🖱️ ScreenPosition: X=%f, Y=%f"), ScreenPosition.X, ScreenPosition.Y);

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("❌ PlayerController not found"));
		return;
	}

	if (!DroppedData.MaterialAsset)
	{
		UE_LOG(LogTemp, Error, TEXT("❌ Dropped material is null"));
		return;
	}

	FVector WorldOrigin, WorldDirection;
	if (!PC->DeprojectScreenPositionToWorld(ScreenPosition.X, ScreenPosition.Y, WorldOrigin, WorldDirection))
	{
		UE_LOG(LogTemp, Error, TEXT("❌ Deprojection failed"));
		return;
	}

	FVector TraceStart = WorldOrigin;
	FVector TraceEnd = WorldOrigin + (WorldDirection * 10000.0f);

	UE_LOG(LogTemp, Log, TEXT("🧭 LineTrace: %s -> %s"), *TraceStart.ToString(), *TraceEnd.ToString());

	DrawDebugLine(GetWorld(), TraceStart, TraceEnd, FColor::Red, false, 5.0f, 0, 2.0f);

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
	{
		DrawDebugSphere(GetWorld(), Hit.ImpactPoint, 25.0f, 16, FColor::Green, false, 5.0f);

		UPrimitiveComponent* HitComp = Hit.GetComponent();
		UE_LOG(LogTemp, Log, TEXT("✅ Hit component: %s"), *GetNameSafe(HitComp));

		if (UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(HitComp))
		{
			MeshComp->SetMaterial(0, DroppedData.MaterialAsset);
			UE_LOG(LogTemp, Log, TEXT("🎯 Applied material to mesh: %s"), *MeshComp->GetName());
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("❌ Hit component is not UStaticMeshComponent"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("❌ Line trace did not hit anything"));
	}
}
