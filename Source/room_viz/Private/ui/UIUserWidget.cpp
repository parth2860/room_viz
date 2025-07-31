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
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Input/Reply.h"
#include "Input/Events.h"




void UUIUserWidget::NativeConstruct()
{
    Super::NativeConstruct();

    const FString MaterialPath = TEXT("/Game/assets/M_BaseMaterial.M_BaseMaterial");
    BaseMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *MaterialPath));
    if (!BaseMaterial)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to load BaseMaterial from: %s"), *MaterialPath);
    }

    SetIsFocusable(true);  // Critical
    SetVisibility(ESlateVisibility::Visible);

    // Root canvas
    if (!WidgetTree->RootWidget)
    {
        UCanvasPanel* RootPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
        WidgetTree->RootWidget = RootPanel;
    }

    // Create ScrollBox (direct child of root)
    MaterialsScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("MaterialsScrollBox"));

    if (UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget))
    {
        UCanvasPanelSlot* ScrollSlot = RootCanvas->AddChildToCanvas(MaterialsScrollBox);
        ScrollSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
        ScrollSlot->SetOffsets(FMargin(0.f));
    }

    // Bind material manager
    if (UWorld* World = GetWorld())
    {
        for (TActorIterator<AMaterialAPIManager> It(World); It; ++It)
        {
            if (AMaterialAPIManager* Mgr = *It)
            {
                Mgr->OnMaterialsReady.AddDynamic(this, &UUIUserWidget::HandleMaterialsReady);
                Mgr->FetchTileMaterials();
                break;
            }
        }
    }
}


void UUIUserWidget::HandleMaterialsReady(const TArray<FTileMaterialData>& DownloadedTiles)
{
    if (!BaseMaterial)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ BaseMaterial is not set in UIUserWidget"));
        return;
    }

    TArray<FFloorMaterialData> FinalTiles;

    for (const FTileMaterialData& T : DownloadedTiles)
    {
        FFloorMaterialData D;
        D.Name = T.ID;
        D.PreviewTexture = T.DownloadedTexture;
        D.MaterialURL = T.BaseColorURL;

        if (T.DownloadedTexture)
        {
            UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMaterial, this);
            DynMat->SetTextureParameterValue(FName("BaseColor"), T.DownloadedTexture);
            D.MaterialAsset = DynMat;

            UE_LOG(LogTemp, Log, TEXT("✅ Material created for: %s"), *T.ID);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("❌ Missing texture for %s"), *T.ID);
        }

        FinalTiles.Add(D);
    }

    InitializeMaterials(FinalTiles);
    UE_LOG(LogTemp, Log, TEXT("HandleMaterialsReady: received %d tiles"), DownloadedTiles.Num());
}

void UUIUserWidget::InitializeMaterials(const TArray<FFloorMaterialData>& Materials)
{
    if (!MaterialsScrollBox)
    {
        MaterialsScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());

        // Only set the root if it's not already set
        if (!WidgetTree->RootWidget)
        {
            UCanvasPanel* RootPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
            WidgetTree->RootWidget = RootPanel;

            RootPanel->SetVisibility(ESlateVisibility::Visible);
            RootPanel->SetIsEnabled(true);

            UCanvasPanelSlot* ScrollSlot = RootPanel->AddChildToCanvas(MaterialsScrollBox);
            ScrollSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
            ScrollSlot->SetOffsets(FMargin(0.f));
        }
    }

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
        FVector2D ScreenPos = InMouseEvent.GetScreenSpacePosition();
        CachedMousePosition = ScreenPos;  // ✅ FIX: Cache for drop use

        for (const TPair<UBorder*, FFloorMaterialData>& Pair : MaterialEntryMap)
        {
            UBorder* Entry = Pair.Key;
            if (!Entry) continue;

            const FGeometry& EntryGeo = Entry->GetCachedGeometry();

            if (EntryGeo.IsUnderLocation(ScreenPos))
            {
                DraggedBorder = Entry;
                UE_LOG(LogTemp, Log, TEXT("Mouse down on: %s"), *Pair.Value.Name);

                return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, Entry, EKeys::LeftMouseButton).NativeReply;
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
bool UUIUserWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    return true;
}

bool UUIUserWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    if (!InOperation || !InOperation->Payload) return false;

    UBorder* Dropped = Cast<UBorder>(InOperation->Payload);
    if (!Dropped || !MaterialEntryMap.Contains(Dropped)) return false;

    const FFloorMaterialData& Data = MaterialEntryMap[Dropped];

    // Convert drop screen position to world
    FVector2D ScreenPos = InDragDropEvent.GetScreenSpacePosition();

    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC) return false;

    FVector WorldOrigin, WorldDirection;
    if (PC->DeprojectScreenPositionToWorld(ScreenPos.X, ScreenPos.Y, WorldOrigin, WorldDirection))
    {
        FVector TraceStart = WorldOrigin;
        FVector TraceEnd = WorldOrigin + (WorldDirection * 10000.f);

        FHitResult Hit;
        FCollisionQueryParams Params;
        Params.bReturnPhysicalMaterial = false;
        Params.AddIgnoredActor(PC->GetPawn());

        if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
        {
            UPrimitiveComponent* HitComp = Hit.GetComponent();
            if (HitComp && Data.MaterialAsset)
            {
                HitComp->SetMaterial(0, Data.MaterialAsset);
                UE_LOG(LogTemp, Log, TEXT("✅ Applied material: %s to %s"), *Data.Name, *HitComp->GetName());
                return true;
            }
        }
    }

    return false;
}
