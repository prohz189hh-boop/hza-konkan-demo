#if WITH_DEV_AUTOMATION_TESTS
#include "HzaHandInteractor.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHzaRackPlacementTest,"HZA.Hand.RackPlacement",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHzaRackPlacementTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    if (!TestNotNull(TEXT("Temporary world"),World)) return false;
    AHzaHandInteractor* Hand=World->SpawnActor<AHzaHandInteractor>();
    Hand->Slots.SetNum(30);
    TArray<AActor*> Tiles;
    for (int32 i=0;i<15;++i) { AActor* Tile=World->SpawnActor<AActor>(); Tiles.Add(Tile); Hand->Slots[i]=Tile; }
    Hand->PressedTile=Tiles[0];
    TestTrue(TEXT("Move to empty upper-row edge"),Hand->BuildPreview(29));
    TestTrue(TEXT("Destination retains same physical tile"),Hand->Preview[29]==Tiles[0]);
    TestNull(TEXT("Source vacated"),Hand->Preview[0].Get());
    TestTrue(TEXT("Preview does not mutate committed order"),Hand->Slots[0]==Tiles[0]);
    Hand->CancelGesture();
    TestTrue(TEXT("Cancel restores original slot"),Hand->Preview[0]==Tiles[0]);
    Hand->PressedTile=Tiles[0];
    TestTrue(TEXT("Insert at occupied end of full lower row"),Hand->BuildPreview(14));
    TestTrue(TEXT("Insertion destination"),Hand->Preview[14]==Tiles[0]);
    TestTrue(TEXT("Neighbor shifts toward vacated source"),Hand->Preview[0]==Tiles[1]);
    TSet<AActor*> Seen;
    int32 Count=0;
    for (AActor* Tile : Hand->Preview) if(Tile) { ++Count; Seen.Add(Tile); }
    TestEqual(TEXT("Tile count preserved"),Count,15);
    TestEqual(TEXT("No duplicated physical actors"),Seen.Num(),15);
    TestFalse(TEXT("Out-of-rack drop rejected"),Hand->BuildPreview(-1));
    TestTrue(TEXT("Invalid drop keeps original hand"),Hand->Slots[0]==Tiles[0]);
    Hand->Select(Tiles[3]); TestTrue(TEXT("Select"),Hand->SelectedTile==Tiles[3]);
    Hand->Select(Tiles[3]); TestNull(TEXT("Deselect"),Hand->SelectedTile.Get());
    // An authoritative draw/discard updates ownership without resetting local order.
    Hand->PressedTile=Tiles[0]; Hand->BuildPreview(14); Hand->Slots=Hand->Preview;
    TArray<AActor*> Updated=Tiles; Updated.Remove(Tiles[5]);
    AActor* Drawn=World->SpawnActor<AActor>(); Updated.Add(Drawn);
    Hand->SelectedTile=Tiles[5]; Hand->SynchronizeTiles(Updated);
    TestTrue(TEXT("Snapshot keeps privately reordered last tile"),Hand->Slots[14]==Tiles[0]);
    TestTrue(TEXT("Draw fills vacated position"),Hand->Slots[4]==Drawn);
    TestFalse(TEXT("Discarded tile loses rack ownership"),Hand->Slots.Contains(Tiles[5]));
    TestNull(TEXT("Removed tile loses selection"),Hand->SelectedTile.Get());
    Updated.Add(Drawn); Hand->SynchronizeTiles(Updated);
    TestEqual(TEXT("Duplicate source actor is not duplicated in hand"),Hand->HandTiles.Num(),15);
    Hand->SynchronizeTiles(Updated,true);
    TestTrue(TEXT("New deal resets local ordering"),Hand->Slots[0]==Tiles[0]);
    World->DestroyWorld(false);
    return true;
}
#endif
