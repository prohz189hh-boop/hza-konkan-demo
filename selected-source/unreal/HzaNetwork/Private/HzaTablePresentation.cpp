#include "HzaTablePresentation.h"
#include "HzaHandInteractor.h"
#include "HzaTableHUD.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    bool FaceFromJson(const TSharedPtr<FJsonObject>& Json,FHzaTile& Face)
    {
        double Number;
        if (!Json || !Json->TryGetStringField(TEXT("id"),Face.Id) || Face.Id.IsEmpty() ||
            !Json->TryGetStringField(TEXT("color"),Face.Suit) || !Json->TryGetNumberField(TEXT("n"),Number) ||
            !FMath::IsFinite(Number) || FMath::FloorToDouble(Number)!=Number || Number<0 || Number>13) return false;
        Json->TryGetBoolField(TEXT("antique"),Face.bAntique); Face.Number=int32(Number);
        return Face.bAntique || ((Face.Suit==TEXT("red") || Face.Suit==TEXT("black") || Face.Suit==TEXT("blue") || Face.Suit==TEXT("yellow")) && Face.Number>=1);
    }
    TSharedRef<FJsonObject> Action(const TCHAR* Type)
    { auto Json=MakeShared<FJsonObject>(); Json->SetStringField(TEXT("type"),Type); return Json; }
}
AHzaTablePresentation::AHzaTablePresentation() { PrimaryActorTick.bCanEverTick=true; bReplicates=false; }
void AHzaTablePresentation::BeginPlay()
{
    Super::BeginPlay();
    if (!Hand) for (TActorIterator<AHzaHandInteractor> It(GetWorld());It;++It) { Hand=*It; break; }
    Client=GetGameInstance()->GetSubsystem<UHzaRoomClient>();
    if (Client)
    {
        Client->OnSnapshot.AddDynamic(this,&AHzaTablePresentation::ApplySnapshot);
        Client->OnConnection.AddDynamic(this,&AHzaTablePresentation::ConnectionChanged);
    }
    APlayerController* PC=GetWorld()->GetFirstPlayerController();
    if (PC && PC->IsLocalController())
    {
        HUD=CreateWidget<UHzaTableHUD>(PC,UHzaTableHUD::StaticClass());
        HUD->Table=this; HUD->AddToViewport();
        HUD->SetVisibility(bFrontendVisible?ESlateVisibility::Collapsed:ESlateVisibility::Visible);
    }
    if (Client && Client->Snapshot.Version>=0) ApplySnapshot(Client->Snapshot);
}
void AHzaTablePresentation::SetFrontendVisible(bool bVisible)
{
    bFrontendVisible=bVisible;
    if(HUD)HUD->SetVisibility(bVisible?ESlateVisibility::Collapsed:ESlateVisibility::Visible);
}
void AHzaTablePresentation::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Client) { Client->OnSnapshot.RemoveAll(this); Client->OnConnection.RemoveAll(this); }
    if (HUD) HUD->RemoveFromParent();
    Super::EndPlay(Reason);
}
FVector AHzaTablePresentation::SeatPosition(int32 Seat,int32 Index) const
{
    return FRotator(0,90*Seat,0).RotateVector(FVector((7-Index%15)*3.3,-51+(Index/15)*4,83+(Index/15)*3.8));
}
AActor* AHzaTablePresentation::TileActor(const FString& Key,const FHzaTile* Face)
{
    if (const auto* Existing=TileActors.Find(Key)) return Existing->Get();
    const FString FaceKey=!Face ? TEXT("Hidden") : Face->bAntique ? TEXT("antique") : FString::Printf(TEXT("%s_%02d"),*Face->Suit,Face->Number);
    UStaticMesh* Mesh=Meshes.FindRef(FaceKey);
    if (!Mesh)
    {
        const FString Name=TEXT("SM_HZA_Tile_")+FaceKey;
        const FString Path=TEXT("/Game/HZA/Revamp/Tiles/HZA_Tiles_Revamp/StaticMeshes/")+Name+TEXT(".")+Name;
        Mesh=LoadObject<UStaticMesh>(nullptr,*Path);
        if (!Mesh) { Feedback=TEXT("Tile artwork unavailable"); return nullptr; }
        Meshes.Add(FaceKey,Mesh);
    }
    AStaticMeshActor* Tile=GetWorld()->SpawnActor<AStaticMeshActor>();
    if (!Tile) return nullptr;
    Tile->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
    Tile->GetStaticMeshComponent()->SetStaticMesh(Mesh);
    Tile->SetActorEnableCollision(false);
    Tile->SetActorLocation(FVector(-14,0,84));
    Tile->SetActorRotation(FRotator(0,180,0));
    TileActors.Add(Key,Tile); return Tile;
}
void AHzaTablePresentation::Place(AActor* Tile,const FTransform& Target,bool Animate,float Delay)
{
    if (!Tile) return;
    Flights.RemoveAll([Tile](const FHzaTileFlight& Flight) { return Flight.Actor.Get()==Tile; });
    if (!Animate || Tile->GetActorTransform().Equals(Target,0.01)) { Tile->SetActorTransform(Target); return; }
    FHzaTileFlight Flight;
    Flight.Actor=Tile; Flight.Start=Tile->GetActorTransform(); Flight.End=Target;
    Flight.Started=GetWorld()->GetTimeSeconds()+Delay; Flights.Add(Flight);
}
void AHzaTablePresentation::ApplySnapshot(const FHzaRoomSnapshot& Snapshot)
{
    if (!Hand || Snapshot.OwnSeat<0 || Snapshot.OwnSeat>3 || Snapshot.Seats.Num()!=4 || Snapshot.OwnHand.Num()>30) return;
    if (View.RoomId==Snapshot.RoomId && Snapshot.Version<View.Version) return;
    TSharedPtr<FJsonObject> Room;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Snapshot.RoomJson),Room) || !Room) return;
    const TSharedPtr<FJsonObject>* Game;
    const bool HasGame=Room->TryGetObjectField(TEXT("game"),Game);
    const bool NewDeal=View.RoomId!=Snapshot.RoomId || View.Round!=Snapshot.Round ||
        (HasGame && View.Status==TEXT("roundEnd") && Snapshot.Status==TEXT("playing"));
    const bool Initial=View.Version<0 || View.RoomId!=Snapshot.RoomId;
    const bool Changed=Initial || Snapshot.Version!=View.Version || Snapshot.Status!=View.Status || NewDeal || Snapshot.OwnSeat!=View.OwnSeat;
    View=Snapshot; bAwaitingReply=false; Connection=TEXT("Synchronized");
    DealUntilMs=0;
    Room->TryGetNumberField(TEXT("dealUntil"),DealUntilMs);
    // Presence/chat/reconnect acknowledgements can repeat a game version. Do not
    // interrupt a private drag or restart an in-flight animation for those packets.
    if (!Changed) { if(HUD)HUD->Refresh(); return; }
    double DealStarted=0; Room->TryGetNumberField(TEXT("dealStartedAt"),DealStarted);
    const double Now=ServerNowMs();
    const bool Deal=HasGame && NewDeal && IsDealing();
    const bool Animate=Changed && (!Initial || Deal);
    if (Initial) for (AActor* Demo : DemoActors) if (IsValid(Demo)) { Demo->SetActorHiddenInGame(true); Demo->SetActorEnableCollision(false); }
    if (NewDeal) { Flights.Reset(); PreviousOwnIds.Reset(); }
    Hand->CancelGesture();
    TSet<FString> Wanted; TArray<AActor*> OwnActors;
    for (const FHzaTile& Tile : Snapshot.OwnHand)
    {
        const FString Key=TEXT("tile:")+Tile.Id; Wanted.Add(Key);
        if (AActor* Actor=TileActor(Key,&Tile)) OwnActors.Add(Actor);
    }
    Hand->SynchronizeTiles(OwnActors,NewDeal);
    TSet<FString> NextOwnIds;
    for (int32 i=0;i<Snapshot.OwnHand.Num();++i)
    {
        const FHzaTile& Tile=Snapshot.OwnHand[i]; NextOwnIds.Add(Tile.Id);
        AActor* Actor=TileActors.FindRef(TEXT("tile:")+Tile.Id);
        if (!Actor) continue;
        if (NewDeal) Actor->SetActorLocation(FVector(-14,0,84));
        if (Deal || !PreviousOwnIds.Contains(Tile.Id))
        {
            float Delay=Deal ? FMath::Max(0.0,DealStarted+((i*4+Snapshot.OwnSeat)*55)-Now)/1000.f : 0;
            Delay=FMath::Min(Delay,float(FMath::Max(0.0,DealUntilMs-Now-300)/1000));
            Place(Actor,FTransform(FRotator(0,180,0),Hand->GetSlotPosition(Actor)),Animate,Delay);
        }
    }
    PreviousOwnIds=MoveTemp(NextOwnIds);
    for (int32 Seat=0;Seat<4;++Seat) if (Seat!=Snapshot.OwnSeat)
    {
        const int32 Relative=(Seat-Snapshot.OwnSeat+4)%4;
        for (int32 i=0;i<FMath::Min(Snapshot.Seats[Seat].TileCount,30);++i)
        {
            const FString Key=FString::Printf(TEXT("back:%d:%d"),Seat,i); Wanted.Add(Key);
            AActor* Actor=TileActor(Key,nullptr);
            if (Actor && NewDeal) Actor->SetActorLocation(FVector(-14,0,84));
            const float Delay=Deal ? float(FMath::Clamp((DealStarted+(i*4+Seat)*55-Now)/1000,0.0,FMath::Max(0.0,(DealUntilMs-Now-300)/1000))) : 0;
            Place(Actor,FTransform(FRotator(0,180+Relative*90,0),SeatPosition(Relative,i)),Animate,Delay);
        }
    }
    DiscardHistory.Reset(); RoundSummary.Empty(); PublicTileCount=0; ScoreA=0; ScoreB=0;
    if (HasGame)
    {
        const TArray<TSharedPtr<FJsonValue>>* Scores;
        if ((*Game)->TryGetArrayField(TEXT("scores"),Scores) && Scores->Num()==2)
        { ScoreA=int32((*Scores)[0]->AsNumber()); ScoreB=int32((*Scores)[1]->AsNumber()); }
        const TArray<TSharedPtr<FJsonValue>>* Discards;
        if ((*Game)->TryGetArrayField(TEXT("discards"),Discards))
            for (int32 i=0;i<Discards->Num();++i)
            {
                const TSharedPtr<FJsonObject>* Item; const TSharedPtr<FJsonObject>* TileJson; FHzaTile Tile;
                if (!(*Discards)[i]->TryGetObject(Item) || !(*Item)->TryGetObjectField(TEXT("tile"),TileJson) || !FaceFromJson(*TileJson,Tile)) continue;
                if (i>=Discards->Num()-5) DiscardHistory.Add(Tile.bAntique?TEXT("Antique"):FString::Printf(TEXT("%s %d"),*Tile.Suit,Tile.Number));
                if (i!=Discards->Num()-1) continue;
                const FString Key=TEXT("tile:")+Tile.Id; Wanted.Add(Key); ++PublicTileCount;
                Place(TileActor(Key,&Tile),FTransform(FRotator(0,180,90),FVector(14,0,79.5)),Animate);
            }
        const TArray<TSharedPtr<FJsonValue>>* Melds;
        if ((*Game)->TryGetArrayField(TEXT("melds"),Melds))
            for (int32 Row=0;Row<Melds->Num();++Row)
            {
                const TSharedPtr<FJsonObject>* Meld; const TArray<TSharedPtr<FJsonValue>>* Tiles;
                if (!(*Melds)[Row]->TryGetObject(Meld) || !(*Meld)->TryGetArrayField(TEXT("tiles"),Tiles)) continue;
                for (int32 i=0;i<Tiles->Num();++i)
                {
                    const TSharedPtr<FJsonObject>* TileJson; FHzaTile Tile;
                    if (!(*Tiles)[i]->TryGetObject(TileJson) || !FaceFromJson(*TileJson,Tile)) continue;
                    const FString Key=TEXT("tile:")+Tile.Id; Wanted.Add(Key); ++PublicTileCount;
                    const FVector Position(32-(i%10)*3.1,-20+(Row%7)*6,79.5+(Row/7)*0.3);
                    Place(TileActor(Key,&Tile),FTransform(FRotator(0,180,90),Position),Animate);
                }
            }
        if (Snapshot.Status==TEXT("roundEnd") || Snapshot.Status==TEXT("finished"))
        {
            FString Reason; double Points=0;
            (*Game)->TryGetStringField(TEXT("reason"),Reason); (*Game)->TryGetNumberField(TEXT("roundPoints"),Points);
            RoundSummary=FString::Printf(TEXT("%s  |  %d points  |  %s"),Snapshot.Status==TEXT("finished")?TEXT("Match complete"):TEXT("Round complete"),int32(Points),*Reason);
        }
    }
    for (auto It=TileActors.CreateIterator();It;++It) if (!Wanted.Contains(It.Key()))
    {
        if (AActor* Actor=It.Value()) { Flights.RemoveAll([Actor](const FHzaTileFlight& F){return F.Actor.Get()==Actor;}); Actor->Destroy(); }
        It.RemoveCurrent();
    }
    Hand->SetPresentationLocked(!Flights.IsEmpty());
    if (HUD) HUD->Refresh();
}
void AHzaTablePresentation::Tick(float Dt)
{
    Super::Tick(Dt);
    const double Now=GetWorld()->GetTimeSeconds();
    for (int32 i=Flights.Num()-1;i>=0;--i)
    {
        const FHzaTileFlight& F=Flights[i]; AActor* Tile=F.Actor.Get();
        if (!Tile) { Flights.RemoveAtSwap(i); continue; }
        const float Alpha=FMath::Clamp(float((Now-F.Started)/F.Duration),0.f,1.f);
        const float Ease=Alpha*Alpha*(3-2*Alpha);
        FVector Position=FMath::Lerp(F.Start.GetLocation(),F.End.GetLocation(),Ease);
        Position.Z+=FMath::Sin(Alpha*PI)*3;
        Tile->SetActorLocationAndRotation(Position,FQuat::Slerp(F.Start.GetRotation(),F.End.GetRotation(),Ease));
        if (Alpha>=1) { Tile->SetActorTransform(F.End); Flights.RemoveAtSwap(i); }
    }
    if (Hand) Hand->SetPresentationLocked(!Flights.IsEmpty());
    if (FeedbackUntil>0 && Now>=FeedbackUntil) { Feedback.Empty(); FeedbackUntil=0; }
    if (HUD) HUD->Refresh();
}
void AHzaTablePresentation::ConnectionChanged(const FString& State,const FString& Code)
{
    if (State==TEXT("ActionRejected"))
    { Feedback=TEXT("Move rejected: ")+Code; FeedbackUntil=GetWorld()->GetTimeSeconds()+5; if(Hand)Hand->CancelGesture(); }
    else { Connection=State; bAwaitingReply=false; }
}
double AHzaTablePresentation::ServerNowMs() const
{
    return View.ServerNowUnixMs>0 ? View.ServerNowUnixMs+FMath::Max(0.0,FPlatformTime::Seconds()-View.ReceivedAtSeconds)*1000 : 0;
}
double AHzaTablePresentation::RemainingSeconds() const
{ return View.ServerNowUnixMs>0 ? FMath::Max(0.0,(View.DeadlineUnixMs-ServerNowMs())/1000) : 0; }
bool AHzaTablePresentation::IsDealing() const
{ return View.ServerNowUnixMs>0 && DealUntilMs>ServerNowMs(); }
bool AHzaTablePresentation::CanAct() const
{
    return Client && Connection==TEXT("Synchronized") && !bAwaitingReply && Flights.IsEmpty() &&
        View.Status==TEXT("playing") && View.OwnSeat==View.Turn && !IsDealing() && RemainingSeconds()>0;
}
void AHzaTablePresentation::SendAction(const TSharedRef<FJsonObject>& Json)
{
    if (!Client) return;
    FString Text; FJsonSerializer::Serialize(Json,TJsonWriterFactory<>::Create(&Text));
    if (Client->SendMove(Text)) { bAwaitingReply=true; Feedback=TEXT("Waiting for server"); FeedbackUntil=GetWorld()->GetTimeSeconds()+5; }
    else { Feedback=TEXT("Synchronizing table"); FeedbackUntil=GetWorld()->GetTimeSeconds()+3; }
}
void AHzaTablePresentation::DrawStock() { if(CanAct()&&View.Phase==TEXT("draw")) {auto J=Action(TEXT("draw"));J->SetStringField(TEXT("source"),TEXT("deck"));SendAction(J);} }
void AHzaTablePresentation::DrawDiscard() { if(CanAct()&&View.Phase==TEXT("draw")) {auto J=Action(TEXT("draw"));J->SetStringField(TEXT("source"),TEXT("discard"));SendAction(J);} }
void AHzaTablePresentation::DiscardSelected()
{
    if (!CanAct() || View.Phase!=TEXT("play") || !Hand || !Hand->SelectedTile) return;
    for (const FHzaTile& Tile : View.OwnHand) if (TileActors.FindRef(TEXT("tile:")+Tile.Id)==Hand->SelectedTile)
    {auto J=Action(TEXT("discard"));J->SetStringField(TEXT("tile"),Tile.Id);SendAction(J);return;}
}
void AHzaTablePresentation::NextRound()
{ if(Client&&!bAwaitingReply&&Connection==TEXT("Synchronized")&&View.Status==TEXT("roundEnd"))SendAction(Action(TEXT("next"))); }
