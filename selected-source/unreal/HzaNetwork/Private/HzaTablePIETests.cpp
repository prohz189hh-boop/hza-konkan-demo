#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "HzaTablePresentation.h"
#include "HzaHandInteractor.h"
#include "HzaTableHUD.h"
#include "Components/Widget.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "Tests/AutomationEditorCommon.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "GameFramework/PlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHzaTablePIETest,"HZA.Live.TablePresentation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHzaTablePIETest::RunTest(const FString& Parameters)
{
    if(Parameters!=TEXT("Ready"))
    {
        if(GEditor->GetEditorWorldContext().World()->GetOutermost()->GetName()!=TEXT("/Game/HZA/Test/L_Konkan_Test"))
        {AddError(TEXT("Load L_Konkan_Test first"));return false;}
        FRequestPlaySessionParams Params; Params.WorldType=EPlaySessionWorldType::PlayInEditor;
        ADD_LATENT_AUTOMATION_COMMAND(FStartPIEForAutomationCommand(Params));
        ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this](){RunTest(TEXT("Ready"));return true;}));
        ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
    }
    AHzaTablePresentation* Table=nullptr;
    for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::PIE)
        for(TActorIterator<AHzaTablePresentation> It(Context.World());It;++It){Table=*It;break;}
    if(!TestNotNull(TEXT("Installed table presenter"),Table)||!TestNotNull(TEXT("Existing hand"),Table->Hand.Get()))return false;
    bool VisibleHUD=false;
    for(TObjectIterator<UHzaTableHUD> It;It;++It)if(It->Table==Table)
        VisibleHUD=It->IsInViewport()&&It->GetRootWidget()&&It->GetRootWidget()->GetCachedWidget().IsValid();
    TestTrue(TEXT("HUD root was constructed before Slate rebuild"),VisibleHUD);
    auto Room=MakeShared<FJsonObject>(); auto Game=MakeShared<FJsonObject>();
    Room->SetStringField(TEXT("id"),TEXT("presentation-fixture"));Room->SetStringField(TEXT("code"),TEXT("TEST"));
    Room->SetStringField(TEXT("status"),TEXT("playing"));Room->SetNumberField(TEXT("version"),1);
    // Deliberately skew the server clock by a day; local wall time must not affect countdown.
    const double ServerNow=(FDateTime::UtcNow().ToUnixTimestampDecimal()+86400)*1000;
    Room->SetNumberField(TEXT("serverNow"),ServerNow);
    Room->SetNumberField(TEXT("deadline"),ServerNow+30000);
    TArray<TSharedPtr<FJsonValue>> Seats,Players,Own;
    for(int32 i=0;i<15;++i){auto Tile=MakeShared<FJsonObject>();Tile->SetStringField(TEXT("id"),FString::Printf(TEXT("red-%d-%d"),i%13+1,i/13));Tile->SetStringField(TEXT("color"),TEXT("red"));Tile->SetNumberField(TEXT("n"),i%13+1);Own.Add(MakeShared<FJsonValueObject>(Tile));}
    for(int32 i=0;i<4;++i){auto Seat=MakeShared<FJsonObject>();Seat->SetStringField(TEXT("id"),FString::FromInt(i));Seat->SetStringField(TEXT("name"),FString::Printf(TEXT("Fixture seat %d"),i+1));Seats.Add(MakeShared<FJsonValueObject>(Seat));auto Player=MakeShared<FJsonObject>();Player->SetArrayField(TEXT("hand"),i==0?Own:TArray<TSharedPtr<FJsonValue>>{});Player->SetNumberField(TEXT("tileCount"),i==0?15:14);Players.Add(MakeShared<FJsonValueObject>(Player));}
    Room->SetArrayField(TEXT("seats"),Seats);Room->SetObjectField(TEXT("game"),Game);
    Game->SetArrayField(TEXT("players"),Players);Game->SetNumberField(TEXT("seat"),0);Game->SetNumberField(TEXT("turn"),0);
    Game->SetNumberField(TEXT("deckCount"),48);Game->SetStringField(TEXT("phase"),TEXT("play"));Game->SetNumberField(TEXT("round"),1);Game->SetNumberField(TEXT("rounds"),7);
    Game->SetArrayField(TEXT("scores"),{MakeShared<FJsonValueNumber>(0),MakeShared<FJsonValueNumber>(0)});
    FHzaRoomSnapshot Snapshot;
    if(!TestTrue(TEXT("Fixture passes private-hand parser"),UHzaRoomClient::ParseSnapshot(Room,0,Snapshot)))return false;
    Table->ApplySnapshot(Snapshot);Table->Tick(1);Table->Hand->Tick(1);
    TestEqual(TEXT("Fifteen authoritative hand actors"),Table->Hand->HandTiles.Num(),15);
    TSet<AActor*> Original;for(AActor* Tile:Table->Hand->HandTiles)Original.Add(Tile);
    AActor* Dragged=Table->Hand->HandTiles[0];
    FVector2D From,To;
    APlayerController* PC=Table->GetWorld()->GetFirstPlayerController();
    if(!TestNotNull(TEXT("Presentation local controller"),PC))return false;
    PC->ProjectWorldLocationToScreen(Dragged->GetActorLocation(),From);
    PC->ProjectWorldLocationToScreen(Table->Hand->GetSlotPosition(Table->Hand->HandTiles[4]),To);
    Table->Hand->BeginGesture(From,true);Table->Hand->UpdateGesture(To);
    TestTrue(TEXT("Begin real native gesture on authoritative tile"),Table->Hand->bDragging);
    Table->ApplySnapshot(Snapshot);
    TestTrue(TEXT("Same-version presence snapshot preserves an active drag"),Table->Hand->bDragging);
    Table->Hand->EndGesture(To);
    TestEqual(TEXT("Drag can finish after repeated snapshot"),Table->Hand->Slots[4].Get(),Dragged);
    Table->ApplySnapshot(Snapshot);
    TestEqual(TEXT("Repeated snapshot preserves reordered private slot"),Table->Hand->Slots[4].Get(),Dragged);
    TestEqual(TEXT("Duplicate snapshot does not duplicate actors"),Table->Hand->HandTiles.Num(),15);
    for(AActor* Tile:Table->Hand->HandTiles)TestTrue(TEXT("Duplicate snapshot keeps tile identity"),Original.Contains(Tile));
    int32 Backs=0;for(TActorIterator<AStaticMeshActor> It(Table->GetWorld());It;++It)
        if(!It->IsHidden()&&It->GetStaticMeshComponent()->GetStaticMesh()&&It->GetStaticMeshComponent()->GetStaticMesh()->GetName()==TEXT("SM_HZA_Tile_Hidden"))++Backs;
    TestEqual(TEXT("Opponent rendering uses exactly 42 backs"),Backs,42);
    TestTrue(TEXT("Timer reads server deadline"),Table->RemainingSeconds()>0&&Table->RemainingSeconds()<=30);
    Snapshot.ReceivedAtSeconds-=3;
    Table->ApplySnapshot(Snapshot);
    TestTrue(TEXT("Monotonic elapsed time advances server countdown"),Table->RemainingSeconds()>25&&Table->RemainingSeconds()<=27);
    Snapshot.ServerNowUnixMs=0;Table->ApplySnapshot(Snapshot);
    TestEqual(TEXT("Missing server clock never invents time"),Table->RemainingSeconds(),0.0);
    TestFalse(TEXT("Missing server clock disables actions"),Table->CanAct());
    auto Discard=MakeShared<FJsonObject>();Discard->SetObjectField(TEXT("tile"),Own.Last()->AsObject());
    Game->SetArrayField(TEXT("discards"),{MakeShared<FJsonValueObject>(Discard)});Own.Pop();
    Players[0]->AsObject()->SetArrayField(TEXT("hand"),Own);Players[0]->AsObject()->SetNumberField(TEXT("tileCount"),14);
    Room->SetNumberField(TEXT("version"),2);
    if(!TestTrue(TEXT("Discard fixture validates"),UHzaRoomClient::ParseSnapshot(Room,0,Snapshot)))return false;
    Table->ApplySnapshot(Snapshot);
    TestEqual(TEXT("Discard removes private ownership"),Table->Hand->HandTiles.Num(),14);
    TestEqual(TEXT("Public discard appears"),Table->PublicTileCount,1);
    Room->SetStringField(TEXT("status"),TEXT("roundEnd"));Room->SetNumberField(TEXT("version"),3);
    Game->SetNumberField(TEXT("roundPoints"),100);Game->SetStringField(TEXT("reason"),TEXT("finish"));
    Game->SetArrayField(TEXT("scores"),{MakeShared<FJsonValueNumber>(100),MakeShared<FJsonValueNumber>(0)});
    UHzaRoomClient::ParseSnapshot(Room,0,Snapshot);Table->ApplySnapshot(Snapshot);
    TestEqual(TEXT("Scores come from authoritative snapshot"),Table->ScoreA,100);
    TestFalse(TEXT("Round end disables turn actions"),Table->CanAct());
    TestFalse(TEXT("Round result is presented"),Table->RoundSummary.IsEmpty());
    return true;
}
#endif
