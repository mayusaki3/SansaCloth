#include "SansaClothBackendProbeValidationSystemComponent.h"

#include <AzCore/Math/MathUtils.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace SansaClothBackendProbeValidation
{
    namespace
    {
        constexpr float Tolerance = 1.0e-5f;

        // Canonical: +X right, +Y up, +Z forward.
        // O3DE:      +X right, +Y forward, +Z up.
        AZ::Vector3 CanonicalToO3de(const AZ::Vector3& canonical)
        {
            return AZ::Vector3(canonical.GetX(), canonical.GetZ(), canonical.GetY());
        }

        void LogVector(const char* caseId, const char* key, const AZ::Vector3& value)
        {
            AZ_Printf(
                "SansaClothBackendProbe",
                "SANSA_O3DE|OBF-006.%s.%s|%.9g,%.9g,%.9g\n",
                caseId, key, value.GetX(), value.GetY(), value.GetZ());
        }

        bool CheckGravityCase(
            const char* caseId,
            const AZ::Vector3& canonicalWorldGravity,
            const AZ::Transform& bodyTransform,
            bool expectDifferentLocal)
        {
            // World Gravity is an explicit input, not read from a physics scene
            // and not derived from the body transform.
            const AZ::Vector3 worldGravity = CanonicalToO3de(canonicalWorldGravity);

            // O3DE Transform is used only when a body-local gravity vector is
            // explicitly required. Converting back must recover the world input.
            const AZ::Vector3 bodyLocalGravity =
                bodyTransform.GetInverse().TransformVector(worldGravity);
            const AZ::Vector3 reconstructedWorldGravity =
                bodyTransform.TransformVector(bodyLocalGravity);

            LogVector(caseId, "WORLD_GRAVITY_INPUT", worldGravity);
            LogVector(caseId, "BODY_LOCAL_GRAVITY", bodyLocalGravity);
            LogVector(caseId, "RECONSTRUCTED_WORLD_GRAVITY", reconstructedWorldGravity);

            const bool roundTripMatch =
                reconstructedWorldGravity.IsClose(worldGravity, Tolerance);
            const bool localDifferenceMatch =
                !expectDifferentLocal
                || !bodyLocalGravity.IsClose(worldGravity, Tolerance);
            const bool result = roundTripMatch && localDifferenceMatch;

            AZ_Printf(
                "SansaClothBackendProbe",
                "SANSA_O3DE|OBF-006.%s.RESULT|%s\n",
                caseId, result ? "PASS" : "FAIL");
            return result;
        }
    } // namespace

    void SystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<SystemComponent, AZ::Component>()
                ->Version(1);
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext
                ->Method("RunObf006", &SystemComponent::RunObf006)
                ->Attribute(AZ::Script::Attributes::Module, "sansacloth_probe")
                ->Attribute(
                    AZ::Script::Attributes::Scope,
                    AZ::Script::Attributes::ScopeFlags::Common);
        }
    }

    bool SystemComponent::RunObf006()
    {
        const AZ::Vector3 arbitraryCanonicalGravity(1.25f, -3.75f, 2.5f);
        const AZ::Vector3 verticalCanonicalGravity(0.0f, -9.81f, 0.0f);
        const AZ::Vector3 zeroGravity = AZ::Vector3::CreateZero();

        const AZ::Transform identity = AZ::Transform::CreateIdentity();
        const AZ::Transform rotatedY =
            AZ::Transform::CreateRotationY(AZ::DegToRad(90.0f));
        const AZ::Transform rotatedX =
            AZ::Transform::CreateRotationX(AZ::DegToRad(-90.0f));

        // Non-axis-aligned and vertical inputs cover two distinct directions.
        // Rotations must affect body-local coordinates, not world-space input.
        const bool identityResult =
            CheckGravityCase("C01", arbitraryCanonicalGravity, identity, false);
        const bool rotatedYResult =
            CheckGravityCase("C02", arbitraryCanonicalGravity, rotatedY, true);
        const bool rotatedXResult =
            CheckGravityCase("C03", verticalCanonicalGravity, rotatedX, true);
        const bool zeroResult =
            CheckGravityCase("C04", zeroGravity, rotatedY, false);

        const bool result =
            identityResult && rotatedYResult && rotatedXResult && zeroResult;
        AZ_Printf(
            "SansaClothBackendProbe",
            "SANSA_O3DE|OBF-006.RESULT|%s\n",
            result ? "PASS" : "FAIL");
        return result;
    }
} // namespace SansaClothBackendProbeValidation
