#include "CursorZoomOrbitCamera.h"

#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/SceneComponent.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Kismet/GameplayStatics.h"

ACursorZoomOrbitCamera::ACursorZoomOrbitCamera()
{
    PrimaryActorTick.bCanEverTick = true;

    Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));

    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(Pivot);
    SpringArm->TargetArmLength = 800.f;
    SpringArm->bDoCollisionTest = false;
    SpringArm->bUsePawnControlRotation = false;
    SpringArm->bInheritPitch = false;
    SpringArm->bInheritYaw = false;
    SpringArm->bInheritRoll = false;

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(SpringArm);

    RootComponent = Pivot;
}

void ACursorZoomOrbitCamera::BeginPlay()
{
    Super::BeginPlay();

    DefaultArmLength = SpringArm->TargetArmLength;
    DefaultArmRot = SpringArm->GetRelativeRotation();
    DefaultPivotLoc = Pivot->GetComponentLocation();

    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        PC->bShowMouseCursor = true;
        FInputModeGameOnly Mode;
        Mode.SetConsumeCaptureMouseDown(false);
        PC->SetInputMode(Mode);

        if (ULocalPlayer* LP = PC->GetLocalPlayer())
        {
            if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
            {
                if (IMC_Camera)
                {
                    Subsystem->AddMappingContext(IMC_Camera, 0);
                }
            }
        }
    }
}

void ACursorZoomOrbitCamera::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);
}

void ACursorZoomOrbitCamera::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (IA_LMB)
        {
            EIC->BindAction(IA_LMB, ETriggerEvent::Started,   this, &ACursorZoomOrbitCamera::OnLMBStarted);
            EIC->BindAction(IA_LMB, ETriggerEvent::Completed, this, &ACursorZoomOrbitCamera::OnLMBCompleted);
            EIC->BindAction(IA_LMB, ETriggerEvent::Canceled,  this, &ACursorZoomOrbitCamera::OnLMBCompleted);
        }
        if (IA_MMB)
        {
            EIC->BindAction(IA_MMB, ETriggerEvent::Started,   this, &ACursorZoomOrbitCamera::OnMMBStarted);
            EIC->BindAction(IA_MMB, ETriggerEvent::Completed, this, &ACursorZoomOrbitCamera::OnMMBCompleted);
            EIC->BindAction(IA_MMB, ETriggerEvent::Canceled,  this, &ACursorZoomOrbitCamera::OnMMBCompleted);
        }
        if (IA_MouseXY)
        {
            EIC->BindAction(IA_MouseXY, ETriggerEvent::Triggered, this, &ACursorZoomOrbitCamera::OnMouseXY);
        }
        if (IA_MouseWheel)
        {
            EIC->BindAction(IA_MouseWheel, ETriggerEvent::Triggered, this, &ACursorZoomOrbitCamera::OnMouseWheel);
        }
        if (IA_Reset)
        {
            EIC->BindAction(IA_Reset, ETriggerEvent::Triggered, this, &ACursorZoomOrbitCamera::OnReset);
        }
    }
}

void ACursorZoomOrbitCamera::OnLMBStarted(const FInputActionValue& /*Value*/)
{
    bOrbiting = true;
}

void ACursorZoomOrbitCamera::OnLMBCompleted(const FInputActionValue& /*Value*/)
{
    bOrbiting = false;
}

void ACursorZoomOrbitCamera::OnMMBStarted(const FInputActionValue& /*Value*/)
{
    bPanning = true;
}

void ACursorZoomOrbitCamera::OnMMBCompleted(const FInputActionValue& /*Value*/)
{
    bPanning = false;
}

void ACursorZoomOrbitCamera::OnMouseXY(const FInputActionValue& Value) {
    const float DX = Value.Get<FVector2D>().X;
    const float DY = Value.Get<FVector2D>().Y;

    if (bOrbiting) {
        ApplyOrbit(DX, DY);
    } else if (bPanning) {
        ApplyPan(DX, DY);
    }
}

void ACursorZoomOrbitCamera::OnMouseWheel(const FInputActionValue& Value)
{
    if (!SpringArm || !Camera || !Pivot) {
        return;
    }

    APlayerController* PC = nullptr;
    if (APawn* AsPawn = Cast<APawn>(this)) {
        PC = Cast<APlayerController>(AsPawn->GetController());
    }
    if (!PC) {
        PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    }
    if (!PC) {
        return;
    }

    const float MinZoom = 150.f;
    const float MaxZoom = 6000.f;
    const float ZoomSpeed = 200.f;

    const float W  = Value.Get<float>();
    const float L0 = SpringArm->TargetArmLength;
    float L1 = FMath::Clamp(L0 - W * ZoomSpeed, MinZoom, MaxZoom);
    const float f = (L0 > KINDA_SMALL_NUMBER) ? (L1 / L0) : 1.f;

    if (FMath::IsNearlyEqual(f, 1.f)) {
        return;
    }

    float MouseX = 0.f, MouseY = 0.f;
    if (!PC->GetMousePosition(MouseX, MouseY)) {
        SpringArm->TargetArmLength = L1;
        return;
    }

    FVector RayOrigin, RayDir;
    if (!UGameplayStatics::DeprojectScreenToWorld(PC, FVector2D(MouseX, MouseY), RayOrigin, RayDir)) {
        SpringArm->TargetArmLength = L1;
        return;
    }

    FVector Anchor = FVector::ZeroVector;
    {
        FHitResult Hit;
        const float TraceDist = 100000.f;
        const FVector TraceEnd = RayOrigin + RayDir * TraceDist;

        FCollisionQueryParams Params(SCENE_QUERY_STAT(ZoomTrace), true, this);
        const bool bHit = GetWorld()->LineTraceSingleByChannel(
            Hit, RayOrigin, TraceEnd, ECC_Visibility, Params
        );

        if (bHit) {
            Anchor = Hit.ImpactPoint;
        } else {
            const FVector P = Pivot->GetComponentLocation();
            float t = FVector::DotProduct(P - RayOrigin, RayDir);
            if (t < 0.f) {
                t = 0.f;
            }
            Anchor = RayOrigin + RayDir * t;
        }
    }

    const FVector C0 = Camera->GetComponentLocation();
    const FVector Fwd = Camera->GetForwardVector();

    const FVector DeltaC = (1.f - f) * (Anchor - C0);
    const FVector DeltaP = DeltaC + Fwd * (L1 - L0);

    Pivot->AddWorldOffset(DeltaP, false);
    SpringArm->TargetArmLength = L1;
}

void ACursorZoomOrbitCamera::OnReset(const FInputActionValue& /*Value*/)
{
    SpringArm->TargetArmLength = DefaultArmLength;
    SpringArm->SetRelativeRotation(DefaultArmRot);
    Pivot->SetWorldLocation(DefaultPivotLoc);
}

void ACursorZoomOrbitCamera::ApplyOrbit(float DeltaX, float DeltaY)
{
    float OrbitSpeed = 1.5f;

    constexpr float YSign = -1.f;
    const float s = OrbitSpeed * 3.0f;

    const FQuat q = SpringArm->GetComponentQuat();
    const FVector camRight = q.GetRightVector();
    const FVector camUp = q.GetUpVector();

    const FQuat dq = FQuat(camUp, FMath::DegreesToRadians(DeltaX * s)) * FQuat(camRight, FMath::DegreesToRadians(DeltaY * s * YSign));

    SpringArm->SetRelativeRotation((dq * q).GetNormalized());
}

void ACursorZoomOrbitCamera::ApplyPan(float DeltaX, float DeltaY)
{
    float PanSpeed = 7.0f;
    const FVector Right = Camera->GetRightVector();
    const FVector Up = Camera->GetUpVector();
    const float PanScale = PanSpeed * (SpringArm->TargetArmLength / 800.f);

    const float PanYSign = -1.f;
    const FVector Offset = (-Right * DeltaX + Up * (PanYSign * DeltaY)) * PanScale;

    Pivot->AddWorldOffset(Offset, false);
}
