#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HzaRoomClient.h"
#include "HzaTablePresentation.generated.h"

class AHzaHandInteractor;
class UHzaTableHUD;
class UStaticMesh;

struct FHzaTileFlight
{
    TWeakObjectPtr<AActor> Actor;
    FTransform Start, End;
    double Started=0;
    float Duration=0.3f;
};

/** Presentation of verified per-seat snapshots; no client game rules or scoring. */
UCLASS()
class HZANETWORK_API AHzaTablePresentation : public AActor
{
    GENERATED_BODY()
public:
    AHzaTablePresentation();
    UPROPERTY(EditInstanceOnly,Category="HZA|Table") TObjectPtr<AHzaHandInteractor> Hand;
    UPROPERTY(EditInstanceOnly,Category="HZA|Table") TArray<TObjectPtr<AActor>> DemoActors;
    UPROPERTY(BlueprintReadOnly,Category="HZA|Table") FHzaRoomSnapshot View;
    UPROPERTY(BlueprintReadOnly,Category="HZA|Table") FString Connection=TEXT("Offline preview");
    UPROPERTY(BlueprintReadOnly,Category="HZA|Table") FString Feedback;
    UPROPERTY(BlueprintReadOnly,Category="HZA|Table") int32 PublicTileCount=0;
    UPROPERTY(BlueprintReadOnly,Category="HZA|Table") int32 ScoreA=0;
    UPROPERTY(BlueprintReadOnly,Category="HZA|Table") int32 ScoreB=0;
    UPROPERTY(BlueprintReadOnly,Category="HZA|Table") FString RoundSummary;
    UPROPERTY(BlueprintReadOnly,Category="HZA|Table") double DealUntilMs=0;
    UPROPERTY(BlueprintReadOnly,Category="HZA|Table") TArray<FString> DiscardHistory;
    UFUNCTION() void ApplySnapshot(const FHzaRoomSnapshot& Snapshot);
    UFUNCTION() void ConnectionChanged(const FString& State,const FString& Code);
    void DrawStock();
    void DrawDiscard();
    void DiscardSelected();
    void NextRound();
    bool CanAct() const;
    double RemainingSeconds() const;
    bool IsDealing() const;
    void SetFrontendVisible(bool bVisible);
    virtual void Tick(float DeltaSeconds) override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY() TObjectPtr<UHzaRoomClient> Client;
    UPROPERTY() TObjectPtr<UHzaTableHUD> HUD;
    UPROPERTY() TMap<FString,TObjectPtr<AActor>> TileActors;
    UPROPERTY() TMap<FString,TObjectPtr<UStaticMesh>> Meshes;
    TArray<FHzaTileFlight> Flights;
    TSet<FString> PreviousOwnIds;
    bool bAwaitingReply=false;
    bool bFrontendVisible=false;
    double FeedbackUntil=0;
    double ServerNowMs() const;
    AActor* TileActor(const FString& Key,const FHzaTile* Face);
    void Place(AActor* Tile,const FTransform& Target,bool bAnimate,float Delay=0);
    FVector SeatPosition(int32 RelativeSeat,int32 Index) const;
    void SendAction(const TSharedRef<FJsonObject>& Action);
};
