#if WITH_DEV_AUTOMATION_TESTS
#include "HzaAuthClient.h"
#include "HzaRoomClient.h"
#include "Engine/Engine.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Engine/GameInstance.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHzaAuthContractTest, "HZA.Auth.ProfileAndVault", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHzaAuthContractTest::RunTest(const FString&)
{
    TestTrue(TEXT("HZA remains valid"), UHzaAuthClient::IsUsernameValid(TEXT("HZA")));
    TestTrue(TEXT("ASCII underscore username"), UHzaAuthClient::IsUsernameValid(TEXT("Player_123")));
    for (const FString Bad : {TEXT(""), TEXT("ab"), TEXT("abcdefghijklmnopqrstu"), TEXT("hello there"), TEXT("ADMIN"), TEXT("a-b"), TEXT("__\n")})
        TestFalse(TEXT("Invalid or reserved username"), UHzaAuthClient::IsUsernameValid(Bad));
    const FString Id = TEXT("00000000-0000-4000-8000-000000000001");
    TSharedPtr<FJsonObject> Json;
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT(R"({"id":"00000000-0000-4000-8000-000000000001","username":"Hza_123","display_name":"Player","avatar_url":null,"level":1})")), Json);
    FHzaProfile Profile;
    TestTrue(TEXT("Accept own server profile"), UHzaAuthClient::ParseProfile(Json, Id, Profile));
    TestFalse(TEXT("Reject someone else's UUID"), UHzaAuthClient::ParseProfile(Json, TEXT("00000000-0000-4000-8000-000000000002"), Profile));
    Json->SetNumberField(TEXT("level"), 1.5);
    TestFalse(TEXT("Reject fractional level"), UHzaAuthClient::ParseProfile(Json, Id, Profile));
    Json->SetNumberField(TEXT("level"), 1);
    Json->SetField(TEXT("username"), MakeShared<FJsonValueNull>());
    TestTrue(TEXT("Auto-created profile needs onboarding"), UHzaAuthClient::ParseProfile(Json, Id, Profile));
    TestTrue(TEXT("Incomplete username"), Profile.Username.IsEmpty());
    Json->SetNumberField(TEXT("username"), 42);
    TestFalse(TEXT("Wrong-typed username is not an incomplete profile"), UHzaAuthClient::ParseProfile(Json, Id, Profile));
    Json->SetField(TEXT("username"), MakeShared<FJsonValueNull>());
    Json->SetBoolField(TEXT("avatar_url"), true);
    TestFalse(TEXT("Wrong-typed avatar is not an absent avatar"), UHzaAuthClient::ParseProfile(Json, Id, Profile));
    Json->RemoveField(TEXT("avatar_url"));
    TestFalse(TEXT("Truncated profile without avatar field is rejected"), UHzaAuthClient::ParseProfile(Json, Id, Profile));
    Json->SetField(TEXT("avatar_url"), MakeShared<FJsonValueNull>());
    Json->RemoveField(TEXT("username"));
    TestFalse(TEXT("Truncated profile without username field is rejected"), UHzaAuthClient::ParseProfile(Json, Id, Profile));

    auto* Instance = NewObject<UGameInstance>(GEngine);
    Instance->Init();
    auto* Client = Instance->GetSubsystem<UHzaAuthClient>();
    auto* Room = Instance->GetSubsystem<UHzaRoomClient>();
    if (!TestNotNull(TEXT("Real auth subsystem"), Client) || !TestNotNull(TEXT("Real room subsystem"), Room)) { Instance->Shutdown(); return false; }
    // Unique test target ensures the user's actual saved login is never touched.
    Client->VaultTarget = TEXT("HZA/Automation/") + FGuid::NewGuid().ToString();
    Client->RefreshToken = TEXT("synthetic-vault-regression-token");
    if (Client->CanRememberSession())
    {
        TestTrue(TEXT("Write OS-protected refresh token"), Client->SaveRefreshToken());
        Client->RefreshToken.Empty();
        TestTrue(TEXT("Restore from Windows Credential Manager"), Client->LoadRefreshToken());
        TestEqual(TEXT("Vault round trip"), Client->RefreshToken, FString(TEXT("synthetic-vault-regression-token")));
        Client->EraseRefreshToken(); Client->RefreshToken.Empty();
        TestFalse(TEXT("Sign-out erases saved session"), Client->LoadRefreshToken());
    }
    Client->AccessToken = TEXT("synthetic-access-token");
    Client->ExpiresAt = FPlatformTime::Seconds() - 1;
    TestFalse(TEXT("Expired access cannot authorize UI"), Client->HasValidSession());
    Client->ExpiresAt = FPlatformTime::Seconds() + 60;
    TestTrue(TEXT("Expiry uses monotonic time"), Client->HasValidSession());
    Client->ClearSession(true);
    TestFalse(TEXT("Sign-out clears in-memory access"), Client->HasValidSession());
    TestTrue(TEXT("Sign-out clears refresh"), Client->RefreshToken.IsEmpty());

    struct FPendingAuthRequest
    {
        FString Path, Verb, Body;
        bool bAuthenticated;
        TFunction<void(int32, const FString&)> Complete;
    };
    TArray<FPendingAuthRequest> Pending;
    // Drive the real state machine with explicitly ordered responses. No request
    // reaches Supabase, and all credentials/identities below are synthetic fixtures.
    Client->TestTransport = [&Pending](const FString& Path, const FString& Verb, const FString& Body, bool bAuthenticated,
        TFunction<void(int32, const FString&)> Completion)
    {
        Pending.Add({Path, Verb, Body, bAuthenticated, MoveTemp(Completion)});
    };
    auto Respond = [this, &Pending](int32 Index, int32 Status, const FString& Body)
    {
        if (!Pending.IsValidIndex(Index)) { AddError(TEXT("Expected auth request was not queued")); return; }
        auto Completion = MoveTemp(Pending[Index].Complete);
        Pending.RemoveAt(Index);
        Completion(Status, Body);
    };
    auto Reset = [Client, &Pending]()
    {
        Client->ClearSession(true);
        Client->SetState(EHzaAuthState::SignedOut);
        Pending.Empty();
    };
    const FString Session = TEXT(R"({"access_token":"synthetic-access-token","refresh_token":"synthetic-refresh-token","expires_in":3600,"user":{"id":"00000000-0000-4000-8000-000000000001"}})");
    const FString RotatedSession = TEXT(R"({"access_token":"synthetic-access-rotated","refresh_token":"synthetic-refresh-rotated","expires_in":3600,"user":{"id":"00000000-0000-4000-8000-000000000001"}})");
    const FString ProfileObject = TEXT(R"({"id":"00000000-0000-4000-8000-000000000001","username":"Hza_123","display_name":"Player","avatar_url":null,"level":1})");
    const FString ProfileRows = TEXT("[") + ProfileObject + TEXT("]");
    auto Login = [this, Client, &Pending, &Respond, &Session, &ProfileRows]()
    {
        Client->SignIn(TEXT("fixture@example.invalid"), TEXT("synthetic-password"), false);
        if (Pending.Num() != 1) { AddError(TEXT("Sign-in must queue one request")); return; }
        TestEqual(TEXT("Password grant path"), Pending[0].Path, FString(TEXT("/auth/v1/token?grant_type=password")));
        TestFalse(TEXT("Password grant has no stale bearer"), Pending[0].bAuthenticated);
        Respond(0, 200, Session);
        if (Pending.Num() != 1) { AddError(TEXT("Accepted session must fetch its profile")); return; }
        TestTrue(TEXT("Profile fetch is authenticated"), Pending[0].bAuthenticated);
        Respond(0, 200, ProfileRows);
        TestTrue(TEXT("Valid login and complete profile enter Ready"), Client->State == EHzaAuthState::Ready);
    };

    Reset();
    Client->RestoreSession();
    TestTrue(TEXT("No stored session stays signed out"), Client->State == EHzaAuthState::SignedOut);
    TestEqual(TEXT("No stored session makes no request"), Pending.Num(), 0);
    Login();
    TestTrue(TEXT("Login accepts usable session"), Client->HasValidSession());
    TestEqual(TEXT("Login uses server UUID"), Client->Profile.Id, Id);
    TestEqual(TEXT("Login propagates token to actual room subsystem"), Room->Token, Client->AccessToken);

    Reset();
    Client->SignIn(TEXT("fixture@example.invalid"), TEXT("synthetic-password"), false);
    Client->SignIn(TEXT("fixture@example.invalid"), TEXT("synthetic-password"), false);
    TestEqual(TEXT("Double login submit stays single-flight"), Pending.Num(), 1);
    Respond(0, 400, TEXT(R"({"error_code":"invalid_credentials","message":"synthetic-secret-must-not-reach-ui"})"));
    TestTrue(TEXT("Invalid login remains signed out"), Client->State == EHzaAuthState::SignedOut);
    TestEqual(TEXT("Invalid login has safe error code"), Client->ErrorCode, FString(TEXT("INVALID_CREDENTIALS")));
    TestFalse(TEXT("Raw server message is never exposed"), Client->ErrorCode.Contains(TEXT("synthetic-secret")));
    TestFalse(TEXT("Invalid login clears busy state"), Client->bBusy);

    Reset();
    Client->SignIn(TEXT("fixture@example.invalid"), TEXT("synthetic-password"), false);
    Respond(0, 0, TEXT(""));
    TestEqual(TEXT("Network login failure is actionable"), Client->ErrorCode, FString(TEXT("NETWORK_ERROR")));
    TestFalse(TEXT("Offline login cannot create a session"), Client->HasValidSession());
    Client->SignIn(TEXT("fixture@example.invalid"), TEXT("synthetic-password"), false);
    Respond(0, 200, TEXT("not json"));
    TestEqual(TEXT("Malformed login JSON is rejected"), Client->ErrorCode, FString(TEXT("INVALID_RESPONSE")));
    TestFalse(TEXT("Malformed login clears busy state"), Client->bBusy);
    Client->SignIn(TEXT("fixture@example.invalid"), TEXT("synthetic-password"), false);
    Respond(0, 200, TEXT("{}"));
    TestEqual(TEXT("Missing session fields are rejected"), Client->ErrorCode, FString(TEXT("INVALID_SESSION")));
    TestFalse(TEXT("Missing session fields cannot authorize"), Client->HasValidSession());

    Reset();
    Client->SignIn(TEXT("fixture@example.invalid"), TEXT("synthetic-password"), false);
    if (!Pending.IsEmpty())
    {
        auto Completion = MoveTemp(Pending[0].Complete); Pending.RemoveAt(0);
        Completion(200, Session); Completion(200, Session);
        TestEqual(TEXT("Transport completion delivered twice starts only one profile request"), Pending.Num(), 1);
        Respond(0, 200, ProfileRows);
    }

    Reset();
    Client->SignIn(TEXT("fixture@example.invalid"), TEXT("synthetic-password"), false);
    Respond(0, 200, Session);
    Respond(0, 200, ProfileRows.Replace(TEXT("00000000-0000-4000-8000-000000000001"), TEXT("00000000-0000-4000-8000-000000000002")));
    TestEqual(TEXT("Wrong-owner profile fails closed"), Client->ErrorCode, FString(TEXT("INVALID_PROFILE")));
    TestFalse(TEXT("Wrong-owner profile cannot enter Ready"), Client->State == EHzaAuthState::Ready);

    Reset();
    Client->SignUp(TEXT("fixture@example.invalid"), TEXT("synthetic-password"), false);
    Respond(0, 200, TEXT("{}"));
    TestFalse(TEXT("Malformed signup is not email confirmation"), Client->State == EHzaAuthState::ConfirmEmail);
    Client->SignUp(TEXT("fixture@example.invalid"), TEXT("synthetic-password"), false);
    Respond(0, 200, TEXT(R"({"id":"00000000-0000-4000-8000-000000000001","email":"fixture@example.invalid"})"));
    TestTrue(TEXT("Valid pending signup requests confirmation"), Client->State == EHzaAuthState::ConfirmEmail);

    Reset();
    Client->SignIn(TEXT("fixture@example.invalid"), TEXT("synthetic-password"), false);
    Respond(0, 200, Session);
    Respond(0, 200, TEXT("[]"));
    TestTrue(TEXT("Missing profile enters onboarding"), Client->State == EHzaAuthState::NeedsUsername);
    TestEqual(TEXT("Missing profile retains authenticated UUID"), Client->Profile.Id, Id);
    Client->CompleteProfile(TEXT("Hza_123"));
    Client->CompleteProfile(TEXT("hZA_123"));
    TestEqual(TEXT("Duplicate onboarding submit sends once while busy"), Pending.Num(), 1);
    if (!Pending.IsEmpty())
    {
        TSharedPtr<FJsonObject> Claim;
        TestTrue(TEXT("Onboarding body is JSON"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Pending[0].Body), Claim));
        if (Claim)
        {
            TestEqual(TEXT("Onboarding supplies username only"), Claim->Values.Num(), 1);
            TestTrue(TEXT("No client ownership UUID"), Claim->HasField(TEXT("p_username")) && !Claim->HasField(TEXT("id")) && !Claim->HasField(TEXT("user_id")));
        }
    }
    Respond(0, 200, TEXT(R"({"ok":false,"code":"USERNAME_TAKEN"})"));
    TestEqual(TEXT("Database collision shows username taken"), Client->ErrorCode, FString(TEXT("USERNAME_TAKEN")));
    TestTrue(TEXT("Collision remains on onboarding"), Client->State == EHzaAuthState::NeedsUsername);
    Client->CompleteProfile(TEXT("Hza_123"));
    Respond(0, 200, TEXT("{\"ok\":true,\"profile\":") + ProfileObject + TEXT("}"));
    TestTrue(TEXT("Successful onboarding enters Ready"), Client->State == EHzaAuthState::Ready);
    Client->CompleteProfile(TEXT("hZA_123"));
    Respond(0, 200, TEXT("{\"ok\":true,\"profile\":") + ProfileObject + TEXT("}"));
    TestEqual(TEXT("Idempotent differently-cased retry preserves chosen name"), Client->Profile.Username, FString(TEXT("Hza_123")));

    int32 UsernameEvents = 0;
    FString CheckedUsername, CheckCode;
    bool bAvailable = false;
    Client->TestUsernameResult = [&UsernameEvents, &CheckedUsername, &CheckCode, &bAvailable](const FString& Username, bool Available, const FString& Code)
    { ++UsernameEvents; CheckedUsername = Username; bAvailable = Available; CheckCode = Code; };
    Client->CheckUsername(TEXT("First_Name"));
    Client->CheckUsername(TEXT("Second_Name"));
    TestEqual(TEXT("Availability checks can overlap"), Pending.Num(), 2);
    Respond(1, 200, TEXT(R"({"available":true})"));
    TestEqual(TEXT("Latest availability query is delivered"), CheckedUsername, FString(TEXT("Second_Name")));
    TestTrue(TEXT("Available username reported"), bAvailable);
    Respond(0, 200, TEXT(R"({"available":false})"));
    TestEqual(TEXT("Stale availability response cannot replace latest result"), UsernameEvents, 1);
    Client->CheckUsername(TEXT("hZA_123"));
    Respond(0, 200, TEXT(R"({"available":false})"));
    TestEqual(TEXT("Case-collision availability reported as taken"), CheckCode, FString(TEXT("USERNAME_TAKEN")));

    Reset(); Login();
    Client->ExpiresAt = FPlatformTime::Seconds() - 1;
    Client->FetchProfile();
    TestEqual(TEXT("Expired access starts exactly one refresh"), Pending.Num(), 1);
    if (!Pending.IsEmpty()) TestEqual(TEXT("Expired access uses refresh grant"), Pending[0].Path, FString(TEXT("/auth/v1/token?grant_type=refresh_token")));
    Respond(0, 200, RotatedSession);
    TestTrue(TEXT("Refresh creates valid access window"), Client->HasValidSession());
    TestEqual(TEXT("Rotated refresh replaces old token"), Client->RefreshToken, FString(TEXT("synthetic-refresh-rotated")));
    TestEqual(TEXT("Refresh reaches room before profile request completes"), Room->Token, Client->AccessToken);
    Respond(0, 503, TEXT("{}"));
    TestTrue(TEXT("Profile outage is recoverable"), Client->State == EHzaAuthState::NetworkError);
    TestEqual(TEXT("Profile outage does not restore old room token"), Room->Token, FString(TEXT("synthetic-access-rotated")));
    const uint64 RoomGeneration = Room->Generation;
    Client->FetchProfile(); Respond(0, 200, ProfileRows);
    TestEqual(TEXT("Profile retry does not reconnect healthy room"), Room->Generation, RoomGeneration);
    TestTrue(TEXT("Refresh returns to Ready"), Client->State == EHzaAuthState::Ready);

    Client->RefreshSession();
    Respond(0, 400, TEXT(R"({"error_code":"refresh_token_not_found"})"));
    TestTrue(TEXT("Rejected refresh signs out"), Client->State == EHzaAuthState::SignedOut);
    TestEqual(TEXT("Rejected refresh is session expired"), Client->ErrorCode, FString(TEXT("SESSION_EXPIRED")));
    TestTrue(TEXT("Rejected refresh clears both tokens"), Client->AccessToken.IsEmpty() && Client->RefreshToken.IsEmpty());
    TestTrue(TEXT("Rejected refresh clears room token"), Room->Token.IsEmpty());

    Reset(); Login();
    Client->ExpiresAt = FPlatformTime::Seconds() - 1;
    Client->RefreshSession();
    Respond(0, 0, TEXT(""));
    TestTrue(TEXT("Offline refresh enters retryable network state"), Client->State == EHzaAuthState::NetworkError);
    TestFalse(TEXT("Offline refresh retains no valid access"), Client->HasValidSession());
    TestTrue(TEXT("Expired offline auth cannot send room REST requests"), Room->Token.IsEmpty());
    TestFalse(TEXT("Offline refresh preserves retry token"), Client->RefreshToken.IsEmpty());
    TestTrue(TEXT("Offline refresh backs off"), Client->RefreshAt > FPlatformTime::Seconds());
    Client->RefreshSession();
    Respond(0, 200, TEXT("not json"));
    TestFalse(TEXT("Malformed refresh cannot authorize"), Client->HasValidSession());

    Reset(); Login();
    Client->FetchProfile(); Respond(0, 401, TEXT("{}"));
    TestEqual(TEXT("Unauthorized profile starts refresh"), Pending.Num(), 1);
    Respond(0, 200, RotatedSession); Respond(0, 401, TEXT("{}"));
    TestTrue(TEXT("Repeated unauthorized profile cannot loop refresh"), Client->State == EHzaAuthState::SignedOut);
    TestEqual(TEXT("Unauthorized refresh loop is bounded"), Pending.Num(), 0);

    Reset(); Login();
    Client->RefreshSession();
    Respond(0, 200, Session.Replace(TEXT("00000000-0000-4000-8000-000000000001"), TEXT("00000000-0000-4000-8000-000000000002")));
    TestEqual(TEXT("Refresh cannot switch auth UUID"), Client->ErrorCode, FString(TEXT("INVALID_SESSION")));
    TestFalse(TEXT("Cross-identity refresh clears access"), Client->HasValidSession());

    Reset();
    Client->SignIn(TEXT("fixture@example.invalid"), TEXT("synthetic-password"), false);
    Client->SignOut();
    Respond(0, 200, Session);
    TestTrue(TEXT("Late login response cannot sign user back in"), Client->State == EHzaAuthState::SignedOut);
    TestEqual(TEXT("Late login cannot start a profile fetch"), Pending.Num(), 0);
    TestFalse(TEXT("Late login cannot restore access"), Client->HasValidSession());
    TestTrue(TEXT("Late login cannot restore room credentials"), Room->Token.IsEmpty());

    Reset(); Login();
    Client->FetchProfile();
    auto OldProfileCompletion = MoveTemp(Pending[0].Complete); Pending.RemoveAt(0);
    Client->SignOut(); Pending.Empty();
    Client->SignIn(TEXT("replacement@example.invalid"), TEXT("synthetic-password"), false);
    const FString OtherId = TEXT("00000000-0000-4000-8000-000000000002");
    Respond(0, 200, RotatedSession.Replace(*Id, *OtherId));
    Respond(0, 200, ProfileRows.Replace(*Id, *OtherId));
    OldProfileCompletion(200, ProfileRows);
    TestEqual(TEXT("Old callback cannot overwrite replacement identity"), Client->Profile.Id, OtherId);
    TestEqual(TEXT("Old callback cannot overwrite replacement room token"), Room->Token, FString(TEXT("synthetic-access-rotated")));

    Reset(); Login();
    Client->CheckUsername(TEXT("Last_Name"));
    const int32 EventsBeforeSignOut = UsernameEvents;
    Client->FetchProfile();
    Client->SignOut();
    // Pending availability, profile and best-effort logout all predate sign-out.
    Respond(0, 200, TEXT(R"({"available":true})"));
    Respond(0, 200, ProfileRows);
    Respond(0, 204, TEXT(""));
    TestEqual(TEXT("Sign-out suppresses pending username callbacks"), UsernameEvents, EventsBeforeSignOut);
    TestTrue(TEXT("Sign-out suppresses pending profile callbacks"), Client->State == EHzaAuthState::SignedOut && Client->Profile.Id.IsEmpty());

    if (Client->CanRememberSession())
    {
        Reset();
        Client->RefreshToken = TEXT("synthetic-saved-refresh");
        TestTrue(TEXT("Prepare secure restore fixture"), Client->SaveRefreshToken());
        Client->ClearSession(false);
        Client->RestoreSession();
        TestEqual(TEXT("Saved session validates by refresh before use"), Pending.Num(), 1);
        TestFalse(TEXT("Stored refresh alone does not authorize"), Client->HasValidSession());
        Respond(0, 200, Session); Respond(0, 200, ProfileRows);
        TestTrue(TEXT("Restored session reaches Ready"), Client->State == EHzaAuthState::Ready);
        TestTrue(TEXT("Rotated restored session stays remembered"), Client->bSessionRemembered);
        Client->SignOut();
        TestFalse(TEXT("Real sign-out removes vault entry"), Client->LoadRefreshToken());
        TestTrue(TEXT("Real sign-out clears room credentials"), Room->Token.IsEmpty());
        Respond(0, 204, TEXT(""));
        Client->RefreshToken = TEXT("corrupted token with whitespace");
        TestTrue(TEXT("Write isolated corrupt vault fixture"), Client->SaveRefreshToken());
        Client->RefreshToken.Empty(); Client->RestoreSession();
        TestEqual(TEXT("Corrupt saved session makes no request"), Pending.Num(), 0);
        TestTrue(TEXT("Corrupt stored session stays signed out"), Client->State == EHzaAuthState::SignedOut);
        TestFalse(TEXT("Corrupt vault entry is removed"), Client->LoadRefreshToken());
    }
    Client->ClearSession(true);
    Client->TestTransport = nullptr;
    Client->TestUsernameResult = nullptr;
    Instance->Shutdown();
    return true;
}
#endif
