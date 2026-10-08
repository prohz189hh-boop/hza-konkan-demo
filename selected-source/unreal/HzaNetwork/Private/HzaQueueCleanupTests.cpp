#if WITH_DEV_AUTOMATION_TESTS
#include "HzaRoomClient.h"
#include "Interfaces/IHttpRequest.h"
#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHzaQueueCleanupTest,"HZA.Network.QueueCleanup",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHzaQueueCleanupTest::RunTest(const FString&)
{
    auto* Instance=NewObject<UGameInstance>();
    auto* Room=NewObject<UHzaRoomClient>(Instance);
    TArray<TSharedRef<IHttpRequest,ESPMode::ThreadSafe>> Requests;
    Room->TestApiTransport=[&Requests](TSharedRef<IHttpRequest,ESPMode::ThreadSafe> Request){Requests.Add(Request);return true;};
    TestTrue(TEXT("Synthetic authenticated service configured"),Room->ConfigureSession(TEXT("https://fixture.invalid"),TEXT("synthetic-access")));
    Room->RequestApi(TEXT("/api/matchmaking/join"),TEXT("POST"),TEXT("{}"),TEXT("join"));
    const FString OldTicket=Room->QueueTicket;
    TestFalse(TEXT("Join owns a cancellation ticket"),OldTicket.IsEmpty());
    Room->UpdateAccessToken(TEXT("synthetic-refreshed"));
    Room->SignOut();
    if(TestEqual(TEXT("Sign-out dispatches one cleanup"),Requests.Num(),2))
    {
        const auto& Cleanup=Requests[1];
        TestTrue(TEXT("Cleanup calls existing leave endpoint"),Cleanup->GetURL().EndsWith(TEXT("/api/matchmaking/leave")));
        TestEqual(TEXT("Cleanup uses latest auth"),Cleanup->GetHeader(TEXT("Authorization")),FString(TEXT("Bearer synthetic-refreshed")));
        TSharedPtr<FJsonObject> Json;
        const TArray<uint8>& Bytes=Cleanup->GetContent();
        const FUTF8ToTCHAR Text(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()),Bytes.Num());
        TestTrue(TEXT("Cleanup JSON parses"),FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(FString(Text.Length(),Text.Get())),Json));
        if(Json)TestEqual(TEXT("Cleanup scoped to original attempt"),Json->GetStringField(TEXT("queueTicket")),OldTicket);
    }
    TestTrue(TEXT("Local credentials and ticket cleared immediately"),Room->Token.IsEmpty()&&Room->QueueTicket.IsEmpty());
    Room->SignOut(); TestEqual(TEXT("Repeated sign-out does not resend cleanup"),Requests.Num(),2);
    Room->ConfigureSession(TEXT("https://fixture.invalid"),TEXT("synthetic-new-session"));
    Room->RequestApi(TEXT("/api/matchmaking/join"),TEXT("POST"),TEXT("{}"),TEXT("new-join"));
    TestNotEqual(TEXT("New search has independent ticket"),Room->QueueTicket,OldTicket);
    Room->SuspendAuthorization();
    TestEqual(TEXT("Expired session also requests cleanup"),Requests.Num(),4);
    TestTrue(TEXT("Expired session clears credentials"),Room->Token.IsEmpty());
    Room->TestApiTransport=nullptr;
    return true;
}
#endif
