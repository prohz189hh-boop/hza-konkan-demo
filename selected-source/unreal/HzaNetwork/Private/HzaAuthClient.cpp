#include "HzaAuthClient.h"
#include "HzaRoomClient.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/GameInstance.h"
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <wincred.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace
{
    // Public deployment identifiers, not credentials. Private/service keys never belong here.
    const FString Supabase = TEXT("https://YOUR_PROJECT.supabase.co");
    const FString PublishableKey = TEXT("YOUR_SUPABASE_PUBLISHABLE_KEY");
    const FString Worker = TEXT("https://YOUR_WORKER.example.invalid");
    bool Decode(const FString& Text, TSharedPtr<FJsonObject>& Json)
    { return Text.Len() <= 65536 && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Json) && Json.IsValid(); }
    FString Encode(const TSharedRef<FJsonObject>& Json)
    { FString Text; FJsonSerializer::Serialize(Json, TJsonWriterFactory<>::Create(&Text)); return Text; }
    bool IsToken(const FString& Token)
    {
        if (Token.IsEmpty() || Token.Len() > 8192) return false;
        for (TCHAR C : Token) if (C <= 32 || C >= 127 || C == ',' || C == '"' || C == '\\') return false;
        return true;
    }
    bool IsUserId(const FString& Id) { FGuid Guid; return FGuid::ParseExact(Id, EGuidFormats::DigitsWithHyphens, Guid) && Guid.IsValid(); }
    FString Failure(int32 Status)
    {
        if (Status == 0 || Status >= 500) return TEXT("NETWORK_ERROR");
        if (Status == 429) return TEXT("TOO_MANY_ATTEMPTS");
        return TEXT("AUTH_FAILED");
    }
    // Only known error codes reach UI; never forward server response bodies/tokens.
    FString AuthFailure(int32 Status, const FString& Body)
    {
        TSharedPtr<FJsonObject> Json; FString Code;
        if (Decode(Body, Json)) Json->TryGetStringField(TEXT("error_code"), Code);
        if (Code == TEXT("email_not_confirmed")) return TEXT("CONFIRM_EMAIL");
        if (Code == TEXT("captcha_failed")) return TEXT("CAPTCHA_REQUIRED");
        if (Code == TEXT("invalid_credentials")) return TEXT("INVALID_CREDENTIALS");
        if (Code == TEXT("weak_password")) return TEXT("WEAK_PASSWORD");
        if (Code == TEXT("signup_disabled")) return TEXT("SIGNUP_DISABLED");
        return Failure(Status);
    }
}

void UHzaAuthClient::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UHzaRoomClient>();
    FString Slot = TEXT("default"), Requested;
    if (FParse::Value(FCommandLine::Get(), TEXT("HzaSession="), Requested) && IsUsernameValid(Requested)) Slot = Requested;
    VaultTarget = TEXT("HZA/YOUR_PROJECT/") + Slot;
    Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float)
    {
        if (!bBusy && !RefreshToken.IsEmpty() && FPlatformTime::Seconds() >= RefreshAt) RefreshSession();
        return true;
    }), 1.0f);
}

void UHzaAuthClient::Deinitialize()
{
    FTSTicker::GetCoreTicker().RemoveTicker(Ticker);
    ClearSession(false);
    Super::Deinitialize();
}

bool UHzaAuthClient::CanRememberSession() const
{
#if PLATFORM_WINDOWS
    return true;
#else
    // Android/iOS must add their platform vault; never fall back to plaintext.
    return false;
#endif
}

bool UHzaAuthClient::HasValidSession() const { return !AccessToken.IsEmpty() && FPlatformTime::Seconds() < ExpiresAt; }
void UHzaAuthClient::SetState(EHzaAuthState NewState, const FString& Code) { State = NewState; ErrorCode = Code; OnChanged.Broadcast(); }

void UHzaAuthClient::Request(const FString& Path, const FString& Verb, const FString& Body, bool bAuthenticated,
    TFunction<void(int32, const FString&)> Completion, bool bMainRequest)
{
    const uint64 Current = Generation;
    auto Delivered = MakeShared<bool>(false);
    TWeakObjectPtr<UHzaAuthClient> WeakThis(this);
    auto Deliver = [WeakThis, Current, Delivered, Completion = MoveTemp(Completion)](int32 Status, const FString& Text)
    {
        if (*Delivered || !WeakThis.IsValid() || Current != WeakThis->Generation) return;
        *Delivered = true;
        Completion(Status, Text.Len() <= 65536 ? Text : TEXT(""));
    };
#if WITH_DEV_AUTOMATION_TESTS
    if (TestTransport) { TestTransport(Path, Verb, Body, bAuthenticated, MoveTemp(Deliver)); return; }
#endif
    auto Http = FHttpModule::Get().CreateRequest();
    Http->SetURL(Supabase + Path); Http->SetVerb(Verb); Http->SetTimeout(20.0f);
    Http->SetHeader(TEXT("apikey"), PublishableKey); Http->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    if (bAuthenticated) Http->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + AccessToken);
    if (!Body.IsEmpty()) Http->SetContentAsString(Body);
    Http->OnProcessRequestComplete().BindWeakLambda(this, [this, Current, Deliver, bMainRequest](FHttpRequestPtr, FHttpResponsePtr Response, bool bOk)
    {
        if (Current != Generation) return;
        if (bMainRequest) ActiveRequest.Reset();
        const int32 Status = bOk && Response.IsValid() ? Response->GetResponseCode() : 0;
        const FString Text = Response.IsValid() && Response->GetContentLength() <= 65536 ? Response->GetContentAsString() : TEXT("");
        Deliver(Status, Text);
    });
    if (bMainRequest) ActiveRequest = Http;
    if (!Http->ProcessRequest())
    {
        // Engine may also invoke completion on failure; unbind to avoid two transitions.
        Http->OnProcessRequestComplete().ExecuteIfBound(Http, nullptr, false);
        Http->OnProcessRequestComplete().Unbind();
    }
}

void UHzaAuthClient::Authenticate(const FString& Email, const FString& Password, bool bRemember, bool bSignup)
{
    if (bBusy) return;
    if (!Email.Contains(TEXT("@")) || Email.Len() > 254 || Password.Len() < (bSignup ? 8 : 1) || Password.Len() > 1024)
    { SetState(EHzaAuthState::SignedOut, TEXT("CHECK_EMAIL_PASSWORD")); return; }
    ClearSession(true); bRememberSession = bRemember && CanRememberSession(); bBusy = true;
    SetState(EHzaAuthState::Working);
    auto Json = MakeShared<FJsonObject>(); Json->SetStringField(TEXT("email"), Email.TrimStartAndEnd()); Json->SetStringField(TEXT("password"), Password);
    Request(bSignup ? TEXT("/auth/v1/signup") : TEXT("/auth/v1/token?grant_type=password"), TEXT("POST"), Encode(Json), false,
        [this, bSignup](int32 Status, const FString& Body)
    {
        bBusy = false; TSharedPtr<FJsonObject> Response;
        if (Status != 200 && Status != 201) { SetState(EHzaAuthState::SignedOut, AuthFailure(Status, Body)); return; }
        if (!Decode(Body, Response)) { SetState(EHzaAuthState::SignedOut, TEXT("INVALID_RESPONSE")); return; }
        if (bSignup && !Response->HasField(TEXT("access_token")))
        {
            FString Id;
            if (!Response->TryGetStringField(TEXT("id"), Id) || !IsUserId(Id)) { SetState(EHzaAuthState::SignedOut, TEXT("INVALID_RESPONSE")); return; }
            SetState(EHzaAuthState::ConfirmEmail); return;
        }
        AcceptSession(Response, false);
    });
}
void UHzaAuthClient::SignIn(const FString& Email, const FString& Password, bool bRemember) { Authenticate(Email, Password, bRemember, false); }
void UHzaAuthClient::SignUp(const FString& Email, const FString& Password, bool bRemember) { Authenticate(Email, Password, bRemember, true); }

void UHzaAuthClient::AcceptSession(const TSharedPtr<FJsonObject>& Json, bool bRefresh)
{
    FString Access, Refresh, Id; double Lifetime = 0; const TSharedPtr<FJsonObject>* User = nullptr;
    if (!Json.IsValid() || !Json->TryGetStringField(TEXT("access_token"), Access) || !IsToken(Access) ||
        !Json->TryGetStringField(TEXT("refresh_token"), Refresh) || !IsToken(Refresh) ||
        !Json->TryGetNumberField(TEXT("expires_in"), Lifetime) || !FMath::IsFinite(Lifetime) || Lifetime < 30 || Lifetime > 604800 ||
        !Json->TryGetObjectField(TEXT("user"), User) || !(*User)->TryGetStringField(TEXT("id"), Id) || !IsUserId(Id) ||
        (bRefresh && !UserId.IsEmpty() && UserId != Id))
    { ClearSession(true); SetState(EHzaAuthState::SignedOut, TEXT("INVALID_SESSION")); return; }
    AccessToken = Access; RefreshToken = Refresh; UserId = Id;
    ExpiresAt = FPlatformTime::Seconds() + Lifetime;
    RefreshAt = ExpiresAt - FMath::Min(60.0, Lifetime / 2);
    bSessionRemembered = bRememberSession && SaveRefreshToken();
    if (bRememberSession && !bSessionRemembered) EraseRefreshToken();
    // A transient profile endpoint failure must not leave the room with an old token.
    if (bRoomConfigured)
        if (auto* Room = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHzaRoomClient>() : nullptr) Room->UpdateAccessToken(AccessToken);
    FetchProfile();
}

void UHzaAuthClient::RestoreSession()
{
    if (bBusy || HasValidSession()) return;
    if (bVaultEraseFailed) { SignOut(); return; }
    if (!LoadRefreshToken()) { SetState(EHzaAuthState::SignedOut); return; }
    bRememberSession = true; RefreshSession();
}
void UHzaAuthClient::RefreshSession()
{
    if (bBusy || RefreshToken.IsEmpty()) return;
    bBusy = true;
    if (State != EHzaAuthState::Ready) SetState(EHzaAuthState::Working);
    auto Json = MakeShared<FJsonObject>(); Json->SetStringField(TEXT("refresh_token"), RefreshToken);
    Request(TEXT("/auth/v1/token?grant_type=refresh_token"), TEXT("POST"), Encode(Json), false, [this](int32 Status, const FString& Body)
    {
        bBusy = false; TSharedPtr<FJsonObject> Response;
        if (Status == 400 || Status == 401 || Status == 403)
        { ClearSession(true); SetState(EHzaAuthState::SignedOut, TEXT("SESSION_EXPIRED")); return; }
        if (Status != 200 || !Decode(Body, Response))
        {
            RefreshAt = FPlatformTime::Seconds() + 30;
            if (!HasValidSession())
                if (auto* Room = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHzaRoomClient>() : nullptr) Room->SuspendAuthorization();
            SetState(EHzaAuthState::NetworkError, Failure(Status)); return;
        }
        AcceptSession(Response, true);
    });
}

bool UHzaAuthClient::IsUsernameValid(const FString& Username)
{
    if (Username.Len() < 3 || Username.Len() > 20) return false;
    for (TCHAR C : Username) if (!((C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') || (C >= '0' && C <= '9') || C == '_')) return false;
    const TSet<FString> Reserved = {TEXT("admin"),TEXT("administrator"),TEXT("moderator"),TEXT("support"),TEXT("system"),TEXT("official"),TEXT("server"),TEXT("supabase"),TEXT("null"),TEXT("undefined"),TEXT("deleted"),TEXT("anonymous")};
    return !Reserved.Contains(Username.ToLower());
}
bool UHzaAuthClient::ParseProfile(const TSharedPtr<FJsonObject>& Json, const FString& ExpectedId, FHzaProfile& Out)
{
    FHzaProfile Parsed; double Level = 1;
    if (!Json.IsValid() || !Json->TryGetStringField(TEXT("id"), Parsed.Id) || Parsed.Id != ExpectedId || !IsUserId(Parsed.Id) ||
        !Json->TryGetStringField(TEXT("display_name"), Parsed.DisplayName) || Parsed.DisplayName.Len() > 256 ||
        !Json->TryGetNumberField(TEXT("level"), Level) || !FMath::IsFinite(Level) || Level < 1 || Level > MAX_int32 || FMath::FloorToDouble(Level) != Level) return false;
    const auto ReadNullable = [&Json](const TCHAR* Key, FString& Value)
    {
        const auto Field = Json->TryGetField(Key);
        return Field.IsValid() && (Field->IsNull() || (Field->Type == EJson::String && Field->TryGetString(Value)));
    };
    if (!ReadNullable(TEXT("username"), Parsed.Username) || !ReadNullable(TEXT("avatar_url"), Parsed.AvatarUrl)) return false;
    if ((!Parsed.Username.IsEmpty() && !IsUsernameValid(Parsed.Username)) || Parsed.AvatarUrl.Len() > 2048) return false;
    Parsed.Level = static_cast<int32>(Level); Out = Parsed; return true;
}
void UHzaAuthClient::FetchProfile()
{
    if (bBusy) return;
    if (!HasValidSession()) { RefreshSession(); return; }
    bBusy = true;
    if (State != EHzaAuthState::Ready) SetState(EHzaAuthState::Working);
    Request(TEXT("/rest/v1/profiles?select=id,username,display_name,avatar_url,level&id=eq.") + UserId, TEXT("GET"), TEXT(""), true,
        [this](int32 Status, const FString& Body)
    {
        bBusy = false;
        if (Status == 401)
        {
            if (bProfileRefreshAttempted) { ClearSession(true); SetState(EHzaAuthState::SignedOut, TEXT("SESSION_EXPIRED")); return; }
            bProfileRefreshAttempted = true; ExpiresAt = 0; RefreshSession(); return;
        }
        TArray<TSharedPtr<FJsonValue>> Rows;
        if (Status != 200 || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body), Rows) || Rows.Num() > 1)
        { SetState(EHzaAuthState::NetworkError, Status == 200 ? TEXT("INVALID_PROFILE") : Failure(Status)); return; }
        if (Rows.IsEmpty()) { bProfileRefreshAttempted = false; Profile = FHzaProfile(); Profile.Id = UserId; SetState(EHzaAuthState::NeedsUsername); return; }
        FHzaProfile Value;
        if (Rows[0]->Type != EJson::Object || !ParseProfile(Rows[0]->AsObject(), UserId, Value))
        { SetState(EHzaAuthState::NetworkError, TEXT("INVALID_PROFILE")); return; }
        ProfileReady(Value);
    });
}
void UHzaAuthClient::ProfileReady(const FHzaProfile& Value)
{
    bProfileRefreshAttempted = false;
    Profile = Value;
    if (Profile.Username.IsEmpty()) { SetState(EHzaAuthState::NeedsUsername); return; }
    if (auto* Room = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHzaRoomClient>() : nullptr)
    {
        if (!bRoomConfigured || !Room->UpdateAccessToken(AccessToken)) Room->ConfigureSession(Worker, AccessToken);
        bRoomConfigured = true;
    }
    SetState(EHzaAuthState::Ready, bRememberSession && !bSessionRemembered ? TEXT("SESSION_NOT_SAVED") : TEXT(""));
}
void UHzaAuthClient::UsernameResult(const FString& Username, bool bAvailable, const FString& Code)
{
#if WITH_DEV_AUTOMATION_TESTS
    if (TestUsernameResult) TestUsernameResult(Username, bAvailable, Code);
#endif
    OnUsernameChecked.Broadcast(Username, bAvailable, Code);
}
void UHzaAuthClient::CheckUsername(const FString& Username)
{
    const uint64 Check = ++AvailabilityGeneration;
    if (!IsUsernameValid(Username)) { UsernameResult(Username, false, TEXT("USERNAME_INVALID")); return; }
    if (!HasValidSession()) { UsernameResult(Username, false, TEXT("SESSION_EXPIRED")); return; }
    auto Json = MakeShared<FJsonObject>(); Json->SetStringField(TEXT("p_username"), Username);
    Request(TEXT("/rest/v1/rpc/username_available"), TEXT("POST"), Encode(Json), true, [this, Username, Check](int32 Status, const FString& Body)
    {
        if (Check != AvailabilityGeneration) return;
        TSharedPtr<FJsonObject> Json; bool Available = false;
        if (Status != 200 || !Decode(Body, Json) || !Json->TryGetBoolField(TEXT("available"), Available))
        { UsernameResult(Username, false, Status == 401 ? TEXT("SESSION_EXPIRED") : Failure(Status)); return; }
        UsernameResult(Username, Available, Available ? TEXT("") : TEXT("USERNAME_TAKEN"));
    }, false);
}
void UHzaAuthClient::CompleteProfile(const FString& Username)
{
    if (bBusy) return;
    if (!IsUsernameValid(Username)) { SetState(EHzaAuthState::NeedsUsername, TEXT("USERNAME_INVALID")); return; }
    if (!HasValidSession()) { RefreshSession(); return; }
    bBusy = true; SetState(EHzaAuthState::Working);
    auto Json = MakeShared<FJsonObject>(); Json->SetStringField(TEXT("p_username"), Username);
    Request(TEXT("/rest/v1/rpc/complete_profile"), TEXT("POST"), Encode(Json), true, [this](int32 Status, const FString& Body)
    {
        bBusy = false; TSharedPtr<FJsonObject> Response; bool Ok = false;
        if (Status == 401) { ExpiresAt = 0; RefreshSession(); return; }
        if (Status != 200 || !Decode(Body, Response) || !Response->TryGetBoolField(TEXT("ok"), Ok))
        { SetState(EHzaAuthState::NeedsUsername, Failure(Status)); return; }
        if (!Ok)
        {
            FString Code; Response->TryGetStringField(TEXT("code"), Code);
            if (Code == TEXT("PROFILE_ALREADY_COMPLETE")) { FetchProfile(); return; }
            SetState(EHzaAuthState::NeedsUsername, Code == TEXT("USERNAME_TAKEN") ? Code : TEXT("USERNAME_INVALID")); return;
        }
        const TSharedPtr<FJsonObject>* Json = nullptr; FHzaProfile Value;
        if (!Response->TryGetObjectField(TEXT("profile"), Json) || !ParseProfile(*Json, UserId, Value) || Value.Username.IsEmpty())
        { SetState(EHzaAuthState::NeedsUsername, TEXT("INVALID_PROFILE")); return; }
        ProfileReady(Value);
    });
}

void UHzaAuthClient::ClearSession(bool bEraseVault)
{
    ++Generation; ++AvailabilityGeneration;
    if (ActiveRequest) { ActiveRequest->OnProcessRequestComplete().Unbind(); ActiveRequest->CancelRequest(); ActiveRequest.Reset(); }
    if (bEraseVault) EraseRefreshToken();
    AccessToken.Empty(); RefreshToken.Empty(); UserId.Empty(); ExpiresAt = 0; RefreshAt = 0;
    Profile = FHzaProfile(); bBusy = false; bRoomConfigured = false; bSessionRemembered = false; bProfileRefreshAttempted = false;
    if (auto* Room = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHzaRoomClient>() : nullptr) Room->SignOut();
}
void UHzaAuthClient::SignOut()
{
    // Local credentials disappear even offline; remote revocation is best effort.
    if (!AccessToken.IsEmpty()) Request(TEXT("/auth/v1/logout?scope=local"), TEXT("POST"), TEXT(""), true, [](int32, const FString&) {}, false);
    ClearSession(true); SetState(EHzaAuthState::SignedOut, bVaultEraseFailed ? TEXT("SESSION_CLEAR_FAILED") : TEXT(""));
}

bool UHzaAuthClient::SaveRefreshToken()
{
#if PLATFORM_WINDOWS
    if (VaultTarget.IsEmpty() || RefreshToken.Len() * sizeof(TCHAR) > CRED_MAX_CREDENTIAL_BLOB_SIZE) return false;
    CREDENTIALW Credential{};
    Credential.Type = CRED_TYPE_GENERIC; Credential.TargetName = const_cast<LPWSTR>(*VaultTarget);
    Credential.CredentialBlobSize = RefreshToken.Len() * sizeof(TCHAR);
    Credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<TCHAR*>(*RefreshToken));
    Credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    return CredWriteW(&Credential, 0) != 0;
#else
    return false;
#endif
}
bool UHzaAuthClient::LoadRefreshToken()
{
#if PLATFORM_WINDOWS
    PCREDENTIALW Credential = nullptr;
    if (VaultTarget.IsEmpty() || !CredReadW(*VaultTarget, CRED_TYPE_GENERIC, 0, &Credential)) return false;
    const bool Sized = Credential->CredentialBlobSize > 0 && Credential->CredentialBlobSize <= CRED_MAX_CREDENTIAL_BLOB_SIZE && Credential->CredentialBlobSize % sizeof(TCHAR) == 0;
    RefreshToken.Empty();
    if (Sized) RefreshToken = FString(Credential->CredentialBlobSize / sizeof(TCHAR), reinterpret_cast<const TCHAR*>(Credential->CredentialBlob));
    CredFree(Credential);
    if (!IsToken(RefreshToken)) { RefreshToken.Empty(); EraseRefreshToken(); return false; }
    return true;
#else
    return false;
#endif
}
void UHzaAuthClient::EraseRefreshToken()
{
#if PLATFORM_WINDOWS
    bVaultEraseFailed = !VaultTarget.IsEmpty() && !CredDeleteW(*VaultTarget, CRED_TYPE_GENERIC, 0) && GetLastError() != ERROR_NOT_FOUND;
#endif
    bSessionRemembered = false;
}
