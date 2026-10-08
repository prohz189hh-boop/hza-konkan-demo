#if WITH_DEV_AUTOMATION_TESTS
#include "HzaRoomClient.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHzaSnapshotPrivacyTest, "HZA.Network.SnapshotPrivacy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHzaSnapshotPrivacyTest::RunTest(const FString&)
{
    const FString Fixture=TEXT(R"({"id":"test-room","code":"ABCD","version":1,"status":"playing","deadline":1000,"seats":[{"id":"a","name":"A"},{"id":"b","name":"B"},{"id":"c","name":"C"},{"id":"d","name":"D"}],"game":{"seat":0,"turn":0,"deckCount":48,"phase":"play","round":1,"rounds":7,"players":[{"hand":[{"id":"red-1-0","color":"red","n":1}],"tileCount":1},{"hand":[],"tileCount":14},{"hand":[],"tileCount":14},{"hand":[],"tileCount":14}]}})");
    TSharedPtr<FJsonObject> Room;
    TestTrue(TEXT("Parse fixture"),FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Fixture),Room));
    if (!Room) return false;
    FHzaRoomSnapshot Snapshot;
    TestTrue(TEXT("Accept per-seat view"),UHzaRoomClient::ParseSnapshot(Room,0,Snapshot));
    TestEqual(TEXT("Only own hand"),Snapshot.OwnHand.Num(),1);
    TestEqual(TEXT("Opposite team"),Snapshot.Seats[2].Team,Snapshot.Seats[0].Team);
    Room->SetStringField(TEXT("serverNow"),TEXT("invalid"));
    TestFalse(TEXT("Reject invalid server clock"),UHzaRoomClient::ParseSnapshot(Room,0,Snapshot));
    Room->SetNumberField(TEXT("serverNow"),100);
    TestTrue(TEXT("Accept valid server clock"),UHzaRoomClient::ParseSnapshot(Room,0,Snapshot));
    TestEqual(TEXT("Preserve server clock"),Snapshot.ServerNowUnixMs,100.0);
    TestTrue(TEXT("Record monotonic receipt time"),Snapshot.ReceivedAtSeconds>0);
    TestFalse(TEXT("Reject wrong recipient"),UHzaRoomClient::ParseSnapshot(Room,1,Snapshot));
    const auto Game=Room->GetObjectField(TEXT("game"));
    Game->SetNumberField(TEXT("seed"),123);
    TestFalse(TEXT("Reject hidden randomness"),UHzaRoomClient::ParseSnapshot(Room,0,Snapshot));
    Game->RemoveField(TEXT("seed"));
    auto Players=Game->GetArrayField(TEXT("players"));
    auto Own=Players[0]->AsObject()->GetArrayField(TEXT("hand"));
    Players[1]->AsObject()->SetArrayField(TEXT("hand"),Own);
    TestFalse(TEXT("Reject another player's identity"),UHzaRoomClient::ParseSnapshot(Room,0,Snapshot));
    Players[1]->AsObject()->SetArrayField(TEXT("hand"),{});
    const auto DuplicateTile = Own[0];
    Own.Add(DuplicateTile);Players[0]->AsObject()->SetArrayField(TEXT("hand"),Own);Players[0]->AsObject()->SetNumberField(TEXT("tileCount"),2);
    TestFalse(TEXT("Reject duplicated physical tile"),UHzaRoomClient::ParseSnapshot(Room,0,Snapshot));
    Room->SetArrayField(TEXT("game"),{});
    TestFalse(TEXT("Reject invalid game shape"),UHzaRoomClient::ParseSnapshot(Room,0,Snapshot));
    return true;
}
#endif
