#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HzaLobbyScreen.generated.h"
class UHzaLobbyClient;
class UHzaAuthClient;
class UTextBlock;
class UButton;
class UBorder;
class UEditableTextBox;
class UVerticalBox;
class UTexture2D;
UCLASS()
class HZANETWORK_API UHzaLobbyScreen : public UUserWidget
{
    GENERATED_BODY()
public:
    UHzaLobbyScreen(const FObjectInitializer& ObjectInitializer);
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& Geometry,float DeltaSeconds) override;
private:
    friend class FHzaAuthPIETest;
    UPROPERTY() TObjectPtr<UTexture2D> BackgroundArt;
    UPROPERTY() TObjectPtr<UTexture2D> MenuAtlas;
    UPROPERTY() TObjectPtr<UHzaLobbyClient> Lobby;
    UPROPERTY() TObjectPtr<UHzaAuthClient> Auth;
    UPROPERTY() TObjectPtr<UTextBlock> Title;
    UPROPERTY() TObjectPtr<UTextBlock> Account;
    UPROPERTY() TObjectPtr<UTextBlock> Notice;
    UPROPERTY() TObjectPtr<UBorder> NoticePanel;
    UPROPERTY() TObjectPtr<UTextBlock> ModeText;
    UPROPERTY() TObjectPtr<UTextBlock> RoomText;
    UPROPERTY() TObjectPtr<UVerticalBox> Home;
    UPROPERTY() TObjectPtr<UVerticalBox> PlayPanel;
    UPROPERTY() TObjectPtr<UVerticalBox> PrivatePanel;
    UPROPERTY() TObjectPtr<UVerticalBox> ProfilePanel;
    UPROPERTY() TObjectPtr<UTextBlock> ProfileDetails;
    UPROPERTY() TObjectPtr<UTextBlock> Avatar;
    UPROPERTY() TObjectPtr<UTextBlock> CoinValue;
    UPROPERTY() TObjectPtr<UTextBlock> RatingValue;
    UPROPERTY() TObjectPtr<UTextBlock> ProfileName;
    UPROPERTY() TObjectPtr<UTextBlock> ProfileXP;
    UPROPERTY() TObjectPtr<UTextBlock> PartyStatus;
    UPROPERTY() TObjectPtr<UTextBlock> SearchTitle;
    UPROPERTY() TObjectPtr<UTextBlock> SearchMode;
    UPROPERTY() TObjectPtr<UTextBlock> RegularMark;
    UPROPERTY() TObjectPtr<UTextBlock> TurboMark;
    UPROPERTY() TObjectPtr<UTextBlock> FooterStatus;
    UPROPERTY() TObjectPtr<UVerticalBox> TransitionArea;
    UPROPERTY() TArray<TObjectPtr<UTextBlock>> SeatLabels;
    float PageTime=1.f;
    float RegularScale=1.f,TurboScale=1.f;
    UPROPERTY() TObjectPtr<UButton> PlayNav;
    UPROPERTY() TObjectPtr<UButton> PrivateNav;
    UPROPERTY() TObjectPtr<UButton> ProfileNav;
    uint8 Menu = 0;
    UPROPERTY() TObjectPtr<UVerticalBox> Search;
    UPROPERTY() TObjectPtr<UVerticalBox> Waiting;
    UPROPERTY() TObjectPtr<UVerticalBox> ReconnectPanel;
    UPROPERTY() TObjectPtr<UEditableTextBox> CodeInput;
    UPROPERTY() TObjectPtr<UButton> RegularButton;
    UPROPERTY() TObjectPtr<UButton> TurboButton;
    UPROPERTY() TObjectPtr<UButton> MatchButton;
    UPROPERTY() TObjectPtr<UButton> CreateButton;
    UPROPERTY() TObjectPtr<UButton> JoinButton;
    UPROPERTY() TObjectPtr<UButton> ReadyButton;
    UPROPERTY() TObjectPtr<UButton> StartButton;
    UFUNCTION() void Refresh();
    UFUNCTION() void Regular();
    UFUNCTION() void Turbo();
    UFUNCTION() void Find();
    UFUNCTION() void Cancel();
    UFUNCTION() void Create();
    UFUNCTION() void Join();
    UFUNCTION() void Ready();
    UFUNCTION() void StartMatch();
    UFUNCTION() void Reconnect();
    UFUNCTION() void RefreshAccount();
    UFUNCTION() void SignOut();
    UFUNCTION() void OpenPlay();
    UFUNCTION() void OpenPrivate();
    UFUNCTION() void OpenProfile();
};
