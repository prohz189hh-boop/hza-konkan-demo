#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "HzaAuthClient.generated.h"

class FJsonObject;
class IHttpRequest;

UENUM(BlueprintType)
enum class EHzaAuthState : uint8 { SignedOut, Working, ConfirmEmail, NeedsUsername, Ready, NetworkError };

USTRUCT(BlueprintType)
struct HZANETWORK_API FHzaProfile
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="HZA|Auth") FString Id;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Auth") FString Username;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Auth") FString DisplayName;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Auth") FString AvatarUrl;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Auth") int32 Level = 1;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHzaAuthChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FHzaUsernameChecked, const FString&, Username, bool, bAvailable, const FString&, Code);

/** Owns Supabase authentication. Credentials never enter reflected properties,
 * logs, Blueprint events, SaveGames or plain-text config files. */
UCLASS()
class HZANETWORK_API UHzaAuthClient : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintAssignable, Category="HZA|Auth") FHzaAuthChanged OnChanged;
    UPROPERTY(BlueprintAssignable, Category="HZA|Auth") FHzaUsernameChecked OnUsernameChecked;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Auth") EHzaAuthState State = EHzaAuthState::SignedOut;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Auth") FString ErrorCode;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Auth") FHzaProfile Profile;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Auth") bool bBusy = false;
    UPROPERTY(BlueprintReadOnly, Category="HZA|Auth") bool bSessionRemembered = false;

    UFUNCTION(BlueprintCallable, Category="HZA|Auth") void RestoreSession();
    UFUNCTION(BlueprintCallable, Category="HZA|Auth") void SignIn(const FString& Email, const FString& Password, bool bRemember);
    UFUNCTION(BlueprintCallable, Category="HZA|Auth") void SignUp(const FString& Email, const FString& Password, bool bRemember);
    UFUNCTION(BlueprintCallable, Category="HZA|Auth") void RefreshSession();
    UFUNCTION(BlueprintCallable, Category="HZA|Auth") void FetchProfile();
    UFUNCTION(BlueprintCallable, Category="HZA|Auth") void CheckUsername(const FString& Username);
    UFUNCTION(BlueprintCallable, Category="HZA|Auth") void CompleteProfile(const FString& Username);
    UFUNCTION(BlueprintCallable, Category="HZA|Auth") void SignOut();
    UFUNCTION(BlueprintPure, Category="HZA|Auth") bool CanRememberSession() const;
    UFUNCTION(BlueprintPure, Category="HZA|Auth") bool HasValidSession() const;
    static bool IsUsernameValid(const FString& Username);
    static bool ParseProfile(const TSharedPtr<FJsonObject>& Json, const FString& ExpectedId, FHzaProfile& Out);
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
private:
    friend class FHzaAuthContractTest;
    friend class FHzaAuthPIETest;
    friend class FHzaLobbyContractTest;
    FString AccessToken, RefreshToken, UserId;
    FString VaultTarget;
    bool bRememberSession = false;
    bool bRoomConfigured = false;
    bool bProfileRefreshAttempted = false;
    bool bVaultEraseFailed = false;
    double RefreshAt = 0, ExpiresAt = 0;
    uint64 Generation = 0, AvailabilityGeneration = 0;
    FTSTicker::FDelegateHandle Ticker;
    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActiveRequest;
#if WITH_DEV_AUTOMATION_TESTS
    TFunction<void(const FString&, const FString&, const FString&, bool, TFunction<void(int32, const FString&)>)> TestTransport;
    TFunction<void(const FString&, bool, const FString&)> TestUsernameResult;
#endif
    void UsernameResult(const FString& Username, bool bAvailable, const FString& Code);
    void SetState(EHzaAuthState NewState, const FString& Code = TEXT(""));
    void Authenticate(const FString& Email, const FString& Password, bool bRemember, bool bSignup);
    void AcceptSession(const TSharedPtr<FJsonObject>& Json, bool bRefresh);
    void ClearSession(bool bEraseVault);
    void Request(const FString& Path, const FString& Verb, const FString& Body, bool bAuthenticated,
        TFunction<void(int32, const FString&)> Completion, bool bMainRequest = true);
    bool SaveRefreshToken();
    bool LoadRefreshToken();
    void EraseRefreshToken();
    void ProfileReady(const FHzaProfile& Value);
};
