#include "SansaClothBackendProbeValidationSystemComponent.h"

#include <AzCore/Math/MathUtils.h>
#include <AzCore/Math/Vector2.h>

#include <array>
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
            const bool localDifferenceMatch = expectDifferentLocal
                ? !bodyLocalGravity.IsClose(worldGravity, Tolerance)
                : bodyLocalGravity.IsClose(worldGravity, Tolerance);
            const bool result = roundTripMatch && localDifferenceMatch;

            AZ_Printf(
                "SansaClothBackendProbe",
                "SANSA_O3DE|OBF-006.%s.RESULT|%s\n",
                caseId, result ? "PASS" : "FAIL");
            return result;
        }

        // BF-003: validation-only single-domain fixture. The logical key is
        // (domainId, u, v); triangle indices are ephemeral resolver output.
        struct SurfaceReference
        {
            AZ::u64 m_domainId;
            AZ::Vector2 m_uv;
        };

        struct SurfaceResolveResult
        {
            AZ::Vector3 m_localPosition = AZ::Vector3::CreateZero();
            int m_resolvedTriangle = -1;
        };

        constexpr AZ::u64 FixtureDomainId = 1;

        bool ResolveSurfaceReference(
            const SurfaceReference& reference,
            const std::array<AZ::Vector3, 4>& vertices,
            const std::array<AZ::Vector2, 4>& uvs,
            const std::array<int, 6>& triangleIndices,
            SurfaceResolveResult& result)
        {
            if (reference.m_domainId != FixtureDomainId)
            {
                return false;
            }

            const float u = reference.m_uv.GetX();
            const float v = reference.m_uv.GetY();
            if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f)
            {
                return false;
            }

            constexpr float DegenerateEpsilon = 1.0e-8f;
            constexpr float InsideEpsilon = 1.0e-6f;

            for (int triangle = 0; triangle < 2; ++triangle)
            {
                const int i0 = triangleIndices[triangle * 3];
                const int i1 = triangleIndices[triangle * 3 + 1];
                const int i2 = triangleIndices[triangle * 3 + 2];

                const AZ::Vector2& a = uvs[i0];
                const AZ::Vector2& b = uvs[i1];
                const AZ::Vector2& c = uvs[i2];
                const float abU = b.GetX() - a.GetX();
                const float abV = b.GetY() - a.GetY();
                const float acU = c.GetX() - a.GetX();
                const float acV = c.GetY() - a.GetY();
                const float apU = u - a.GetX();
                const float apV = v - a.GetY();
                const float denominator = abU * acV - acU * abV;

                if (AZ::GetAbs(denominator) <= DegenerateEpsilon)
                {
                    continue;
                }

                const float w1 = (apU * acV - acU * apV) / denominator;
                const float w2 = (abU * apV - apU * abV) / denominator;
                const float w0 = 1.0f - w1 - w2;

                if (w0 >= -InsideEpsilon && w1 >= -InsideEpsilon
                    && w2 >= -InsideEpsilon)
                {
                    result.m_localPosition =
                        vertices[i0] * w0 + vertices[i1] * w1 + vertices[i2] * w2;
                    result.m_resolvedTriangle = triangle;
                    return true;
                }
            }
            return false;
        }

        void LogBf003Vector(const char* key, const AZ::Vector3& value)
        {
            AZ_Printf(
                "SansaClothBackendProbe",
                "SANSA_O3DE|BF-003.%s|%.9g,%.9g,%.9g\n",
                key, value.GetX(), value.GetY(), value.GetZ());
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
            behaviorContext->Method("RunBf003", &SystemComponent::RunBf003)
                ->Attribute(AZ::Script::Attributes::Module, "sansacloth_probe")
                ->Attribute(
                    AZ::Script::Attributes::Scope,
                    AZ::Script::Attributes::ScopeFlags::Common);
            behaviorContext->Method("RunObf006", &SystemComponent::RunObf006)
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

    bool SystemComponent::RunBf003()
    {
        // Reference flat fixture: 0.20m x 0.10m. The canonical Y/Z
        // permutation reverses orientation, so O3DE triangle winding is
        // reversed relative to the canonical fixture.
        const std::array<AZ::Vector3, 4> vertices = {
            AZ::Vector3(-0.10f, -0.05f, 0.0f), // V00
            AZ::Vector3(-0.10f,  0.05f, 0.0f), // V01
            AZ::Vector3( 0.10f, -0.05f, 0.0f), // V10
            AZ::Vector3( 0.10f,  0.05f, 0.0f)  // V11
        };
        const std::array<AZ::Vector2, 4> uvs = {
            AZ::Vector2(0.0f, 0.0f),
            AZ::Vector2(0.0f, 1.0f),
            AZ::Vector2(1.0f, 0.0f),
            AZ::Vector2(1.0f, 1.0f)
        };
        const std::array<int, 6> triangles = { 0, 3, 1, 0, 2, 3 };
        const std::array<int, 6> reorderedTriangles = { 0, 2, 3, 0, 3, 1 };
        const SurfaceReference reference{ FixtureDomainId, AZ::Vector2(0.25f, 0.75f) };

        SurfaceResolveResult identityResult;
        SurfaceResolveResult reorderedResult;
        const bool identityResolved =
            ResolveSurfaceReference(reference, vertices, uvs, triangles, identityResult);
        const bool reorderedResolved =
            ResolveSurfaceReference(reference, vertices, uvs, reorderedTriangles, reorderedResult);

        // canonical position (-0.05,0,0.025) -> O3DE (-0.05,0.025,0).
        const AZ::Vector3 expectedLocal(-0.05f, 0.025f, 0.0f);
        const bool identityMatch =
            identityResolved && identityResult.m_localPosition.IsClose(expectedLocal, Tolerance);

        // canonical rotation Z -90 degrees -> O3DE rotation Y +90 degrees.
        // canonical translation (0.30,0.20,-0.10) -> O3DE (0.30,-0.10,0.20).
        AZ::Transform bodyTransform = AZ::Transform::CreateRotationY(AZ::DegToRad(90.0f));
        bodyTransform.SetTranslation(AZ::Vector3(0.30f, -0.10f, 0.20f));
        const AZ::Vector3 transformedPosition =
            bodyTransform.TransformPoint(identityResult.m_localPosition);
        const AZ::Vector3 expectedTransformed(0.30f, -0.075f, 0.25f);
        const bool transformMatch =
            identityResolved && transformedPosition.IsClose(expectedTransformed, Tolerance);

        const bool reorderedMatch =
            reorderedResolved
            && reorderedResult.m_localPosition.IsClose(expectedLocal, Tolerance)
            && identityResult.m_resolvedTriangle != reorderedResult.m_resolvedTriangle;

        SurfaceResolveResult invalidResult;
        const bool invalidDomainRejected = !ResolveSurfaceReference(
            SurfaceReference{ 2, reference.m_uv }, vertices, uvs, triangles, invalidResult);
        const bool invalidUvRejected = !ResolveSurfaceReference(
            SurfaceReference{ FixtureDomainId, AZ::Vector2(1.25f, 0.75f) },
            vertices, uvs, triangles, invalidResult);

        AZ_Printf("SansaClothBackendProbe",
            "SANSA_O3DE|BF-003.DOMAIN_ID|%llu\n",
            static_cast<unsigned long long>(reference.m_domainId));
        AZ_Printf("SansaClothBackendProbe",
            "SANSA_O3DE|BF-003.SURFACE_REFERENCE_UV|%.9g,%.9g\n",
            reference.m_uv.GetX(), reference.m_uv.GetY());
        AZ_Printf("SansaClothBackendProbe",
            "SANSA_O3DE|BF-003.RESOLVED_TRIANGLE|%d\n",
            identityResult.m_resolvedTriangle);
        AZ_Printf("SansaClothBackendProbe",
            "SANSA_O3DE|BF-003.REORDERED_RESOLVED_TRIANGLE|%d\n",
            reorderedResult.m_resolvedTriangle);
        LogBf003Vector("IDENTITY_SURFACE_POSITION", identityResult.m_localPosition);
        LogBf003Vector("TRANSFORMED_SURFACE_POSITION", transformedPosition);
        LogBf003Vector("REORDERED_SURFACE_POSITION", reorderedResult.m_localPosition);

        const bool result =
            identityMatch && transformMatch && reorderedMatch
            && invalidDomainRejected && invalidUvRejected;
        AZ_Printf("SansaClothBackendProbe",
            "SANSA_O3DE|BF-003.IDENTITY_RESULT|%s\n", identityMatch ? "PASS" : "FAIL");
        AZ_Printf("SansaClothBackendProbe",
            "SANSA_O3DE|BF-003.TRANSFORM_RESULT|%s\n", transformMatch ? "PASS" : "FAIL");
        AZ_Printf("SansaClothBackendProbe",
            "SANSA_O3DE|BF-003.TRIANGLE_REORDER_RESULT|%s\n",
            reorderedMatch ? "PASS" : "FAIL");
        AZ_Printf("SansaClothBackendProbe",
            "SANSA_O3DE|BF-003.INVALID_DOMAIN_REJECTED|%s\n",
            invalidDomainRejected ? "TRUE" : "FALSE");
        AZ_Printf("SansaClothBackendProbe",
            "SANSA_O3DE|BF-003.INVALID_UV_REJECTED|%s\n",
            invalidUvRejected ? "TRUE" : "FALSE");
        AZ_Printf("SansaClothBackendProbe",
            "SANSA_O3DE|BF-003.RESULT|%s\n", result ? "PASS" : "FAIL");
        return result;
    }

} // namespace SansaClothBackendProbeValidation
