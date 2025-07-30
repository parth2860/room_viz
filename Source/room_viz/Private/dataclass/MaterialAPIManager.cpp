// Fill out your copyright notice in the Description page of Project Settings.

#include "dataclass/MaterialAPIManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "ImageUtils.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"

// Sets default values
AMaterialAPIManager::AMaterialAPIManager()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AMaterialAPIManager::BeginPlay()
{
	Super::BeginPlay();
    // Initiate request on spawn
    FetchTileMaterials();
}

// Called every frame
void AMaterialAPIManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}
void AMaterialAPIManager::FetchTileMaterials()
{
    const FString Url = TEXT("https://raw.githubusercontent.com/Ghanshyam-Shinde/realestateinfo/refs/heads/master/FloorTiles.json");

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->OnProcessRequestComplete().BindUObject(this, &AMaterialAPIManager::OnResponseReceived);
    Request->SetURL(Url);
    Request->SetVerb("GET");
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->ProcessRequest();
}
void AMaterialAPIManager::OnResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid()) return;

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return;

	const TArray<TSharedPtr<FJsonValue>>* Array;
	if (!Root->TryGetArrayField(TEXT("Tiles"), Array)) return;

	ParsedTiles.Empty();
	PendingImages = 0;

	for (auto& Val : *Array)
	{
		if (auto Obj = Val->AsObject())
		{
			FTileMaterialData Tile;
			Tile.ID = Obj->GetStringField("id");
			Tile.BaseColorURL = Obj->GetStringField("baseColorUrl");
			ParsedTiles.Add(Tile);
			PendingImages++;
			DownloadTileImage(Tile.BaseColorURL, Tile.ID);
		}
	}
}

void AMaterialAPIManager::DownloadTileImage(const FString& URL, const FString& TileID)
{
	auto Req = FHttpModule::Get().CreateRequest();
	Req->OnProcessRequestComplete().BindLambda(
		[this, TileID](FHttpRequestPtr R, FHttpResponsePtr Res, bool bOK)
		{ OnImageDownloaded(R, Res, bOK, TileID); });
	Req->SetURL(URL);
	Req->SetVerb("GET");
	Req->ProcessRequest();
}

void AMaterialAPIManager::OnImageDownloaded(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FString TileID)
{
	if (bWasSuccessful && Response.IsValid())
	{
		const TArray<uint8>& Bytes = Response->GetContent();
		auto& IWM = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
		TSharedPtr<IImageWrapper> IW = IWM.CreateImageWrapper(EImageFormat::JPEG);
		if (IW.IsValid() && IW->SetCompressed(Bytes.GetData(), Bytes.Num()))
		{
			TArray<uint8> Raw;
			if (IW->GetRaw(ERGBFormat::BGRA, 8, Raw))
			{
				UTexture2D* Tex = UTexture2D::CreateTransient(IW->GetWidth(), IW->GetHeight(), PF_B8G8R8A8);
				void* Dest = Tex->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
				FMemory::Memcpy(Dest, Raw.GetData(), Raw.Num());
				Tex->GetPlatformData()->Mips[0].BulkData.Unlock();
				Tex->UpdateResource();

				for (auto& T : ParsedTiles)
					if (T.ID == TileID)
						T.DownloadedTexture = Tex;
			}
		}
	}

	PendingImages--;
	if (PendingImages <= 0)
	{
		OnMaterialsReady.Broadcast(ParsedTiles);
	}
}
/*
void AMaterialAPIManager::OnResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Tile JSON request failed."));
		return;
	}

	FString JsonString = Response->GetContentAsString();
	TSharedPtr<FJsonObject> RootObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (FJsonSerializer::Deserialize(Reader, RootObj) && RootObj.IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* TilesArray;
		if (RootObj->TryGetArrayField(TEXT("Tiles"), TilesArray))
		{
			ParsedTiles.Empty(); // Clear previous entries

			for (const TSharedPtr<FJsonValue>& Entry : *TilesArray)
			{
				TSharedPtr<FJsonObject> TileObj = Entry->AsObject();
				if (!TileObj.IsValid()) continue;

				FTileMaterialData TileData;
				TileData.ID = TileObj->GetStringField(TEXT("id"));
				TileData.BaseColorURL = TileObj->GetStringField(TEXT("baseColorUrl"));

				ParsedTiles.Add(TileData);

				// Trigger download
				DownloadTileImage(TileData.BaseColorURL, TileData.ID);
			}

			// Print results
			for (const FTileMaterialData& Tile : ParsedTiles)
			{
				UE_LOG(LogTemp, Log, TEXT("Tile ID: %s | BaseColorURL: %s"), *Tile.ID, *Tile.BaseColorURL);
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Tiles array missing from JSON."));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to parse Tile JSON."));
	}
}

// Step 3: Download image per tile
void AMaterialAPIManager::DownloadTileImage(const FString& URL, const FString& TileID)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> ImageRequest = FHttpModule::Get().CreateRequest();
	ImageRequest->OnProcessRequestComplete().BindLambda(
		[this, TileID](FHttpRequestPtr Req, FHttpResponsePtr Res, bool bSuccess)
		{
			OnImageDownloaded(Req, Res, bSuccess, TileID);
		});
	ImageRequest->SetURL(URL);
	ImageRequest->SetVerb("GET");
	ImageRequest->ProcessRequest();
}

// Step 4: Convert image to texture
void AMaterialAPIManager::OnImageDownloaded(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FString TileID)
{
	if (!bWasSuccessful || !Response.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to download texture for Tile ID: %s"), *TileID);
		return;
	}

	const TArray<uint8>& ImageData = Response->GetContent();

	// Decode image (assumed JPEG, can switch to PNG if needed)
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);

	if (ImageWrapper.IsValid() && ImageWrapper->SetCompressed(ImageData.GetData(), ImageData.Num()))
	{
		TArray<uint8> RawData;
		if (ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
		{
			UTexture2D* Texture = UTexture2D::CreateTransient(
				ImageWrapper->GetWidth(),
				ImageWrapper->GetHeight(),
				PF_B8G8R8A8
			);

			if (Texture)
			{
				// Copy pixel data
				void* TextureData = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
				FMemory::Memcpy(TextureData, RawData.GetData(), RawData.Num());
				Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
				Texture->UpdateResource();

				// Store texture in parsed tile
				for (FTileMaterialData& Tile : ParsedTiles)
				{
					if (Tile.ID == TileID)
					{
						Tile.DownloadedTexture = Texture;
						break;
					}
				}

				UE_LOG(LogTemp, Log, TEXT("Tile ID: %s | Texture downloaded and created."), *TileID);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to create texture for Tile ID: %s"), *TileID);
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to decode raw image data for Tile ID: %s"), *TileID);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Image format not recognized or failed to parse for Tile ID: %s"), *TileID);
	}
}
*/
