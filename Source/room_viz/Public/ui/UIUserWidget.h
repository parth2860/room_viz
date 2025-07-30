// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UIUserWidget.generated.h"

class UScrollBox;
class UUserWidget;
class UBorder;
class UImage;
class UTextBlock;
class UHorizontalBox;
class UWidget;

USTRUCT(BlueprintType)
struct FFloorMaterialData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Floor Material")
    FString Name;

    UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Floor Material")
    UTexture2D* PreviewTexture;

    UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Floor Material")
    FString MaterialURL;
};

UCLASS()
class ROOM_VIZ_API UUIUserWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
    virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

    UFUNCTION(BlueprintCallable, Category = "Floor Materials")
    void InitializeMaterials(const TArray<FFloorMaterialData>& Materials);

    UFUNCTION()
    void HandleMaterialsReady(const TArray<FTileMaterialData>& DownloadedTiles);

    UPROPERTY(meta = (BindWidget))
    class UScrollBox* MaterialsScrollBox;

    //UUserWidget* CreateMaterialEntry(const FFloorMaterialData& Data);
    //UWidget* CreateMaterialEntry(const FFloorMaterialData& Data);
    //
   

    UPROPERTY(EditAnywhere, Category = "UI")
    TSubclassOf<UUserWidget> MaterialEntryWidgetClass;
    // if you prefer your existing CreateMaterialEntry you'd skip this and use the UBorder hack below


    // Map each entry Border back to its data
    TMap<UBorder*, FFloorMaterialData> MaterialEntryMap;

    // Helper to spawn one entry
    UBorder* CreateMaterialEntry(const FFloorMaterialData& Data);
    UBorder* DraggedBorder = nullptr;
    bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation);
};