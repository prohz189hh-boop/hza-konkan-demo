#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "HzaAuthClient.h"
#include "HzaAuthScreen.generated.h"

class UButton;
class UCheckBox;
class UEditableTextBox;
class UScrollBox;
class UTextBlock;
class UVerticalBox;

/** Native account and first-time username screens. The owner controls input mode
 * and navigation; constructing this widget does not start an auth request. */
UCLASS()
class HZANETWORK_API UHzaAuthScreen : public UUserWidget
{
    GENERATED_BODY()
public:
    /** Call after CreateWidget. Restore is attempted at most once per widget. */
    UFUNCTION(BlueprintCallable, Category="HZA|Auth")
    void Start(bool bRestoreRememberedSession = true);

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

private:
    friend class FHzaAuthPIETest;
    UPROPERTY(Transient) TObjectPtr<UHzaAuthClient> Auth;
    UPROPERTY(Transient) TObjectPtr<UScrollBox> Scroll;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Heading;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Description;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Notice;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> SubmitCaption;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ModeCaption;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> SignOutCaption;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> RememberHint;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> UsernameFeedback;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ReadySummary;
    UPROPERTY(Transient) TObjectPtr<UEditableTextBox> EmailInput;
    UPROPERTY(Transient) TObjectPtr<UEditableTextBox> PasswordInput;
    UPROPERTY(Transient) TObjectPtr<UEditableTextBox> UsernameInput;
    UPROPERTY(Transient) TObjectPtr<UCheckBox> RememberCheck;
    UPROPERTY(Transient) TObjectPtr<UButton> SubmitButton;
    UPROPERTY(Transient) TObjectPtr<UButton> ModeButton;
    UPROPERTY(Transient) TObjectPtr<UButton> BackButton;
    UPROPERTY(Transient) TObjectPtr<UButton> CheckButton;
    UPROPERTY(Transient) TObjectPtr<UButton> ClaimButton;
    UPROPERTY(Transient) TObjectPtr<UButton> RetryButton;
    UPROPERTY(Transient) TObjectPtr<UButton> SignOutButton;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> LoginPanel;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> ConfirmPanel;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> UsernamePanel;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> ReadyPanel;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> RetryPanel;

    bool bStarted = false;
    bool bRestoreAttempted = false;
    bool bSignup = false;
    bool bCheckingUsername = false;
    bool bIgnoreAuthError = false;
    EHzaAuthState DisplayState = EHzaAuthState::SignedOut;
    FString LocalError;
    void BindAuth();
    void Refresh();
    static FText FriendlyError(const FString& Code);
    UFUNCTION() void AuthChanged();
    UFUNCTION() void UsernameChecked(const FString& Username, bool bAvailable, const FString& Code);
    UFUNCTION() void CredentialsEdited(const FText& Text);
    UFUNCTION() void UsernameEdited(const FText& Text);
    UFUNCTION() void PasswordCommitted(const FText& Text, ETextCommit::Type Method);
    UFUNCTION() void UsernameCommitted(const FText& Text, ETextCommit::Type Method);
    UFUNCTION() void Submit();
    UFUNCTION() void ToggleMode();
    UFUNCTION() void BackToSignIn();
    UFUNCTION() void CheckUsername();
    UFUNCTION() void ClaimUsername();
    UFUNCTION() void Retry();
    UFUNCTION() void SignOut();
};
