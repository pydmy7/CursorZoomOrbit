#include "CursorZoomOrbitCamera.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"
#include "SceneView.h"
#include "UnrealClient.h"

#include <imgui.h>

#include <imoguizmo.hpp>

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

    ImGuiIO& io = ImGui::GetIO();
    ImGui::Begin("Info");
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
    ImGui::Text("Mouse Position: [%.0f,%.0f]", io.MousePos.x, io.MousePos.y);

    ImGui::Text("We're inside: %ls", *GetName());
    FVector CameraPos = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0)->GetCameraLocation();
    ImGui::Text("Camera Position: %.2f %.2f %.2f", CameraPos.X, CameraPos.Y, CameraPos.Z);

    ImGui::End();

    this->ActorDebugger();

    CoordinateSystemViewGizmo(DeltaTime);
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

void ACursorZoomOrbitCamera::ActorDebugger()
{
	static bool bIsPickingActor = false;
	static TWeakObjectPtr<AActor> PickedActor = nullptr;

	ImGui::Begin("Actor Debugger");

	if(ImGui::Button(bIsPickingActor ? "Stop Picking" : "Start Picking")) {
		bIsPickingActor = !bIsPickingActor;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(this, EGetWorldErrorMode::LogAndReturnNull);
	ULocalPlayer* LP = World ? World->GetFirstLocalPlayerFromController() : nullptr;
	if(bIsPickingActor) {
		if (LP && LP->ViewportClient) {
			// get the projection data
			FSceneViewProjectionData ProjectionData;
			if (LP->GetProjectionData(LP->ViewportClient->Viewport, ProjectionData)) {
				FMatrix const InvViewProjMatrix = ProjectionData.ComputeViewProjectionMatrix().InverseFast();
				ImVec2 ScreenPosImGui = ImGui::GetMousePos();
				FVector2D ScreenPos = {ScreenPosImGui.x, ScreenPosImGui.y};
				FVector WorldPosition, WorldDirection;
				FSceneView::DeprojectScreenToWorld(ScreenPos, ProjectionData.GetConstrainedViewRect(), InvViewProjMatrix, WorldPosition, WorldDirection);

				FCollisionQueryParams Params("DevGuiActorPickerTrace", SCENE_QUERY_STAT_ONLY(UDevGuiSubsystem), true);
				Params.bReturnPhysicalMaterial = false;
				Params.bReturnFaceIndex = false;

				FCollisionObjectQueryParams ObjectParams(
					ECC_TO_BITFIELD(ECC_WorldStatic)
					| ECC_TO_BITFIELD(ECC_WorldDynamic)
					| ECC_TO_BITFIELD(ECC_Pawn)
					| ECC_TO_BITFIELD(ECC_PhysicsBody));

				PickedActor = nullptr;
				FHitResult OutHit;
				if(World->LineTraceSingleByObjectType(
						OutHit,
						WorldPosition + WorldDirection * 100.0,
						WorldPosition + WorldDirection * 10000.0,
						ObjectParams,
						Params))
				{
					PickedActor = OutHit.GetActor();
				}
			}
		}

		if(ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			bIsPickingActor = false;
		}
	}

	if(AActor* Actor = PickedActor.Get()) {
		// ImGui::BeginChild("PickedActorFrame", ImVec2(), true);
		ImGui::Text("Picked Actor: %ls", *Actor->GetName());

		Actor->ForEachComponent<UStaticMeshComponent>(true, [](UStaticMeshComponent* Mesh) {
			auto NameANSI = StringCast<ANSICHAR>(*Mesh->GetName());
			if(ImGui::CollapsingHeader(NameANSI.Get(), ImGuiTreeNodeFlags_DefaultOpen)) {
				if(auto StaticMesh = Mesh->GetStaticMesh()) {
					ImGui::Text("Mesh name: %ls", *StaticMesh->GetName());
					ImGui::Text("Nanite Enabled: %s", StaticMesh->NaniteSettings.bEnabled ? "true" : "false");
				}

				// ImGui::PushID(NameANSI.Get());
				if(Mesh->IsSimulatingPhysics()) {
					FString ButtonLabel = FString::Printf(TEXT("Add Vertical Force##%s"), NameANSI.Get());
					if(ImGui::Button(TCHAR_TO_UTF8(*ButtonLabel))) {
						Mesh->AddForce(FVector{0.0, 0.0, 50000.0}, NAME_None, true);
					}
				}
				// ImGui::PopID();
			}
		});

		// ImGui::EndChild();
	} else {
		PickedActor = nullptr;
	}

	ImGui::End();
}

void ACursorZoomOrbitCamera::CoordinateSystemViewGizmo(float DeltaTime)
{
    FMinimalViewInfo DesiredView;
    Camera->GetCameraView(DeltaTime, DesiredView);

    FMatrix ViewMatrix = FMatrix::Identity;
    FMatrix ProjectionMatrix = FMatrix::Identity;
    FMatrix ViewProjectionMatrix = FMatrix::Identity;
    UGameplayStatics::GetViewProjectionMatrix(DesiredView, ViewMatrix, ProjectionMatrix, ViewProjectionMatrix);

    static float ViewMatrixArray[16];
    static float ProjectionMatrixArray[16];
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            ViewMatrixArray[row * 4 + col] = ViewMatrix.M[row][col];
            ProjectionMatrixArray[row * 4 + col] = ProjectionMatrix.M[row][col];
        }
    }

    FIntPoint Size = GEngine->GameViewport->Viewport->GetSizeXY();
    ImOGuizmo::SetRect(Size.X - 120, Size.Y - 120, 120);
    ImOGuizmo::BeginFrame();

    ImOGuizmo::DrawGizmo(ViewMatrixArray, ProjectionMatrixArray, 1);
}

