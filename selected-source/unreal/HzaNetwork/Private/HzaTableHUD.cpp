#include "HzaTableHUD.h"
#include "HzaTablePresentation.h"
#include "HzaHandInteractor.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SafeZone.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Brushes/SlateRoundedBoxBrush.h"

namespace
{
    const FLinearColor Navy(0.014f,0.025f,0.045f,0.94f), Gold(0.65f,0.43f,0.19f,1), Ink(0.92f,0.92f,0.87f,1);
}
void UHzaTableHUD::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    USafeZone* Safe=WidgetTree->ConstructWidget<USafeZone>(); WidgetTree->RootWidget=Safe;
    UCanvasPanel* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>(); Safe->AddChild(Canvas);
    Safe->SetVisibility(ESlateVisibility::SelfHitTestInvisible); Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    const auto Label=[&](int32 Size) {
        UTextBlock* Text=WidgetTree->ConstructWidget<UTextBlock>();
        FSlateFontInfo Font=Text->GetFont(); Font.Size=Size; Text->SetFont(Font);
        Text->SetColorAndOpacity(Ink); Text->SetVisibility(ESlateVisibility::HitTestInvisible); return Text;
    };
    const auto Position=[&](UWidget* Widget,FVector2D Anchor,FVector2D Align,FVector2D Offset,FVector2D Size) {
        auto* Slot=Canvas->AddChildToCanvas(Widget); Slot->SetAnchors(FAnchors(Anchor.X,Anchor.Y));
        Slot->SetAlignment(Align); Slot->SetPosition(Offset); Slot->SetSize(Size);
    };
    for(int32 Relative=0;Relative<4;++Relative)
    {
        UBorder* Border=WidgetTree->ConstructWidget<UBorder>();
        Border->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White,8.f)); Border->SetBrushColor(Navy); Border->SetPadding(FMargin(20,14));
        Border->SetVisibility(ESlateVisibility::HitTestInvisible);
        UVerticalBox* Box=WidgetTree->ConstructWidget<UVerticalBox>(); Border->AddChild(Box);
        auto* Name=Label(26); auto* Details=Label(20); Box->AddChildToVerticalBox(Name); Box->AddChildToVerticalBox(Details);
        Name->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
        Details->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
        SeatNames.Add(Name); SeatDetails.Add(Details); SeatBorders.Add(Border);
        if(Relative==0)Position(Border,{0,1},{0,1},{24,-24},{320,104});
        if(Relative==1)Position(Border,{1,0.42},{1,0.5},{-24,0},{300,104});
        if(Relative==2)Position(Border,{0.5,0},{0.5,0},{0,24},{340,104});
        if(Relative==3)Position(Border,{0,0.42},{0,0.5},{24,0},{300,104});
    }
    Status=Label(22); Position(Status,{0,0},{0,0},{24,24},{550,36});
    Score=Label(24); Position(Score,{1,0},{1,0},{-24,24},{480,38}); Score->SetJustification(ETextJustify::Right);
    Phase=Label(26); Position(Phase,{0.5,1},{0.5,1},{0,-132},{720,40}); Phase->SetJustification(ETextJustify::Center);
    Notice=Label(24); Position(Notice,{0.5,0},{0.5,0},{0,145},{1050,40}); Notice->SetJustification(ETextJustify::Center); Notice->SetColorAndOpacity(Gold);
    History=Label(20); Position(History,{1,1},{1,1},{-24,-24},{320,96}); History->SetAutoWrapText(true); History->SetJustification(ETextJustify::Right);
    UHorizontalBox* Actions=WidgetTree->ConstructWidget<UHorizontalBox>(); Position(Actions,{0.5,1},{0.5,1},{0,-24},{700,96});
    const auto Button=[&](const TCHAR* Caption) {
        UButton* B=WidgetTree->ConstructWidget<UButton>();
        FButtonStyle Style=B->GetStyle();
        Style.SetNormal(FSlateRoundedBoxBrush(Navy,10.f,Gold,1.f));
        Style.SetHovered(FSlateRoundedBoxBrush(FLinearColor(0.04f,0.07f,0.11f,1),10.f,Gold,2.f));
        Style.SetPressed(FSlateRoundedBoxBrush(FLinearColor(0.13f,0.08f,0.04f,1),10.f,Gold,2.f));
        Style.SetDisabled(FSlateRoundedBoxBrush(FLinearColor(0.02f,0.025f,0.035f,0.75f),10.f,FLinearColor(0.16f,0.14f,0.12f,1),1.f));
        B->SetStyle(Style);
        UTextBlock* Text=Label(24); Text->SetText(FText::FromString(Caption)); Text->SetColorAndOpacity(Gold); B->AddChild(Text);
        auto* Slot=Actions->AddChildToHorizontalBox(B); Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); Slot->SetPadding(FMargin(6,0)); return B;
    };
    DrawButton=Button(TEXT("DRAW")); DrawButton->OnClicked.AddDynamic(this,&UHzaTableHUD::Draw);
    TakeButton=Button(TEXT("TAKE")); TakeButton->OnClicked.AddDynamic(this,&UHzaTableHUD::Take);
    DiscardButton=Button(TEXT("DISCARD")); DiscardButton->OnClicked.AddDynamic(this,&UHzaTableHUD::Discard);
    NextButton=Button(TEXT("NEXT ROUND")); NextButton->OnClicked.AddDynamic(this,&UHzaTableHUD::Next);
    Refresh();
}
void UHzaTableHUD::Refresh()
{
    if (!Table || !Status) return;
    const FHzaRoomSnapshot& View=Table->View;
    const bool Live=View.Version>=0;
    Status->SetText(FText::FromString(TEXT("HZA KONKAN  /  ")+Table->Connection));
    Score->SetText(FText::FromString(Live?FString::Printf(TEXT("TEAM A  %d  :  %d  TEAM B"),Table->ScoreA,Table->ScoreB):TEXT("TABLE PREVIEW")));
    for(int32 Relative=0;Relative<4;++Relative)
    {
        const int32 Seat=Live?(View.OwnSeat+Relative)%4:Relative;
        const FHzaSeat* Member=View.Seats.IsValidIndex(Seat)?&View.Seats[Seat]:nullptr;
        const bool Active=Live&&View.Status==TEXT("playing")&&View.Turn==Seat;
        SeatNames[Relative]->SetText(FText::FromString(Member&&!Member->Name.IsEmpty()?Member->Name:Relative==0?TEXT("Your seat"):TEXT("Waiting for player")));
        SeatDetails[Relative]->SetText(FText::FromString(FString::Printf(TEXT("TEAM %s  /  %s"),Seat%2==0?TEXT("A"):TEXT("B"),Member?*FString::Printf(TEXT("%d tiles%s"),Member->TileCount,Member->bBot?TEXT("  /  BOT"):TEXT("")):TEXT("Offline"))));
        SeatBorders[Relative]->SetBrushColor(Active?FLinearColor(0.18f,0.10f,0.036f,0.96f):Navy);
        SeatNames[Relative]->SetColorAndOpacity(Active?Gold:Ink);
    }
    FString PhaseText=Live?FString::Printf(TEXT("ROUND %d/%d   %s   %ds"),View.Round,View.RoundCount,*View.Phase.ToUpper(),FMath::CeilToInt(Table->RemainingSeconds())):TEXT("Connect to a room to play");
    if(Live&&View.ServerNowUnixMs<=0)PhaseText=TEXT("Waiting for server clock");
    if(Table->IsDealing())PhaseText=TEXT("DEALING");
    if(!Table->RoundSummary.IsEmpty())PhaseText=TEXT("ROUND COMPLETE");
    Phase->SetText(FText::FromString(PhaseText));
    Notice->SetText(FText::FromString(Table->Feedback.IsEmpty()?Table->RoundSummary:Table->Feedback));
    History->SetText(FText::FromString(Table->DiscardHistory.IsEmpty()?TEXT(""):TEXT("DISCARDS  ")+FString::Join(Table->DiscardHistory,TEXT(" / "))));
    DrawButton->SetIsEnabled(Table->CanAct()&&View.Phase==TEXT("draw"));
    TakeButton->SetIsEnabled(Table->CanAct()&&View.Phase==TEXT("draw")&&!Table->DiscardHistory.IsEmpty());
    DiscardButton->SetIsEnabled(Table->CanAct()&&View.Phase==TEXT("play")&&Table->Hand&&Table->Hand->SelectedTile);
    NextButton->SetVisibility(View.Status==TEXT("roundEnd")?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
}
void UHzaTableHUD::Draw(){if(Table)Table->DrawStock();}
void UHzaTableHUD::Take(){if(Table)Table->DrawDiscard();}
void UHzaTableHUD::Discard(){if(Table)Table->DiscardSelected();}
void UHzaTableHUD::Next(){if(Table)Table->NextRound();}
