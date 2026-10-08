#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HzaRoomClient.generated.h"

class IWebSocket;
class FJsonObject;

USTRUCT(BlueprintType)
struct HZANETWORK_API FHzaTile
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="HZA|Network") FString Id;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Network") FString Suit;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Network") int32 Number = 0;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Network") bool bAntique = false;
};

USTRUCT(BlueprintType)
struct HZANETWORK_API FHzaSeat
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="HZA|Network") FString UserId;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Network") FString Name;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Network") int32 Seat = -1;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Network") int32 Team = 0;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Network") int32 TileCount = 0;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Network") bool bReady = false;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Network") bool bBot = false;
};

USTRUCT(BlueprintType)
struct HZANETWORK_API FHzaRoomSnapshot
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="HZA|Network") FString RoomId;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Network") FString Code;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Network") int32 OwnSeat = -1;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Network") int32 Version = -1;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Network") int32 Turn = -1;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Network") FString Phase;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Network") FString Status;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Network") int32 Round = 0;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Network") int32 RoundCount = 0;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Network") int32 DeckCount = 0;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Network") double DeadlineUnixMs = 0;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Network") double ServerNowUnixMs = 0;
    // Local monotonic receipt timestamp, never transmitted or saved.
    double ReceivedAtSeconds = 0;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Network") TArray<FHzaSeat> Seats;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Network") TArray<FHzaTile> OwnHand;
    // Validated seat view, for incremental meld/discard/UI integration.
    UPROPERTY(BlueprintReadOnly, Category="HZA|Network") FString RoomJson;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHzaSnapshotEvent, const FHzaRoomSnapshot&, Snapshot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FHzaConnectionEvent, const FString&, State, const FString&, Code);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FHzaHttpEvent, const FString&, RequestId, int32, Status, const FString&, Json);

/** One service per local client. Renders server truth; never calculates rewards or legality. */
UCLASS()
class HZANETWORK_API UHzaRoomClient : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintAssignable, Category="HZA|Network") FHzaSnapshotEvent OnSnapshot;
    UPROPERTY(BlueprintAssignable, Category="HZA|Network") FHzaConnectionEvent OnConnection;
    UPROPERTY(BlueprintAssignable, Category="HZA|Network") FHzaHttpEvent OnHttpResponse;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Network") FHzaRoomSnapshot Snapshot;

    // Token is memory-only and is never saved to a UObject property or logged.
    UFUNCTION(BlueprintCallable, Category="HZA|Network") bool ConfigureSession(const FString& WorkerOrigin, const FString& AccessToken);
    // PasswordProof is lowercase SHA-256 of "room:" + password; empty for unlocked rooms.
    UFUNCTION(BlueprintCallable, Category="HZA|Network") bool ConnectRoom(const FString& RoomCode, const FString& PasswordProof);
    UFUNCTION(BlueprintCallable, Category="HZA|Network") bool ReconnectWithToken(const FString& FreshAccessToken);
    // Rotate HTTP/future-connect credentials without disturbing a healthy room.
    bool UpdateAccessToken(const FString& FreshAccessToken);
    void SuspendAuthorization();
    UFUNCTION(BlueprintCallable, Category="HZA|Network") void Disconnect();
    UFUNCTION(BlueprintCallable, Category="HZA|Network") void SignOut();
    UFUNCTION(BlueprintCallable, Category="HZA|Network") bool RequestApi(const FString& Path, const FString& Verb, const FString& JsonBody, const FString& RequestId);
    UFUNCTION(BlueprintCallable, Category="HZA|Network") bool SetReady(bool bReady);
    UFUNCTION(BlueprintCallable, Category="HZA|Network") bool StartMatch();
    UFUNCTION(BlueprintCallable, Category="HZA|Network") bool RequestState();
    UFUNCTION(BlueprintCallable, Category="HZA|Network") bool LeaveRoom();
    UFUNCTION(BlueprintCallable, Category="HZA|Network") bool SendMove(const FString& ActionJson);
    virtual void Deinitialize() override;

    static bool ParseSnapshot(const TSharedPtr<FJsonObject>& Room, int32 OwnSeat, FHzaRoomSnapshot& Out);
private:
    friend class FHzaAuthContractTest;
    friend class FHzaQueueCleanupTest;
    FString Origin;
    FString Token;
    FString CurrentRoom;
    FString CurrentProof;
    FString QueueTicket;
#if WITH_DEV_AUTOMATION_TESTS
    TFunction<bool(TSharedRef<class IHttpRequest, ESPMode::ThreadSafe>)> TestApiTransport;
#endif
    TSharedPtr<IWebSocket> Socket;
    uint64 Generation = 0;
    uint64 SessionGeneration = 0;
    bool bSynchronized = false;
    bool Send(const TSharedRef<FJsonObject>& Message);
    void CancelQueueBeforeClearingSession();
    void Receive(const FString& Message, uint64 ConnectionGeneration);
};
