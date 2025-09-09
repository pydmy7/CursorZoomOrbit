// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "CursorZoomOrbitCamera.generated.h"

class USceneComponent;
class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

UCLASS()
class CURSORZOOMORBIT_API ACursorZoomOrbitCamera : public APawn
{
	GENERATED_BODY()

public:
	ACursorZoomOrbitCamera();

protected:
	virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    UPROPERTY(EditDefaultsOnly, Category="Input")
    TObjectPtr<UInputMappingContext> IMC_Camera = nullptr;

    UPROPERTY(EditDefaultsOnly, Category="Input")
    TObjectPtr<UInputAction> IA_MouseXY = nullptr;

    UPROPERTY(EditDefaultsOnly, Category="Input")
    TObjectPtr<UInputAction> IA_MouseWheel = nullptr;

    UPROPERTY(EditDefaultsOnly, Category="Input")
    TObjectPtr<UInputAction> IA_LMB = nullptr;

    UPROPERTY(EditDefaultsOnly, Category="Input")
    TObjectPtr<UInputAction> IA_MMB = nullptr;

    UPROPERTY(EditDefaultsOnly, Category="Input")
    TObjectPtr<UInputAction> IA_Reset = nullptr;

private:
    UPROPERTY()
    TObjectPtr<USceneComponent> Pivot;
    UPROPERTY()
    TObjectPtr<USpringArmComponent> SpringArm;
    UPROPERTY()
    TObjectPtr<UCameraComponent> Camera;

    float DefaultArmLength = 800.f;
    FRotator DefaultArmRot = FRotator::ZeroRotator;
    FVector DefaultPivotLoc = FVector::ZeroVector;

    bool bOrbiting = false;
    bool bPanning = false;

    void OnLMBStarted(const FInputActionValue& Value);
    void OnLMBCompleted(const FInputActionValue& Value);
    void OnMMBStarted(const FInputActionValue& Value);
    void OnMMBCompleted(const FInputActionValue& Value);

    void OnMouseXY(const FInputActionValue& Value);
    void OnMouseWheel(const FInputActionValue& Value);
    void OnReset(const FInputActionValue& Value);

    void ApplyOrbit(float DeltaX, float DeltaY);
    void ApplyPan(float DeltaX, float DeltaY);
};
