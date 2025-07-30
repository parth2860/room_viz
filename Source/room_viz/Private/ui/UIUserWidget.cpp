// Fill out your copyright notice in the Description page of Project Settings.


#include "ui/UIUserWidget.h"
#include "Components/ScrollBox.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Engine/Texture2D.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/VerticalBox.h"
#include "Blueprint/UserWidget.h"
#include "dataclass/MaterialAPIManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBoxSlot.h"
#include "Blueprint/DragDropOperation.h"
#include "room_viz/room_vizCharacter.h" // YOUR CHARACTER HEADER

void UUIUserWidget::NativeConstruct()
{
    Super::NativeConstruct();

    SetIsFocusable(true); // ✅ Required for drag input from widget

    UWorld* World = GetWorld();
    if (!World) return;

    // Find the Material Manager actor in the level
    for (TActorIterator<AMaterialAPIManager> It(World); It; ++It)
    {
        AMaterialAPIManager* Mgr = *It;
        if (!Mgr) continue;

        // Bind to its delegate and kick off fetching
        Mgr->OnMaterialsReady.AddDynamic(this, &UUIUserWidget::HandleMaterialsReady);
        Mgr->FetchTileMaterials();
        break;
    }
}

void UUIUserWidget::HandleMaterialsReady(const TArray<FTileMaterialData>& DownloadedTiles)
{
    TArray<FFloorMaterialData> UIData;
    for (auto& T : DownloadedTiles)
    {
        FFloorMaterialData D;
        D.Name = T.ID;
        D.PreviewTexture = T.DownloadedTexture;
        D.MaterialURL = T.BaseColorURL;
        UIData.Add(D);
    }
    InitializeMaterials(UIData);
    UE_LOG(LogTemp, Log, TEXT("HandleMaterialsReady: received %d tiles"), DownloadedTiles.Num());
}

void UUIUserWidget::InitializeMaterials(const TArray<FFloorMaterialData>& Materials)
{
    if (!MaterialsScrollBox) return;

    MaterialsScrollBox->ClearChildren();
    MaterialEntryMap.Empty();

    for (const FFloorMaterialData& Data : Materials)
    {
        UBorder* Entry = Cast<UBorder>(CreateMaterialEntry(Data));
        if (!Entry) continue;

        MaterialsScrollBox->AddChild(Entry);
        MaterialEntryMap.Add(Entry, Data);
    }
}

UBorder* UUIUserWidget::CreateMaterialEntry(const FFloorMaterialData& Data)
{
    // 1) Border container
    UBorder* Border = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Border->SetToolTipText(FText::FromString(Data.Name)); // Optional: helps debug
    Border->SetPadding(5);
    Border->SetBrushColor(FLinearColor::Gray);
    // ✅ Enable drag support
    Border->SetIsEnabled(true);
    //Border->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    Border->SetVisibility(ESlateVisibility::Visible); // ✅ critical fix

    // 2) Horizontal box
    UHorizontalBox* HBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    Border->SetContent(HBox);
    
    // 3) Preview image
    UImage* Img = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
    if (Data.PreviewTexture) Img->SetBrushFromTexture(Data.PreviewTexture);
    HBox->AddChildToHorizontalBox(Img)->SetPadding(2);

    // 4) Name text
    UTextBlock* Txt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Txt->SetText(FText::FromString(Data.Name));
    HBox->AddChildToHorizontalBox(Txt)->SetPadding(2);

    return Border;
}

// Start drag when clicking on one of our Borders
FReply UUIUserWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
    {
        for (const TPair<UBorder*, FFloorMaterialData>& Pair : MaterialEntryMap)
        {
            UBorder* Entry = Pair.Key;
            if (!Entry) continue;

            FVector2D ScreenPos = InMouseEvent.GetScreenSpacePosition();
            const FGeometry& EntryGeo = Entry->GetCachedGeometry();

            // Log screen position & border info
            UE_LOG(LogTemp, Log, TEXT("Checking entry: %s"), *Pair.Value.Name);

            if (EntryGeo.IsUnderLocation(ScreenPos))
            {
                DraggedBorder = Entry;
                UE_LOG(LogTemp, Log, TEXT("Mouse down on: %s"), *Pair.Value.Name);

               // return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, Entry, EKeys::LeftMouseButton).NativeReply;
               // ✅ FIX: Pass `this` to DetectDragIfPressed
                return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;

            }
        }
    }

    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}


void UUIUserWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
    if (!DraggedBorder || !MaterialEntryMap.Contains(DraggedBorder)) return;

    const FFloorMaterialData& DraggedData = MaterialEntryMap[DraggedBorder];

    UDragDropOperation* DragOp = UWidgetBlueprintLibrary::CreateDragDropOperation(UDragDropOperation::StaticClass());
    DragOp->Payload = DraggedBorder;
    DragOp->DefaultDragVisual = DraggedBorder;
    DragOp->Pivot = EDragPivot::CenterCenter;
    OutOperation = DragOp;

    UE_LOG(LogTemp, Log, TEXT("Dragging material: %s"), *DraggedData.Name);
}

bool UUIUserWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    if (!InOperation || !InOperation->Payload) return false;

    UBorder* Dropped = Cast<UBorder>(InOperation->Payload);
    if (!Dropped || !MaterialEntryMap.Contains(Dropped)) return false;

    const FFloorMaterialData& Data = MaterialEntryMap[Dropped];

    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC) return false;

    Aroom_vizCharacter* Character = Cast<Aroom_vizCharacter>(PC->GetPawn());
    if (Character)
    {
        Character->OnMaterialDropped(Data); // Call your character logic
        return true;
    }

    return false;
}

bool UUIUserWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    return true;
}


