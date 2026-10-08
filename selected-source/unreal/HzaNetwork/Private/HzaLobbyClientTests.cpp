#if WITH_DEV_AUTOMATION_TESTS
#include "HzaLobbyClient.h"
#include "HzaAuthClient.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHzaLobbyContractTest,"HZA.Lobby.Lifecycle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHzaLobbyContractTest::RunTest(const FString&)
{
    auto* Instance=NewObject<UGameInstance>(GEngine); Instance->Init();
    auto* Auth=Instance->GetSubsystem<UHzaAuthClient>(); auto* Lobby=Instance->GetSubsystem<UHzaLobbyClient>();
    if(!TestNotNull(TEXT("Auth subsystem"),Auth)||!TestNotNull(TEXT("Lobby subsystem"),Lobby)){Instance->Shutdown();return false;}
    Auth->VaultTarget=TEXT("HZA/Automation/")+FGuid::NewGuid().ToString();
    Auth->TestTransport=[](const FString&,const FString&,const FString&,bool,TFunction<void(int32,const FString&)>){};
    struct FRequest { FString Path,Verb,Body,Id; };
    TArray<FRequest> Requests; TArray<FString> Connections;
    Lobby->TestSend=[&Requests](const FString& Path,const FString& Verb,const FString& Body,const FString& Id){Requests.Add({Path,Verb,Body,Id});return true;};
    Lobby->TestConnect=[&Connections](const FString& Code){Connections.Add(Code);return true;};
    const FString Identity=TEXT("00000000-0000-4000-8000-000000000001");
    auto Login=[Auth,Lobby,&Identity,&Requests]()
    {
        Auth->SignOut(); Requests.Empty();
        Auth->AccessToken=TEXT("synthetic-lobby-access"); Auth->ExpiresAt=FPlatformTime::Seconds()+3600;
        Auth->Profile.Id=Identity; Auth->Profile.Username=TEXT("Fixture_Player"); Auth->State=EHzaAuthState::Ready;
        Lobby->Activate();
    };
    auto Respond=[this,Lobby,&Requests](int32 Status,const FString& Body)
    {
        if(Requests.IsEmpty()){AddError(TEXT("Expected lobby request"));return;}
        const auto Request=Requests[0]; Requests.RemoveAt(0); Lobby->HttpResponse(Request.Id,Status,Body);
    };
    Login();
    TestEqual(TEXT("Lobby uses existing account endpoint"),Requests[0].Path,FString(TEXT("/api/me")));
    const FString Account=TEXT(R"({"profile":{"id":"00000000-0000-4000-8000-000000000001","xp":123},"wallet":{"user_id":"00000000-0000-4000-8000-000000000001","coin_balance":42},"stats":{"user_id":"00000000-0000-4000-8000-000000000001","rating":1000}})");
    Respond(200,Account);
    TestTrue(TEXT("Server account bundle accepted"),Lobby->bAccountLoaded);
    TestEqual(TEXT("Coin balance comes from server"),Lobby->Coins,int64(42));
    Lobby->RefreshAccount(); Respond(200,Account.Replace(TEXT("coin_balance\":42"),TEXT("coin_balance\":-1")));
    TestFalse(TEXT("Invalid account balance rejected"),Lobby->bAccountLoaded);
    Lobby->RefreshAccount(); Respond(200,Account.Replace(*Identity,TEXT("00000000-0000-4000-8000-000000000002")));
    TestFalse(TEXT("Wrong identity bundle rejected"),Lobby->bAccountLoaded);
    Lobby->SelectMode(true); Lobby->FindMatch(); Lobby->FindMatch();
    TestEqual(TEXT("Join is single flight"),Requests.Num(),1);
    TestTrue(TEXT("Turbo sent without client UUID"),Requests[0].Body.Contains(TEXT("turbo"))&&!Requests[0].Body.Contains(Identity));
    Lobby->CancelSearch();
    TestEqual(TEXT("Cancel waits for in-flight join"),Requests.Num(),1);
    Respond(200,TEXT(R"({"status":"queued"})"));
    TestEqual(TEXT("Cancel uses existing leave endpoint"),Requests[0].Path,FString(TEXT("/api/matchmaking/leave")));
    Respond(200,TEXT(R"({"status":"left"})"));
    TestEqual(TEXT("Leave is verified with server assignment status"),Requests[0].Path,FString(TEXT("/api/matchmaking/status")));
    Respond(200,TEXT(R"({"status":"idle"})"));
    TestTrue(TEXT("Confirmed cancel returns to lobby"),Lobby->State==EHzaLobbyState::Lobby);
    Lobby->FindMatch(); Lobby->CancelSearch(); Respond(200,TEXT(R"({"status":"matched","roomCode":"ABC12345"})"));
    Respond(200,TEXT(R"({"status":"left"})")); Respond(200,TEXT(R"({"status":"matched","roomCode":"ABC12345"})"));
    TestEqual(TEXT("Assignment that beat cancellation is retained"),Connections.Last(),FString(TEXT("ABC12345")));
    TestTrue(TEXT("Socket initiation does not prematurely show game"),Lobby->State==EHzaLobbyState::JoiningRoom);
    FHzaRoomSnapshot Snapshot; Snapshot.Code=TEXT("ABC12345"); Snapshot.Status=TEXT("waiting");
    Lobby->SnapshotChanged(Snapshot); TestTrue(TEXT("Server waiting snapshot owns room navigation"),Lobby->State==EHzaLobbyState::WaitingRoom);
    Snapshot.Status=TEXT("playing"); Lobby->SnapshotChanged(Snapshot);
    TestTrue(TEXT("Server playing snapshot owns table navigation"),Lobby->State==EHzaLobbyState::InGame);
    Login(); Respond(200,Account); Lobby->FindMatch();
    const FString OldRequest=Requests[0].Id; Auth->SignOut(); Requests.Empty();
    const int32 ConnectionsBefore=Connections.Num();
    Lobby->HttpResponse(OldRequest,200,TEXT(R"({"status":"matched","roomCode":"OLDROOM"})"));
    TestTrue(TEXT("Late match cannot restore signed-out lobby"),Lobby->State==EHzaLobbyState::Closed);
    TestEqual(TEXT("Late match cannot open socket"),Connections.Num(),ConnectionsBefore);
    Login(); Respond(200,Account); Lobby->FindMatch();
    Lobby->HttpResponse(OldRequest,200,TEXT(R"({"status":"matched","roomCode":"OLDROOM"})"));
    TestTrue(TEXT("Old response cannot clear newer busy request"),Lobby->bBusy);
    Respond(503,TEXT("{}")); TestFalse(TEXT("Failed queue request releases busy state"),Lobby->bBusy);
    TestTrue(TEXT("Uncertain queue request stays recoverable by polling"),Lobby->State==EHzaLobbyState::Searching);
    Lobby->CancelSearch(); Respond(200,TEXT(R"({"status":"left"})")); Respond(200,TEXT(R"({"status":"idle"})"));
    Lobby->CreatePrivateRoom(); Respond(200,TEXT(R"({"room":{"ok":true,"room":{"code":"ROOM1234"}}})"));
    TestEqual(TEXT("Existing nested room response is accepted"),Connections.Last(),FString(TEXT("ROOM1234")));
    Login(); Respond(200,Account); Lobby->RefreshParty(); Lobby->RefreshParty();
    TestEqual(TEXT("Party read is single flight"),Requests.Num(),1);
    TestEqual(TEXT("Party uses existing authenticated endpoint"),Requests[0].Path,FString(TEXT("/api/party")));
    Respond(200,TEXT(R"({"members":[{"user_id":"00000000-0000-4000-8000-000000000001","party_id":"ours"},{"user_id":"teammate","party_id":"ours"},{"user_id":"teammate","party_id":"ours"},{"user_id":"unrelated","party_id":"other"}]})"));
    TestTrue(TEXT("Party response loaded"),Lobby->bPartyLoaded);
    TestEqual(TEXT("Party counts distinct members of own party only"),Lobby->PartyMemberCount,2);
    Lobby->RefreshParty(); const FString OldPartyRequest=Requests[0].Id;
    Login(); Respond(200,Account);
    Lobby->HttpResponse(OldPartyRequest,200,TEXT(R"({"members":[]})"));
    TestFalse(TEXT("Old session party response ignored"),Lobby->bPartyLoaded);
    Lobby->RefreshParty(); Respond(503,TEXT("{}"));
    TestFalse(TEXT("Party failure is not an empty party"),Lobby->bPartyLoaded);
    Lobby->RefreshParty(); Respond(200,TEXT(R"({"members":[]})"));
    TestTrue(TEXT("Empty party is authoritative"),Lobby->bPartyLoaded);
    TestEqual(TEXT("Empty party count"),Lobby->PartyMemberCount,0);
    Auth->SignOut(); Auth->TestTransport=nullptr; Lobby->TestSend=nullptr; Lobby->TestConnect=nullptr;
    Instance->Shutdown();return true;
}
#endif
