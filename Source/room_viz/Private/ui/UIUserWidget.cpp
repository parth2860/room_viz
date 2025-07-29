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

void UUIUserWidget::NativeConstruct()
{
    Super::NativeConstruct();

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

    for (auto& Data : Materials)
    {
        /*if (UUserWidget* Entry = CreateMaterialEntry(Data))
            MaterialsScrollBox->AddChild(Entry);*/
        //
        if (UWidget* Entry = CreateMaterialEntry(Data))
        {
            MaterialsScrollBox->AddChild(Entry);
        }
    }
}

UWidget* UUIUserWidget::CreateMaterialEntry(const FFloorMaterialData& Data)
{
    // Create Border
    UBorder* Border = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Border->SetPadding(FMargin(5));
    Border->SetBrushColor(FLinearColor::Gray);

    // Create HorizontalBox
    UHorizontalBox* HBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    Border->SetContent(HBox);

    // Add Image
    UImage* Img = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
    if (Data.PreviewTexture)
    {
        Img->SetBrushFromTexture(Data.PreviewTexture);
    }
    UHorizontalBoxSlot* ImgSlot = HBox->AddChildToHorizontalBox(Img);
    ImgSlot->SetPadding(FMargin(2));

    // Add Text
    UTextBlock* Txt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Txt->SetText(FText::FromString(Data.Name));
    UHorizontalBoxSlot* TxtSlot = HBox->AddChildToHorizontalBox(Txt);
    TxtSlot->SetPadding(FMargin(2));

    // Return the border directly — it's a valid widget and can be added to ScrollBox
    return Border;
}
