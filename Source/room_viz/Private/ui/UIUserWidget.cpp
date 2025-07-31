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
#include "room_viz/room_vizCharacter.h" //  CHARACTER HEADER
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

    // Make widget focusable and visible so it can receive drag/drop
    SetIsFocusable(true);
    SetVisibility(ESlateVisibility::Visible);

    // Ensure root canvas exists and can get events
    if (!WidgetTree->RootWidget)
    {
        UCanvasPanel* RootPanel = WidgetTree->ConstructWidget<UCanvasPanel>(
            UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
        WidgetTree->RootWidget = RootPanel;
        RootPanel->SetVisibility(ESlateVisibility::Visible);
        RootPanel->SetIsEnabled(true);
    }

    // Create the scroll box for materials
    MaterialsScrollBox = WidgetTree->ConstructWidget<UScrollBox>(
        UScrollBox::StaticClass(), TEXT("MaterialsScrollBox"));

    // ── CRITICAL FIX: let pointer events pass through to the root ──
    MaterialsScrollBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    MaterialsScrollBox->SetIsEnabled(false);

    // Add it under the root canvas
    if (UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget))
    {
        UCanvasPanelSlot* ScrollSlot = RootCanvas->AddChildToCanvas(MaterialsScrollBox);
        ScrollSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
        ScrollSlot->SetOffsets(FMargin(0.f));
    }

    // Bind to the API manager to get our textures
    if (UWorld* World = GetWorld())
    {
        for (TActorIterator<AMaterialAPIManager> It(World); It; ++It)
        {
            AMaterialAPIManager* Mgr = *It;
            Mgr->OnMaterialsReady.AddDynamic(this, &UUIUserWidget::HandleMaterialsReady);
            Mgr->FetchTileMaterials();
            break;
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
// 1) Mouse‐down: start drag
FReply UUIUserWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
    {
        UE_LOG(LogTemp, Warning, TEXT("[UI] NativeOnMouseButtonDown fired at (%f,%f)"),
            InMouseEvent.GetScreenSpacePosition().X,
            InMouseEvent.GetScreenSpacePosition().Y);

        const FVector2D ScreenPos = InMouseEvent.GetScreenSpacePosition();
        CachedMousePosition = ScreenPos;

        for (auto& Pair : MaterialEntryMap)
        {
            UBorder* Entry = Pair.Key;
            if (!Entry) continue;

            if (Entry->GetCachedGeometry().IsUnderLocation(ScreenPos))
            {
                DraggedBorder = Entry;
                UE_LOG(LogTemp, Warning, TEXT("[UI] Detected drag start on '%s'"),
                    *Pair.Value.Name);

                return UWidgetBlueprintLibrary::DetectDragIfPressed(
                    InMouseEvent, Entry, EKeys::LeftMouseButton
                ).NativeReply;
            }
        }
    }

    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

// 2) Drag‐detected: create payload
void UUIUserWidget::NativeOnDragDetected(const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent,
    UDragDropOperation*& OutOperation)
{
    if (!DraggedBorder || !MaterialEntryMap.Contains(DraggedBorder))
    {
        UE_LOG(LogTemp, Warning, TEXT("[UI] NativeOnDragDetected: no valid DraggedBorder"));
        return;
    }

    const auto& Data = MaterialEntryMap[DraggedBorder];
    UE_LOG(LogTemp, Warning, TEXT("[UI] NativeOnDragDetected: creating op for '%s'"),
        *Data.Name);

    UDragDropOperation* DragOp = UWidgetBlueprintLibrary::CreateDragDropOperation(
        UDragDropOperation::StaticClass()
    );
    DragOp->Payload = DraggedBorder;
    DragOp->DefaultDragVisual = DraggedBorder;
    DragOp->Pivot = EDragPivot::CenterCenter;
    OutOperation = DragOp;
}

// 3) Drag‐over: let us know when pointer moves during a drag
bool UUIUserWidget::NativeOnDragOver(const FGeometry& InGeometry,
    const FDragDropEvent& InDragDropEvent,
    UDragDropOperation* InOperation)
{
    UE_LOG(LogTemp, Verbose, TEXT("[UI] NativeOnDragOver at (%f,%f)"),
        InDragDropEvent.GetScreenSpacePosition().X,
        InDragDropEvent.GetScreenSpacePosition().Y);
    // return true so that OnDrop will be called
    return true;
}

// 4) Drop: apply material
bool UUIUserWidget::NativeOnDrop(const FGeometry& InGeometry,
    const FDragDropEvent& InDragDropEvent,
    UDragDropOperation* InOperation)
{
    UE_LOG(LogTemp, Warning, TEXT("[UI] NativeOnDrop at (%f,%f)"),
        InDragDropEvent.GetScreenSpacePosition().X,
        InDragDropEvent.GetScreenSpacePosition().Y);

    if (!InOperation || !InOperation->Payload)
        return false;

    UBorder* DroppedBorder = Cast<UBorder>(InOperation->Payload);
    if (!DroppedBorder || !MaterialEntryMap.Contains(DroppedBorder))
        return false;

    const FFloorMaterialData& Data = MaterialEntryMap[DroppedBorder];
    if (!Data.MaterialAsset)
    {
        UE_LOG(LogTemp, Error, TEXT("[UI] Drop: no MaterialAsset"));
        return false;
    }

    // Forward to your character
    APlayerController* PC = GetOwningPlayer();
    if (PC && PC->GetPawn())
    {
        if (Aroom_vizCharacter* C = Cast<Aroom_vizCharacter>(PC->GetPawn()))
        {
            C->OnMaterialDropped(Data, InDragDropEvent.GetScreenSpacePosition());
            UE_LOG(LogTemp, Log, TEXT("[UI] Forwarded '%s' to character"), *Data.Name);
            return true;
        }
    }

    return false;
}

// 5) Mouse‐up: finalize drop
FReply UUIUserWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && DraggedBorder)
    {
        // Validate pointer to prevent crash
        if (!IsValid(DraggedBorder) || !MaterialEntryMap.Contains(DraggedBorder))
        {
            UE_LOG(LogTemp, Error, TEXT("[UI] MouseUp: DraggedBorder is invalid or missing from map"));
            DraggedBorder = nullptr;
            return FReply::Handled();
        }

        const FVector2D ScreenPos = InMouseEvent.GetScreenSpacePosition();
        const FFloorMaterialData& Data = MaterialEntryMap[DraggedBorder];

        // Defensive: material asset check
        if (!Data.MaterialAsset)
        {
            UE_LOG(LogTemp, Error, TEXT("[UI] DropBackstop: '%s' has no MaterialAsset"), *Data.Name);
            DraggedBorder = nullptr;
            return FReply::Handled();
        }

        DraggedBorder = nullptr; // clear state early

        UWorld* World = GetWorld();
        APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
        if (!PC)
        {
            UE_LOG(LogTemp, Error, TEXT("[UI] DropBackstop: missing player controller"));
            return FReply::Handled();
        }

        FVector WorldOrigin, WorldDir;
        if (!PC->DeprojectScreenPositionToWorld(ScreenPos.X, ScreenPos.Y, WorldOrigin, WorldDir))
        {
            UE_LOG(LogTemp, Error, TEXT("[UI] DropBackstop: failed to deproject"));
            return FReply::Handled();
        }

        FHitResult Hit;
        FCollisionQueryParams Params;
        if (APawn* Pawn = PC->GetPawn()) Params.AddIgnoredActor(Pawn);

        const bool bHit = World->LineTraceSingleByChannel(
            Hit,
            WorldOrigin,
            WorldOrigin + WorldDir * 10000.f,
            ECC_Visibility,
            Params
        );

        if (bHit && Hit.GetComponent())
        {
            Hit.GetComponent()->SetMaterial(0, Data.MaterialAsset);
            UE_LOG(LogTemp, Log, TEXT("[UI] ✅ DropBackstop applied '%s' to %s"),
                *Data.Name, *Hit.GetComponent()->GetName());
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[UI] DropBackstop trace missed or hit null component"));
        }

        return FReply::Handled();
    }

    return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

