// InputTrigger_BlockWhenImGuiActive.h
#pragma once

#include "CoreMinimal.h"
#include "InputTriggers.h"              // Enhanced Input 头
#include "InputTrigger_BlockWhenImGuiActive.generated.h"

UCLASS(meta=(DisplayName="Block When ImGui Wants Input"))
class CURSORZOOMORBIT_API UInputTrigger_BlockWhenImGuiActive : public UInputTrigger
{
    GENERATED_BODY()

public:
    // 标记为 Blocker 类型
    virtual ETriggerType GetTriggerType_Implementation() const override
    {
        return ETriggerType::Blocker;
    }

    // 每帧评估触发状态
    virtual ETriggerState UpdateState_Implementation(
        const UEnhancedPlayerInput* PlayerInput,
        FInputActionValue ModifiedValue,
        float DeltaTime) override;
};
