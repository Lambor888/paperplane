#include "PaperPlaneGameMode.h"

#include "PaperPlanePawn.h"

APaperPlaneGameMode::APaperPlaneGameMode()
{
	DefaultPawnClass = APaperPlanePawn::StaticClass();
}
