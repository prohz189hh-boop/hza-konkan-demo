#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HzaFrontend.generated.h"

class UHzaAuthClient;
class UHzaAuthScreen;
class UHzaLobbyClient;
class UHzaLobbyScreen;
class AHzaHandInteractor;

/** The existing table map's startup overlay; owns no authentication or game rules. */
UCLASS()
class HZANETWORK_API AHzaFrontend : public AActor
{
    GENERATED_BODY()
public:
    AHzaFrontend();
    UFUNCTION(BlueprintCallable, Category="HZA|Frontend") void Open(bool bRestoreSession = true);
    UPROPERTY(BlueprintReadOnly, Transient, Category="HZA|Frontend") TObjectPtr<UHzaAuthScreen> Screen;
    UPROPERTY(BlueprintReadOnly, Transient, Category="HZA|Frontend") TObjectPtr<UHzaLobbyScreen> LobbyScreen;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY() TObjectPtr<AHzaHandInteractor> Hand;
    UPROPERTY() TObjectPtr<UHzaAuthClient> Auth;
    UPROPERTY() TObjectPtr<UHzaLobbyClient> Lobby;
    bool bSyncing = false;
    int8 InputOwner = -1;
    UFUNCTION() void SyncState();
};
