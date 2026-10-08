#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "HzaHandInteractor.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "InputKeyEventArgs.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Editor.h"
#include "Tests/AutomationEditorCommon.h"

// Run explicitly with L_Konkan_Test loaded. This exercises the loaded
// scene and native touch queue; it is not a physical Android/device test.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHzaPIETouchInputTest,"HZA.Live.HandTouchInput",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHzaPIETouchInputTest::RunTest(const FString& Parameters)
{
    if (Parameters!=TEXT("PIEReady"))
    {
        UWorld* EditorWorld=GEditor->GetEditorWorldContext().World();
        if (!EditorWorld || EditorWorld->GetOutermost()->GetName()!=TEXT("/Game/HZA/Test/L_Konkan_Test"))
        { AddError(TEXT("Load L_Konkan_Test before this live test")); return false; }
        // The automation controller ends an existing play session before a run.
        // Start the fixture inside the test, and wait for real BeginPlay.
        FRequestPlaySessionParams Params;
        Params.WorldType=EPlaySessionWorldType::PlayInEditor;
        FAutomationEditorCommonUtils::SetPlaySessionStartToActiveViewport(Params);
        ADD_LATENT_AUTOMATION_COMMAND(FStartPIEForAutomationCommand(Params));
        ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this]() { RunTest(TEXT("PIEReady")); return true; }));
        ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
        return true;
    }
    AHzaHandInteractor* Hand=nullptr;
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
        if (Context.WorldType==EWorldType::PIE && Context.World())
            for (TActorIterator<AHzaHandInteractor> It(Context.World());It;++It) { Hand=*It; break; }
    if (!TestNotNull(TEXT("Start PIE in L_Konkan_Test before this live test"),Hand)) return false;
    APlayerController* PC=Hand->Controller;
    if (!TestNotNull(TEXT("Local controller"),PC) || !TestNotNull(TEXT("Native input"),PC->PlayerInput.Get()) ||
        !TestNotNull(TEXT("Bound input component"),Hand->InputComponent.Get())) return false;
    if (!TestEqual(TEXT("Fixture has 15 local tiles"),Hand->HandTiles.Num(),15) ||
        !TestEqual(TEXT("Fixture has 30 rack slots"),Hand->Slots.Num(),30)) return false;
    const auto OriginalSlots=Hand->Slots;
    AActor* OriginalSelection=Hand->SelectedTile;
    AActor* OriginalHover=Hand->HoveredTile;
    const FVector2D OriginalPointer=Hand->Pointer;
    TMap<AActor*,FTransform> OriginalTransforms;
    for (AActor* Tile : OriginalSlots) if (IsValid(Tile)) OriginalTransforms.Add(Tile,Tile->GetActorTransform());
    const FTouchId Finger(FInputDeviceId::CreateFromInternalId(0),ETouchIndex::Touch1);
    TArray<UInputComponent*> Stack{Hand->InputComponent};
    const auto Dispatch=[&]() { PC->PlayerInput->ProcessInputStack(Stack,1.f/60.f,false); };
    const auto Touch=[&](ETouchType::Type Type,const FVector2D& Point) {
        PC->InputTouch(Finger,Type,Point,Type==ETouchType::Ended?0.f:1.f,FPlatformTime::Cycles64());
    };
    ON_SCOPE_EXIT
    {
        Touch(ETouchType::Ended,FVector2D::ZeroVector); Dispatch();
        Hand->Slots=OriginalSlots; Hand->CancelGesture();
        Hand->SelectedTile=nullptr; Hand->Select(OriginalSelection);
        Hand->HoveredTile=OriginalHover; Hand->Pointer=OriginalPointer;
        for (const auto& Pair : OriginalTransforms) if (IsValid(Pair.Key)) Pair.Key->SetActorTransform(Pair.Value);
    };
    const auto ScreenSlot=[&](int32 Slot) {
        FVector2D Point;
        TestTrue(TEXT("Rack slot projects into gameplay viewport"),PC->ProjectWorldLocationToScreen(Hand->SlotPosition(Slot),Point));
        return Point;
    };
    const auto Settle=[&]() { Hand->Tick(1.f); };
    const auto Integrity=[&]() {
        TSet<AActor*> Unique; TSet<int32> Occupied;
        for (int32 i=0;i<Hand->Slots.Num();++i) if (AActor* Tile=Hand->Slots[i])
        {
            TestTrue(TEXT("Every rack actor belongs to the original private hand"),OriginalTransforms.Contains(Tile));
            TestFalse(TEXT("Each tile occurs once"),Unique.Contains(Tile)); Unique.Add(Tile); Occupied.Add(i);
            FVector Expected=Hand->SlotPosition(i);
            if (Tile==Hand->SelectedTile) Expected.Z+=1.5f;
            else if (Tile==Hand->HoveredTile) Expected.Z+=0.45f;
            TestTrue(TEXT("Tile settles at its assigned slot"),Tile->GetActorLocation().Equals(Expected,0.01f));
            TestFalse(TEXT("No invalid transform"),Tile->GetActorTransform().ContainsNaN());
            TestTrue(TEXT("Rejected-drop tilt is cleared"),FMath::IsNearlyZero(Tile->GetActorRotation().Roll,0.01));
        }
        TestEqual(TEXT("All fifteen physical tiles survive"),Unique.Num(),15);
        TestEqual(TEXT("Fifteen distinct slots"),Occupied.Num(),15);
        TestFalse(TEXT("Release clears drag"),Hand->bDragging);
        TestFalse(TEXT("Release clears pointer capture"),Hand->bPointerDown);
        TestEqual(TEXT("Release clears target slot"),Hand->DropSlot,INDEX_NONE);
    };
    Hand->CancelGesture(); Hand->SelectedTile=nullptr; Hand->Select(nullptr); Settle();
    AActor* First=Hand->Slots[0];
    if (!TestNotNull(TEXT("First slot is occupied"),First)) return false;
    Touch(ETouchType::Began,ScreenSlot(0)); Touch(ETouchType::Ended,ScreenSlot(0)); Dispatch();
    TestTrue(TEXT("Queued touch tap selects exactly once"),Hand->SelectedTile==First);
    Touch(ETouchType::Began,ScreenSlot(0)); Touch(ETouchType::Ended,ScreenSlot(0)); Dispatch();
    TestNull(TEXT("Second tap deselects"),Hand->SelectedTile.Get());
    const auto Drag=[&](int32 Source,int32 Destination) {
        const FVector2D Start=ScreenSlot(Source), End=ScreenSlot(Destination);
        Touch(ETouchType::Began,Start); Touch(ETouchType::Moved,End); Touch(ETouchType::Ended,End);
        Dispatch(); Settle(); Integrity();
    };
    Drag(0,14);
    TestTrue(TEXT("Same-frame touch drag keeps the press tile"),Hand->Slots[14]==First);
    for (int32 i=0;i<14;++i) TestTrue(TEXT("Occupied insertion shifts each neighbor once"),Hand->Slots[i]==OriginalSlots[i+1]);
    Drag(14,15); TestTrue(TEXT("Lower to upper"),Hand->Slots[15]==First);
    Drag(15,0); TestTrue(TEXT("Upper to lower first slot"),Hand->Slots[0]==First);
    const auto BeforeRejected=Hand->Slots;
    Touch(ETouchType::Began,ScreenSlot(0)); Dispatch();
    Touch(ETouchType::Moved,ScreenSlot(14)); Dispatch();
    TestTrue(TEXT("Gesture is dragging before cancellation"),Hand->bDragging);
    PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::RightMouseButton,IE_Pressed,1.f)); Dispatch();
    PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::RightMouseButton,IE_Released,0.f)); Dispatch();
    Touch(ETouchType::Ended,ScreenSlot(14)); Dispatch(); Settle();
    TestTrue(TEXT("Cancel binding restores order even after later release"),Hand->Slots==BeforeRejected); Integrity();
    Touch(ETouchType::Began,ScreenSlot(0)); Touch(ETouchType::Moved,FVector2D(-100,-100));
    Touch(ETouchType::Ended,FVector2D(-100,-100)); Dispatch(); Settle();
    TestTrue(TEXT("Invalid touch drop restores order"),Hand->Slots==BeforeRejected); Integrity();
    TestNull(TEXT("Touch drags do not synthesize selection"),Hand->SelectedTile.Get());
    return true;
}
#endif
