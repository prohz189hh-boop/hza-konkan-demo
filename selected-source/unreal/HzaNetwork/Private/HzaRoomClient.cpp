#include "HzaRoomClient.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "IWebSocket.h"
#include "WebSocketsModule.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    FString Encode(const TSharedRef<FJsonObject>& Object)
    {
        FString Text;
        FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Text));
        return Text;
    }
    bool Decode(const FString& Text, TSharedPtr<FJsonObject>& Object)
    {
        return Text.Len() <= 1024 * 1024 && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Object) && Object.IsValid();
    }
    bool IsCode(const FString& Code)
    {
        if (Code.Len() < 3 || Code.Len() > 20) return false;
        for (TCHAR C : Code) if (!FChar::IsAlnum(C) || C > 127) { if (C != '_' && C != '-') return false; }
        return true;
    }
    bool ReadInt(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, int32& Out, int32 Min, int32 Max)
    {
        double Number;
        if (!Object->TryGetNumberField(Key, Number) || !FMath::IsFinite(Number) || Number < Min || Number > Max || FMath::FloorToDouble(Number) != Number) return false;
        Out = static_cast<int32>(Number); return true;
    }
    TSharedRef<FJsonObject> MessageOf(const TCHAR* Type)
    {
        auto Message = MakeShared<FJsonObject>(); Message->SetStringField(TEXT("type"), Type); return Message;
    }
}

bool UHzaRoomClient::ConfigureSession(const FString& WorkerOrigin, const FString& AccessToken)
{
    FString Clean = WorkerOrigin;
    Clean.RemoveFromEnd(TEXT("/"));
    const bool Secure = Clean.StartsWith(TEXT("https://"));
    const bool Local = Clean.StartsWith(TEXT("http://127.0.0.1:")) || Clean.StartsWith(TEXT("http://localhost:"));
    const FString Host = Clean.Mid(Secure ? 8 : 7);
    if ((!Secure && !Local) || Host.IsEmpty() || Host.Contains(TEXT("/")) || Host.Contains(TEXT("?")) || Host.Contains(TEXT("#")) || Host.Contains(TEXT("@")) || AccessToken.IsEmpty()) return false;
    for (TCHAR C : Host) if (FChar::IsWhitespace(C) || C == '\\') return false;
    for (TCHAR C : AccessToken) if (FChar::IsWhitespace(C) || C == ',') return false;
    SignOut(); Origin = Clean; Token = AccessToken; return true;
}

void UHzaRoomClient::Disconnect()
{
    ++Generation; bSynchronized = false;
    if (Socket)
    {
        Socket->OnConnected().Clear(); Socket->OnClosed().Clear(); Socket->OnConnectionError().Clear(); Socket->OnMessage().Clear();
        Socket->Close(1000, TEXT("client_close")); Socket.Reset();
    }
    OnConnection.Broadcast(TEXT("Offline"), TEXT(""));
}
void UHzaRoomClient::SignOut()
{
    CancelQueueBeforeClearingSession();
    Disconnect(); ++SessionGeneration; Token.Empty(); Origin.Empty(); CurrentRoom.Empty(); CurrentProof.Empty(); Snapshot = FHzaRoomSnapshot();
}
void UHzaRoomClient::CancelQueueBeforeClearingSession()
{
    if (!QueueTicket.IsEmpty())
    {
        // Best effort online cleanup; local sign-out never waits for the network.
        // The server ticket prevents this leave from cancelling a newer session's search.
        RequestApi(TEXT("/api/matchmaking/leave"), TEXT("POST"), TEXT("{}"), TEXT("queue-cleanup"));
        QueueTicket.Empty();
    }
}
void UHzaRoomClient::Deinitialize() { SignOut(); Super::Deinitialize(); }

bool UHzaRoomClient::ConnectRoom(const FString& RoomCode, const FString& PasswordProof)
{
    if (Origin.IsEmpty() || Token.IsEmpty() || !IsCode(RoomCode)) return false;
    if (!PasswordProof.IsEmpty())
    {
        if (PasswordProof.Len() != 64) return false;
        for (TCHAR C : PasswordProof) if (!FChar::IsHexDigit(C)) return false;
    }
    Disconnect(); CurrentRoom = RoomCode.ToUpper(); CurrentProof = PasswordProof.ToLower(); Snapshot = FHzaRoomSnapshot();
    FString WsOrigin = Origin; WsOrigin.ReplaceInline(TEXT("https://"), TEXT("wss://")); WsOrigin.ReplaceInline(TEXT("http://"), TEXT("ws://"));
    TArray<FString> Protocols = { TEXT("hza-konkan"), TEXT("auth.") + Token };
    if (!CurrentProof.IsEmpty()) Protocols.Add(TEXT("roompw.") + CurrentProof);
    Socket = FWebSocketsModule::Get().CreateWebSocket(WsOrigin + TEXT("/room/") + CurrentRoom, Protocols);
    const uint64 Id = Generation;
    Socket->OnConnected().AddWeakLambda(this, [this, Id]() { if (Id == Generation) OnConnection.Broadcast(TEXT("Connected"), TEXT("")); });
    Socket->OnConnectionError().AddWeakLambda(this, [this, Id](const FString&) { if (Id == Generation) { bSynchronized = false; OnConnection.Broadcast(TEXT("ReconnectRequired"), TEXT("CONNECTION_FAILED")); } });
    Socket->OnClosed().AddWeakLambda(this, [this, Id](int32 Code, const FString&, bool)
    {
        if (Id != Generation) return;
        bSynchronized = false;
        OnConnection.Broadcast(Code == 4001 ? TEXT("Replaced") : TEXT("ReconnectRequired"), FString::FromInt(Code));
    });
    Socket->OnMessage().AddWeakLambda(this, [this, Id](const FString& Text) { Receive(Text, Id); });
    Socket->Connect(); return true;
}

bool UHzaRoomClient::ReconnectWithToken(const FString& FreshAccessToken)
{
    if (FreshAccessToken.IsEmpty() || CurrentRoom.IsEmpty()) return false;
    for (TCHAR C : FreshAccessToken) if (FChar::IsWhitespace(C) || C == ',') return false;
    Token = FreshAccessToken;
    const FString Code = CurrentRoom, Proof = CurrentProof;
    return ConnectRoom(Code, Proof);
}

bool UHzaRoomClient::UpdateAccessToken(const FString& FreshAccessToken)
{
    if (Origin.IsEmpty() || FreshAccessToken.IsEmpty()) return false;
    for (TCHAR C : FreshAccessToken) if (FChar::IsWhitespace(C) || C == ',') return false;
    Token = FreshAccessToken;
    return true;
}

void UHzaRoomClient::SuspendAuthorization()
{
    CancelQueueBeforeClearingSession();
    Disconnect(); ++SessionGeneration; Token.Empty();
}

bool UHzaRoomClient::RequestApi(const FString& Path, const FString& Verb, const FString& JsonBody, const FString& RequestId)
{
    if (Token.IsEmpty() || Origin.IsEmpty() || !Path.StartsWith(TEXT("/api/")) || Path.Contains(TEXT("..")) || Path.Contains(TEXT("\r")) || Path.Contains(TEXT("\n")) || (Verb != TEXT("GET") && Verb != TEXT("POST") && Verb != TEXT("PATCH")) || JsonBody.Len() > 16384) return false;
    TSharedPtr<FJsonObject> ParsedBody;
    if (!JsonBody.IsEmpty() && !Decode(JsonBody, ParsedBody)) return false;
    FString BodyToSend = JsonBody;
    if (Verb == TEXT("POST") && (Path == TEXT("/api/matchmaking/join") || Path == TEXT("/api/matchmaking/leave")))
    {
        if (!ParsedBody) ParsedBody = MakeShared<FJsonObject>();
        if (Path.EndsWith(TEXT("/join"))) QueueTicket = FGuid::NewGuid().ToString();
        if (!QueueTicket.IsEmpty()) ParsedBody->SetStringField(TEXT("queueTicket"), QueueTicket);
        BodyToSend = Encode(ParsedBody.ToSharedRef());
    }
    auto Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(Origin + Path); Request->SetVerb(Verb); Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + Token);
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json")); Request->SetContentAsString(BodyToSend); Request->SetTimeout(15.0f);
    const uint64 Session = SessionGeneration;
    Request->OnProcessRequestComplete().BindWeakLambda(this, [this, Session, RequestId](FHttpRequestPtr, FHttpResponsePtr Response, bool bOk)
    {
        if (Session != SessionGeneration) return;
        const int32 Status = bOk && Response ? Response->GetResponseCode() : 0;
        FString Body = bOk && Response ? Response->GetContentAsString() : TEXT("{\"error\":\"CONNECTION_FAILED\"}");
        TSharedPtr<FJsonObject> Parsed;
        if (!Decode(Body, Parsed)) Body = TEXT("{\"error\":\"INVALID_RESPONSE\"}");
        OnHttpResponse.Broadcast(RequestId, Status, Body);
    });
#if WITH_DEV_AUTOMATION_TESTS
    if (TestApiTransport) return TestApiTransport(Request);
#endif
    return Request->ProcessRequest();
}

bool UHzaRoomClient::Send(const TSharedRef<FJsonObject>& Message)
{
    if (!Socket || !Socket->IsConnected()) return false;
    Socket->Send(Encode(Message)); return true;
}
bool UHzaRoomClient::SetReady(bool bReady) { auto M = MessageOf(TEXT("ready")); M->SetBoolField(TEXT("ready"), bReady); return Send(M); }
bool UHzaRoomClient::StartMatch() { return Send(MessageOf(TEXT("start"))); }
bool UHzaRoomClient::RequestState() { return Send(MessageOf(TEXT("get_state"))); }
bool UHzaRoomClient::LeaveRoom() { return Send(MessageOf(TEXT("leave"))); }
bool UHzaRoomClient::SendMove(const FString& ActionJson)
{
    if (!bSynchronized || Snapshot.Version < 0 || ActionJson.Len() > 12000) return false;
    TSharedPtr<FJsonObject> Action; FString Type;
    if (!Decode(ActionJson, Action) || !Action->TryGetStringField(TEXT("type"), Type)) return false;
    static const TSet<FString> Actions = {TEXT("draw"), TEXT("discard"), TEXT("meld"), TEXT("extend"), TEXT("steal"), TEXT("finish"), TEXT("next")};
    if (!Actions.Contains(Type)) return false;
    auto M = MessageOf(TEXT("move")); M->SetNumberField(TEXT("version"), Snapshot.Version); M->SetObjectField(TEXT("action"), Action);
    // One outstanding move; state/error resynchronizes. Never replay uncertain actions.
    if (!Send(M)) return false;
    bSynchronized = false; return true;
}

void UHzaRoomClient::Receive(const FString& Text, uint64 Id)
{
    if (Id != Generation) return;
    TSharedPtr<FJsonObject> Message; FString Type;
    if (!Decode(Text, Message) || !Message->TryGetStringField(TEXT("type"), Type)) { Disconnect(); OnConnection.Broadcast(TEXT("ProtocolError"), TEXT("INVALID_MESSAGE")); return; }
    if (Type == TEXT("error")) { FString Code; Message->TryGetStringField(TEXT("code"), Code); OnConnection.Broadcast(TEXT("ActionRejected"), Code); RequestState(); return; }
    if (Type != TEXT("welcome") && Type != TEXT("state")) return;
    int32 Seat = Snapshot.OwnSeat;
    if (Type == TEXT("welcome") && !ReadInt(Message, TEXT("seat"), Seat, 0, 3)) { Disconnect(); OnConnection.Broadcast(TEXT("ProtocolError"), TEXT("INVALID_SEAT")); return; }
    const TSharedPtr<FJsonObject>* Room;
    FHzaRoomSnapshot Next;
    if (!Message->TryGetObjectField(TEXT("room"), Room) || !ParseSnapshot(*Room, Seat, Next) || Next.Code != CurrentRoom)
    { Disconnect(); OnConnection.Broadcast(TEXT("ProtocolError"), TEXT("INVALID_SNAPSHOT")); return; }
    if (Next.RoomId == Snapshot.RoomId && Next.Version < Snapshot.Version) return;
    Snapshot = MoveTemp(Next); bSynchronized = true; OnSnapshot.Broadcast(Snapshot);
}

bool UHzaRoomClient::ParseSnapshot(const TSharedPtr<FJsonObject>& Room, int32 OwnSeat, FHzaRoomSnapshot& Out)
{
    if (!Room || OwnSeat < 0 || OwnSeat > 3 || Room->HasField(TEXT("passwordHash"))) return false;
    FHzaRoomSnapshot Next; Next.OwnSeat = OwnSeat;
    if (!Room->TryGetStringField(TEXT("id"), Next.RoomId) || !Room->TryGetStringField(TEXT("code"), Next.Code) || !ReadInt(Room, TEXT("version"), Next.Version, 0, MAX_int32)) return false;
    Room->TryGetStringField(TEXT("status"), Next.Status); Room->TryGetNumberField(TEXT("deadline"), Next.DeadlineUnixMs);
    if (!FMath::IsFinite(Next.DeadlineUnixMs) || Next.DeadlineUnixMs<0) return false;
    if (Room->HasField(TEXT("serverNow")) &&
        (!Room->TryGetNumberField(TEXT("serverNow"),Next.ServerNowUnixMs) ||
         !FMath::IsFinite(Next.ServerNowUnixMs) || Next.ServerNowUnixMs<=0)) return false;
    Next.ReceivedAtSeconds=FPlatformTime::Seconds();
    const TArray<TSharedPtr<FJsonValue>>* Seats;
    if (!Room->TryGetArrayField(TEXT("seats"), Seats) || Seats->Num() != 4) return false;
    for (int32 Index = 0; Index < 4; ++Index)
    {
        FHzaSeat Seat; Seat.Seat = Index; Seat.Team = Index % 2 + 1;
        if ((*Seats)[Index]->Type != EJson::Null)
        {
            const TSharedPtr<FJsonObject>* Member;
            if (!(*Seats)[Index]->TryGetObject(Member)) return false;
            (*Member)->TryGetStringField(TEXT("id"), Seat.UserId); (*Member)->TryGetStringField(TEXT("name"), Seat.Name);
            (*Member)->TryGetBoolField(TEXT("ready"), Seat.bReady); (*Member)->TryGetBoolField(TEXT("bot"), Seat.bBot);
        }
        Next.Seats.Add(Seat);
    }
    const TSharedPtr<FJsonObject>* Game;
    const auto GameValue = Room->Values.Find(TEXT("game"));
    if (!GameValue || !GameValue->IsValid() || ((*GameValue)->Type != EJson::Null && (*GameValue)->Type != EJson::Object)) return false;
    if (Room->TryGetObjectField(TEXT("game"), Game))
    {
        if ((*Game)->HasField(TEXT("deck")) || (*Game)->HasField(TEXT("seed")) || (*Game)->HasField(TEXT("entropy"))) return false;
        int32 ViewSeat;
        if (!ReadInt(*Game,TEXT("seat"),ViewSeat,0,3) || ViewSeat != OwnSeat) return false;
        if (!ReadInt(*Game,TEXT("turn"),Next.Turn,0,3) || !ReadInt(*Game,TEXT("deckCount"),Next.DeckCount,0,106)) return false;
        (*Game)->TryGetStringField(TEXT("phase"),Next.Phase); ReadInt(*Game,TEXT("round"),Next.Round,1,100); ReadInt(*Game,TEXT("rounds"),Next.RoundCount,1,100);
        const TArray<TSharedPtr<FJsonValue>>* Players;
        if (!(*Game)->TryGetArrayField(TEXT("players"),Players) || Players->Num()!=4) return false;
        TSet<FString> Ids;
        for (int32 Index=0;Index<4;++Index)
        {
            const TSharedPtr<FJsonObject>* Player; const TArray<TSharedPtr<FJsonValue>>* Hand;
            if (!(*Players)[Index]->TryGetObject(Player) || !(*Player)->TryGetArrayField(TEXT("hand"),Hand)) return false;
            if (!ReadInt(*Player,TEXT("tileCount"),Next.Seats[Index].TileCount,0,106) || (Index!=OwnSeat && !Hand->IsEmpty())) return false;
            if (Index!=OwnSeat)
            {
                const TArray<TSharedPtr<FJsonValue>>* Stolen;
                if ((*Player)->TryGetArrayField(TEXT("stolen"),Stolen) && !Stolen->IsEmpty()) return false;
                continue;
            }
            if (Hand->Num()!=Next.Seats[Index].TileCount) return false;
            for (const auto& Value:*Hand)
            {
                const TSharedPtr<FJsonObject>* Tile; FHzaTile T;
                if (!Value->TryGetObject(Tile) || !(*Tile)->TryGetStringField(TEXT("id"),T.Id) || T.Id.IsEmpty() || Ids.Contains(T.Id)) return false;
                if (!(*Tile)->TryGetStringField(TEXT("color"),T.Suit) || !ReadInt(*Tile,TEXT("n"),T.Number,0,13)) return false;
                (*Tile)->TryGetBoolField(TEXT("antique"),T.bAntique); Ids.Add(T.Id); Next.OwnHand.Add(T);
            }
        }
    }
    Next.RoomJson=Encode(Room.ToSharedRef()); Out=MoveTemp(Next); return true;
}
