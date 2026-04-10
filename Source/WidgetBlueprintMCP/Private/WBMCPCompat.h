// Copyright (c) 2024 XLGames, Inc. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformFileManager.h"
#include "UObject/Package.h"

/**
 * Engine-version compatibility shims for WidgetBlueprintMCP.
 * All engine-version branches are isolated here.
 * Callers do not need to know which engine version is active.
 */
namespace FWBMCPCompat
{
    /**
     * Defers Work to the next GameThread tick.
     * Avoids re-entrant tick deadlock when called from inside an HTTP handler tick.
     */
    inline void DeferToNextTick(TFunction<void()> Work)
    {
#if ENGINE_MAJOR_VERSION >= 5
        FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateLambda([Work = MoveTemp(Work)](float) mutable -> bool
            {
                Work();
                return false;
            }), 0.0f);
#else
        FTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateLambda([Work = MoveTemp(Work)](float) mutable -> bool
            {
                Work();
                return false;
            }), 0.0f);
#endif
    }

    /**
     * Saves Pkg/Asset to Filename.
     * Releases memory-mapped file handle (ResetLoaders) and forces the file writable
     * before saving — no source control checkout required.
     * Returns true on success.
     */
    inline bool SavePackageForce(UPackage* Pkg, UObject* Asset, const FString& Filename)
    {
        ResetLoaders(Pkg);
        FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*Filename, false);

#if ENGINE_MAJOR_VERSION >= 5
        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        SaveArgs.bForceByteSwapping = false;
        SaveArgs.bWarnOfLongFilename = true;
        return UPackage::SavePackage(Pkg, Asset, *Filename, SaveArgs);
#else
        return !!UPackage::SavePackage(Pkg, Asset, RF_Public | RF_Standalone, *Filename,
            nullptr, nullptr, false, true, SAVE_None, nullptr, FDateTime::MinValue(), false);
#endif
    }
}
