#include "HzaLobbyScreen.h"
#include "HzaLobbyClient.h"
#include "HzaAuthClient.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/SafeZone.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/CircularThrobber.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/TextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#define LOCTEXT_NAMESPACE "HzaLobby"

namespace
{
    const FLinearColor Navy(.008f,.015f,.026f,1), Surface(.014f,.027f,.045f,.91f), Gold(.64f,.39f,.17f,1), Ink(.92f,.90f,.84f,1), Muted(.43f,.51f,.60f,1);
    FButtonStyle MenuStyle(bool Selected=false, bool Primary=false)
    {
        FButtonStyle S;
        S.SetNormal(FSlateRoundedBoxBrush(Primary?Gold:Selected?FLinearColor(.028f,.036f,.045f,.96f):Surface,12.f,Selected?Gold:FLinearColor(.10f,.15f,.20f,.6f),Selected?2.f:1.f));
        S.SetHovered(FSlateRoundedBoxBrush(Primary?FLinearColor(.91f,.70f,.43f,1):FLinearColor(.065f,.092f,.12f,1),12.f,Gold,2.f));
        S.SetPressed(FSlateRoundedBoxBrush(FLinearColor(.25f,.18f,.10f,1),12.f,Gold,2.f));
        S.SetDisabled(FSlateRoundedBoxBrush(Surface,12.f));
        S.SetNormalPadding(FMargin(20,12)); S.SetPressedPadding(FMargin(20,13,20,11));
        return S;
    }
}
UHzaLobbyScreen::UHzaLobbyScreen(const FObjectInitializer& ObjectInitializer):Super(ObjectInitializer)
{
    // Hard references keep the owner's artwork in packaged builds.
    static ConstructorHelpers::FObjectFinder<UTexture2D> Background(TEXT("/HzaNetwork/UI/T_HZA_MoonlitBackground.T_HZA_MoonlitBackground"));
    static ConstructorHelpers::FObjectFinder<UTexture2D> Atlas(TEXT("/HzaNetwork/UI/T_HZA_MoonlitAtlas.T_HZA_MoonlitAtlas"));
    BackgroundArt=Background.Object;MenuAtlas=Atlas.Object;
}
void UHzaLobbyScreen::NativeOnInitialized()
{
    Super::NativeOnInitialized(); SetIsFocusable(true);
    Lobby=GetGameInstance()->GetSubsystem<UHzaLobbyClient>(); Auth=GetGameInstance()->GetSubsystem<UHzaAuthClient>();
    auto* Layers=WidgetTree->ConstructWidget<UOverlay>();WidgetTree->RootWidget=Layers;
    auto* Backdrop=WidgetTree->ConstructWidget<UImage>();Backdrop->SetBrushFromTexture(BackgroundArt);Backdrop->SetVisibility(ESlateVisibility::HitTestInvisible);
    auto* BackdropSlot=Layers->AddChildToOverlay(Backdrop);BackdropSlot->SetHorizontalAlignment(HAlign_Fill);BackdropSlot->SetVerticalAlignment(VAlign_Fill);
    auto* Back=WidgetTree->ConstructWidget<UBorder>();auto* ForegroundSlot=Layers->AddChildToOverlay(Back);ForegroundSlot->SetHorizontalAlignment(HAlign_Fill);ForegroundSlot->SetVerticalAlignment(VAlign_Fill);Back->SetBrushColor(Navy.CopyWithNewOpacity(.12f)); Back->SetPadding(FMargin(0));
    auto* Safe=WidgetTree->ConstructWidget<USafeZone>(); Back->AddChild(Safe);
    auto* Scale=WidgetTree->ConstructWidget<UScaleBox>(); Scale->SetStretch(EStretch::ScaleToFit); Safe->AddChild(Scale);
    auto* Frame=WidgetTree->ConstructWidget<USizeBox>(); Frame->SetWidthOverride(1440); Frame->SetHeightOverride(810); Scale->AddChild(Frame);
    auto* ContentMargin=WidgetTree->ConstructWidget<UBorder>(); ContentMargin->SetBrushColor(FLinearColor::Transparent); ContentMargin->SetPadding(FMargin(48,36)); Frame->AddChild(ContentMargin);
    auto* Root=WidgetTree->ConstructWidget<UVerticalBox>(); ContentMargin->AddChild(Root);
    auto Label=[&](const FText& Value,int32 Size,FLinearColor Color=Ink)
    {
        auto* T=WidgetTree->ConstructWidget<UTextBlock>(); auto F=T->GetFont();F.Size=Size;T->SetFont(F);T->SetText(Value);T->SetColorAndOpacity(Color);T->SetAutoWrapText(true);return T;
    };
    auto Add=[&](UVerticalBox* P,UWidget* W,float Gap=14.f){auto* Slot=P->AddChildToVerticalBox(W);Slot->SetPadding(FMargin(0,0,0,Gap));return Slot;};
    auto Gap=[&](float Height){auto* S=WidgetTree->ConstructWidget<USpacer>();S->SetSize(FVector2D(1,Height));return S;};
    auto Button=[&](const FText& Caption,bool Primary=false)
    {
        auto* B=WidgetTree->ConstructWidget<UButton>();B->SetStyle(MenuStyle(false,Primary));
        auto* CaptionLabel=Label(Caption,19,Primary?Navy:Ink);CaptionLabel->SetAutoWrapText(false);B->AddChild(CaptionLabel);return B;
    };
    auto Box=[&](UWidget* Child,FMargin Inset=FMargin(18))
    {auto* B=WidgetTree->ConstructWidget<UBorder>();B->SetBrush(FSlateRoundedBoxBrush(Surface,14.f,Gold.CopyWithNewOpacity(.35f),1.f));B->SetPadding(Inset);B->AddChild(Child);return B;};
    auto AtlasImage=[&](FVector2D Min,FVector2D Max)
    {
        auto* Art=WidgetTree->ConstructWidget<UImage>();FSlateBrush Brush;Brush.SetResourceObject(MenuAtlas);Brush.DrawAs=ESlateBrushDrawType::Image;Brush.SetUVRegion(FBox2f(FVector2f(Min),FVector2f(Max)));Art->SetBrush(Brush);Art->SetVisibility(ESlateVisibility::HitTestInvisible);return Art;
    };
    auto* Header=WidgetTree->ConstructWidget<UHorizontalBox>();Add(Root,Header,26);
    auto* Brand=WidgetTree->ConstructWidget<UVerticalBox>();Header->AddChildToHorizontalBox(Brand)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto* Logo=WidgetTree->ConstructWidget<USizeBox>();Logo->SetWidthOverride(210);Logo->SetHeightOverride(78);Logo->AddChild(AtlasImage(FVector2D(45.f/1672,15.f/941),FVector2D(297.f/1672,112.f/941)));Add(Brand,Logo,0)->SetHorizontalAlignment(HAlign_Left);
    auto* RatingBadge=WidgetTree->ConstructWidget<UVerticalBox>();Add(RatingBadge,Label(LOCTEXT("RatingBadge","RATING"),10,Gold),2);RatingValue=Label(FText::GetEmpty(),18);Add(RatingBadge,RatingValue,0);Header->AddChildToHorizontalBox(RatingBadge)->SetPadding(FMargin(16,0,24,0));
    auto* CoinsBadge=WidgetTree->ConstructWidget<UVerticalBox>();Add(CoinsBadge,Label(LOCTEXT("CoinsBadge","◈ COINS"),10,Gold),2);CoinValue=Label(FText::GetEmpty(),18);Add(CoinsBadge,CoinValue,0);Header->AddChildToHorizontalBox(CoinsBadge)->SetPadding(FMargin(0,0,24,0));
    Avatar=Label(FText::GetEmpty(),22,Gold);auto* AvatarSize=WidgetTree->ConstructWidget<USizeBox>();AvatarSize->SetWidthOverride(54);AvatarSize->SetHeightOverride(54);AvatarSize->AddChild(Box(Avatar,FMargin(12)));Header->AddChildToHorizontalBox(AvatarSize)->SetPadding(FMargin(0,0,10,0));
    auto* Player=Button(FText::GetEmpty());Player->OnClicked.AddDynamic(this,&UHzaLobbyScreen::OpenProfile);
    auto* PlayerInfo=WidgetTree->ConstructWidget<UVerticalBox>();Player->SetContent(PlayerInfo);
    Title=Label(FText::GetEmpty(),22);Add(PlayerInfo,Title,3);Account=Label(FText::GetEmpty(),14,Muted);Add(PlayerInfo,Account,0);
    Header->AddChildToHorizontalBox(Player)->SetVerticalAlignment(VAlign_Center);
    auto* Body=WidgetTree->ConstructWidget<UHorizontalBox>();Add(Root,Body,18)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto* RailWidth=WidgetTree->ConstructWidget<USizeBox>();RailWidth->SetWidthOverride(152);Body->AddChildToHorizontalBox(RailWidth)->SetPadding(FMargin(0,4,28,0));
    auto* Rail=WidgetTree->ConstructWidget<UVerticalBox>();RailWidth->AddChild(Box(Rail,FMargin(6,12)));
    PlayNav=Button(LOCTEXT("PlayNav","PLAY"));Add(Rail,PlayNav,12);PlayNav->OnClicked.AddDynamic(this,&UHzaLobbyScreen::OpenPlay);
    auto DisabledNav=[&](const FText& Name){auto* B=Button(Name);B->SetIsEnabled(false);B->SetToolTipText(LOCTEXT("NotConnected","This menu is not available yet."));auto S=B->GetStyle();S.SetDisabled(FSlateRoundedBoxBrush(FLinearColor(.01f,.02f,.03f,.25f),6.f));S.SetNormalPadding(FMargin(12,10));B->SetStyle(S);Add(Rail,B,6);};
    DisabledNav(LOCTEXT("PartyNav","PARTY"));DisabledNav(LOCTEXT("FriendsNav","FRIENDS"));
    ProfileNav=Button(LOCTEXT("ProfileNav","PROFILE"));Add(Rail,ProfileNav,12);ProfileNav->OnClicked.AddDynamic(this,&UHzaLobbyScreen::OpenProfile);
    DisabledNav(LOCTEXT("RewardsNav","REWARDS"));DisabledNav(LOCTEXT("ShopNav","SHOP"));DisabledNav(LOCTEXT("RankNav","RANK"));DisabledNav(LOCTEXT("SettingsNav","SETTINGS"));
    PrivateNav=Button(LOCTEXT("PrivateNav","PRIVATE"));Add(Rail,PrivateNav,12);PrivateNav->OnClicked.AddDynamic(this,&UHzaLobbyScreen::OpenPrivate);
    Add(Rail,Gap(20))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto* Content=WidgetTree->ConstructWidget<UVerticalBox>();TransitionArea=Content;Body->AddChildToHorizontalBox(Content)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto* SocialWidth=WidgetTree->ConstructWidget<USizeBox>();SocialWidth->SetWidthOverride(218);Body->AddChildToHorizontalBox(SocialWidth)->SetPadding(FMargin(26,4,0,0));
    auto* Social=WidgetTree->ConstructWidget<UVerticalBox>();SocialWidth->AddChild(Social);
    auto* PartyCard=WidgetTree->ConstructWidget<UVerticalBox>();Add(Social,Box(PartyCard),16);Add(PartyCard,Label(LOCTEXT("YourParty","YOUR PARTY"),15,Gold),18);PartyStatus=Label(FText::GetEmpty(),18);Add(PartyCard,PartyStatus,20);
    auto* Invite=Button(LOCTEXT("Invite","INVITE"));Invite->SetIsEnabled(false);Invite->SetToolTipText(LOCTEXT("InvitePending","Party invitations are not connected in this native menu yet."));Add(PartyCard,Invite,0);
    auto* FriendsCard=WidgetTree->ConstructWidget<UVerticalBox>();Add(Social,Box(FriendsCard),0);Add(FriendsCard,Label(LOCTEXT("Together","BETTER TOGETHER"),14,Gold),12);Add(FriendsCard,Label(LOCTEXT("PrivateSocial","Bring your friends to a private table. Share your room code and play together."),16,Muted),20);
    auto* PrivateShortcut=Button(LOCTEXT("PrivateShortcut","PRIVATE ROOM"));auto* PrivateCaption=Cast<UTextBlock>(PrivateShortcut->GetContent());auto PrivateFont=PrivateCaption->GetFont();PrivateFont.Size=14;PrivateCaption->SetFont(PrivateFont);PrivateShortcut->OnClicked.AddDynamic(this,&UHzaLobbyScreen::OpenPrivate);Add(FriendsCard,PrivateShortcut,0);
    auto Panel=[&](UVerticalBox* Parent){auto* P=WidgetTree->ConstructWidget<UVerticalBox>();Add(Parent,P,0);return P;};
    Home=Panel(Content);PlayPanel=Panel(Home);PrivatePanel=Panel(Home);ProfilePanel=Panel(Home);
    Add(PlayPanel,Label(LOCTEXT("Headline","PLAY KONKAN"),34),6);
    Add(PlayPanel,Label(LOCTEXT("Choose","Choose how you want to play."),18,Ink),20);
    auto* Modes=WidgetTree->ConstructWidget<UHorizontalBox>();Add(PlayPanel,Modes,20);
    auto ModeCard=[&](bool TurboMode)
    {
        auto* B=WidgetTree->ConstructWidget<UButton>();B->SetStyle(MenuStyle());
        auto* Fixed=WidgetTree->ConstructWidget<USizeBox>();Fixed->SetHeightOverride(230);Fixed->AddChild(B);
        auto* Slot=Modes->AddChildToHorizontalBox(Fixed);Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));Slot->SetPadding(TurboMode?FMargin(10,0,0,0):FMargin(0,0,10,0));
        auto* CardLayers=WidgetTree->ConstructWidget<UOverlay>();auto* CardSlot=Cast<UButtonSlot>(B->AddChild(CardLayers));CardSlot->SetHorizontalAlignment(HAlign_Fill);CardSlot->SetVerticalAlignment(VAlign_Fill);
        auto* Art=AtlasImage(FVector2D((TurboMode?748.f:292.f)/1672,230.f/941),FVector2D((TurboMode?1136.f:720.f)/1672,475.f/941));
        auto* ArtSlot=CardLayers->AddChildToOverlay(Art);ArtSlot->SetHorizontalAlignment(HAlign_Fill);ArtSlot->SetVerticalAlignment(VAlign_Fill);
        auto* Shade=WidgetTree->ConstructWidget<UBorder>();Shade->SetBrushColor(Navy.CopyWithNewOpacity(.32f));Shade->SetPadding(FMargin(18,10));auto* ShadeSlot=CardLayers->AddChildToOverlay(Shade);ShadeSlot->SetHorizontalAlignment(HAlign_Fill);ShadeSlot->SetVerticalAlignment(VAlign_Fill);
        auto* C=WidgetTree->ConstructWidget<UVerticalBox>();Shade->AddChild(C);
        auto* Mark=Label(FText::GetEmpty(),11,Gold);Add(C,Mark,12);if(TurboMode)TurboMark=Mark;else RegularMark=Mark;
        Add(C,Label(TurboMode?LOCTEXT("Turbo","TURBO"):LOCTEXT("Regular","REGULAR"),30),4);
        Add(C,Label(TurboMode?LOCTEXT("Five","5 GAMES"):LOCTEXT("Seven","7 GAMES"),19,Ink),12);
        Add(C,Label(TurboMode?LOCTEXT("TurboDesc","Faster Session"):LOCTEXT("RegularDesc","Full Konkan Match"),17),12);
        Add(C,Label(LOCTEXT("Teams","4 players  ·  2 teams"),14,Muted),0);
        return B;
    };
    RegularButton=ModeCard(false);RegularButton->OnClicked.AddDynamic(this,&UHzaLobbyScreen::Regular);
    TurboButton=ModeCard(true);TurboButton->OnClicked.AddDynamic(this,&UHzaLobbyScreen::Turbo);
    MatchButton=Button(LOCTEXT("Find","FIND MATCH  →"),true);Add(PlayPanel,MatchButton,14);MatchButton->OnClicked.AddDynamic(this,&UHzaLobbyScreen::Find);
    ModeText=Label(LOCTEXT("PublicMatch","Public matchmaking · Four players"),14,Muted);Add(PlayPanel,ModeText,0);
    Add(PrivatePanel,Label(LOCTEXT("PrivateTitle","Your people. Your table."),38),8);
    Add(PrivatePanel,Label(LOCTEXT("PrivateIntro","Start a private room or join your friends with a code."),19,Muted),28);
    CreateButton=Button(LOCTEXT("CreateRoom","CREATE A ROOM"),true);Add(PrivatePanel,CreateButton,26);CreateButton->OnClicked.AddDynamic(this,&UHzaLobbyScreen::Create);
    Add(PrivatePanel,Label(LOCTEXT("HaveCode","HAVE A ROOM CODE?"),14,Gold),12);
    auto* JoinRow=WidgetTree->ConstructWidget<UHorizontalBox>();Add(PrivatePanel,JoinRow,12);
    CodeInput=WidgetTree->ConstructWidget<UEditableTextBox>();CodeInput->SetHintText(LOCTEXT("Code","Enter room code"));
    auto Field=CodeInput->GetWidgetStyle();Field.TextStyle.Font.Size=22;Field.SetPadding(FMargin(18));Field.SetForegroundColor(Ink);
    Field.SetBackgroundImageNormal(FSlateRoundedBoxBrush(Surface,10.f,Muted,1.f));Field.SetBackgroundImageHovered(FSlateRoundedBoxBrush(Surface,10.f,Gold,1.f));Field.SetBackgroundImageFocused(FSlateRoundedBoxBrush(Surface,10.f,Gold,2.f));CodeInput->SetWidgetStyle(Field);
    JoinRow->AddChildToHorizontalBox(CodeInput)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    JoinButton=Button(LOCTEXT("Join","JOIN ROOM"));JoinRow->AddChildToHorizontalBox(JoinButton)->SetPadding(FMargin(16,0,0,0));JoinButton->OnClicked.AddDynamic(this,&UHzaLobbyScreen::Join);
    Add(ProfilePanel,Label(LOCTEXT("ProfileTitle","Player profile"),40),20);
    auto* ProfileCard=WidgetTree->ConstructWidget<UVerticalBox>();Add(ProfilePanel,Box(ProfileCard,FMargin(28)),24);
    ProfileName=Label(FText::GetEmpty(),36,Gold);Add(ProfileCard,ProfileName,16);ProfileDetails=Label(FText::GetEmpty(),21);Add(ProfileCard,ProfileDetails,22);
    ProfileXP=Label(FText::GetEmpty(),18,Muted);Add(ProfileCard,ProfileXP,12);
    auto* RefreshButton=Button(LOCTEXT("Refresh","Refresh profile"));Add(ProfilePanel,RefreshButton,16);RefreshButton->OnClicked.AddDynamic(this,&UHzaLobbyScreen::RefreshAccount);
    auto* Logout=Button(LOCTEXT("SignOut","Sign out"));Add(ProfilePanel,Logout,0);Logout->OnClicked.AddDynamic(this,&UHzaLobbyScreen::SignOut);
    Search=Panel(Content);Add(Search,Gap(26));SearchTitle=Label(LOCTEXT("SearchTitle","FINDING YOUR TABLE"),34);Add(Search,SearchTitle,14);SearchMode=Label(FText::GetEmpty(),18,Gold);Add(Search,SearchMode,22);
    auto* Spinner=WidgetTree->ConstructWidget<UCircularThrobber>();Spinner->SetNumberOfPieces(6);Spinner->SetRadius(24);Add(Search,Spinner,28);
    Add(Search,Label(LOCTEXT("SearchBody","Looking for the other players. Your table will appear here."),20,Muted),30);
    auto* CancelButton=Button(LOCTEXT("Cancel","Cancel search"));Add(Search,CancelButton);CancelButton->OnClicked.AddDynamic(this,&UHzaLobbyScreen::Cancel);
    Waiting=Panel(Content);Add(Waiting,Label(LOCTEXT("WaitingTitle","YOUR PRIVATE TABLE"),32),12);RoomText=Label(FText::GetEmpty(),18,Gold);Add(Waiting,RoomText,20);
    auto* SeatsGrid=WidgetTree->ConstructWidget<UUniformGridPanel>();SeatsGrid->SetSlotPadding(FMargin(6));Add(Waiting,SeatsGrid,18);
    for(int32 I=0;I<4;++I){auto* SeatLabel=Label(FText::GetEmpty(),18);SeatLabels.Add(SeatLabel);auto* SeatSize=WidgetTree->ConstructWidget<USizeBox>();SeatSize->SetMinDesiredHeight(92);SeatSize->AddChild(Box(SeatLabel,FMargin(14)));SeatsGrid->AddChildToUniformGrid(SeatSize,I/2,I%2);}
    ReadyButton=Button(LOCTEXT("Ready","Toggle ready"));Add(Waiting,ReadyButton);ReadyButton->OnClicked.AddDynamic(this,&UHzaLobbyScreen::Ready);
    StartButton=Button(LOCTEXT("Start","START MATCH"),true);Add(Waiting,StartButton,0);StartButton->OnClicked.AddDynamic(this,&UHzaLobbyScreen::StartMatch);
    ReconnectPanel=Panel(Content);Add(ReconnectPanel,Label(LOCTEXT("ReconnectTitle","Return to your table"),36),24);
    auto* Retry=Button(LOCTEXT("Reconnect","RECONNECT"),true);Add(ReconnectPanel,Retry);Retry->OnClicked.AddDynamic(this,&UHzaLobbyScreen::Reconnect);
    Notice=Label(FText::GetEmpty(),17,Ink);NoticePanel=Box(Notice,FMargin(12,7));Add(Root,NoticePanel,6);
    FooterStatus=Label(FText::GetEmpty(),12,Muted);Add(Root,FooterStatus,0);
    Refresh();
}
void UHzaLobbyScreen::NativeConstruct() { Super::NativeConstruct(); if(Lobby)Lobby->OnChanged.AddUniqueDynamic(this,&UHzaLobbyScreen::Refresh); Refresh(); }
void UHzaLobbyScreen::NativeDestruct() { if(Lobby)Lobby->OnChanged.RemoveAll(this); Super::NativeDestruct(); }
void UHzaLobbyScreen::Refresh()
{
    if(!Lobby || !Title)return;
    const auto Show=[](UWidget* Widget,bool Visible){Widget->SetVisibility(Visible?ESlateVisibility::Visible:ESlateVisibility::Collapsed);};
    Title->SetText(FText::FromString(Auth->Profile.Username));
    Account->SetText(FText::Format(LOCTEXT("HeaderLevel","LEVEL {0}"),FText::AsNumber(Auth->Profile.Level)));
    Avatar->SetText(FText::FromString(Auth->Profile.Username.Left(1).ToUpper()));
    CoinValue->SetText(Lobby->bAccountLoaded?FText::AsNumber(Lobby->Coins):LOCTEXT("Unknown","—"));
    RatingValue->SetText(Lobby->bAccountLoaded?FText::AsNumber(Lobby->Rating):LOCTEXT("Unknown","—"));
    ProfileName->SetText(FText::FromString(Auth->Profile.Username));ProfileDetails->SetText(Account->GetText());
    ProfileXP->SetText(Lobby->bAccountLoaded?FText::Format(LOCTEXT("TotalXP","{0} XP earned"),FText::AsNumber(Lobby->XP)):LOCTEXT("ProfilePending","Refresh to load your progress."));
    if(!Lobby->bPartyRequested)Lobby->RefreshParty();
    PartyStatus->SetText(!Lobby->bPartyLoaded?LOCTEXT("PartyUnknown","Party details unavailable."):Lobby->PartyMemberCount==0?LOCTEXT("NoParty","You aren't in a party yet."):FText::Format(LOCTEXT("PartyMembers","{0} players in your party"),FText::AsNumber(Lobby->PartyMemberCount)));
    FooterStatus->SetText(Lobby->bAccountLoaded?LOCTEXT("Online","ONLINE   ·   HZA KONKAN"):LOCTEXT("ConnectingFooter","CONNECTING   ·   HZA KONKAN"));
    const auto State=Lobby->State;
    if(State==EHzaLobbyState::Closed)Menu=0;
    Show(Home,State==EHzaLobbyState::Lobby||Menu==2);Show(PlayPanel,Menu==0);Show(PrivatePanel,Menu==1);Show(ProfilePanel,Menu==2);
    Show(Search,(State==EHzaLobbyState::Searching||State==EHzaLobbyState::JoiningRoom)&&Menu!=2);
    SearchTitle->SetText(State==EHzaLobbyState::JoiningRoom?LOCTEXT("JoiningTitle","JOINING YOUR TABLE"):LOCTEXT("SearchTitle","FINDING YOUR TABLE"));
    SearchMode->SetText(Lobby->Mode==TEXT("turbo")?LOCTEXT("TurboSearch","TURBO  ·  5 GAMES"):LOCTEXT("RegularSearch","REGULAR  ·  7 GAMES"));
    Show(Waiting,State==EHzaLobbyState::WaitingRoom&&Menu!=2); Show(ReconnectPanel,State==EHzaLobbyState::Reconnecting&&Menu!=2);
    PlayNav->SetStyle(MenuStyle(Menu==0));PrivateNav->SetStyle(MenuStyle(Menu==1));ProfileNav->SetStyle(MenuStyle(Menu==2));
    PrivateNav->SetIsEnabled(State==EHzaLobbyState::Lobby&&!Lobby->bBusy);
    RegularButton->SetStyle(MenuStyle(Lobby->Mode==TEXT("regular")));TurboButton->SetStyle(MenuStyle(Lobby->Mode==TEXT("turbo")));
    RegularMark->SetText(Lobby->Mode==TEXT("regular")?LOCTEXT("Selected","◆ SELECTED"):LOCTEXT("Classic","THE CLASSIC GAME"));
    TurboMark->SetText(Lobby->Mode==TEXT("turbo")?LOCTEXT("Selected","◆ SELECTED"):LOCTEXT("Quick","QUICK SESSION"));
    RegularButton->SetIsEnabled(!Lobby->bBusy); TurboButton->SetIsEnabled(!Lobby->bBusy);
    MatchButton->SetIsEnabled(!Lobby->bBusy); CreateButton->SetIsEnabled(!Lobby->bBusy); JoinButton->SetIsEnabled(!Lobby->bBusy);
    StartButton->SetIsEnabled(Lobby->CanStart());
    FText Message;
    if(State==EHzaLobbyState::JoiningRoom)Message=LOCTEXT("Joining","Joining your table…");
    else if(Lobby->bBusy)Message=LOCTEXT("Connecting","Connecting…");
    if(!Lobby->ErrorCode.IsEmpty())
    {
        if(Lobby->ErrorCode==TEXT("INVALID_ROOM")){if(Menu==1)Message=LOCTEXT("InvalidCode","Check the room code and try again.");}
        else if(Lobby->ErrorCode==TEXT("SEARCH_ENDED")){if(Menu==0)Message=LOCTEXT("Ended","Search cancelled. Choose a match when you are ready.");}
        else if(Lobby->ErrorCode==TEXT("SESSION_REPLACED"))Message=LOCTEXT("Replaced","Your account connected to this table from another session.");
        else if(Lobby->ErrorCode==TEXT("ROOM_ACTION_REJECTED"))Message=LOCTEXT("Rejected","The room has changed. Check the players and try again.");
        else if(Lobby->ErrorCode==TEXT("ACCOUNT_UNAVAILABLE"))Message=LOCTEXT("AccountUnavailable","Profile details are unavailable. Choose Refresh profile to retry.");
        else Message=LOCTEXT("ConnectionFailed","We could not complete the connection. Check your connection and try again.");
    }
    Notice->SetText(Message); Show(NoticePanel,!Message.IsEmpty());
    if(State==EHzaLobbyState::WaitingRoom)
    {
        RoomText->SetText(FText::Format(LOCTEXT("RoomCode","ROOM CODE   {0}"),FText::FromString(Lobby->RoomCode)));
        for(int32 I=0;I<4;++I)
        {
            const auto& Seats=Lobby->GetRoom().Seats;
            const FHzaSeat* Seat=Seats.IsValidIndex(I)?&Seats[I]:nullptr;
            SeatLabels[I]->SetText(FText::Format(LOCTEXT("SeatCard","TEAM {0} · SEAT {1}\n{2}\n{3}"),FText::FromString(I%2==0?TEXT("A"):TEXT("B")),FText::AsNumber(I+1),Seat&&!Seat->UserId.IsEmpty()?FText::FromString(Seat->Name):LOCTEXT("OpenSeat","Open seat"),Seat&&Seat->bReady?LOCTEXT("ReadyState","READY"):LOCTEXT("WaitingState","Waiting")));
        }
        Show(StartButton,Lobby->IsHost());
    }
}
void UHzaLobbyScreen::Regular(){Lobby->SelectMode(false);} void UHzaLobbyScreen::Turbo(){Lobby->SelectMode(true);}
void UHzaLobbyScreen::Find(){Lobby->FindMatch();} void UHzaLobbyScreen::Cancel(){Lobby->CancelSearch();}
void UHzaLobbyScreen::Create(){Lobby->CreatePrivateRoom();} void UHzaLobbyScreen::Join(){Lobby->JoinPrivateRoom(CodeInput->GetText().ToString());}
void UHzaLobbyScreen::Ready(){Lobby->ToggleReady();} void UHzaLobbyScreen::StartMatch(){Lobby->StartMatch();}
void UHzaLobbyScreen::Reconnect(){Lobby->Reconnect();} void UHzaLobbyScreen::RefreshAccount(){Lobby->RefreshAccount();}
void UHzaLobbyScreen::SignOut(){Auth->SignOut();}
void UHzaLobbyScreen::OpenPlay(){Menu=0;PageTime=0;Refresh();}
void UHzaLobbyScreen::OpenPrivate(){if(Lobby->State==EHzaLobbyState::Lobby){Menu=1;PageTime=0;Refresh();}}
void UHzaLobbyScreen::OpenProfile(){Menu=2;PageTime=0;Refresh();}
void UHzaLobbyScreen::NativeTick(const FGeometry& Geometry,float Dt)
{
    Super::NativeTick(Geometry,Dt);
    if(!TransitionArea||!Lobby)return;
    PageTime=FMath::Min(PageTime+Dt,.2f);const float Alpha=PageTime/.2f;
    TransitionArea->SetRenderOpacity(.35f+.65f*Alpha);TransitionArea->SetRenderTranslation(FVector2D((1-Alpha)*12,0));
    RegularScale=FMath::FInterpTo(RegularScale,RegularButton->IsHovered()?1.025f:Lobby->Mode==TEXT("regular")?1.012f:1.f,Dt,16.f);
    TurboScale=FMath::FInterpTo(TurboScale,TurboButton->IsHovered()?1.025f:Lobby->Mode==TEXT("turbo")?1.012f:1.f,Dt,16.f);
    RegularButton->SetRenderScale(FVector2D(RegularScale));TurboButton->SetRenderScale(FVector2D(TurboScale));
}
#undef LOCTEXT_NAMESPACE
