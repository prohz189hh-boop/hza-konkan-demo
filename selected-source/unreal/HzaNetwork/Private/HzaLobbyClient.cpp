#include "HzaLobbyClient.h"
#include "HzaAuthClient.h"
#include "Engine/GameInstance.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    bool DecodeLobby(const FString& Text, TSharedPtr<FJsonObject>& Out)
    { return Text.Len() <= 1024 * 1024 && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Out) && Out.IsValid(); }
    bool RoomCodeValid(const FString& Code)
    {
        if (Code.Len() < 3 || Code.Len() > 20) return false;
        for (TCHAR C : Code) if (!((C >= 'A' && C <= 'Z') || (C >= '0' && C <= '9') || C == '_' || C == '-')) return false;
        return true;
    }
    bool Counter(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, int64& Value)
    {
        double Number;
        if (!Object || !Object->TryGetNumberField(Key, Number) || !FMath::IsFinite(Number) || Number < 0 || Number > 9007199254740991.0 || FMath::FloorToDouble(Number) != Number) return false;
        Value = static_cast<int64>(Number); return true;
    }
}

void UHzaLobbyClient::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection); Collection.InitializeDependency<UHzaAuthClient>();
    Auth = GetGameInstance()->GetSubsystem<UHzaAuthClient>(); Room = GetGameInstance()->GetSubsystem<UHzaRoomClient>();
    Auth->OnChanged.AddDynamic(this, &UHzaLobbyClient::AuthChanged);
    Room->OnHttpResponse.AddDynamic(this, &UHzaLobbyClient::HttpResponse);
    Room->OnSnapshot.AddDynamic(this, &UHzaLobbyClient::SnapshotChanged);
    Room->OnConnection.AddDynamic(this, &UHzaLobbyClient::ConnectionChanged);
    Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float)
    { if (State == EHzaLobbyState::Searching && !bBusy && FPlatformTime::Seconds() >= NextPoll && Authorized()) Poll(); return true; }), 1.f);
}
void UHzaLobbyClient::Deinitialize()
{
    FTSTicker::GetCoreTicker().RemoveTicker(Ticker);
    if (Auth) Auth->OnChanged.RemoveAll(this);
    if (Room) { Room->OnHttpResponse.RemoveAll(this); Room->OnSnapshot.RemoveAll(this); Room->OnConnection.RemoveAll(this); }
    Reset(); Super::Deinitialize();
}
bool UHzaLobbyClient::Authorized() const { return Auth && Auth->State == EHzaAuthState::Ready && Auth->HasValidSession() && Auth->Profile.Id == Identity; }
void UHzaLobbyClient::Reset()
{
    Identity.Empty(); ActiveRequest.Empty(); AccountRequest.Empty(); PendingAction.Empty(); RoomCode.Empty(); ErrorCode.Empty();
    bBusy = false; bCancelRequested = false; bAccountLoaded = false; Coins = XP = 0; Rating = 0; State = EHzaLobbyState::Closed;
    PartyRequest.Empty();bPartyRequested=false;bPartyLoaded=false;PartyMemberCount=0;
}
void UHzaLobbyClient::AuthChanged()
{
    if (Auth->State == EHzaAuthState::SignedOut || (!Identity.IsEmpty() && !Auth->Profile.Id.IsEmpty() && Identity != Auth->Profile.Id))
    { Reset(); OnChanged.Broadcast(); }
}
void UHzaLobbyClient::Activate()
{
    if (!Auth || Auth->State != EHzaAuthState::Ready || !Auth->HasValidSession()) return;
    if (Identity == Auth->Profile.Id && State != EHzaLobbyState::Closed) return;
    Reset(); Identity = Auth->Profile.Id; State = EHzaLobbyState::Lobby; RefreshAccount();
}
bool UHzaLobbyClient::Send(const FString& Path, const FString& Verb, const FString& Body, const FString& Id)
{
#if WITH_DEV_AUTOMATION_TESTS
    if (TestSend) return TestSend(Path, Verb, Body, Id);
#endif
    return Room->RequestApi(Path, Verb, Body, Id);
}
void UHzaLobbyClient::RefreshAccount()
{
    if (!Authorized() || !AccountRequest.IsEmpty()) return;
    AccountRequest = TEXT("account:") + FGuid::NewGuid().ToString();
    if (!Send(TEXT("/api/me"), TEXT("GET"), TEXT(""), AccountRequest)) { AccountRequest.Empty(); ErrorCode = TEXT("CONNECTION_FAILED"); }
    OnChanged.Broadcast();
}
void UHzaLobbyClient::SelectMode(bool bTurbo)
{ if (State == EHzaLobbyState::Lobby && !bBusy) { Mode = bTurbo ? TEXT("turbo") : TEXT("regular"); OnChanged.Broadcast(); } }
void UHzaLobbyClient::RefreshParty()
{
    if(!Authorized()||!PartyRequest.IsEmpty())return;
    bPartyRequested=true;PartyRequest=TEXT("party:")+FGuid::NewGuid().ToString();
    if(!Send(TEXT("/api/party"),TEXT("GET"),TEXT(""),PartyRequest)){PartyRequest.Empty();bPartyLoaded=false;}
}
void UHzaLobbyClient::Request(const FString& Action, const FString& Body)
{
    if (!Authorized() || bBusy) return;
    PendingAction = Action; ActiveRequest = TEXT("lobby:") + FGuid::NewGuid().ToString(); bBusy = true; ErrorCode.Empty();
    const bool bStatus = Action == TEXT("status");
    const FString Path = Action == TEXT("create") ? TEXT("/api/rooms/create") : TEXT("/api/matchmaking/") + Action;
    if (!Send(Path, bStatus ? TEXT("GET") : TEXT("POST"), bStatus ? TEXT("") : Body, ActiveRequest)) HttpResponse(ActiveRequest, 0, TEXT("{}"));
    OnChanged.Broadcast();
}
void UHzaLobbyClient::FindMatch()
{
    if (!Authorized() || bBusy || State != EHzaLobbyState::Lobby) return;
    State = EHzaLobbyState::Searching; bCancelRequested = false;
    Request(TEXT("join"), FString::Printf(TEXT("{\"mode\":\"%s\",\"region\":\"auto\",\"allowBots\":false}"), *Mode));
}
void UHzaLobbyClient::CancelSearch()
{
    if (State != EHzaLobbyState::Searching || !Authorized()) return;
    bCancelRequested = true;
    // Serialize leave after an in-flight join/status; a late join must not requeue us.
    if (!bBusy) Request(TEXT("leave"));
    OnChanged.Broadcast();
}
void UHzaLobbyClient::Poll() { Request(TEXT("status")); }
void UHzaLobbyClient::CreatePrivateRoom()
{
    if (State != EHzaLobbyState::Lobby || bBusy) return;
    Request(TEXT("create"), FString::Printf(TEXT("{\"mode\":\"%s\",\"name\":\"HZA table\",\"public\":false}"), *Mode));
}
void UHzaLobbyClient::JoinPrivateRoom(const FString& Code)
{ if (State == EHzaLobbyState::Lobby && !bBusy && Authorized()) Connect(Code.TrimStartAndEnd().ToUpper()); }
void UHzaLobbyClient::Connect(const FString& Code)
{
    if (!RoomCodeValid(Code)) { ErrorCode = TEXT("INVALID_ROOM"); OnChanged.Broadcast(); return; }
    RoomCode = Code; State = EHzaLobbyState::JoiningRoom; ErrorCode.Empty();
    bool bConnected;
#if WITH_DEV_AUTOMATION_TESTS
    if (TestConnect) bConnected = TestConnect(Code); else
#endif
    bConnected = Room->ConnectRoom(Code, TEXT(""));
    if (!bConnected) { State = EHzaLobbyState::Reconnecting; ErrorCode = TEXT("CONNECTION_FAILED"); }
    OnChanged.Broadcast();
}
void UHzaLobbyClient::Reconnect() { if (State == EHzaLobbyState::Reconnecting && Authorized()) Connect(RoomCode); }
void UHzaLobbyClient::HttpResponse(const FString& Id, int32 Status, const FString& Body)
{
    if (Identity.IsEmpty()) return;
    TSharedPtr<FJsonObject> Json;
    const bool bOk = Status == 200 && DecodeLobby(Body, Json);
    if(!PartyRequest.IsEmpty()&&Id==PartyRequest)
    {
        PartyRequest.Empty();bPartyLoaded=false;PartyMemberCount=0;
        const TArray<TSharedPtr<FJsonValue>>* Members=nullptr;
        if(bOk&&Json->TryGetArrayField(TEXT("members"),Members))
        {
            FString OwnParty;
            for(const auto& Value:*Members){const TSharedPtr<FJsonObject>* Member;FString User;if(Value->TryGetObject(Member)&&(*Member)->TryGetStringField(TEXT("user_id"),User)&&User==Identity)(*Member)->TryGetStringField(TEXT("party_id"),OwnParty);}
            TSet<FString> Users;
            if(!OwnParty.IsEmpty())for(const auto& Value:*Members){const TSharedPtr<FJsonObject>* Member;FString Party,User;if(Value->TryGetObject(Member)&&(*Member)->TryGetStringField(TEXT("party_id"),Party)&&Party==OwnParty&&(*Member)->TryGetStringField(TEXT("user_id"),User)&&!User.IsEmpty())Users.Add(User);}
            PartyMemberCount=Users.Num();bPartyLoaded=true;
        }
        OnChanged.Broadcast();return;
    }
    if (!AccountRequest.IsEmpty() && Id == AccountRequest)
    {
        AccountRequest.Empty();
        const TSharedPtr<FJsonObject> *Profile, *Wallet, *Stats;
        FString ProfileId, WalletId, StatsId; int64 NewXP, NewCoins, NewRating;
        if (bOk && Json->TryGetObjectField(TEXT("profile"), Profile) && Json->TryGetObjectField(TEXT("wallet"), Wallet) && Json->TryGetObjectField(TEXT("stats"), Stats) &&
            (*Profile)->TryGetStringField(TEXT("id"), ProfileId) && ProfileId == Identity && (*Wallet)->TryGetStringField(TEXT("user_id"), WalletId) && WalletId == Identity &&
            (*Stats)->TryGetStringField(TEXT("user_id"), StatsId) && StatsId == Identity && Counter(*Profile, TEXT("xp"), NewXP) && Counter(*Wallet, TEXT("coin_balance"), NewCoins) &&
            Counter(*Stats, TEXT("rating"), NewRating) && NewRating <= MAX_int32)
        { XP = NewXP; Coins = NewCoins; Rating = int32(NewRating); bAccountLoaded = true; ErrorCode.Empty(); }
        else { bAccountLoaded = false; ErrorCode = TEXT("ACCOUNT_UNAVAILABLE"); }
        OnChanged.Broadcast(); return;
    }
    if (ActiveRequest.IsEmpty() || Id != ActiveRequest) return;
    const FString Action = PendingAction; ActiveRequest.Empty(); PendingAction.Empty(); bBusy = false;
    if (!bOk)
    {
        ErrorCode = Status == 401 ? TEXT("SESSION_EXPIRED") : TEXT("CONNECTION_FAILED");
        if (State == EHzaLobbyState::Searching) NextPoll = FPlatformTime::Seconds() + 5;
        if (bCancelRequested && Action != TEXT("leave") && Authorized()) { Request(TEXT("leave")); return; }
        OnChanged.Broadcast(); return;
    }
    FString StatusText, Code;
    Json->TryGetStringField(TEXT("status"), StatusText);
    if (Action == TEXT("create"))
    {
        // Worker wraps the Durable Object initialization result as {room:{ok,room:{code}}}.
        const TSharedPtr<FJsonObject>* Wrapper; const TSharedPtr<FJsonObject>* Summary;
        if (Json->TryGetObjectField(TEXT("room"), Wrapper) && (*Wrapper)->TryGetObjectField(TEXT("room"), Summary) && (*Summary)->TryGetStringField(TEXT("code"), Code)) Connect(Code);
        else ErrorCode = TEXT("INVALID_RESPONSE");
    }
    else if (Action == TEXT("leave") && StatusText == TEXT("left"))
    {
        // Leaving the queue cannot revoke an assignment already published by the server.
        // Confirm status before reporting cancellation; a winning match still owns the seat.
        bCancelRequested = false; Poll(); return;
    }
    else if (bCancelRequested) { Request(TEXT("leave")); return; }
    else if (StatusText == TEXT("matched") && Json->TryGetStringField(TEXT("roomCode"), Code)) Connect(Code);
    else if (StatusText == TEXT("queued")) { State = EHzaLobbyState::Searching; NextPoll = FPlatformTime::Seconds() + 2; ErrorCode.Empty(); }
    else if (StatusText == TEXT("idle")) { State = EHzaLobbyState::Lobby; ErrorCode = TEXT("SEARCH_ENDED"); }
    else { ErrorCode = TEXT("INVALID_RESPONSE"); NextPoll = FPlatformTime::Seconds() + 5; }
    OnChanged.Broadcast();
}
void UHzaLobbyClient::SnapshotChanged(const FHzaRoomSnapshot& Snapshot)
{
    if (Identity.IsEmpty() || RoomCode.IsEmpty() || Snapshot.Code != RoomCode) return;
    State = Snapshot.Status == TEXT("waiting") ? EHzaLobbyState::WaitingRoom : EHzaLobbyState::InGame;
    ErrorCode.Empty(); OnChanged.Broadcast();
}
void UHzaLobbyClient::ConnectionChanged(const FString& Connection, const FString& Code)
{
    if (RoomCode.IsEmpty() || State == EHzaLobbyState::Closed) return;
    if (Connection == TEXT("ReconnectRequired") || Connection == TEXT("Replaced") || Connection == TEXT("ProtocolError"))
    { State = EHzaLobbyState::Reconnecting; ErrorCode = Connection == TEXT("Replaced") ? TEXT("SESSION_REPLACED") : TEXT("CONNECTION_FAILED"); }
    else if (Connection == TEXT("ActionRejected")) ErrorCode = TEXT("ROOM_ACTION_REJECTED");
    OnChanged.Broadcast();
}
const FHzaRoomSnapshot& UHzaLobbyClient::GetRoom() const { return Room->Snapshot; }
bool UHzaLobbyClient::IsHost() const
{
    TSharedPtr<FJsonObject> Json; FString Host;
    return DecodeLobby(Room->Snapshot.RoomJson, Json) && Json->TryGetStringField(TEXT("host"), Host) && Host == Identity;
}
bool UHzaLobbyClient::CanStart() const
{
    if (State != EHzaLobbyState::WaitingRoom || !IsHost() || Room->Snapshot.Seats.Num() != 4) return false;
    for (const auto& Seat : Room->Snapshot.Seats) if (Seat.UserId.IsEmpty() || !Seat.bReady) return false;
    return true;
}
void UHzaLobbyClient::ToggleReady()
{
    if (State != EHzaLobbyState::WaitingRoom || !Authorized()) return;
    const auto& Snapshot = Room->Snapshot;
    if (Snapshot.Seats.IsValidIndex(Snapshot.OwnSeat)) Room->SetReady(!Snapshot.Seats[Snapshot.OwnSeat].bReady);
}
void UHzaLobbyClient::StartMatch() { if (Authorized() && CanStart()) Room->StartMatch(); }
