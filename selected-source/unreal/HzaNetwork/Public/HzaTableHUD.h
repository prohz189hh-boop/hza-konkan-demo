#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HzaTableHUD.generated.h"
class AHzaTablePresentation;
class UTextBlock;
class UButton;
class UBorder;

UCLASS()
class HZANETWORK_API UHzaTableHUD : public UUserWidget
{
    GENERATED_BODY()
public:
    UPROPERTY() TObjectPtr<AHzaTablePresentation> Table;
    void Refresh();
protected:
    virtual void NativeOnInitialized() override;
private:
    UPROPERTY() TArray<TObjectPtr<UTextBlock>> SeatNames;
    UPROPERTY() TArray<TObjectPtr<UTextBlock>> SeatDetails;
    UPROPERTY() TArray<TObjectPtr<UBorder>> SeatBorders;
    UPROPERTY() TObjectPtr<UTextBlock> Status;
    UPROPERTY() TObjectPtr<UTextBlock> Phase;
    UPROPERTY() TObjectPtr<UTextBlock> Notice;
    UPROPERTY() TObjectPtr<UTextBlock> Score;
    UPROPERTY() TObjectPtr<UTextBlock> History;
    UPROPERTY() TObjectPtr<UButton> DrawButton;
    UPROPERTY() TObjectPtr<UButton> TakeButton;
    UPROPERTY() TObjectPtr<UButton> DiscardButton;
    UPROPERTY() TObjectPtr<UButton> NextButton;
    UFUNCTION() void Draw();
    UFUNCTION() void Take();
    UFUNCTION() void Discard();
    UFUNCTION() void Next();
};
