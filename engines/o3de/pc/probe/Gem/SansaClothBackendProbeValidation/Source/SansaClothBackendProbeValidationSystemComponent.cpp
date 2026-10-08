#include "SansaClothBackendProbeValidationSystemComponent.h"

#include <AzCore/Math/MathUtils.h>
#include <AzCore/std/string/string.h>
#include <AzCore/Math/Vector2.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
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

        struct SurfaceQueryResult
        {
            AZ::Vector3 m_worldPosition = AZ::Vector3::CreateZero();
            AZ::Vector3 m_worldNormal = AZ::Vector3::CreateZero();
            float m_signedSeparation = 0.0f;
        };

        bool QuerySurface(
            const SurfaceReference& reference,
            const std::array<AZ::Vector3, 4>& vertices,
            const std::array<AZ::Vector2, 4>& uvs,
            const std::array<int, 6>& triangleIndices,
            const AZ::Transform& localToWorld,
            const AZ::Vector3& currentWorldPosition,
            SurfaceQueryResult& result)
        {
            SurfaceResolveResult resolved;
            if (!ResolveSurfaceReference(reference, vertices, uvs, triangleIndices, resolved))
            {
                return false;
            }

            const int base = resolved.m_resolvedTriangle * 3;
            const int i0 = triangleIndices[base];
            const int i1 = triangleIndices[base + 1];
            const int i2 = triangleIndices[base + 2];
            const AZ::Vector3 edge1 = vertices[i1] - vertices[i0];
            const AZ::Vector3 edge2 = vertices[i2] - vertices[i0];
            const AZ::Vector3 geometricNormal = edge1.Cross(edge2);
            if (geometricNormal.GetLengthSq() <= 1.0e-12f)
            {
                return false;
            }

            // Rigid transform only. Non-uniform scale requires inverse transpose.
            result.m_worldPosition = localToWorld.TransformPoint(resolved.m_localPosition);
            result.m_worldNormal =
                localToWorld.TransformVector(geometricNormal.GetNormalized()).GetNormalized();
            result.m_signedSeparation =
                (currentWorldPosition - result.m_worldPosition).Dot(result.m_worldNormal);
            return true;
        }

        void LogBf004Vector(const char* key, const AZ::Vector3& value)
        {
            AZ_Printf(
                "SansaClothBackendProbe",
                "SANSA_O3DE|BF-004.%s|%.9g,%.9g,%.9g\n",
                key, value.GetX(), value.GetY(), value.GetZ());
        }

        void LogBf004Scalar(const char* key, float value)
        {
            AZ_Printf(
                "SansaClothBackendProbe",
                "SANSA_O3DE|BF-004.%s|%.9g\n", key, value);
        }

        void LogBf004Check(const char* key, bool result)
        {
            AZ_Printf(
                "SansaClothBackendProbe",
                "SANSA_O3DE|BF-004.%s|%s\n", key, result ? "PASS" : "FAIL");
        }

        // BF-007 reproduces Unity's analytic SR-001..005 fixture formulas.
        // Canonical +Y up is mapped to O3DE +Z up. The side fixture uses
        // O3DE +90deg around Y, equivalent to canonical -90deg around Z.
        constexpr int FixtureUSamples = 21;
        constexpr int FixtureVSamples = 7;
        constexpr int FixtureVertexCount = FixtureUSamples * FixtureVSamples;
        constexpr int FixtureTriangleCount =
            (FixtureUSamples - 1) * (FixtureVSamples - 1) * 2;
        constexpr float FixtureWidthM = 0.20f;
        constexpr float FixtureDepthM = 0.10f;
        constexpr float FixtureFeatureWidthM = 0.10f;
        constexpr float FixturePi = 3.14159265358979323846f;

        enum class BasicFixtureKind
        {
            Flat,
            Convex,
            Concave
        };

        struct BasicFixtureCase
        {
            const char* m_key;
            BasicFixtureKind m_kind;
            float m_magnitudeM;
            bool m_sideOrientation;
            bool m_oneEdgeAnchor;
            float m_expectedMinHeightM;
            float m_expectedMaxHeightM;
            AZ::Vector3 m_expectedCenterPosition;
            AZ::Vector3 m_expectedNormal;
            int m_expectedAnchorCount;
        };

        float BasicFixtureRaisedCosine(float x, float heightM)
        {
            if (AZ::GetAbs(x) > FixtureFeatureWidthM * 0.5f)
            {
                return 0.0f;
            }
            return heightM * 0.5f
                * (1.0f + std::cos(2.0f * FixturePi * x / FixtureFeatureWidthM));
        }

        float BasicFixtureRaisedCosineDerivative(float x, float heightM)
        {
            if (AZ::GetAbs(x) > FixtureFeatureWidthM * 0.5f)
            {
                return 0.0f;
            }
            return -(heightM * FixturePi / FixtureFeatureWidthM)
                * std::sin(2.0f * FixturePi * x / FixtureFeatureWidthM);
        }

        AZ::Vector3 BasicFixtureLocalPosition(
            BasicFixtureKind kind, float magnitudeM, float u, float v)
        {
            const float x = (u - 0.5f) * FixtureWidthM;
            const float y = (v - 0.5f) * FixtureDepthM;
            const float feature = BasicFixtureRaisedCosine(x, magnitudeM);
            const float height = kind == BasicFixtureKind::Flat
                ? 0.0f
                : (kind == BasicFixtureKind::Convex ? feature : -feature);
            return AZ::Vector3(x, y, height);
        }

        AZ::Vector3 BasicFixtureLocalNormal(
            BasicFixtureKind kind, float magnitudeM, float u)
        {
            const float x = (u - 0.5f) * FixtureWidthM;
            const float derivative = BasicFixtureRaisedCosineDerivative(x, magnitudeM);
            const float dzdx = kind == BasicFixtureKind::Flat
                ? 0.0f
                : (kind == BasicFixtureKind::Convex ? derivative : -derivative);
            return AZ::Vector3(-dzdx, 0.0f, 1.0f).GetNormalized();
        }

        bool ProbeBasicFixture(const BasicFixtureCase& fixture)
        {
            const AZ::Transform rotation = fixture.m_sideOrientation
                ? AZ::Transform::CreateRotationY(AZ::DegToRad(90.0f))
                : AZ::Transform::CreateIdentity();

            std::array<AZ::Vector3, FixtureVertexCount> positions{};
            int vertexCount = 0;
            int cpCount = 0;
            int anchorCount = 0;
            float minLocalHeightM = 1.0e10f;
            float maxLocalHeightM = -1.0e10f;
            AZ::Vector3 centerPosition = AZ::Vector3::CreateZero();
            AZ::Vector3 centerNormal = AZ::Vector3::CreateZero();

            for (int vIndex = 0; vIndex < FixtureVSamples; ++vIndex)
            {
                const float v = static_cast<float>(vIndex) / (FixtureVSamples - 1);
                for (int uIndex = 0; uIndex < FixtureUSamples; ++uIndex)
                {
                    const float u = static_cast<float>(uIndex) / (FixtureUSamples - 1);
                    const AZ::Vector3 localPosition =
                        BasicFixtureLocalPosition(fixture.m_kind, fixture.m_magnitudeM, u, v);
                    const AZ::Vector3 worldPosition = rotation.TransformPoint(localPosition);
                    positions[vIndex * FixtureUSamples + uIndex] = worldPosition;

                    minLocalHeightM = std::min(minLocalHeightM, localPosition.GetZ());
                    maxLocalHeightM = std::max(maxLocalHeightM, localPosition.GetZ());
                    ++vertexCount;
                    ++cpCount;

                    const bool anchor = fixture.m_oneEdgeAnchor
                        ? uIndex == 0
                        : (uIndex == 0 || uIndex == FixtureUSamples - 1);
                    anchorCount += anchor ? 1 : 0;

                    if (uIndex == FixtureUSamples / 2 && vIndex == FixtureVSamples / 2)
                    {
                        centerPosition = worldPosition;
                        centerNormal = rotation.TransformVector(
                            BasicFixtureLocalNormal(
                                fixture.m_kind, fixture.m_magnitudeM, u)).GetNormalized();
                    }
                }
            }

            // Unity's V00,V01,V11 order points outward in canonical space.
            // O3DE Y/Z basis swap reverses the handedness, so use V00,V11,V01.
            const AZ::Vector3 v00 = positions[0];
            const AZ::Vector3 v01 = positions[FixtureUSamples];
            const AZ::Vector3 v11 = positions[FixtureUSamples + 1];
            const AZ::Vector3 firstCross = (v11 - v00).Cross(v01 - v00);
            const bool firstTriangleNondegenerate = firstCross.GetLengthSq() > 1.0e-12f;
            const AZ::Vector3 firstTriangleNormal = firstTriangleNondegenerate
                ? firstCross.GetNormalized() : AZ::Vector3::CreateZero();

            // Every grid cell is represented by two triangles, not merely
            // an inferred count. Check degeneracy and winding across all 240.
            int triangleCount = 0;
            bool allTrianglesNondegenerate = true;
            bool allTrianglesOutward = true;
            for (int vIndex = 0; vIndex < FixtureVSamples - 1; ++vIndex)
            {
                for (int uIndex = 0; uIndex < FixtureUSamples - 1; ++uIndex)
                {
                    const int v00Index = vIndex * FixtureUSamples + uIndex;
                    const int v01Index = (vIndex + 1) * FixtureUSamples + uIndex;
                    const int v11Index = v01Index + 1;
                    const int v10Index = v00Index + 1;
                    const std::array<std::array<int, 3>, 2> cellTriangles = {{
                        {{ v00Index, v11Index, v01Index }},
                        {{ v00Index, v10Index, v11Index }}
                    }};
                    for (const auto& indices : cellTriangles)
                    {
                        const AZ::Vector3& a = positions[indices[0]];
                        const AZ::Vector3& b = positions[indices[1]];
                        const AZ::Vector3& d = positions[indices[2]];
                        const AZ::Vector3 geometricCross = (b - a).Cross(d - a);
                        const bool nondegenerate =
                            geometricCross.GetLengthSq() > 1.0e-12f;
                        allTrianglesNondegenerate &= nondegenerate;
                        if (nondegenerate)
                        {
                            // +Z (top) or +X (side) component must be positive.
                            allTrianglesOutward &=
                                geometricCross.GetNormalized().Dot(fixture.m_expectedNormal)
                                > 0.0f;
                        }
                        ++triangleCount;
                    }
                }
            }

            const bool countsMatch = vertexCount == FixtureVertexCount
                && cpCount == FixtureVertexCount
                && triangleCount == FixtureTriangleCount;
            const bool anchorsMatch = anchorCount == fixture.m_expectedAnchorCount;
            const bool heightRangeMatches =
                AZ::GetAbs(minLocalHeightM - fixture.m_expectedMinHeightM) <= Tolerance
                && AZ::GetAbs(maxLocalHeightM - fixture.m_expectedMaxHeightM) <= Tolerance;
            const bool centerMatches =
                centerPosition.IsClose(fixture.m_expectedCenterPosition, Tolerance);
            const bool centerNormalMatches =
                centerNormal.IsClose(fixture.m_expectedNormal, Tolerance);
            const bool firstTriangleNormalMatches = firstTriangleNondegenerate
                && firstTriangleNormal.IsClose(fixture.m_expectedNormal, Tolerance);
            const bool triangleGeometryMatches =
                allTrianglesNondegenerate && allTrianglesOutward;

            AZ_Printf("SansaClothBackendProbe",
                "SANSA_O3DE|BF-007.%s.VERTEX_COUNT|%d\n",
                fixture.m_key, vertexCount);
            AZ_Printf("SansaClothBackendProbe",
                "SANSA_O3DE|BF-007.%s.TRIANGLE_COUNT|%d\n",
                fixture.m_key, triangleCount);
            AZ_Printf("SansaClothBackendProbe",
                "SANSA_O3DE|BF-007.%s.CP_COUNT|%d\n",
                fixture.m_key, cpCount);
            AZ_Printf("SansaClothBackendProbe",
                "SANSA_O3DE|BF-007.%s.ANCHOR_COUNT|%d\n",
                fixture.m_key, anchorCount);
            AZ_Printf("SansaClothBackendProbe",
                "SANSA_O3DE|BF-007.%s.LOCAL_MIN_HEIGHT_M|%.9g\n",
                fixture.m_key, minLocalHeightM);
            AZ_Printf("SansaClothBackendProbe",
                "SANSA_O3DE|BF-007.%s.LOCAL_MAX_HEIGHT_M|%.9g\n",
                fixture.m_key, maxLocalHeightM);
            AZ_Printf("SansaClothBackendProbe",
                "SANSA_O3DE|BF-007.%s.CENTER_POSITION|%.9g,%.9g,%.9g\n",
                fixture.m_key, centerPosition.GetX(), centerPosition.GetY(),
                centerPosition.GetZ());
            AZ_Printf("SansaClothBackendProbe",
                "SANSA_O3DE|BF-007.%s.CENTER_NORMAL|%.9g,%.9g,%.9g\n",
                fixture.m_key, centerNormal.GetX(), centerNormal.GetY(),
                centerNormal.GetZ());
            AZ_Printf("SansaClothBackendProbe",
                "SANSA_O3DE|BF-007.%s.FIRST_TRIANGLE_NORMAL|%.9g,%.9g,%.9g\n",
                fixture.m_key, firstTriangleNormal.GetX(),
                firstTriangleNormal.GetY(), firstTriangleNormal.GetZ());

            const std::array<std::pair<const char*, bool>, 7> checks = {{
                { "COUNTS", countsMatch },
                { "ANCHORS", anchorsMatch },
                { "HEIGHT_RANGE", heightRangeMatches },
                { "CENTER_POSITION", centerMatches },
                { "CENTER_NORMAL", centerNormalMatches },
                { "FIRST_TRIANGLE_NORMAL", firstTriangleNormalMatches },
                { "ALL_TRIANGLE_GEOMETRY", triangleGeometryMatches }
            }};
            bool passed = true;
            for (const auto& check : checks)
            {
                AZ_Printf("SansaClothBackendProbe",
                    "SANSA_O3DE|BF-007.%s.%s.RESULT|%s\n",
                    fixture.m_key, check.first, check.second ? "PASS" : "FAIL");
                passed &= check.second;
            }
            AZ_Printf("SansaClothBackendProbe",
                "SANSA_O3DE|BF-007.%s.RESULT|%s\n",
                fixture.m_key, passed ? "PASS" : "FAIL");
            return passed;
        }

        // BF-005/BF-006: validation-only SR-001-C0-G0 semantic boundary.
        // Contact and anchor are independent input facts; only anchors provide
        // direct support. A nonzero gravity/conformity case is not simulated.
        constexpr int Sr001USamples = 21;
        constexpr int Sr001VSamples = 7;
        constexpr int Sr001ControlPointCount = Sr001USamples * Sr001VSamples;
        constexpr float Sr001WidthM = 0.20f;
        constexpr float Sr001DepthM = 0.10f;
        constexpr float Sr001CollisionToleranceM = 0.0f;

        struct Sr001Input
        {
            int m_stableId = -1;
            SurfaceReference m_reference{ 0, AZ::Vector2::CreateZero() };
            AZ::Vector3 m_initialPosition = AZ::Vector3::CreateZero();
            bool m_anchor = false;
            bool m_contact = false;
        };

        struct Sr001Output
        {
            int m_stableId = -1;
            SurfaceReference m_reference{ 0, AZ::Vector2::CreateZero() };
            AZ::Vector3 m_finalPosition = AZ::Vector3::CreateZero();
            AZ::Vector3 m_surfaceNormal = AZ::Vector3::CreateZero();
            float m_separationM = 0.0f;
            bool m_directSupport = false;
        };

        bool MapSr001Output(
            const Sr001Input& input,
            const AZ::Vector3& worldGravity,
            float conformity,
            const std::array<AZ::Vector3, 4>& surfaceVertices,
            const std::array<AZ::Vector2, 4>& surfaceUvs,
            const std::array<int, 6>& surfaceTriangles,
            Sr001Output& output)
        {
            // C=0/G=0 is the only supported no-deformation validation case.
            // Reject other inputs rather than falsely claiming a solver result.
            if (!worldGravity.IsZero(Tolerance) || AZ::GetAbs(conformity) > Tolerance)
            {
                return false;
            }

            output.m_stableId = input.m_stableId;
            output.m_reference = input.m_reference;
            output.m_finalPosition = input.m_initialPosition;
            output.m_directSupport = input.m_anchor;

            SurfaceQueryResult query;
            if (!QuerySurface(
                input.m_reference, surfaceVertices, surfaceUvs, surfaceTriangles,
                AZ::Transform::CreateIdentity(), output.m_finalPosition, query))
            {
                return false;
            }
            if (!query.m_worldPosition.IsClose(output.m_finalPosition, Tolerance))
            {
                return false;
            }
            output.m_surfaceNormal = query.m_worldNormal;
            output.m_separationM = query.m_signedSeparation;
            return true;
        }

        void LogSr001Scalar(const char* key, int value)
        {
            AZ_Printf(
                "SansaClothBackendProbe",
                "SANSA_O3DE|SR-001.%s|%d\n", key, value);
        }

        void LogSr001Float(const char* key, float value)
        {
            AZ_Printf(
                "SansaClothBackendProbe",
                "SANSA_O3DE|SR-001.%s|%.9g\n", key, value);
        }

        void LogSr001Vector(const char* key, const AZ::Vector3& value)
        {
            AZ_Printf(
                "SansaClothBackendProbe",
                "SANSA_O3DE|SR-001.%s|%.9g,%.9g,%.9g\n",
                key, value.GetX(), value.GetY(), value.GetZ());
        }

        void LogSr001Sample(const char* label, const Sr001Output& output)
        {
            AZ_Printf(
                "SansaClothBackendProbe",
                "SANSA_O3DE|SR-001.%s|id=%d;domain=%llu;uv=%.9g,%.9g;"
                "position=%.9g,%.9g,%.9g;normal=%.9g,%.9g,%.9g;"
                "separation_m=%.9g;support=%s\n",
                label, output.m_stableId,
                static_cast<unsigned long long>(output.m_reference.m_domainId),
                output.m_reference.m_uv.GetX(), output.m_reference.m_uv.GetY(),
                output.m_finalPosition.GetX(), output.m_finalPosition.GetY(),
                output.m_finalPosition.GetZ(),
                output.m_surfaceNormal.GetX(), output.m_surfaceNormal.GetY(),
                output.m_surfaceNormal.GetZ(), output.m_separationM,
                output.m_directSupport ? "Anchor" : "Unsupported");
        }

        void LogBoundaryCheck(const char* gate, const char* key, bool passed)
        {
            AZ_Printf(
                "SansaClothBackendProbe",
                "SANSA_O3DE|%s.%s.RESULT|%s\n",
                gate, key, passed ? "PASS" : "FAIL");
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
            behaviorContext->Method("ProbeHandoffString", &SystemComponent::ProbeHandoffString)
                ->Attribute(AZ::Script::Attributes::Module, "sansacloth_probe")
                ->Attribute(
                    AZ::Script::Attributes::Scope,
                    AZ::Script::Attributes::ScopeFlags::Common);
            behaviorContext->Method("ProbeHandoffString", &SystemComponent::ProbeHandoffString)
                ->Attribute(AZ::Script::Attributes::Module, "sansacloth_probe")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common);
            behaviorContext->Method("RunBf008Capture", &SystemComponent::RunBf008Capture)
                ->Attribute(AZ::Script::Attributes::Module, "sansacloth_probe")
                ->Attribute(
                    AZ::Script::Attributes::Scope,
                    AZ::Script::Attributes::ScopeFlags::Common);
            behaviorContext->Method("RunBf007", &SystemComponent::RunBf007)
                ->Attribute(AZ::Script::Attributes::Module, "sansacloth_probe")
                ->Attribute(
                    AZ::Script::Attributes::Scope,
                    AZ::Script::Attributes::ScopeFlags::Common);
            behaviorContext->Method("RunBf005006", &SystemComponent::RunBf005006)
                ->Attribute(AZ::Script::Attributes::Module, "sansacloth_probe")
                ->Attribute(
                    AZ::Script::Attributes::Scope,
                    AZ::Script::Attributes::ScopeFlags::Common);
            behaviorContext->Method("RunBf004", &SystemComponent::RunBf004)
                ->Attribute(AZ::Script::Attributes::Module, "sansacloth_probe")
                ->Attribute(
                    AZ::Script::Attributes::Scope,
                    AZ::Script::Attributes::ScopeFlags::Common);
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

    bool SystemComponent::RunBf004()
    {
        // Validation-only flat 0.20m x 0.10m fixture. Triangle winding is
        // reversed from canonical after the O3DE Y/Z axis permutation.
        const std::array<AZ::Vector3, 4> vertices = {
            AZ::Vector3(-0.10f, -0.05f, 0.0f),
            AZ::Vector3(-0.10f,  0.05f, 0.0f),
            AZ::Vector3( 0.10f, -0.05f, 0.0f),
            AZ::Vector3( 0.10f,  0.05f, 0.0f)
        };
        const std::array<AZ::Vector2, 4> uvs = {
            AZ::Vector2(0.0f, 0.0f),
            AZ::Vector2(0.0f, 1.0f),
            AZ::Vector2(1.0f, 0.0f),
            AZ::Vector2(1.0f, 1.0f)
        };
        const std::array<int, 6> triangles = { 0, 3, 1, 0, 2, 3 };
        const SurfaceReference reference{ FixtureDomainId, AZ::Vector2(0.25f, 0.75f) };

        const AZ::Vector3 identityExpectedPosition(-0.05f, 0.025f, 0.0f);
        const AZ::Vector3 identityExpectedNormal(0.0f, 0.0f, 1.0f);
        const AZ::Vector3 transformedExpectedPosition(0.30f, -0.075f, 0.25f);
        const AZ::Vector3 transformedExpectedNormal(1.0f, 0.0f, 0.0f);
        constexpr float Offset = 0.01f;

        const AZ::Transform identity = AZ::Transform::CreateIdentity();
        AZ::Transform bodyTransform = AZ::Transform::CreateRotationY(AZ::DegToRad(90.0f));
        bodyTransform.SetTranslation(AZ::Vector3(0.30f, -0.10f, 0.20f));

        SurfaceQueryResult identityOutward;
        SurfaceQueryResult identityInward;
        SurfaceQueryResult transformedOutward;
        SurfaceQueryResult transformedInward;
        SurfaceQueryResult identityTangent;

        const bool resolvedIdentityOutward = QuerySurface(
            reference, vertices, uvs, triangles, identity,
            identityExpectedPosition + identityExpectedNormal * Offset,
            identityOutward);
        const bool resolvedIdentityInward = QuerySurface(
            reference, vertices, uvs, triangles, identity,
            identityExpectedPosition - identityExpectedNormal * Offset,
            identityInward);
        const bool resolvedTransformedOutward = QuerySurface(
            reference, vertices, uvs, triangles, bodyTransform,
            transformedExpectedPosition + transformedExpectedNormal * Offset,
            transformedOutward);
        const bool resolvedTransformedInward = QuerySurface(
            reference, vertices, uvs, triangles, bodyTransform,
            transformedExpectedPosition - transformedExpectedNormal * Offset,
            transformedInward);
        const bool resolvedIdentityTangent = QuerySurface(
            reference, vertices, uvs, triangles, identity,
            identityExpectedPosition + AZ::Vector3(Offset, 0.0f, 0.0f),
            identityTangent);

        const bool sqf001 = resolvedIdentityOutward
            && identityOutward.m_worldPosition.IsClose(identityExpectedPosition, Tolerance)
            && identityOutward.m_worldNormal.IsClose(identityExpectedNormal, Tolerance);
        const bool sqf002 = resolvedIdentityOutward
            && AZ::GetAbs(identityOutward.m_signedSeparation - Offset) <= Tolerance;
        const bool sqf003 = resolvedIdentityInward
            && AZ::GetAbs(identityInward.m_signedSeparation + Offset) <= Tolerance;
        const bool sqf004 = resolvedTransformedOutward
            && transformedOutward.m_worldPosition.IsClose(transformedExpectedPosition, Tolerance)
            && transformedOutward.m_worldNormal.IsClose(transformedExpectedNormal, Tolerance);
        const bool sqf005 = resolvedTransformedOutward
            && AZ::GetAbs(transformedOutward.m_signedSeparation - Offset) <= Tolerance;
        const bool sqf006 = resolvedTransformedInward
            && AZ::GetAbs(transformedInward.m_signedSeparation + Offset) <= Tolerance;
        const bool sqf007 = resolvedIdentityTangent
            && AZ::GetAbs(identityTangent.m_signedSeparation) <= Tolerance;

        LogBf004Vector("IDENTITY_SURFACE_POSITION", identityOutward.m_worldPosition);
        LogBf004Vector("IDENTITY_SURFACE_NORMAL", identityOutward.m_worldNormal);
        LogBf004Scalar("IDENTITY_OUTWARD_SEPARATION_M", identityOutward.m_signedSeparation);
        LogBf004Scalar("IDENTITY_INWARD_SEPARATION_M", identityInward.m_signedSeparation);
        LogBf004Vector("TRANSFORMED_SURFACE_POSITION", transformedOutward.m_worldPosition);
        LogBf004Vector("TRANSFORMED_SURFACE_NORMAL", transformedOutward.m_worldNormal);
        LogBf004Scalar("TRANSFORMED_OUTWARD_SEPARATION_M", transformedOutward.m_signedSeparation);
        LogBf004Scalar("TRANSFORMED_INWARD_SEPARATION_M", transformedInward.m_signedSeparation);
        LogBf004Scalar("IDENTITY_TANGENT_SEPARATION_M", identityTangent.m_signedSeparation);

        LogBf004Check("SQF-001.RESULT", sqf001);
        LogBf004Check("SQF-002.RESULT", sqf002);
        LogBf004Check("SQF-003.RESULT", sqf003);
        LogBf004Check("SQF-004.RESULT", sqf004);
        LogBf004Check("SQF-005.RESULT", sqf005);
        LogBf004Check("SQF-006.RESULT", sqf006);
        LogBf004Check("SQF-007.RESULT", sqf007);

        const bool result = sqf001 && sqf002 && sqf003 && sqf004
            && sqf005 && sqf006 && sqf007;
        LogBf004Check("RESULT", result);
        return result;
    }

    bool SystemComponent::RunBf005006()
    {
        // Analytic 0.20m x 0.10m body patch in O3DE XY plane, outward +Z.
        // The fixture is not imported from an O3DE mesh or JSON file.
        const std::array<AZ::Vector3, 4> surfaceVertices = {
            AZ::Vector3(-0.10f, -0.05f, 0.0f),
            AZ::Vector3(-0.10f,  0.05f, 0.0f),
            AZ::Vector3( 0.10f, -0.05f, 0.0f),
            AZ::Vector3( 0.10f,  0.05f, 0.0f)
        };
        const std::array<AZ::Vector2, 4> surfaceUvs = {
            AZ::Vector2(0.0f, 0.0f),
            AZ::Vector2(0.0f, 1.0f),
            AZ::Vector2(1.0f, 0.0f),
            AZ::Vector2(1.0f, 1.0f)
        };
        const std::array<int, 6> surfaceTriangles = { 0, 3, 1, 0, 2, 3 };

        const AZ::Vector3 canonicalWorldGravity = AZ::Vector3::CreateZero();
        const AZ::Vector3 worldGravity = CanonicalToO3de(canonicalWorldGravity);
        constexpr float conformity = 0.0f;
        const AZ::Vector3 expectedNormal(0.0f, 0.0f, 1.0f);

        std::array<Sr001Input, Sr001ControlPointCount> inputs{};
        std::array<Sr001Output, Sr001ControlPointCount> outputs{};

        int inputCount = 0;
        int anchorCount = 0;
        int contactInputCount = 0;
        int directSupportCount = 0;
        int contactOnlyUnsupportedCount = 0;
        int derivedContactCount = 0;
        int outputCount = 0;
        int referenceOutputCount = 0;
        int normalOutputCount = 0;
        int separationOutputCount = 0;
        int supportOutputCount = 0;
        float maxPositionDeviationM = 0.0f;
        float maxNormalDeviation = 0.0f;
        float maxAbsSeparationM = 0.0f;
        bool stableIdsAndUvValid = true;
        bool allNonAnchorUnsupported = true;
        bool allExpectedPositionsValid = true;
        bool allExpectedNormalsValid = true;
        bool allExpectedSeparationsValid = true;

        for (int vIndex = 0; vIndex < Sr001VSamples; ++vIndex)
        {
            const float v = static_cast<float>(vIndex) / (Sr001VSamples - 1);
            for (int uIndex = 0; uIndex < Sr001USamples; ++uIndex)
            {
                const float u = static_cast<float>(uIndex) / (Sr001USamples - 1);
                const int stableId = vIndex * Sr001USamples + uIndex;
                Sr001Input& input = inputs[stableId];
                input.m_stableId = stableId;
                input.m_reference = SurfaceReference{ FixtureDomainId, AZ::Vector2(u, v) };
                input.m_initialPosition = AZ::Vector3(
                    (u - 0.5f) * Sr001WidthM, (v - 0.5f) * Sr001DepthM, 0.0f);
                input.m_anchor = (uIndex == 0 || uIndex == Sr001USamples - 1);
                input.m_contact = true;
                ++inputCount;
                anchorCount += input.m_anchor ? 1 : 0;
                contactInputCount += input.m_contact ? 1 : 0;
            }
        }

        bool allMapped = true;
        for (const Sr001Input& input : inputs)
        {
            Sr001Output& output = outputs[input.m_stableId];
            if (!MapSr001Output(
                input, worldGravity, conformity, surfaceVertices,
                surfaceUvs, surfaceTriangles, output))
            {
                allMapped = false;
                break;
            }
            ++outputCount;
            ++referenceOutputCount;
            ++normalOutputCount;
            ++separationOutputCount;
            ++supportOutputCount;

            directSupportCount += output.m_directSupport ? 1 : 0;
            contactOnlyUnsupportedCount +=
                (input.m_contact && !output.m_directSupport) ? 1 : 0;
            derivedContactCount +=
                (output.m_separationM <= Sr001CollisionToleranceM) ? 1 : 0;

            const AZ::Vector3 independentExpectedPosition(
                (input.m_reference.m_uv.GetX() - 0.5f) * Sr001WidthM,
                (input.m_reference.m_uv.GetY() - 0.5f) * Sr001DepthM, 0.0f);
            allExpectedPositionsValid &= output.m_finalPosition.IsClose(
                independentExpectedPosition, Tolerance);
            allExpectedNormalsValid &= output.m_surfaceNormal.IsClose(
                expectedNormal, Tolerance);
            allExpectedSeparationsValid &=
                AZ::GetAbs(output.m_separationM) <= Tolerance;
            stableIdsAndUvValid &=
                output.m_stableId == input.m_stableId
                && output.m_reference.m_domainId == FixtureDomainId
                && output.m_reference.m_uv.IsClose(input.m_reference.m_uv, Tolerance);
            allNonAnchorUnsupported &=
                output.m_directSupport == input.m_anchor;
            maxPositionDeviationM = std::max(
                maxPositionDeviationM,
                (output.m_finalPosition - input.m_initialPosition).GetLength());
            maxNormalDeviation = std::max(
                maxNormalDeviation,
                (output.m_surfaceNormal - expectedNormal).GetLength());
            maxAbsSeparationM = std::max(
                maxAbsSeparationM, AZ::GetAbs(output.m_separationM));
        }

        // Reject unimplemented simulation modes rather than silently treating
        // them as the identity C=0/G=0 result.
        Sr001Output rejectedOutput;
        const bool nonzeroGravityRejected = !MapSr001Output(
            inputs[73], AZ::Vector3(0.0f, 0.0f, -9.81f), conformity,
            surfaceVertices, surfaceUvs, surfaceTriangles, rejectedOutput);
        const bool nonzeroConformityRejected = !MapSr001Output(
            inputs[73], worldGravity, 0.5f,
            surfaceVertices, surfaceUvs, surfaceTriangles, rejectedOutput);

        const bool isf001 = inputCount == 147;
        const bool isf002 = anchorCount == 14
            && inputs[0].m_anchor && !inputs[73].m_anchor && inputs[146].m_anchor;
        const bool isf003 = contactInputCount == 147;
        const bool isf004 = directSupportCount == 14
            && contactOnlyUnsupportedCount == 133 && allNonAnchorUnsupported;
        const bool isf005 = worldGravity.IsZero(Tolerance);
        const bool isf006 = AZ::GetAbs(conformity) <= Tolerance
            && nonzeroGravityRejected && nonzeroConformityRejected;
        const bool isf007 = stableIdsAndUvValid
            && inputs[0].m_reference.m_uv.IsClose(AZ::Vector2(0.0f, 0.0f), Tolerance)
            && inputs[73].m_reference.m_uv.IsClose(AZ::Vector2(0.5f, 0.5f), Tolerance)
            && inputs[146].m_reference.m_uv.IsClose(AZ::Vector2(1.0f, 1.0f), Tolerance);

        const bool osf001 = outputCount == 147 && allExpectedPositionsValid;
        const bool osf002 = referenceOutputCount == 147 && stableIdsAndUvValid;
        const bool osf003 = normalOutputCount == 147 && allExpectedNormalsValid;
        const bool osf004 = separationOutputCount == 147 && allExpectedSeparationsValid;
        const bool osf005 = supportOutputCount == 147 && allNonAnchorUnsupported;
        const bool osf006 = maxPositionDeviationM <= Tolerance;
        const bool osf007 = maxNormalDeviation <= Tolerance;
        const bool osf008 = maxAbsSeparationM <= Tolerance;
        const bool osf009 = directSupportCount == 14;
        const bool osf010 = derivedContactCount == 147;

        LogSr001Scalar("CP_COUNT", inputCount);
        LogSr001Scalar("ANCHOR_COUNT", anchorCount);
        LogSr001Scalar("CONTACT_INPUT_COUNT", contactInputCount);
        LogSr001Scalar("DIRECT_SUPPORT_COUNT", directSupportCount);
        LogSr001Scalar("CONTACT_ONLY_UNSUPPORTED_COUNT", contactOnlyUnsupportedCount);
        LogSr001Scalar("OUTPUT_CP_COUNT", outputCount);
        LogSr001Scalar("DERIVED_CONTACT_COUNT", derivedContactCount);
        LogSr001Vector("WORLD_GRAVITY_INPUT", worldGravity);
        LogSr001Float("CONFORMITY_INPUT", conformity);
        LogSr001Float("COLLISION_TOLERANCE_M", Sr001CollisionToleranceM);
        LogSr001Float("FINAL_POSITION_DEVIATION_MAX_M", maxPositionDeviationM);
        LogSr001Float("NORMAL_DEVIATION_MAX", maxNormalDeviation);
        LogSr001Float("SEPARATION_ABS_MAX_M", maxAbsSeparationM);
        AZ_Printf("SansaClothBackendProbe",
            "SANSA_O3DE|SR-001.NONZERO_GRAVITY_REJECTED|%s\n",
            nonzeroGravityRejected ? "TRUE" : "FALSE");
        AZ_Printf("SansaClothBackendProbe",
            "SANSA_O3DE|SR-001.NONZERO_CONFORMITY_REJECTED|%s\n",
            nonzeroConformityRejected ? "TRUE" : "FALSE");

        if (allMapped)
        {
            LogSr001Sample("CP_FIRST", outputs[0]);
            LogSr001Sample("CP_CENTER", outputs[73]);
            LogSr001Sample("CP_LAST", outputs[146]);
        }

        LogBoundaryCheck("BF-005", "ISF-001", isf001);
        LogBoundaryCheck("BF-005", "ISF-002", isf002);
        LogBoundaryCheck("BF-005", "ISF-003", isf003);
        LogBoundaryCheck("BF-005", "ISF-004", isf004);
        LogBoundaryCheck("BF-005", "ISF-005", isf005);
        LogBoundaryCheck("BF-005", "ISF-006", isf006);
        LogBoundaryCheck("BF-005", "ISF-007", isf007);
        LogBoundaryCheck("BF-006", "OSF-001", osf001);
        LogBoundaryCheck("BF-006", "OSF-002", osf002);
        LogBoundaryCheck("BF-006", "OSF-003", osf003);
        LogBoundaryCheck("BF-006", "OSF-004", osf004);
        LogBoundaryCheck("BF-006", "OSF-005", osf005);
        LogBoundaryCheck("BF-006", "OSF-006", osf006);
        LogBoundaryCheck("BF-006", "OSF-007", osf007);
        LogBoundaryCheck("BF-006", "OSF-008", osf008);
        LogBoundaryCheck("BF-006", "OSF-009", osf009);
        LogBoundaryCheck("BF-006", "OSF-010", osf010);

        const bool bf005 = allMapped && isf001 && isf002 && isf003
            && isf004 && isf005 && isf006 && isf007;
        const bool bf006 = allMapped && osf001 && osf002 && osf003
            && osf004 && osf005 && osf006 && osf007 && osf008
            && osf009 && osf010;
        AZ_Printf("SansaClothBackendProbe",
            "SANSA_O3DE|BF-005.RESULT|%s\n", bf005 ? "PASS" : "FAIL");
        AZ_Printf("SansaClothBackendProbe",
            "SANSA_O3DE|BF-006.RESULT|%s\n", bf006 ? "PASS" : "FAIL");
        AZ_Printf("SansaClothBackendProbe",
            "SANSA_O3DE|SR-001.RESULT|%s\n",
            (bf005 && bf006) ? "PASS" : "FAIL");
        return bf005 && bf006;
    }

    bool SystemComponent::RunBf007()
    {
        // Independent expected values are supplied for each fixture, rather
        // than deriving expectations from the generated vertex array.
        const std::array<BasicFixtureCase, 5> fixtures = {{
            { "SR-001.FLAT", BasicFixtureKind::Flat, 0.0f, false, false,
                0.0f, 0.0f, AZ::Vector3(0.0f, 0.0f, 0.0f),
                AZ::Vector3(0.0f, 0.0f, 1.0f), 14 },
            { "SR-002.CONVEX_UP", BasicFixtureKind::Convex, 0.03f, false, false,
                0.0f, 0.03f, AZ::Vector3(0.0f, 0.0f, 0.03f),
                AZ::Vector3(0.0f, 0.0f, 1.0f), 14 },
            { "SR-003.CONVEX_SIDE", BasicFixtureKind::Convex, 0.03f, true, true,
                0.0f, 0.03f, AZ::Vector3(0.03f, 0.0f, 0.0f),
                AZ::Vector3(1.0f, 0.0f, 0.0f), 7 },
            { "SR-004.CONCAVE_SHALLOW", BasicFixtureKind::Concave, 0.02f, false, false,
                -0.02f, 0.0f, AZ::Vector3(0.0f, 0.0f, -0.02f),
                AZ::Vector3(0.0f, 0.0f, 1.0f), 14 },
            { "SR-005.CONCAVE_DEEP", BasicFixtureKind::Concave, 0.05f, false, false,
                -0.05f, 0.0f, AZ::Vector3(0.0f, 0.0f, -0.05f),
                AZ::Vector3(0.0f, 0.0f, 1.0f), 14 }
        }};
        bool passed = true;
        for (const BasicFixtureCase& fixture : fixtures)
        {
            passed &= ProbeBasicFixture(fixture);
        }
        AZ_Printf("SansaClothBackendProbe",
            "SANSA_O3DE|BF-007.RESULT|%s\n", passed ? "PASS" : "FAIL");
        return passed;
    }

    AZStd::string SystemComponent::ProbeHandoffString(const AZStd::string& payload)
    {
        // OXC-001 spike: string roundtrip only, not a fixture importer.
        const bool passed = payload == "SANSA-HANDOFF-V0|probe";
        AZ_Printf("SansaClothBackendProbe",
            "SANSA_O3DE|OXC-001.CPP_RECEIVED_LENGTH|%zu\n", payload.size());
        AZ_Printf("SansaClothBackendProbe",
            "SANSA_O3DE|OXC-001.CPP_RESULT|%s\n", passed ? "PASS" : "FAIL");
        return passed ? AZStd::string("SANSA-HANDOFF-V0|ACK") : AZStd::string{};
    }

    AZStd::string SystemComponent::RunBf008Capture()
    {
        // BF-008: generate measurement records from the same C++ semantic
        // mapper used by BF-005/006. Python owns UTC metadata, disk transport,
        // JSONL read-back validation, and final PASS/FAIL classification.
        // A blank string means the C++ measurement stage failed.
        const std::array<AZ::Vector3, 4> surfaceVertices = {
            AZ::Vector3(-0.10f, -0.05f, 0.0f),
            AZ::Vector3(-0.10f,  0.05f, 0.0f),
            AZ::Vector3( 0.10f, -0.05f, 0.0f),
            AZ::Vector3( 0.10f,  0.05f, 0.0f)
        };
        const std::array<AZ::Vector2, 4> surfaceUvs = {
            AZ::Vector2(0.0f, 0.0f),
            AZ::Vector2(0.0f, 1.0f),
            AZ::Vector2(1.0f, 0.0f),
            AZ::Vector2(1.0f, 1.0f)
        };
        const std::array<int, 6> surfaceTriangles = { 0, 3, 1, 0, 2, 3 };
        const AZ::Vector3 worldGravity = CanonicalToO3de(AZ::Vector3::CreateZero());

        AZStd::string measurements;
        int cpCount = 0;
        int contactCount = 0;
        int supportCount = 0;
        double separationSumM = 0.0;
        float maxSeparationM = 0.0f;
        float maxPenetrationM = 0.0f;
        float maxPositionDeviationM = 0.0f;
        double squaredPositionDeviationSum = 0.0;

        for (int vIndex = 0; vIndex < Sr001VSamples; ++vIndex)
        {
            const float v = static_cast<float>(vIndex) / (Sr001VSamples - 1);
            for (int uIndex = 0; uIndex < Sr001USamples; ++uIndex)
            {
                const float u = static_cast<float>(uIndex) / (Sr001USamples - 1);
                Sr001Input input;
                input.m_stableId = vIndex * Sr001USamples + uIndex;
                input.m_reference = SurfaceReference{ FixtureDomainId, AZ::Vector2(u, v) };
                input.m_initialPosition = AZ::Vector3(
                    (u - 0.5f) * Sr001WidthM, (v - 0.5f) * Sr001DepthM, 0.0f);
                input.m_anchor = uIndex == 0 || uIndex == Sr001USamples - 1;
                input.m_contact = true;

                Sr001Output output;
                if (!MapSr001Output(
                    input, worldGravity, 0.0f,
                    surfaceVertices, surfaceUvs, surfaceTriangles, output))
                {
                    AZ_Printf("SansaClothBackendProbe",
                        "SANSA_O3DE|BF-008.CPP_MEASUREMENTS_RESULT|FAIL|MapSr001Output\n");
                    return {};
                }

                const bool derivedContact =
                    output.m_separationM <= Sr001CollisionToleranceM;
                const float positionDeviationM =
                    (output.m_finalPosition - input.m_initialPosition).GetLength();
                ++cpCount;
                contactCount += derivedContact ? 1 : 0;
                supportCount += output.m_directSupport ? 1 : 0;
                separationSumM += static_cast<double>(output.m_separationM);
                maxSeparationM = std::max(maxSeparationM, output.m_separationM);
                maxPenetrationM = std::max(
                    maxPenetrationM, std::max(0.0f, -output.m_separationM));
                maxPositionDeviationM = std::max(
                    maxPositionDeviationM, positionDeviationM);
                squaredPositionDeviationSum +=
                    static_cast<double>(positionDeviationM) * positionDeviationM;

                // Canonical measurement values are serialized after reversing
                // the O3DE Y/Z component mapping. This matches Unity BF-008
                // and the canonical Reference measurement coordinate system.
                const AZ::Vector3 canonicalPosition(
                    output.m_finalPosition.GetX(), output.m_finalPosition.GetZ(),
                    output.m_finalPosition.GetY());
                const AZ::Vector3 canonicalNormal(
                    output.m_surfaceNormal.GetX(), output.m_surfaceNormal.GetZ(),
                    output.m_surfaceNormal.GetY());

                measurements += AZStd::string::format(
                    "{\"record_type\":\"final_cp\",\"stable_id\":%d,"
                    "\"position_m\":[%.9g,%.9g,%.9g],"
                    "\"surface_reference\":{\"domain_id\":%llu,\"u\":%.9g,\"v\":%.9g},"
                    "\"surface_normal\":[%.9g,%.9g,%.9g],"
                    "\"separation_m\":%.9g,\"support\":\"%s\","
                    "\"derived_contact\":%s}\n",
                    output.m_stableId,
                    canonicalPosition.GetX(), canonicalPosition.GetY(),
                    canonicalPosition.GetZ(),
                    static_cast<unsigned long long>(output.m_reference.m_domainId),
                    output.m_reference.m_uv.GetX(), output.m_reference.m_uv.GetY(),
                    canonicalNormal.GetX(), canonicalNormal.GetY(),
                    canonicalNormal.GetZ(),
                    output.m_separationM,
                    output.m_directSupport ? "Anchor" : "Unsupported",
                    derivedContact ? "true" : "false");
            }
        }

        const double meanSeparationM =
            cpCount > 0 ? separationSumM / cpCount : 0.0;
        const double rmsPositionDeviationM =
            cpCount > 0 ? std::sqrt(squaredPositionDeviationSum / cpCount) : 0.0;

        measurements += AZStd::string::format(
            "{\"record_type\":\"aggregate\","
            "\"contact_count\":%d,\"support_count\":%d,"
            "\"mean_separation_m\":%.17g,\"max_separation_m\":%.9g,"
            "\"max_penetration_m\":%.9g,"
            "\"max_position_deviation_m\":%.9g,"
            "\"rms_position_deviation_m\":%.17g}\n",
            contactCount, supportCount, meanSeparationM, maxSeparationM,
            maxPenetrationM, maxPositionDeviationM, rmsPositionDeviationM);

        const bool valid = cpCount == Sr001ControlPointCount
            && contactCount == Sr001ControlPointCount
            && supportCount == 14
            && AZ::GetAbs(static_cast<float>(meanSeparationM)) <= Tolerance
            && AZ::GetAbs(maxSeparationM) <= Tolerance
            && AZ::GetAbs(maxPenetrationM) <= Tolerance
            && AZ::GetAbs(maxPositionDeviationM) <= Tolerance
            && rmsPositionDeviationM <= Tolerance;
        AZ_Printf("SansaClothBackendProbe",
            "SANSA_O3DE|BF-008.CPP_CP_COUNT|%d\n", cpCount);
        AZ_Printf("SansaClothBackendProbe",
            "SANSA_O3DE|BF-008.CPP_CONTACT_COUNT|%d\n", contactCount);
        AZ_Printf("SansaClothBackendProbe",
            "SANSA_O3DE|BF-008.CPP_SUPPORT_COUNT|%d\n", supportCount);
        AZ_Printf("SansaClothBackendProbe",
            "SANSA_O3DE|BF-008.CPP_MEASUREMENTS_RESULT|%s\n",
            valid ? "PASS" : "FAIL");
        return valid ? measurements : AZStd::string{};
    }

} // namespace SansaClothBackendProbeValidation
