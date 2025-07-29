// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Http.h"
#include "UObject/NoExportTypes.h"
#include "MaterialAPIManager.generated.h"

USTRUCT(BlueprintType)
struct FTileMaterialData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Tile")
	FString ID;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Tile")
	FString BaseColorURL;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	UTexture2D* DownloadedTexture = nullptr;

};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMaterialsReady, const TArray<FTileMaterialData>&, DownloadedTiles);

UCLASS()
class ROOM_VIZ_API AMaterialAPIManager : public  AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMaterialAPIManager();

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(BlueprintAssignable, Category = "Tile API")
	FOnMaterialsReady OnMaterialsReady;

	/** Fetch tiles from remote JSON */
	UFUNCTION(BlueprintCallable, Category = "Tile API")
	void FetchTileMaterials();

	void OnResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void DownloadTileImage(const FString& URL, const FString& TileID);
	void OnImageDownloaded(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FString TileID);

	// Class member array
	UPROPERTY()
	TArray<FTileMaterialData> ParsedTiles;

	int32 PendingImages = 0;
};
