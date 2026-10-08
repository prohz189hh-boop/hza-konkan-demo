#include "HzaFrontend.h"
#include "HzaAuthScreen.h"
#include "HzaLobbyClient.h"
#include "HzaLobbyScreen.h"
#include "HzaHandInteractor.h"
#include "HzaTablePresentation.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

AHzaFrontend::AHzaFrontend() { PrimaryActorTick.bCanEverTick = false; bReplicates = false; }
void AHzaFrontend::BeginPlay()
{
    Super::BeginPlay();
    // Deterministic hand/presentation tests explicitly control their own fixtures.
    // The dedicated auth UI test opens this overlay without touching real sessions.
    if (!GIsAutomationTesting) Open();
}
void AHzaFrontend::Open(bool bRestoreSession)
{
    if (Screen) return;
    auto* PC = GetWorld()->GetFirstPlayerController();
    if (!PC || !PC->IsLocalController()) return;
    for (TActorIterator<AHzaHandInteractor> It(GetWorld()); It; ++It) { Hand = *It; break; }
    if (Hand) Hand->SetFrontendBlocked(true);
    Screen = CreateWidget<UHzaAuthScreen>(PC, UHzaAuthScreen::StaticClass());
    Screen->AddToViewport(100);
    LobbyScreen = CreateWidget<UHzaLobbyScreen>(PC, UHzaLobbyScreen::StaticClass());
    LobbyScreen->AddToViewport(100); LobbyScreen->SetVisibility(ESlateVisibility::Collapsed);
    Auth = GetGameInstance()->GetSubsystem<UHzaAuthClient>();
    Lobby = GetGameInstance()->GetSubsystem<UHzaLobbyClient>();
    Auth->OnChanged.AddDynamic(this, &AHzaFrontend::SyncState);
    Lobby->OnChanged.AddDynamic(this, &AHzaFrontend::SyncState);
    Screen->Start(bRestoreSession);
    SyncState();
}
void AHzaFrontend::SyncState()
{
    if (bSyncing || !Screen || !LobbyScreen || !Auth || !Lobby) return;
    TGuardValue<bool> Guard(bSyncing, true);
    const bool bSignedIn = Auth->State == EHzaAuthState::Ready && Auth->HasValidSession();
    if (bSignedIn) Lobby->Activate();
    const bool bInGame = bSignedIn && Lobby->State == EHzaLobbyState::InGame;
    const ESlateVisibility AuthVisibility = bSignedIn ? ESlateVisibility::Collapsed : ESlateVisibility::Visible;
    const ESlateVisibility LobbyVisibility = bSignedIn && !bInGame ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
    const int8 NextInputOwner = bInGame ? 2 : bSignedIn ? 1 : 0;
    Screen->SetVisibility(AuthVisibility); LobbyScreen->SetVisibility(LobbyVisibility);
    if (Hand) Hand->SetFrontendBlocked(!bInGame);
    for(TActorIterator<AHzaTablePresentation> It(GetWorld());It;++It)It->SetFrontendVisible(!bInGame);
    auto* PC = GetWorld()->GetFirstPlayerController();
    if (!PC) return;
    if (InputOwner == NextInputOwner) return;
    InputOwner = NextInputOwner;
    if (bInGame)
    {
        FInputModeGameAndUI Input; Input.SetHideCursorDuringCapture(false); PC->SetInputMode(Input);
    }
    else
    {
        FInputModeUIOnly Input; Input.SetWidgetToFocus(bSignedIn ? LobbyScreen->TakeWidget() : Screen->TakeWidget()); PC->SetInputMode(Input);
    }
    PC->bShowMouseCursor = true;
}
void AHzaFrontend::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Auth) Auth->OnChanged.RemoveAll(this);
    if (Lobby) Lobby->OnChanged.RemoveAll(this);
    if (Screen) Screen->RemoveFromParent();
    if (LobbyScreen) LobbyScreen->RemoveFromParent();
    if (Hand) Hand->SetFrontendBlocked(false);
    for(TActorIterator<AHzaTablePresentation> It(GetWorld());It;++It)It->SetFrontendVisible(false);
    Super::EndPlay(Reason);
}
