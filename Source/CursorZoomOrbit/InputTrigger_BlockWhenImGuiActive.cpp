// InputTrigger_BlockWhenImGuiActive.cpp
#include "InputTrigger_BlockWhenImGuiActive.h"

#include "ImGuiModule.h"

ETriggerState UInputTrigger_BlockWhenImGuiActive::UpdateState_Implementation(
    const UEnhancedPlayerInput* PlayerInput,
    FInputActionValue ModifiedValue,
    float DeltaTime)
{
    const bool bBlock = FImGuiModule::Get().GetProperties().IsInputEnabled()
                 && ImGui::GetCurrentContext()
                 && (ImGui::GetIO().WantCaptureKeyboard
                     || ImGui::GetIO().WantTextInput
                     || ImGui::GetIO().WantCaptureMouse);

    return bBlock ? ETriggerState::Ongoing : ETriggerState::None;
}
