// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Commandlets/Commandlet.h"
#include "ACLStatsDumpCommandlet.generated.h"

/**
 * 
 */
UCLASS()
class UACLStatsDumpCommandlet : public UCommandlet
{
	GENERATED_UCLASS_BODY()

public:
	virtual int32 Main(const FString& Params) override;
};

UCLASS(MinimalAPI)
class UACLSkeleton : public USkeleton
{
	GENERATED_UCLASS_BODY()

public:
	FReferenceSkeleton& get_ref_skeleton() { return ReferenceSkeleton; }

#if WITH_EDITOR
	//virtual void PostEditUndo() override { Super::PostEditUndo(); }
#endif
};
