#include "HzaAuthScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"
#include "Components/SafeZone.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"

#define LOCTEXT_NAMESPACE "HzaAuthScreen"

namespace
{
    const FLinearColor Backdrop(0.008f, 0.014f, 0.024f, 0.98f);
    const FLinearColor Navy(0.019f, 0.031f, 0.050f, 1.0f);
    const FLinearColor Field(0.009f, 0.017f, 0.029f, 1.0f);
    const FLinearColor Bronze(0.72f, 0.49f, 0.25f, 1.0f);
    const FLinearColor Ink(0.94f, 0.93f, 0.89f, 1.0f);
    const FLinearColor Muted(0.59f, 0.65f, 0.72f, 1.0f);
    const FLinearColor ErrorColor(1.0f, 0.57f, 0.48f, 1.0f);
    const FLinearColor Success(0.53f, 0.82f, 0.66f, 1.0f);
}

void UHzaAuthScreen::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(true);
    UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
    WidgetTree->RootWidget = Background;
    Background->SetBrushColor(Backdrop);
    Background->SetPadding(FMargin(0));
    USafeZone* Safe = WidgetTree->ConstructWidget<USafeZone>();
    Background->AddChild(Safe);
    UBorder* Inset = WidgetTree->ConstructWidget<UBorder>();
    Inset->SetBrushColor(FLinearColor::Transparent);
    Inset->SetPadding(FMargin(24));
    Inset->SetHorizontalAlignment(HAlign_Center);
    Inset->SetVerticalAlignment(VAlign_Center);
    Safe->AddChild(Inset);
    USizeBox* Bounds = WidgetTree->ConstructWidget<USizeBox>();
    Bounds->SetWidthOverride(720);
    Bounds->SetMaxDesiredHeight(1000);
    Inset->AddChild(Bounds);
    // Keep full-size touch controls. Short landscape views scroll instead of shrinking the form.
    Scroll = WidgetTree->ConstructWidget<UScrollBox>();
    Scroll->SetScrollWhenFocusChanges(EScrollWhenFocusChanges::AnimatedScroll);
    Scroll->SetNavigationDestination(EDescendantScrollDestination::IntoView);
    Scroll->SetAllowOverscroll(false);
    Scroll->SetScrollbarThickness(FVector2D(5, 5));
    Bounds->AddChild(Scroll);
    UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
    Card->SetBrush(FSlateRoundedBoxBrush(Navy, 16.f, Bronze.CopyWithNewOpacity(0.45f), 1.f));
    Card->SetPadding(FMargin(36, 28));
    CastChecked<UScrollBoxSlot>(Scroll->AddChild(Card))->SetHorizontalAlignment(HAlign_Fill);
    UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
    Card->AddChild(Content);

    const auto Label = [&](const FText& Text, int32 Size, FLinearColor Color = Ink)
    {
        UTextBlock* Result = WidgetTree->ConstructWidget<UTextBlock>();
        FSlateFontInfo Font = Result->GetFont(); Font.Size = Size; Result->SetFont(Font);
        Result->SetText(Text); Result->SetColorAndOpacity(Color); Result->SetAutoWrapText(true);
        Result->SetVisibility(ESlateVisibility::HitTestInvisible);
        return Result;
    };
    const auto Add = [](UVerticalBox* Box, UWidget* Child, float Bottom = 12.f)
    { Box->AddChildToVerticalBox(Child)->SetPadding(FMargin(0, 0, 0, Bottom)); };
    const auto Panel = [&]()
    {
        UVerticalBox* Result = WidgetTree->ConstructWidget<UVerticalBox>();
        Add(Content, Result, 0); return Result;
    };
    const auto Button = [&](UVerticalBox* Box, const FText& Caption, UTextBlock*& Text, bool bPrimary = false)
    {
        UButton* Result = WidgetTree->ConstructWidget<UButton>();
        FButtonStyle Style = Result->GetStyle();
        const FLinearColor Fill = bPrimary ? FLinearColor(0.24f, 0.14f, 0.066f, 1) : Navy;
        Style.SetNormal(FSlateRoundedBoxBrush(Fill, 8.f, Bronze, 1.f));
        Style.SetHovered(FSlateRoundedBoxBrush(Fill + FLinearColor(0.04f, 0.04f, 0.04f, 0), 8.f, Bronze, 2.f));
        Style.SetPressed(FSlateRoundedBoxBrush(Field, 8.f, Bronze, 2.f));
        Style.SetDisabled(FSlateRoundedBoxBrush(Field, 8.f, Muted.CopyWithNewOpacity(0.25f), 1.f));
        Result->SetStyle(Style);
        Text = Label(Caption, 24, bPrimary ? Ink : Bronze); Text->SetJustification(ETextJustify::Center);
        Result->AddChild(Text);
        USizeBox* TouchTarget = WidgetTree->ConstructWidget<USizeBox>(); TouchTarget->SetMinDesiredHeight(76);
        TouchTarget->AddChild(Result); Add(Box, TouchTarget); return Result;
    };
    const auto Input = [&](UVerticalBox* Box, const FText& Caption, const FText& Hint)
    {
        Add(Box, Label(Caption, 21, Muted), 7);
        UEditableTextBox* Result = WidgetTree->ConstructWidget<UEditableTextBox>();
        FEditableTextBoxStyle Style = Result->GetWidgetStyle();
        Style.SetBackgroundImageNormal(FSlateRoundedBoxBrush(Field, 8.f, Muted.CopyWithNewOpacity(0.3f), 1.f));
        Style.SetBackgroundImageHovered(FSlateRoundedBoxBrush(Field, 8.f, Bronze, 1.f));
        Style.SetBackgroundImageFocused(FSlateRoundedBoxBrush(Field, 8.f, Bronze, 2.f));
        Style.SetBackgroundImageReadOnly(FSlateRoundedBoxBrush(Field, 8.f));
        Style.SetForegroundColor(Ink); Style.SetFocusedForegroundColor(Ink); Style.SetReadOnlyForegroundColor(Muted);
        Style.SetPadding(FMargin(16, 12)); Style.TextStyle.Font.Size = 26;
        Result->SetWidgetStyle(Style); Result->SetHintText(Hint);
        Result->SetClearKeyboardFocusOnCommit(true);
        // Escape must not restore an earlier password after we clear the field.
        Result->SetRevertTextOnEscape(false);
        USizeBox* TouchTarget = WidgetTree->ConstructWidget<USizeBox>(); TouchTarget->SetMinDesiredHeight(76);
        TouchTarget->AddChild(Result); Add(Box, TouchTarget, 16); return Result;
    };

    Add(Content, Label(LOCTEXT("Brand", "HZA KONKAN"), 24, Bronze), 18);
    Heading = Label(FText::GetEmpty(), 34); Add(Content, Heading, 8);
    Description = Label(FText::GetEmpty(), 22, Muted); Add(Content, Description, 24);
    Notice = Label(FText::GetEmpty(), 22, ErrorColor); Add(Content, Notice, 16);

    LoginPanel = Panel();
    EmailInput = Input(LoginPanel, LOCTEXT("Email", "Email"), LOCTEXT("EmailHint", "you@example.com"));
    EmailInput->KeyboardType = EVirtualKeyboardType::Email;
    PasswordInput = Input(LoginPanel, LOCTEXT("Password", "Password"), LOCTEXT("PasswordHint", "Your password"));
    PasswordInput->SetIsPassword(true); PasswordInput->KeyboardType = EVirtualKeyboardType::Password;
    PasswordInput->AllowContextMenu = false;
    EmailInput->OnTextChanged.AddDynamic(this, &UHzaAuthScreen::CredentialsEdited);
    PasswordInput->OnTextChanged.AddDynamic(this, &UHzaAuthScreen::CredentialsEdited);
    PasswordInput->OnTextCommitted.AddDynamic(this, &UHzaAuthScreen::PasswordCommitted);
    RememberCheck = WidgetTree->ConstructWidget<UCheckBox>();
    FCheckBoxStyle CheckStyle = RememberCheck->GetWidgetStyle();
    CheckStyle.SetPadding(FMargin(14, 8)); RememberCheck->SetWidgetStyle(CheckStyle);
    RememberCheck->AddChild(Label(LOCTEXT("Remember", "Remember me on this device"), 22));
    RememberCheck->SetIsChecked(false);
    USizeBox* RememberTarget = WidgetTree->ConstructWidget<USizeBox>(); RememberTarget->SetMinDesiredHeight(64);
    RememberTarget->AddChild(RememberCheck); Add(LoginPanel, RememberTarget, 4);
    RememberHint = Label(FText::GetEmpty(), 18, Muted); Add(LoginPanel, RememberHint, 18);
    UTextBlock* ButtonText = nullptr;
    SubmitButton = Button(LoginPanel, FText::GetEmpty(), ButtonText, true); SubmitCaption = ButtonText;
    SubmitButton->OnClicked.AddDynamic(this, &UHzaAuthScreen::Submit);
    ModeButton = Button(LoginPanel, FText::GetEmpty(), ButtonText); ModeCaption = ButtonText;
    ModeButton->OnClicked.AddDynamic(this, &UHzaAuthScreen::ToggleMode);

    ConfirmPanel = Panel();
    Add(ConfirmPanel, Label(LOCTEXT("ConfirmInstructions", "Open the confirmation link in your email, then return here and sign in. If you do not see it, check your spam folder."), 24), 24);
    BackButton = Button(ConfirmPanel, LOCTEXT("BackToSignIn", "Back to sign in"), ButtonText, true);
    BackButton->OnClicked.AddDynamic(this, &UHzaAuthScreen::BackToSignIn);

    UsernamePanel = Panel();
    UsernameInput = Input(UsernamePanel, LOCTEXT("Username", "Username"), LOCTEXT("UsernameHint", "Choose your player name"));
    UsernameInput->OnTextChanged.AddDynamic(this, &UHzaAuthScreen::UsernameEdited);
    UsernameInput->OnTextCommitted.AddDynamic(this, &UHzaAuthScreen::UsernameCommitted);
    Add(UsernamePanel, Label(LOCTEXT("UsernameRules", "3–20 letters, numbers or underscores. Names are unique regardless of capitalization. Choose carefully: this is your permanent player name."), 20, Muted), 18);
    UsernameFeedback = Label(FText::GetEmpty(), 22); Add(UsernamePanel, UsernameFeedback, 14);
    CheckButton = Button(UsernamePanel, LOCTEXT("CheckUsername", "Check availability"), ButtonText);
    CheckButton->OnClicked.AddDynamic(this, &UHzaAuthScreen::CheckUsername);
    ClaimButton = Button(UsernamePanel, LOCTEXT("ClaimUsername", "Confirm username"), ButtonText, true);
    ClaimButton->OnClicked.AddDynamic(this, &UHzaAuthScreen::ClaimUsername);

    ReadyPanel = Panel();
    ReadySummary = Label(FText::GetEmpty(), 24); Add(ReadyPanel, ReadySummary, 24);
    RetryPanel = Panel();
    RetryButton = Button(RetryPanel, LOCTEXT("Retry", "Try again"), ButtonText, true);
    RetryButton->OnClicked.AddDynamic(this, &UHzaAuthScreen::Retry);
    SignOutButton = Button(Content, LOCTEXT("SignOut", "Sign out"), ButtonText);
    SignOutCaption = ButtonText;
    SignOutButton->OnClicked.AddDynamic(this, &UHzaAuthScreen::SignOut);
    Refresh();
}

void UHzaAuthScreen::BindAuth()
{
    if (!Auth && GetGameInstance()) Auth = GetGameInstance()->GetSubsystem<UHzaAuthClient>();
    if (!Auth) return;
    Auth->OnChanged.AddUniqueDynamic(this, &UHzaAuthScreen::AuthChanged);
    Auth->OnUsernameChecked.AddUniqueDynamic(this, &UHzaAuthScreen::UsernameChecked);
}

void UHzaAuthScreen::Start(bool bRestoreRememberedSession)
{
    bStarted = true; BindAuth();
    if (Auth && bRestoreRememberedSession && !bRestoreAttempted)
    {
        bRestoreAttempted = true;
        Auth->RestoreSession();
    }
    Refresh();
}

void UHzaAuthScreen::NativeConstruct()
{
    Super::NativeConstruct();
    if (bStarted) BindAuth();
    Refresh();
}

void UHzaAuthScreen::NativeDestruct()
{
    if (Auth)
    {
        Auth->OnChanged.RemoveDynamic(this, &UHzaAuthScreen::AuthChanged);
        Auth->OnUsernameChecked.RemoveDynamic(this, &UHzaAuthScreen::UsernameChecked);
    }
    if (PasswordInput) PasswordInput->SetText(FText::GetEmpty());
    bCheckingUsername = false;
    Super::NativeDestruct();
}

FText UHzaAuthScreen::FriendlyError(const FString& Code)
{
    if (Code.IsEmpty()) return FText::GetEmpty();
    if (Code == TEXT("INVALID_CREDENTIALS")) return LOCTEXT("InvalidCredentials", "That email and password did not match. Please try again.");
    if (Code == TEXT("CHECK_EMAIL_PASSWORD")) return LOCTEXT("CheckCredentials", "Enter a valid email and password. New passwords need at least 8 characters.");
    if (Code == TEXT("WEAK_PASSWORD")) return LOCTEXT("WeakPassword", "Choose a stronger password with at least 8 characters.");
    if (Code == TEXT("CONFIRM_EMAIL")) return LOCTEXT("ConfirmEmailError", "Confirm your email before signing in.");
    if (Code == TEXT("TOO_MANY_ATTEMPTS")) return LOCTEXT("RateLimit", "Too many attempts. Please wait a little before trying again.");
    if (Code == TEXT("CAPTCHA_REQUIRED")) return LOCTEXT("Captcha", "This sign-in needs a security check that is not available in the game yet.");
    if (Code == TEXT("SIGNUP_DISABLED")) return LOCTEXT("SignupDisabled", "New accounts are unavailable right now. You can still sign in to an existing account.");
    if (Code == TEXT("SESSION_EXPIRED")) return LOCTEXT("Expired", "Your session has expired. Please sign in again.");
    if (Code == TEXT("SESSION_NOT_SAVED")) return LOCTEXT("NotSaved", "You are signed in, but this device could not remember your session. You will need to sign in next time.");
    if (Code == TEXT("SESSION_CLEAR_FAILED")) return LOCTEXT("ClearFailed", "You are signed out, but this device could not remove the saved login. Choose Retry sign out to remove it before sharing this device.");
    if (Code == TEXT("USERNAME_TAKEN")) return LOCTEXT("NameTaken", "That username is already taken. Please choose another.");
    if (Code == TEXT("USERNAME_INVALID")) return LOCTEXT("NameInvalid", "Use 3–20 letters, numbers or underscores. Reserved names are unavailable.");
    if (Code == TEXT("NETWORK_ERROR")) return LOCTEXT("ConnectionFailed", "We could not reach the account service. Check your connection and try again.");
    if (Code == TEXT("INVALID_PROFILE") || Code == TEXT("INVALID_RESPONSE") || Code == TEXT("INVALID_SESSION"))
        return LOCTEXT("InvalidResponse", "We could not verify the account response. Please try again.");
    return LOCTEXT("AuthFailed", "We could not complete that request. Please try again.");
}

void UHzaAuthScreen::Refresh()
{
    if (!Heading) return;
    const bool bBusy = !Auth || Auth->bBusy || Auth->State == EHzaAuthState::Working;
    const EHzaAuthState State = Auth ? Auth->State : EHzaAuthState::SignedOut;
    if (State != EHzaAuthState::Working) DisplayState = State;
    if (State == EHzaAuthState::SignedOut && Auth && Auth->ErrorCode == TEXT("CONFIRM_EMAIL") && !bIgnoreAuthError)
        DisplayState = EHzaAuthState::ConfirmEmail;
    const auto Show = [](UWidget* Widget, bool bVisible)
    { Widget->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed); };
    Show(LoginPanel, DisplayState == EHzaAuthState::SignedOut);
    Show(ConfirmPanel, DisplayState == EHzaAuthState::ConfirmEmail);
    Show(UsernamePanel, DisplayState == EHzaAuthState::NeedsUsername);
    Show(ReadyPanel, DisplayState == EHzaAuthState::Ready);
    Show(RetryPanel, DisplayState == EHzaAuthState::NetworkError);
    // The size wrapper must collapse too, so short views don't retain an empty action row.
    const bool bRetrySignOut = Auth && Auth->ErrorCode == TEXT("SESSION_CLEAR_FAILED");
    Show(SignOutButton->GetParent(), bRetrySignOut || DisplayState == EHzaAuthState::NeedsUsername || DisplayState == EHzaAuthState::Ready || DisplayState == EHzaAuthState::NetworkError);
    SignOutCaption->SetText(bRetrySignOut ? LOCTEXT("RetrySignOut", "Retry sign out") : LOCTEXT("SignOut", "Sign out"));
    SubmitCaption->SetText(bSignup ? LOCTEXT("CreateAccount", "Create account") : LOCTEXT("SignIn", "Sign in"));
    ModeCaption->SetText(bSignup ? LOCTEXT("ExistingAccount", "Already have an account? Sign in") : LOCTEXT("NewAccount", "New here? Create an account"));
    PasswordInput->SetHintText(bSignup ? LOCTEXT("NewPasswordHint", "At least 8 characters") : LOCTEXT("PasswordHint", "Your password"));
    switch (DisplayState)
    {
    case EHzaAuthState::ConfirmEmail:
        Heading->SetText(LOCTEXT("ConfirmTitle", "Check your email"));
        Description->SetText(FText::Format(LOCTEXT("ConfirmDescription", "Confirmation is required for {0}."), EmailInput->GetText()));
        break;
    case EHzaAuthState::NeedsUsername:
        Heading->SetText(LOCTEXT("UsernameTitle", "Make your name"));
        Description->SetText(LOCTEXT("UsernameDescription", "Choose the name other players will see."));
        break;
    case EHzaAuthState::Ready:
        Heading->SetText(FText::Format(LOCTEXT("Welcome", "Welcome, {0}"), FText::FromString(Auth ? Auth->Profile.Username : FString())));
        Description->SetText(LOCTEXT("ReadyDescription", "Your account is ready."));
        if (Auth) ReadySummary->SetText(FText::Format(LOCTEXT("ReadySummary", "Level {0}\n{1}"), FText::AsNumber(Auth->Profile.Level),
            Auth->bSessionRemembered ? LOCTEXT("SessionRemembered", "This device will remember your session.") : LOCTEXT("SessionTemporary", "Signed in for this session.")));
        break;
    case EHzaAuthState::NetworkError:
        Heading->SetText(LOCTEXT("RetryTitle", "Let's reconnect"));
        Description->SetText(LOCTEXT("RetryDescription", "Your account could not be loaded. Try again when your connection is ready."));
        break;
    default:
        Heading->SetText(bSignup ? LOCTEXT("SignupTitle", "Join the table") : LOCTEXT("LoginTitle", "Welcome back"));
        Description->SetText(bSignup ? LOCTEXT("SignupDescription", "Create your HZA KONKAN account.") : LOCTEXT("LoginDescription", "Sign in to your HZA KONKAN account."));
        break;
    }
    FText Message = FriendlyError(!LocalError.IsEmpty() ? LocalError : Auth && !bIgnoreAuthError ? Auth->ErrorCode : FString());
    Notice->SetColorAndOpacity(ErrorColor);
    if (!Auth) Message = LOCTEXT("Unavailable", "Account services are not available in this session.");
    else if (bBusy) { Message = LOCTEXT("Working", "Connecting securely…"); Notice->SetColorAndOpacity(Bronze); }
    else if (Auth->ErrorCode == TEXT("SESSION_NOT_SAVED")) Notice->SetColorAndOpacity(Bronze);
    Notice->SetText(Message); Show(Notice, !Message.IsEmpty());
    EmailInput->SetIsReadOnly(bBusy); PasswordInput->SetIsReadOnly(bBusy); UsernameInput->SetIsReadOnly(bBusy);
    const bool bCanRemember = Auth && Auth->CanRememberSession();
    RememberCheck->SetIsEnabled(!bBusy && bCanRemember);
    if (!bCanRemember) RememberCheck->SetIsChecked(false);
    RememberHint->SetText(bCanRemember ? LOCTEXT("SecureRemember", "Saved securely using this device's credential storage.")
        : LOCTEXT("NoRemember", "Remembering sessions is not available on this device yet."));
    SubmitButton->SetIsEnabled(!bBusy); ModeButton->SetIsEnabled(!bBusy); BackButton->SetIsEnabled(!bBusy);
    CheckButton->SetIsEnabled(!bBusy && !bCheckingUsername);
    ClaimButton->SetIsEnabled(!bBusy && !bCheckingUsername);
    RetryButton->SetIsEnabled(!bBusy); SignOutButton->SetIsEnabled(!bBusy);
    Show(UsernameFeedback, !UsernameFeedback->GetText().IsEmpty());
}

void UHzaAuthScreen::AuthChanged()
{
    LocalError.Empty(); bIgnoreAuthError = false;
    if (Auth && Auth->State != EHzaAuthState::NeedsUsername && Auth->State != EHzaAuthState::Working) bCheckingUsername = false;
    Refresh();
}

void UHzaAuthScreen::CredentialsEdited(const FText&)
{ LocalError.Empty(); bIgnoreAuthError = true; Refresh(); }

void UHzaAuthScreen::UsernameEdited(const FText&)
{
    bCheckingUsername = false; LocalError.Empty(); bIgnoreAuthError = true;
    UsernameFeedback->SetText(FText::GetEmpty()); Refresh();
}

void UHzaAuthScreen::PasswordCommitted(const FText&, ETextCommit::Type Method)
{ if (Method == ETextCommit::OnEnter) Submit(); }

void UHzaAuthScreen::UsernameCommitted(const FText&, ETextCommit::Type Method)
{ if (Method == ETextCommit::OnEnter) CheckUsername(); }

void UHzaAuthScreen::Submit()
{
    if (!Auth || Auth->bBusy || DisplayState != EHzaAuthState::SignedOut) return;
    FString Password = PasswordInput->GetText().ToString();
    const FString Email = EmailInput->GetText().ToString().TrimStartAndEnd();
    // Clear the masked UI value before starting a request; retain only the email for retry.
    PasswordInput->SetText(FText::GetEmpty()); EmailInput->SetText(FText::FromString(Email));
    const bool bRemember = RememberCheck->IsChecked() && Auth->CanRememberSession();
    if (bSignup) Auth->SignUp(Email, Password, bRemember); else Auth->SignIn(Email, Password, bRemember);
    if (Password.Len() > 0) FMemory::Memzero(Password.GetCharArray().GetData(), Password.Len() * sizeof(TCHAR));
    Password.Empty();
}

void UHzaAuthScreen::ToggleMode()
{
    if (!Auth || Auth->bBusy) return;
    bSignup = !bSignup; PasswordInput->SetText(FText::GetEmpty());
    LocalError.Empty(); bIgnoreAuthError = true; Refresh();
}

void UHzaAuthScreen::BackToSignIn()
{
    if (!Auth || Auth->bBusy) return;
    bSignup = false; PasswordInput->SetText(FText::GetEmpty()); Auth->SignOut();
    Scroll->ScrollToStart();
}

void UHzaAuthScreen::CheckUsername()
{
    if (!Auth || Auth->bBusy || bCheckingUsername || DisplayState != EHzaAuthState::NeedsUsername) return;
    const FString Username = UsernameInput->GetText().ToString().TrimStartAndEnd();
    UsernameInput->SetText(FText::FromString(Username));
    bCheckingUsername = true;
    UsernameFeedback->SetColorAndOpacity(Muted);
    UsernameFeedback->SetText(LOCTEXT("CheckingName", "Checking availability…"));
    Refresh(); Auth->CheckUsername(Username);
}

void UHzaAuthScreen::UsernameChecked(const FString& Username, bool bAvailable, const FString& Code)
{
    if (!Auth || DisplayState != EHzaAuthState::NeedsUsername || Username != UsernameInput->GetText().ToString().TrimStartAndEnd()) return;
    bCheckingUsername = false;
    UsernameFeedback->SetColorAndOpacity(bAvailable ? Success : ErrorColor);
    UsernameFeedback->SetText(bAvailable ? LOCTEXT("NameAvailable", "Available now. Confirm to make it yours.") : FriendlyError(Code));
    Refresh();
}

void UHzaAuthScreen::ClaimUsername()
{
    if (!Auth || Auth->bBusy || bCheckingUsername || DisplayState != EHzaAuthState::NeedsUsername) return;
    const FString Username = UsernameInput->GetText().ToString().TrimStartAndEnd();
    UsernameInput->SetText(FText::FromString(Username));
    UsernameFeedback->SetText(FText::GetEmpty());
    Auth->CompleteProfile(Username);
}

void UHzaAuthScreen::Retry()
{
    if (!Auth || Auth->bBusy) return;
    if (Auth->HasValidSession()) Auth->FetchProfile(); else Auth->RefreshSession();
}

void UHzaAuthScreen::SignOut()
{
    if (!Auth || Auth->bBusy) return;
    bSignup = false; bCheckingUsername = false;
    PasswordInput->SetText(FText::GetEmpty()); UsernameInput->SetText(FText::GetEmpty());
    RememberCheck->SetIsChecked(false); Auth->SignOut(); Scroll->ScrollToStart();
}

#undef LOCTEXT_NAMESPACE
