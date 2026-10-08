#include "HzaHandInteractor.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "InputCoreTypes.h"
#include "Components/InputComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "UnrealClient.h"
#include "UObject/UnrealType.h"

AHzaHandInteractor::AHzaHandInteractor()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = false;
}
void AHzaHandInteractor::BeginPlay()
{
    Super::BeginPlay();
    Controller = UGameplayStatics::GetPlayerController(this,0);
    Slots.SetNum(30);
    TSet<AActor*> Seen;
    int32 Slot = 0;
    for (AActor* Tile : HandTiles)
    {
        if (IsValid(Tile) && !Seen.Contains(Tile) && Slot < Slots.Num())
        { Slots[Slot++] = Tile; Seen.Add(Tile); }
    }
    Preview = Slots;
    if (Controller)
    {
        // Avoid double-toggle from the legacy tile's OnClicked/OnTouch graph.
        bOldClicks = Controller->bEnableClickEvents;
        bOldTouches = Controller->bEnableTouchEvents;
        Controller->bEnableClickEvents = false;
        Controller->bEnableTouchEvents = false;
        // Consume input transitions, including clicks completed within one slow frame.
        EnableInput(Controller);
        InputComponent->BindKey(EKeys::LeftMouseButton,IE_Pressed,this,&AHzaHandInteractor::MousePressed);
        InputComponent->BindKey(EKeys::LeftMouseButton,IE_Released,this,&AHzaHandInteractor::MouseReleased);
        InputComponent->BindKey(EKeys::Escape,IE_Pressed,this,&AHzaHandInteractor::CancelGesture);
        InputComponent->BindKey(EKeys::RightMouseButton,IE_Pressed,this,&AHzaHandInteractor::CancelGesture);
        InputComponent->BindTouch(IE_Pressed,this,&AHzaHandInteractor::TouchPressed);
        InputComponent->BindTouch(IE_Released,this,&AHzaHandInteractor::TouchReleased);
        InputComponent->BindTouch(IE_Repeat,this,&AHzaHandInteractor::TouchMoved);
        if (UGameViewportClient* Viewport=GetWorld()->GetGameViewport())
        {
            InputViewport=Viewport;
            // InputComponent dispatch is deferred. Capture pointer coordinates when
            // the viewport receives each event, before a fast drag advances them.
            ViewportInputHandle=Viewport->OnInputKey().AddUObject(this,&AHzaHandInteractor::ViewportInputKey);
        }
    }
}
void AHzaHandInteractor::EndPlay(const EEndPlayReason::Type Reason)
{
    if (InputViewport.IsValid()) InputViewport->OnInputKey().Remove(ViewportInputHandle);
    if (Controller)
    { DisableInput(Controller); Controller->bEnableClickEvents=bOldClicks; Controller->bEnableTouchEvents=bOldTouches; }
    Super::EndPlay(Reason);
}
FVector AHzaHandInteractor::SlotPosition(int32 Index) const
{
    return RackOrigin + ColumnDirection.GetSafeNormal()*((Index%15-7)*SlotSpacing) + UpperRowOffset*(Index/15);
}
AActor* AHzaHandInteractor::PickTile(const FVector2D& Screen, bool bTouch) const
{
    AActor* Best = nullptr;
    double BestDistance = TNumericLimits<double>::Max();
    for (AActor* Tile : Slots)
    {
        if (!IsValid(Tile)) continue;
        FVector2D Center, Edge;
        if (!Controller->ProjectWorldLocationToScreen(Tile->GetActorLocation(),Center) ||
            !Controller->ProjectWorldLocationToScreen(Tile->GetActorLocation()+ColumnDirection*1.4,Edge)) continue;
        const float HalfWidth = FMath::Max(float((Edge-Center).Length()), bTouch ? 18.f : 10.f);
        const FVector2D Delta=Screen-Center;
        if (FMath::Abs(Delta.X)>HalfWidth || FMath::Abs(Delta.Y)>(bTouch?24:18)) continue;
        const double Distance=Delta.SizeSquared();
        if (Distance<BestDistance) { Best=Tile; BestDistance=Distance; }
    }
    return Best;
}
int32 AHzaHandInteractor::PickSlot(const FVector2D& Screen) const
{
    int32 Best=INDEX_NONE;
    double Distance=TNumericLimits<double>::Max();
    for (int32 i=0;i<30;++i)
    {
        FVector2D Center, Edge;
        if (!Controller->ProjectWorldLocationToScreen(SlotPosition(i),Center) ||
            !Controller->ProjectWorldLocationToScreen(SlotPosition(i)+ColumnDirection*SlotSpacing,Edge)) continue;
        const FVector2D Delta=Screen-Center;
        const float Width=FMath::Max(float((Edge-Center).Length())*0.65f,12.f);
        if (FMath::Abs(Delta.X)>Width || FMath::Abs(Delta.Y)>25) continue;
        if (Delta.SizeSquared()<Distance) { Best=i; Distance=Delta.SizeSquared(); }
    }
    return Best;
}
bool AHzaHandInteractor::BuildPreview(int32 Target)
{
    Preview=Slots;
    const int32 Source=Preview.IndexOfByKey(PressedTile);
    if (Source==INDEX_NONE || !Preview.IsValidIndex(Target)) return false;
    Preview[Source]=nullptr;
    if (Preview[Target])
    {
        int32 Empty=INDEX_NONE;
        // Prefer a vacancy on the destination row; then allow crossing rows.
        for (int32 Pass=0;Pass<2 && Empty==INDEX_NONE;++Pass)
            for (int32 Radius=1;Radius<30 && Empty==INDEX_NONE;++Radius)
                for (int32 Direction : {1,-1})
                {
                    const int32 Candidate=Target+Radius*Direction;
                    if (Preview.IsValidIndex(Candidate) && !Preview[Candidate] &&
                        (Pass==1 || Candidate/15==Target/15)) { Empty=Candidate; break; }
                }
        if (Empty==INDEX_NONE) return false;
        const int32 Step=Empty>Target?1:-1;
        for (int32 i=Empty;i!=Target;i-=Step) Preview[i]=Preview[i-Step];
    }
    Preview[Target]=PressedTile;
    return true;
}
void AHzaHandInteractor::Select(AActor* Tile)
{
    SelectedTile=SelectedTile==Tile ? nullptr : Tile;
    for (AActor* Item : Slots)
        if (IsValid(Item))
            if (FBoolProperty* Selected=FindFProperty<FBoolProperty>(Item->GetClass(),TEXT("Selected")))
                Selected->SetPropertyValue_InContainer(Item,Item==SelectedTile);
}
void AHzaHandInteractor::CancelGesture()
{
    Preview=Slots; PressedTile=nullptr; bDragging=false; DropSlot=INDEX_NONE; bPointerDown=false;
}
void AHzaHandInteractor::BeginGesture(FVector2D Position, bool bTouch)
{
    if (bPresentationLocked || bFrontendBlocked) return;
    if (bPointerDown) return;
    Pointer=Position; PressPosition=Position; bTouchGesture=bTouch; bPointerDown=true;
    PressedTile=PickTile(Pointer,bTouch);
}
void AHzaHandInteractor::UpdateGesture(FVector2D Position)
{
    Pointer=Position;
    if (!bPointerDown || !PressedTile) return;
    if ((Pointer-PressPosition).SizeSquared()>FMath::Square(bTouchGesture?12.f:6.f)) bDragging=true;
    if (bDragging)
    {
        DropSlot=PickSlot(Pointer);
        if (!BuildPreview(DropSlot)) { DropSlot=INDEX_NONE; Preview=Slots; }
    }
}
void AHzaHandInteractor::EndGesture(FVector2D Position)
{
    if (!bPointerDown) return;
    UpdateGesture(Position);
    if (PressedTile && !bDragging) Select(PressedTile);
    else if (bDragging && DropSlot!=INDEX_NONE) Slots=Preview;
    CancelGesture();
}
void AHzaHandInteractor::MousePressed()
{
    if (ViewportInputHandle.IsValid()) return;
    float X,Y;
    if (Controller && Controller->GetMousePosition(X,Y)) BeginGesture(FVector2D(X,Y),false);
}
void AHzaHandInteractor::MouseReleased()
{
    if (ViewportInputHandle.IsValid()) return;
    if (bTouchGesture) return;
    float X,Y;
    if (Controller && Controller->GetMousePosition(X,Y)) EndGesture(FVector2D(X,Y));
    else CancelGesture();
}
void AHzaHandInteractor::ViewportInputKey(const FInputKeyEventArgs& Event)
{
    if (!Controller || !Controller->IsLocalController() || !Event.Viewport || Event.bIsTouchEvent) return;
    if (Event.Key==EKeys::LeftMouseButton)
    {
        FIntPoint Position;
        Event.Viewport->GetMousePos(Position);
        if (Event.Event==IE_Pressed) BeginGesture(FVector2D(Position),false);
        else if (Event.Event==IE_Released && !bTouchGesture) EndGesture(FVector2D(Position));
    }
}
void AHzaHandInteractor::TouchPressed(ETouchIndex::Type Finger,FVector Location)
{ if (Finger==ETouchIndex::Touch1) BeginGesture(FVector2D(Location.X,Location.Y),true); }
void AHzaHandInteractor::TouchReleased(ETouchIndex::Type Finger,FVector Location)
{ if (Finger==ETouchIndex::Touch1 && bTouchGesture) EndGesture(FVector2D(Location.X,Location.Y)); }
void AHzaHandInteractor::TouchMoved(ETouchIndex::Type Finger,FVector Location)
{ if (Finger==ETouchIndex::Touch1 && bTouchGesture) UpdateGesture(FVector2D(Location.X,Location.Y)); }
void AHzaHandInteractor::Tick(float Dt)
{
    Super::Tick(Dt);
    if (bPresentationLocked || bFrontendBlocked) return;
    if (!Controller || !Controller->IsLocalController()) return;
    float X=0,Y=0;
    if (bPointerDown && bTouchGesture)
    {
        bool TouchDown=false;
        Controller->GetInputTouchState(ETouchIndex::Touch1,X,Y,TouchDown);
        if (TouchDown) UpdateGesture(FVector2D(X,Y));
        HoveredTile=nullptr;
    }
    else if (Controller->GetMousePosition(X,Y))
    {
        UpdateGesture(FVector2D(X,Y));
        HoveredTile=PickTile(Pointer,false);
    }
    else { CancelGesture(); HoveredTile=nullptr; }
    const auto& Layout=bDragging ? Preview : Slots;
    for (int32 i=0;i<Layout.Num();++i)
    {
        AActor* Tile=Layout[i];
        if (!IsValid(Tile)) continue;
        FVector Target=SlotPosition(i);
        if (Tile==SelectedTile) Target.Z+=1.5f;
        else if (Tile==HoveredTile) Target.Z+=0.45f;
        if (bDragging && Tile==PressedTile)
        {
            FVector Origin,Direction;
            if (Controller->DeprojectScreenPositionToWorld(Pointer.X,Pointer.Y,Origin,Direction) &&
                FMath::Abs(Direction.Z)>KINDA_SMALL_NUMBER)
            {
                const double T=(RackOrigin.Z+6-Origin.Z)/Direction.Z;
                if (T>0) Target=Origin+Direction*T;
            }
            // Restrained tilt signals a rejected drop; level tile means valid.
            FRotator Rotation=Tile->GetActorRotation(); Rotation.Roll=DropSlot==INDEX_NONE?8.f:0.f;
            Tile->SetActorRotation(Rotation);
        }
        else { FRotator Rotation=Tile->GetActorRotation(); Rotation.Roll=0; Tile->SetActorRotation(Rotation); }
        Tile->SetActorLocation(FMath::VInterpTo(Tile->GetActorLocation(),Target,Dt,22.f));
    }
}

void AHzaHandInteractor::SetPresentationLocked(bool bLocked)
{
    if (bLocked) CancelGesture();
    bPresentationLocked=bLocked;
}

void AHzaHandInteractor::SetFrontendBlocked(bool bBlocked)
{
    if (bBlocked) CancelGesture();
    bFrontendBlocked = bBlocked;
}
FVector AHzaHandInteractor::GetSlotPosition(AActor* Tile) const
{
    const int32 Index=Slots.IndexOfByKey(Tile);
    return Index==INDEX_NONE ? RackOrigin : SlotPosition(Index);
}
void AHzaHandInteractor::SynchronizeTiles(const TArray<AActor*>& Tiles,bool bResetOrder)
{
    CancelGesture();
    TSet<AActor*> Owned;
    for (AActor* Tile : Tiles) if (IsValid(Tile) && Owned.Num()<30) Owned.Add(Tile);
    Slots.SetNum(30);
    for (auto& Tile : Slots) if (bResetOrder || !Owned.Contains(Tile.Get())) Tile=nullptr;
    for (AActor* Tile : Tiles) if (Owned.Contains(Tile) && !Slots.Contains(Tile))
    {
        const int32 Vacancy=Slots.IndexOfByKey(nullptr);
        if (Vacancy!=INDEX_NONE) Slots[Vacancy]=Tile;
    }
    HandTiles.Reset();
    for (AActor* Tile : Slots) if (Tile) HandTiles.Add(Tile);
    if (!Owned.Contains(SelectedTile.Get())) SelectedTile=nullptr;
    Preview=Slots;
}
