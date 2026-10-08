#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "HzaRoomClient.h"
#include "HzaLobbyClient.generated.h"

class UHzaAuthClient;
UENUM(BlueprintType)
enum class EHzaLobbyState : uint8 { Closed, Lobby, Searching, JoiningRoom, WaitingRoom, InGame, Reconnecting };
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHzaLobbyChanged);

/** Native navigation around existing authoritative Worker endpoints. */
UCLASS()
class HZANETWORK_API UHzaLobbyClient : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintAssignable, Category="HZA|Lobby") FHzaLobbyChanged OnChanged;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Lobby") EHzaLobbyState State = EHzaLobbyState::Closed;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Lobby") bool bBusy = false;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Lobby") bool bAccountLoaded = false;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Lobby") int64 Coins = 0;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Lobby") int64 XP = 0;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Lobby") int32 Rating = 0;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Lobby") FString ErrorCode;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Lobby") FString RoomCode;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Lobby") FString Mode = TEXT("regular");
    void Activate();
    void RefreshAccount();
    void RefreshParty();
    bool bPartyRequested=false,bPartyLoaded=false;
    int32 PartyMemberCount=0;
    void SelectMode(bool bTurbo);
    void FindMatch();
    void CancelSearch();
    void CreatePrivateRoom();
    void JoinPrivateRoom(const FString& Code);
    void Reconnect();
    void ToggleReady();
    void StartMatch();
    bool IsHost() const;
    bool CanStart() const;
    const FHzaRoomSnapshot& GetRoom() const;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
private:
    friend class FHzaLobbyContractTest;
    friend class FHzaAuthPIETest;
    UPROPERTY() TObjectPtr<UHzaAuthClient> Auth;
    UPROPERTY() TObjectPtr<UHzaRoomClient> Room;
    FString Identity, ActiveRequest, AccountRequest, PartyRequest;
    FString PendingAction;
    bool bCancelRequested = false;
    double NextPoll = 0;
    FTSTicker::FDelegateHandle Ticker;
#if WITH_DEV_AUTOMATION_TESTS
    TFunction<bool(const FString&, const FString&, const FString&, const FString&)> TestSend;
    TFunction<bool(const FString&)> TestConnect;
#endif
    bool Authorized() const;
    bool Send(const FString& Path, const FString& Verb, const FString& Body, const FString& Id);
    void Request(const FString& Action, const FString& Body = TEXT("{}"));
    void Poll();
    void Connect(const FString& Code);
    void Reset();
    UFUNCTION() void AuthChanged();
    UFUNCTION() void HttpResponse(const FString& Id, int32 Status, const FString& Body);
    UFUNCTION() void SnapshotChanged(const FHzaRoomSnapshot& Snapshot);
    UFUNCTION() void ConnectionChanged(const FString& Connection, const FString& Code);
};
