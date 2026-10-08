#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "HzaAuthClient.h"
#include "HzaAuthScreen.h"
#include "HzaFrontend.h"
#include "HzaLobbyClient.h"
#include "HzaLobbyScreen.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Editor.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHzaAuthPIETest, "HZA.Live.AuthFrontend", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHzaAuthPIETest::RunTest(const FString& Parameters)
{
    if (Parameters != TEXT("Ready"))
    {
        if (GEditor->GetEditorWorldContext().World()->GetOutermost()->GetName() != TEXT("/Game/HZA/Test/L_Konkan_Test"))
        { AddError(TEXT("Load L_Konkan_Test first")); return false; }
        FRequestPlaySessionParams Params; Params.WorldType = EPlaySessionWorldType::PlayInEditor;
        ADD_LATENT_AUTOMATION_COMMAND(FStartPIEForAutomationCommand(Params));
        ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this]() { RunTest(TEXT("Ready")); return true; }));
        ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
    }
    AHzaFrontend* Frontend = nullptr;
    for (const auto& Context : GEngine->GetWorldContexts()) if (Context.WorldType == EWorldType::PIE)
        for (TActorIterator<AHzaFrontend> It(Context.World()); It; ++It) { Frontend = *It; break; }
    if (!TestNotNull(TEXT("Installed frontend actor"), Frontend)) return false;
    auto* Auth = Frontend->GetGameInstance()->GetSubsystem<UHzaAuthClient>();
    if (!TestNotNull(TEXT("PIE auth subsystem"), Auth)) return false;
    // Never read or remove the owner's login. All HTTP below stays in this fixture.
    Auth->VaultTarget = TEXT("HZA/Automation/") + FGuid::NewGuid().ToString();
    auto* Lobby = Frontend->GetGameInstance()->GetSubsystem<UHzaLobbyClient>();
    Lobby->TestSend = [](const FString&, const FString&, const FString&, const FString&) { return true; };
    TArray<TFunction<void(int32, const FString&)>> Requests;
    Auth->TestTransport = [&Requests](const FString&, const FString&, const FString&, bool, TFunction<void(int32, const FString&)> Done)
    { Requests.Add(MoveTemp(Done)); };
    auto Respond = [this, &Requests](const FString& Body)
    {
        if (Requests.IsEmpty()) { AddError(TEXT("Expected UI request")); return; }
        auto Done = MoveTemp(Requests[0]); Requests.RemoveAt(0); Done(200, Body);
    };
    Frontend->Open(false);
    auto* Screen = Frontend->Screen.Get();
    if (!TestNotNull(TEXT("Auth screen"), Screen)) { Auth->TestTransport = nullptr; return false; }
    TestTrue(TEXT("Native screen attached and Slate constructed"), Screen->IsInViewport() && Screen->GetRootWidget()->GetCachedWidget().IsValid());
    TestTrue(TEXT("Login is the primary screen"), Screen->LoginPanel->GetVisibility() == ESlateVisibility::Visible);
    TestTrue(TEXT("Password is masked"), Screen->PasswordInput->GetIsPassword());
    Screen->EmailInput->SetText(FText::FromString(TEXT("ui-fixture@example.invalid")));
    Screen->PasswordInput->SetText(FText::FromString(TEXT("synthetic-ui-password")));
    Screen->Submit(); Screen->Submit();
    TestTrue(TEXT("Password cleared immediately after submit"), Screen->PasswordInput->GetText().IsEmpty());
    TestFalse(TEXT("Busy login disables double submit"), Screen->SubmitButton->GetIsEnabled());
    TestEqual(TEXT("Only one UI login request"), Requests.Num(), 1);
    Respond(TEXT(R"({"access_token":"synthetic-ui-access","refresh_token":"synthetic-ui-refresh","expires_in":3600,"user":{"id":"00000000-0000-4000-8000-000000000001"}})"));
    Respond(TEXT("[]"));
    TestTrue(TEXT("Missing profile opens username screen"), Screen->UsernamePanel->GetVisibility() == ESlateVisibility::Visible);
    TestTrue(TEXT("Login is collapsed during onboarding"), Screen->LoginPanel->GetVisibility() == ESlateVisibility::Collapsed);
    Screen->UsernameInput->SetText(FText::FromString(TEXT("Hza_UI")));
    Screen->CheckUsername(); Respond(TEXT(R"({"available":true})"));
    Screen->ClaimUsername(); Respond(TEXT(R"({"ok":false,"code":"USERNAME_TAKEN"})"));
    TestTrue(TEXT("Claim losing advisory race stays on onboarding"), Screen->UsernamePanel->GetVisibility() == ESlateVisibility::Visible);
    TestTrue(TEXT("Collision has player-facing message"), Screen->Notice->GetText().ToString().Contains(TEXT("already taken")));
    TestTrue(TEXT("Collision enables retry"), Screen->ClaimButton->GetIsEnabled());
    Screen->UsernameInput->SetText(FText::FromString(TEXT("Hza_UI2")));
    Screen->ClaimUsername();
    Respond(TEXT(R"({"ok":true,"profile":{"id":"00000000-0000-4000-8000-000000000001","username":"Hza_UI2","display_name":"Player","avatar_url":null,"level":1}})"));
    TestTrue(TEXT("Successful claim shows account-ready state"), Screen->ReadyPanel->GetVisibility() == ESlateVisibility::Visible);
    TestTrue(TEXT("Successful claim transitions to native lobby"), Frontend->LobbyScreen && Frontend->LobbyScreen->GetVisibility() == ESlateVisibility::Visible);
    TestTrue(TEXT("Auth overlay hidden in lobby"), Screen->GetVisibility() == ESlateVisibility::Collapsed);
    auto* Menu=Frontend->LobbyScreen.Get();
    TestNotNull(TEXT("Owner moonlit background packaged with menu"),Menu->BackgroundArt.Get());
    TestNotNull(TEXT("Owner artwork atlas packaged with menu"),Menu->MenuAtlas.Get());
    TestTrue(TEXT("Play page visible by default"),Menu->PlayPanel->GetVisibility()==ESlateVisibility::Visible);
    Menu->OpenPrivate();
    TestTrue(TEXT("Private rooms have a separate page"),Menu->PrivatePanel->GetVisibility()==ESlateVisibility::Visible&&Menu->PlayPanel->GetVisibility()==ESlateVisibility::Collapsed);
    Menu->OpenProfile();
    TestTrue(TEXT("Profile has a separate page"),Menu->ProfilePanel->GetVisibility()==ESlateVisibility::Visible&&Menu->PrivatePanel->GetVisibility()==ESlateVisibility::Collapsed);
    Menu->OpenPlay();Menu->Turbo();
    TestEqual(TEXT("Mode card updates service selection"),Lobby->Mode,FString(TEXT("turbo")));
    TestTrue(TEXT("Onboarding collapsed after success"), Screen->UsernamePanel->GetVisibility() == ESlateVisibility::Collapsed);
    Screen->SignOut();
    TestTrue(TEXT("Sign-out returns to login"), Screen->LoginPanel->GetVisibility() == ESlateVisibility::Visible);
    TestTrue(TEXT("Ready panel hidden after sign-out"), Screen->ReadyPanel->GetVisibility() == ESlateVisibility::Collapsed);
    TestTrue(TEXT("Lobby is hidden after sign-out"), Frontend->LobbyScreen->GetVisibility() == ESlateVisibility::Collapsed);
    Lobby->TestSend = nullptr;
    Auth->ClearSession(true); Auth->TestTransport = nullptr;
    return true;
}
#endif
