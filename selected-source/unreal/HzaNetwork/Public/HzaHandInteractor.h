#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "HzaHandInteractor.generated.h"

class APlayerController;
class UGameViewportClient;
struct FInputKeyEventArgs;

/** Local presentation only. Never submits a gameplay move or changes ownership. */
UCLASS()
class HZANETWORK_API AHzaHandInteractor : public AActor
{
    GENERATED_BODY()
public:
    AHzaHandInteractor();
    UPROPERTY(EditInstanceOnly, Category="HZA|Hand") TArray<TObjectPtr<AActor>> HandTiles;
    UPROPERTY(EditAnywhere, Category="HZA|Hand") FVector RackOrigin = FVector(0,-51,83);
    UPROPERTY(EditAnywhere, Category="HZA|Hand") FVector ColumnDirection = FVector(-1,0,0);
    UPROPERTY(EditAnywhere, Category="HZA|Hand") FVector UpperRowOffset = FVector(0,4,3.8);
    UPROPERTY(EditAnywhere, Category="HZA|Hand") float SlotSpacing = 3.3f;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Hand") TObjectPtr<AActor> SelectedTile;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Hand") bool bDragging = false;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Hand") int32 DropSlot = INDEX_NONE;
    UFUNCTION(BlueprintCallable, Category="HZA|Hand") void CancelGesture();
    // Authoritative ownership updates preserve surviving tiles' private rack order.
    void SynchronizeTiles(const TArray<AActor*>& Tiles, bool bResetOrder = false);
    FVector GetSlotPosition(AActor* Tile) const;
    void SetPresentationLocked(bool bLocked);
    void SetFrontendBlocked(bool bBlocked);
    UPROPERTY(BlueprintReadOnly, Transient, Category="HZA|Hand") FVector2D Pointer = FVector2D::ZeroVector;
    UPROPERTY(BlueprintReadOnly, Transient, Category="HZA|Hand") TObjectPtr<AActor> HoveredTile;
    virtual void Tick(float DeltaSeconds) override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    friend class FHzaRackPlacementTest;
    friend class FHzaPIETouchInputTest;
    friend class FHzaTablePIETest;
    UPROPERTY() TObjectPtr<APlayerController> Controller;
    TWeakObjectPtr<UGameViewportClient> InputViewport;
    FDelegateHandle ViewportInputHandle;
    UPROPERTY() TArray<TObjectPtr<AActor>> Slots;
    UPROPERTY() TArray<TObjectPtr<AActor>> Preview;
    UPROPERTY() TObjectPtr<AActor> PressedTile;
    FVector2D PressPosition = FVector2D::ZeroVector;
    bool bPointerDown = false;
    bool bTouchGesture = false;
    bool bOldClicks = false;
    bool bOldTouches = false;
    bool bPresentationLocked = false;
    bool bFrontendBlocked = false;
    FVector SlotPosition(int32 Index) const;
    AActor* PickTile(const FVector2D& Screen, bool bTouch) const;
    int32 PickSlot(const FVector2D& Screen) const;
    bool BuildPreview(int32 Target);
    void Select(AActor* Tile);
    void MousePressed();
    void MouseReleased();
    void ViewportInputKey(const FInputKeyEventArgs& Event);
    void TouchPressed(ETouchIndex::Type Finger, FVector Location);
    void TouchReleased(ETouchIndex::Type Finger, FVector Location);
    void TouchMoved(ETouchIndex::Type Finger, FVector Location);
    void BeginGesture(FVector2D Position, bool bTouch);
    void UpdateGesture(FVector2D Position);
    void EndGesture(FVector2D Position);
};
