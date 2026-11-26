#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "PaperPlanePawn.generated.h"

class UStaticMeshComponent;
class USpringArmComponent;
class UCameraComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class UInputAction;
class UInputMappingContext;

/**
 * Physics-driven paper plane.
 *
 * Motion only ever changes through forces and torques: gravity (from the physics engine),
 * aerodynamic lift/drag/side force computed from the airflow relative to the plane,
 * control-surface torques from player input, and collision responses. Velocity and
 * rotation are never written directly, so the trajectory always evolves smoothly.
 *
 * Aerodynamics use real-world SI values (mass, wing area, coefficients). Forces are turned
 * into accelerations with the real mass (a = F / m) and applied as acceleration changes,
 * so the physics body's own mass only matters for collisions with other objects.
 */
UCLASS()
class PAPERPLANE_API APaperPlanePawn : public APawn
{
	GENERATED_BODY()

public:
	APaperPlanePawn();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;

	/** Wind velocity in cm/s at a world location. Returns zero until wind volumes exist. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "PaperPlane|Aerodynamics")
	FVector GetWindVelocityAt(const FVector& WorldLocation) const;

	/** Speed of the plane relative to the surrounding air, m/s. */
	UFUNCTION(BlueprintPure, Category = "PaperPlane|Aerodynamics")
	float GetAirspeed() const { return Airspeed; }

	/** Current angle of attack, degrees. */
	UFUNCTION(BlueprintPure, Category = "PaperPlane|Aerodynamics")
	float GetAngleOfAttackDeg() const { return FMath::RadiansToDegrees(AngleOfAttack); }

protected:
	virtual void BeginPlay() override;

	// ---------------------------------------------------------------- Components

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UNiagaraComponent> TrailLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UNiagaraComponent> TrailRight;

	// ---------------------------------------------------------------- Aerodynamics (SI units)

	/** Real mass of the paper plane, kg. Used for a = F / m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PaperPlane|Aerodynamics", meta = (ClampMin = "0.0001", Units = "kg"))
	float RealMassKg = 0.005f;

	/** Reference wing area, m^2. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PaperPlane|Aerodynamics", meta = (ClampMin = "0.0001"))
	float WingAreaM2 = 0.02f;

	/** Air density, kg/m^3. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PaperPlane|Aerodynamics", meta = (ClampMin = "0.0"))
	float AirDensity = 1.225f;

	/** Lift coefficient gained per radian of angle of attack before the stall. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PaperPlane|Aerodynamics", meta = (ClampMin = "0.0"))
	float LiftSlope = 3.0f;

	/** Angle of attack where the wing starts to stall, degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PaperPlane|Aerodynamics", meta = (ClampMin = "1.0", ClampMax = "45.0"))
	float StallAngleDeg = 14.0f;

	/** Width of the smooth transition from attached flow to full stall, degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PaperPlane|Aerodynamics", meta = (ClampMin = "1.0", ClampMax = "45.0"))
	float StallBlendDeg = 8.0f;

	/** Peak lift coefficient of the stalled (flat plate) wing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PaperPlane|Aerodynamics", meta = (ClampMin = "0.0"))
	float PostStallLiftScale = 0.6f;

	/** Drag coefficient at zero lift. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PaperPlane|Aerodynamics", meta = (ClampMin = "0.0"))
	float ZeroLiftDrag = 0.03f;

	/** Induced drag factor: extra drag = factor * Cl^2. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PaperPlane|Aerodynamics", meta = (ClampMin = "0.0"))
	float InducedDragFactor = 0.1f;

	/** Drag coefficient of the fully stalled wing broadside to the flow. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PaperPlane|Aerodynamics", meta = (ClampMin = "0.0"))
	float PostStallDrag = 1.2f;

	/** Side force coefficient per radian of sideslip (keel and winglets). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PaperPlane|Aerodynamics", meta = (ClampMin = "0.0"))
	float SideForceSlope = 0.6f;

	// ---------------------------------------------------------------- Rotation (angular accelerations, rad/s^2)

	/** Angle of attack the plane naturally settles to; sets the cruise speed. Degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PaperPlane|Rotation")
	float TrimAngleDeg = 4.0f;

	/** Airspeed (m/s) at which the stability, damping and control gains below apply 1:1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PaperPlane|Rotation", meta = (ClampMin = "0.1"))
	float ReferenceAirspeed = 5.0f;

	/** Upper bound of (airspeed / reference)^2 so very high speeds stay controllable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PaperPlane|Rotation", meta = (ClampMin = "1.0"))
	float MaxPressureRatio = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PaperPlane|Rotation")
	float PitchStability = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PaperPlane|Rotation")
	float YawStability = 20.0f;

	/** Roll caused by sideslip (the upturned winglets act as dihedral). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PaperPlane|Rotation")
	float DihedralEffect = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PaperPlane|Rotation")
	float PitchDamping = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PaperPlane|Rotation")
	float RollDamping = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PaperPlane|Rotation")
	float YawDamping = 6.0f;

	// ---------------------------------------------------------------- Controls

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PaperPlane|Controls")
	float PitchControl = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PaperPlane|Controls")
	float RollControl = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PaperPlane|Controls")
	float YawControl = 15.0f;

	/** Seconds for a control surface to travel from neutral to full deflection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PaperPlane|Controls", meta = (ClampMin = "0.01", Units = "s"))
	float InputRampTime = 0.15f;

	/** Default: W / stick forward = nose down. Enable to make W pull the nose up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PaperPlane|Controls")
	bool bInvertPitch = false;

	// ---------------------------------------------------------------- Launch

	/** Forward speed reached by the opening throw, m/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PaperPlane|Launch", meta = (ClampMin = "0.0"))
	float LaunchSpeed = 5.0f;

	/** Duration of the throw; the launch speed is built up by a force over this time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PaperPlane|Launch", meta = (ClampMin = "0.01", Units = "s"))
	float LaunchDuration = 0.2f;

	// ---------------------------------------------------------------- Camera

	/** How quickly the chase camera turns to follow the nose. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PaperPlane|Camera", meta = (ClampMin = "0.0"))
	float CameraFollowSpeed = 4.0f;

	/** Constant downward tilt of the chase camera, degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PaperPlane|Camera")
	float CameraPitchOffsetDeg = -12.0f;

	/** Degrees of free look per unit of mouse movement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PaperPlane|Camera", meta = (ClampMin = "0.0"))
	float MouseLookScale = 0.5f;

	/** Free look turn rate at full right-stick deflection, degrees per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PaperPlane|Camera", meta = (ClampMin = "0.0"))
	float StickLookRate = 140.0f;

	/** Seconds without look input before the camera swings back behind the plane. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PaperPlane|Camera", meta = (ClampMin = "0.0", Units = "s"))
	float FreeLookRecenterDelay = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PaperPlane|Camera", meta = (ClampMin = "0.0"))
	float FreeLookRecenterSpeed = 3.0f;

	// ---------------------------------------------------------------- Trails

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PaperPlane|Trails")
	TSoftObjectPtr<UNiagaraSystem> TrailSystem;

	/** Trail emitter position on the right wingtip, local cm. The left one is mirrored. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PaperPlane|Trails")
	FVector TrailWingtipOffset = FVector(-8.0f, 9.3f, 3.2f);

	/** Airspeed (m/s) above which trails start. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PaperPlane|Trails", meta = (ClampMin = "0.0"))
	float TrailStartSpeed = 6.5f;

	/** Airspeed (m/s) below which trails stop emitting and fade out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PaperPlane|Trails", meta = (ClampMin = "0.0"))
	float TrailStopSpeed = 5.5f;

private:
	void EnsureInputAssets();
	void ApplyAerodynamics(float DeltaSeconds);
	void UpdateCamera(float DeltaSeconds);
	void UpdateTrails();
	float ReadAxis(const UInputAction* Action) const;
	FVector2D ReadAxis2D(const UInputAction* Action) const;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PitchAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> RollAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> YawAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MouseLookAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> StickLookAction;

	float PitchInput = 0.0f;
	float RollInput = 0.0f;
	float YawInput = 0.0f;

	FRotator CameraBaseRotation = FRotator::ZeroRotator;
	FVector2D LookOffset = FVector2D::ZeroVector;
	float TimeSinceLookInput = 0.0f;

	float Airspeed = 0.0f;
	float AngleOfAttack = 0.0f;
	float LaunchTimeRemaining = 0.0f;
	bool bTrailsActive = false;
};
