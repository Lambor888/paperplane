#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PaperPlaneGameMode.generated.h"

/** Spawns the player as a paper plane. */
UCLASS()
class PAPERPLANE_API APaperPlaneGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APaperPlaneGameMode();
};
