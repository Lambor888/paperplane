#include "PaperPlanePawn.h"

#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"

namespace PaperPlaneInput
{
	UInputAction* MakeAction(UObject* Outer, const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(Outer, Name);
		Action->ValueType = Type;
		return Action;
	}

	FEnhancedActionKeyMapping& Map(UInputMappingContext* Context, UInputAction* Action, const FKey& Key, bool bNegate = false, bool bSwizzle = false, bool bDeadZone = false)
	{
		FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, Key);
		if (bDeadZone)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierDeadZone>(Context));
		}
		if (bSwizzle)
		{
			UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(Context);
			Swizzle->Order = EInputAxisSwizzle::YXZ;
			Mapping.Modifiers.Add(Swizzle);
		}
		if (bNegate)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(Context));
		}
		return Mapping;
	}
}

APaperPlanePawn::APaperPlanePawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(
		TEXT("/Game/PaperAirplane/SM_PaperAirplane_SweptWinglet.SM_PaperAirplane_SweptWinglet"));
	if (PlaneMesh.Succeeded())
	{
		Mesh->SetStaticMesh(PlaneMesh.Object);
	}
	Mesh->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	Mesh->SetSimulatePhysics(true);
	Mesh->SetEnableGravity(true);
	// Physics mass only affects collisions with other bodies; flight uses RealMassKg.
	Mesh->SetMassOverrideInKg(NAME_None, 0.1f, true);
	Mesh->SetLinearDamping(0.0f);
	Mesh->SetAngularDamping(0.5f);
	Mesh->BodyInstance.bUseCCD = true;

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(Mesh);
	SpringArm->SetUsingAbsoluteRotation(true);
	SpringArm->TargetArmLength = 90.0f;
	SpringArm->TargetOffset = FVector(0.0f, 0.0f, 12.0f);
	SpringArm->bUsePawnControlRotation = false;
	SpringArm->bDoCollisionTest = true;
	SpringArm->ProbeSize = 4.0f;
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 12.0f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->SetFieldOfView(85.0f);

	TrailRight = CreateDefaultSubobject<UNiagaraComponent>(TEXT("TrailRight"));
	TrailRight->SetupAttachment(Mesh);
	TrailRight->SetRelativeLocation(TrailWingtipOffset);
	TrailRight->SetAutoActivate(false);

	TrailLeft = CreateDefaultSubobject<UNiagaraComponent>(TEXT("TrailLeft"));
	TrailLeft->SetupAttachment(Mesh);
	TrailLeft->SetRelativeLocation(TrailWingtipOffset * FVector(1.0f, -1.0f, 1.0f));
	TrailLeft->SetAutoActivate(false);

	TrailSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/PaperAirplane/FX/NS_PaperPlaneTrail.NS_PaperPlaneTrail")));
}

void APaperPlanePawn::BeginPlay()
{
	Super::BeginPlay();

	TrailRight->SetRelativeLocation(TrailWingtipOffset);
	TrailLeft->SetRelativeLocation(TrailWingtipOffset * FVector(1.0f, -1.0f, 1.0f));
	if (UNiagaraSystem* System = TrailSystem.LoadSynchronous())
	{
		TrailRight->SetAsset(System);
		TrailLeft->SetAsset(System);
	}

	FRotator Nose = Mesh->GetForwardVector().Rotation();
	Nose.Roll = 0.0f;
	CameraBaseRotation = Nose;
	LaunchTimeRemaining = LaunchDuration;
}

void APaperPlanePawn::EnsureInputAssets()
{
	if (MappingContext)
	{
		return;
	}
	using namespace PaperPlaneInput;

	PitchAction = MakeAction(this, TEXT("IA_Pitch"), EInputActionValueType::Axis1D);
	RollAction = MakeAction(this, TEXT("IA_Roll"), EInputActionValueType::Axis1D);
	YawAction = MakeAction(this, TEXT("IA_Yaw"), EInputActionValueType::Axis1D);
	MouseLookAction = MakeAction(this, TEXT("IA_MouseLook"), EInputActionValueType::Axis2D);
	StickLookAction = MakeAction(this, TEXT("IA_StickLook"), EInputActionValueType::Axis2D);

	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_PaperPlane"));

	// Pitch: +1 = stick / W forward.
	Map(MappingContext, PitchAction, EKeys::W);
	Map(MappingContext, PitchAction, EKeys::S, true);
	Map(MappingContext, PitchAction, EKeys::Gamepad_LeftY, false, false, true);

	// Roll: +1 = roll right.
	Map(MappingContext, RollAction, EKeys::D);
	Map(MappingContext, RollAction, EKeys::A, true);
	Map(MappingContext, RollAction, EKeys::Gamepad_LeftX, false, false, true);

	// Yaw: +1 = nose right.
	Map(MappingContext, YawAction, EKeys::E);
	Map(MappingContext, YawAction, EKeys::Q, true);
	Map(MappingContext, YawAction, EKeys::Gamepad_RightShoulder);
	Map(MappingContext, YawAction, EKeys::Gamepad_LeftShoulder, true);

	// Free look.
	Map(MappingContext, MouseLookAction, EKeys::MouseX);
	Map(MappingContext, MouseLookAction, EKeys::MouseY, false, true);
	Map(MappingContext, StickLookAction, EKeys::Gamepad_RightX, false, false, true);
	Map(MappingContext, StickLookAction, EKeys::Gamepad_RightY, false, true, true);
}

void APaperPlanePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	EnsureInputAssets();

	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		Input->BindActionValue(PitchAction);
		Input->BindActionValue(RollAction);
		Input->BindActionValue(YawAction);
		Input->BindActionValue(MouseLookAction);
		Input->BindActionValue(StickLookAction);
	}
}

void APaperPlanePawn::PawnClientRestart()
{
	EnsureInputAssets();
	Super::PawnClientRestart();

	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(MappingContext, 0);
		}
	}
}

float APaperPlanePawn::ReadAxis(const UInputAction* Action) const
{
	const UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent);
	return (Input && Action) ? Input->GetBoundActionValue(Action).Get<float>() : 0.0f;
}

FVector2D APaperPlanePawn::ReadAxis2D(const UInputAction* Action) const
{
	const UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent);
	return (Input && Action) ? Input->GetBoundActionValue(Action).Get<FVector2D>() : FVector2D::ZeroVector;
}

FVector APaperPlanePawn::GetWindVelocityAt_Implementation(const FVector& WorldLocation) const
{
	return FVector::ZeroVector;
}

void APaperPlanePawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (DeltaSeconds <= 0.0f)
	{
		return;
	}

	// Control surfaces travel at a finite rate, so control torques never jump.
	const float Rate = 1.0f / InputRampTime;
	const float PitchUpTarget = (bInvertPitch ? 1.0f : -1.0f) * ReadAxis(PitchAction);
	PitchInput = FMath::FInterpConstantTo(PitchInput, PitchUpTarget, DeltaSeconds, Rate);
	RollInput = FMath::FInterpConstantTo(RollInput, ReadAxis(RollAction), DeltaSeconds, Rate);
	YawInput = FMath::FInterpConstantTo(YawInput, ReadAxis(YawAction), DeltaSeconds, Rate);

	ApplyAerodynamics(DeltaSeconds);
	UpdateCamera(DeltaSeconds);
	UpdateTrails();
}

void APaperPlanePawn::ApplyAerodynamics(float DeltaSeconds)
{
	if (!Mesh->IsSimulatingPhysics())
	{
		return;
	}

	const FTransform& Xf = Mesh->GetComponentTransform();
	const FVector Fwd = Xf.GetUnitAxis(EAxis::X);
	const FVector Right = Xf.GetUnitAxis(EAxis::Y);
	const FVector Up = Xf.GetUnitAxis(EAxis::Z);

	// Airflow relative to the plane, m/s. Wind enters the model only through this.
	const FVector Wind = GetWindVelocityAt(Mesh->GetCenterOfMass());
	const FVector RelVel = (Mesh->GetPhysicsLinearVelocity() - Wind) * 0.01f;
	const float Speed = RelVel.Size();
	Airspeed = Speed;

	const float Vx = FVector::DotProduct(RelVel, Fwd);
	const float Vy = FVector::DotProduct(RelVel, Right);
	const float Vz = FVector::DotProduct(RelVel, Up);
	AngleOfAttack = FMath::Atan2(-Vz, Vx);
	const float SinSideslip = Speed > KINDA_SMALL_NUMBER ? FMath::Clamp(Vy / Speed, -1.0f, 1.0f) : 0.0f;
	const float Sideslip = FMath::Asin(SinSideslip);

	// ---- Linear: lift, drag and side force -> acceleration (a = F / m).
	FVector LinearAccel = FVector::ZeroVector; // m/s^2
	if (Speed > 0.05f)
	{
		const float StallStart = FMath::DegreesToRadians(StallAngleDeg);
		const float StallEnd = StallStart + FMath::DegreesToRadians(StallBlendDeg);
		const float StallBlend = FMath::SmoothStep(StallStart, StallEnd, FMath::Abs(AngleOfAttack));

		const float ClAttached = LiftSlope * AngleOfAttack;
		const float ClStalled = PostStallLiftScale * FMath::Sin(2.0f * AngleOfAttack);
		const float Cl = FMath::Lerp(ClAttached, ClStalled, StallBlend);

		const float SinAoA = FMath::Sin(AngleOfAttack);
		const float Cd = ZeroLiftDrag
			+ InducedDragFactor * FMath::Square(ClAttached) * (1.0f - StallBlend)
			+ PostStallDrag * SinAoA * SinAoA * StallBlend;
		const float Cy = -SideForceSlope * Sideslip;

		const FVector FlowDir = RelVel / Speed;
		const FVector LiftDir = FVector::CrossProduct(FlowDir, Right).GetSafeNormal();
		const float QS = 0.5f * AirDensity * Speed * Speed * WingAreaM2;
		LinearAccel = (LiftDir * Cl - FlowDir * Cd + Right * Cy) * (QS / RealMassKg);
	}

	// Opening throw: a force applied over LaunchDuration, not an instant velocity.
	if (LaunchTimeRemaining > 0.0f)
	{
		const float Portion = FMath::Min(DeltaSeconds, LaunchTimeRemaining) / DeltaSeconds;
		LinearAccel += Fwd * (LaunchSpeed / LaunchDuration) * Portion;
		LaunchTimeRemaining -= DeltaSeconds;
	}

	Mesh->AddForce(LinearAccel * 100.0f, NAME_None, /*bAccelChange*/ true);

	// ---- Angular: stability, damping and control surfaces -> angular acceleration.
	// Body rates: roll right about -X, pitch up about -Y, yaw right about +Z.
	const FVector Omega = Mesh->GetPhysicsAngularVelocityInRadians();
	const float RollRate = FVector::DotProduct(Omega, -Fwd);
	const float PitchRate = FVector::DotProduct(Omega, -Right);
	const float YawRate = FVector::DotProduct(Omega, Up);

	const float SpeedRatio = Speed / ReferenceAirspeed;
	const float Pressure = FMath::Min(SpeedRatio * SpeedRatio, MaxPressureRatio);
	const float DampingScale = FMath::Min(SpeedRatio, FMath::Sqrt(MaxPressureRatio));
	const float Trim = FMath::DegreesToRadians(TrimAngleDeg);

	const float PitchUpAccel = Pressure * (PitchStability * FMath::Sin(Trim - AngleOfAttack) + PitchControl * PitchInput)
		- PitchDamping * DampingScale * PitchRate;
	const float RollRightAccel = Pressure * (-DihedralEffect * SinSideslip + RollControl * RollInput)
		- RollDamping * DampingScale * RollRate;
	const float YawRightAccel = Pressure * (YawStability * SinSideslip + YawControl * YawInput)
		- YawDamping * DampingScale * YawRate;

	const FVector AngularAccel = (-Fwd) * RollRightAccel + (-Right) * PitchUpAccel + Up * YawRightAccel;
	Mesh->AddTorqueInRadians(AngularAccel, NAME_None, /*bAccelChange*/ true);
}

void APaperPlanePawn::UpdateCamera(float DeltaSeconds)
{
	// Chase: follow the nose heading and pitch, never the roll.
	FRotator Nose = Mesh->GetForwardVector().Rotation();
	Nose.Roll = 0.0f;
	CameraBaseRotation = FMath::RInterpTo(CameraBaseRotation, Nose, DeltaSeconds, CameraFollowSpeed);

	// Free look on top of the chase rotation; swings back after a pause.
	const FVector2D Look = ReadAxis2D(MouseLookAction) * MouseLookScale
		+ ReadAxis2D(StickLookAction) * StickLookRate * DeltaSeconds;
	if (!Look.IsNearlyZero())
	{
		LookOffset += Look;
		TimeSinceLookInput = 0.0f;
	}
	else
	{
		TimeSinceLookInput += DeltaSeconds;
		if (TimeSinceLookInput > FreeLookRecenterDelay)
		{
			LookOffset = FMath::Vector2DInterpTo(LookOffset, FVector2D::ZeroVector, DeltaSeconds, FreeLookRecenterSpeed);
		}
	}
	LookOffset.X = FRotator::NormalizeAxis(LookOffset.X);
	LookOffset.Y = FMath::Clamp(LookOffset.Y, -70.0f, 70.0f);

	FRotator ArmRotation = CameraBaseRotation;
	ArmRotation.Yaw += LookOffset.X;
	ArmRotation.Pitch = FMath::Clamp(ArmRotation.Pitch + CameraPitchOffsetDeg + LookOffset.Y, -80.0f, 80.0f);
	SpringArm->SetWorldRotation(ArmRotation);
}

void APaperPlanePawn::UpdateTrails()
{
	if (!TrailLeft->GetAsset())
	{
		return;
	}
	if (!bTrailsActive && Airspeed > TrailStartSpeed)
	{
		bTrailsActive = true;
		TrailLeft->Activate(true);
		TrailRight->Activate(true);
	}
	else if (bTrailsActive && Airspeed < TrailStopSpeed)
	{
		// Stop emitting; existing ribbon segments fade out over their lifetime.
		bTrailsActive = false;
		TrailLeft->Deactivate();
		TrailRight->Deactivate();
	}
}
