#include "SansaClothBackendProbeValidationSystemComponent.h"

#include <AzCore/Math/MathUtils.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
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
            behaviorContext->Method("ProbeSurfaceResponseJson", &SystemComponent::ProbeSurfaceResponseJson)
                ->Attribute(AZ::Script::Attributes::Module, "sansacloth_probe")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common);
            behaviorContext->Method("ProbeFixtureJson", &SystemComponent::ProbeFixtureJson)
                ->Attribute(AZ::Script::Attributes::Module, "sansacloth_probe")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common);
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

    AZStd::string SystemComponent::ProbeFixtureJson(const AZStd::string& payload)
    {
        // OXC-002: consume real Reference JSON, not a regenerated analytic fixture.
        // Parsing and all derived counts are local to this call; no borrowed data
        // survives the BehaviorContext boundary. Full semantic checks are later gates.
        rapidjson::Document doc;
        doc.Parse(payload.c_str());
        bool valid = !doc.HasParseError() && doc.IsObject();
        if (valid)
        {
            valid = doc.HasMember("format") && doc["format"].IsString()
                && AZStd::string(doc["format"].GetString()) == "sansacloth.validation.fixture-exchange/0"
                && doc.HasMember("case_id") && doc["case_id"].IsString()
                && AZStd::string(doc["case_id"].GetString()) == "SR-001-C0-G0"
                && doc.HasMember("body_surface") && doc["body_surface"].IsObject()
                && doc.HasMember("cloth") && doc["cloth"].IsObject()
                && doc.HasMember("inputs") && doc["inputs"].IsObject();
        }
        // OXC-009 phase 1: parse matrix identity; this does not grant ACK.
        int matrixScenario = 0;
        double matrixConformity = -1.0;
        int matrixGravity = -1;
        if (!doc.HasParseError() && doc.IsObject()
            && doc.HasMember("case_id") && doc["case_id"].IsString())
        {
            const AZStd::string id(doc["case_id"].GetString());
            for (int scenario = 1; scenario <= 5; ++scenario)
            {
                for (int conformity = 0; conformity <= 2; ++conformity)
                {
                    const char* cCode = conformity == 0 ? "0" : (conformity == 1 ? "05" : "1");
                    for (int gravity = 0; gravity <= 1; ++gravity)
                    {
                        if (id == AZStd::string::format("SR-%03d-C%s-G%d", scenario, cCode, gravity))
                        {
                            matrixScenario = scenario;
                            matrixConformity = conformity == 0 ? 0.0 : (conformity == 1 ? 0.5 : 1.0);
                            matrixGravity = gravity;
                        }
                    }
                }
            }
        }
        AZ_Printf("SansaClothBackendProbe",
            "SANSA_O3DE|OXC-009.CPP_MATRIX_ID|SCENARIO=%d|C=%.1f|G=%d\n",
            matrixScenario, matrixConformity, matrixGravity);
        // OXC-009 phase 2: independent non-baseline matrix structural gate.
        // No ACK is issued until scenario-specific numeric geometry checks are ready.
        if (matrixScenario != 0 &&
            !(matrixScenario == 1 && matrixConformity == 0.0 && matrixGravity == 0))
        {
            bool matrixValid = doc.HasMember("format") && doc["format"].IsString()
                && AZStd::string(doc["format"].GetString()) == "sansacloth.validation.fixture-exchange/0"
                && doc.HasMember("body_surface") && doc["body_surface"].IsObject()
                && doc.HasMember("cloth") && doc["cloth"].IsObject()
                && doc.HasMember("inputs") && doc["inputs"].IsObject();
            int matrixAnchors = 0;
            int matrixContacts = 0;
            if (matrixValid)
            {
                const auto& body = doc["body_surface"];
                const auto& cloth = doc["cloth"];
                const auto& inputs = doc["inputs"];
                matrixValid = body.HasMember("domain_id") && body["domain_id"].IsUint64()
                    && body["domain_id"].GetUint64() == 1
                    && body.HasMember("vertices") && body["vertices"].IsArray()
                    && body["vertices"].Size() == 147
                    && body.HasMember("triangles") && body["triangles"].IsArray()
                    && body["triangles"].Size() == 240
                    && cloth.HasMember("control_points") && cloth["control_points"].IsArray()
                    && cloth["control_points"].Size() == 147
                    && inputs.HasMember("conformity") && inputs["conformity"].IsNumber()
                    && std::isfinite(inputs["conformity"].GetDouble())
                    && std::abs(inputs["conformity"].GetDouble() - matrixConformity) < 1.0e-9
                    && inputs.HasMember("collision_tolerance_m")
                    && inputs["collision_tolerance_m"].IsNumber()
                    && inputs["collision_tolerance_m"].GetDouble() == 0.0
                    && inputs.HasMember("world_gravity_m_per_s2")
                    && inputs["world_gravity_m_per_s2"].IsArray()
                    && inputs["world_gravity_m_per_s2"].Size() == 3;
                if (matrixValid)
                {
                    const auto& gravity = inputs["world_gravity_m_per_s2"];
                    for (rapidjson::SizeType axis = 0; axis < 3; ++axis)
                    {
                        matrixValid = matrixValid && gravity[axis].IsNumber()
                            && std::isfinite(gravity[axis].GetDouble());
                    }
                    if (matrixValid)
                    {
                        const double magnitudeSq = gravity[0].GetDouble() * gravity[0].GetDouble()
                            + gravity[1].GetDouble() * gravity[1].GetDouble()
                            + gravity[2].GetDouble() * gravity[2].GetDouble();
                        matrixValid = (magnitudeSq > 1.0e-12) == (matrixGravity == 1);
                    }
                }
                if (matrixValid)
                {
                    std::array<bool, 147> seen{};
                    for (const auto& point : cloth["control_points"].GetArray())
                    {
                        if (!point.IsObject() || !point.HasMember("stable_id")
                            || !point["stable_id"].IsUint() || !point.HasMember("anchor")
                            || !point["anchor"].IsBool() || !point.HasMember("contact")
                            || !point["contact"].IsBool())
                        {
                            matrixValid = false;
                            break;
                        }
                        const unsigned id = point["stable_id"].GetUint();
                        if (id >= seen.size() || seen[id])
                        {
                            matrixValid = false;
                            break;
                        }
                        seen[id] = true;
                        matrixAnchors += point["anchor"].GetBool() ? 1 : 0;
                        matrixContacts += point["contact"].GetBool() ? 1 : 0;
                    }
                    matrixValid = matrixValid && matrixAnchors == (matrixScenario == 3 ? 7 : 14)
                        && matrixContacts == (matrixScenario >= 4 ? 84 : 147);
                }
            }
            // OXC-009 numeric integrity gate. Curved scenarios have displaced
            // body surfaces and independently positioned cloth control points;
            // do not assume body position equals cloth position.
            bool numericValid = matrixValid;
            if (numericValid)
            {
                const auto& vertices = doc["body_surface"]["vertices"];
                const auto& triangles = doc["body_surface"]["triangles"];
                const auto& points = doc["cloth"]["control_points"];
                auto finite = [](const rapidjson::Value& v)
                {
                    return v.IsNumber() && std::isfinite(v.GetDouble());
                };
                for (rapidjson::SizeType i = 0; i < vertices.Size() && numericValid; ++i)
                {
                    const auto& vertex = vertices[i];
                    if (!vertex.IsObject() || !vertex.HasMember("position_m")
                        || !vertex.HasMember("normal") || !vertex.HasMember("uv"))
                    {
                        numericValid = false;
                        break;
                    }
                    const auto& pos = vertex["position_m"];
                    const auto& normal = vertex["normal"];
                    const auto& uv = vertex["uv"];
                    numericValid = pos.IsArray() && pos.Size() == 3
                        && normal.IsArray() && normal.Size() == 3
                        && uv.IsArray() && uv.Size() == 2;
                    if (!numericValid) break;
                    for (rapidjson::SizeType axis = 0; axis < 3; ++axis)
                    {
                        numericValid = numericValid && finite(pos[axis]) && finite(normal[axis]);
                    }
                    numericValid = numericValid && finite(uv[0]) && finite(uv[1]);
                    if (!numericValid) break;
                    const double lengthSq = normal[0].GetDouble() * normal[0].GetDouble()
                        + normal[1].GetDouble() * normal[1].GetDouble()
                        + normal[2].GetDouble() * normal[2].GetDouble();
                    // Scenario reference shape: a cosine bump centered on
                    // strip column 10. The cloth remains on its base plane.
                    const unsigned col = static_cast<unsigned>(i % 21);
                    const unsigned row = static_cast<unsigned>(i / 21);
                    const double lateral = -0.1 + static_cast<double>(col) * 0.01;
                    const double longitudinal = -0.05 + static_cast<double>(row) / 60.0;
                    const double bump = (col >= 6 && col <= 14)
                        ? 0.5 * (1.0 + std::cos(3.14159265358979323846
                            * (static_cast<double>(col) - 10.0) / 5.0)) : 0.0;
                    const double amplitude = matrixScenario == 2 ? 0.03
                        : (matrixScenario == 3 ? 0.03
                        : (matrixScenario == 4 ? -0.02
                        : (matrixScenario == 5 ? -0.05 : 0.0)));
                    const double expectedX = matrixScenario == 3 ? amplitude * bump : lateral;
                    const double expectedY = matrixScenario == 3 ? -lateral : amplitude * bump;
                    numericValid = numericValid
                        && std::abs(pos[0].GetDouble() - expectedX) < 1.0e-6
                        && std::abs(pos[1].GetDouble() - expectedY) < 1.0e-6
                        && std::abs(pos[2].GetDouble() - longitudinal) < 1.0e-6;
                    // Analytic normal of the cosine displacement. The
                    // derivative is zero outside the active columns.
                    const double slope = (col >= 6 && col <= 14)
                        ? -amplitude * (3.14159265358979323846 / 0.1)
                            * std::sin(3.14159265358979323846
                                * (static_cast<double>(col) - 10.0) / 5.0)
                        : 0.0;
                    const double invNormalLength = 1.0 / std::sqrt(1.0 + slope * slope);
                    const double expectedNx = matrixScenario == 3
                        ? invNormalLength : -slope * invNormalLength;
                    const double expectedNy = matrixScenario == 3
                        ? slope * invNormalLength : invNormalLength;
                    numericValid = numericValid
                        && std::abs(normal[0].GetDouble() - expectedNx) < 1.0e-5
                        && std::abs(normal[1].GetDouble() - expectedNy) < 1.0e-5
                        && std::abs(normal[2].GetDouble()) < 1.0e-6;
                    numericValid = numericValid && std::abs(lengthSq - 1.0) < 1.0e-5
                        && std::abs(uv[0].GetDouble() - static_cast<double>(i % 21) / 20.0) < 1.0e-6
                        && std::abs(uv[1].GetDouble() - static_cast<double>(i / 21) / 6.0) < 1.0e-6;
                }
                for (rapidjson::SizeType i = 0; i < triangles.Size() && numericValid; ++i)
                {
                    const auto& tri = triangles[i];
                    const unsigned row = static_cast<unsigned>(i / 2) / 20;
                    const unsigned col = static_cast<unsigned>(i / 2) % 20;
                    const unsigned v00 = row * 21 + col;
                    numericValid = tri.IsArray() && tri.Size() == 3
                        && tri[0].IsUint() && tri[1].IsUint() && tri[2].IsUint();
                    if (!numericValid) break;
                    numericValid = tri[0].GetUint() == v00
                        && tri[1].GetUint() == (i % 2 == 0 ? v00 + 21 : v00 + 22)
                        && tri[2].GetUint() == (i % 2 == 0 ? v00 + 22 : v00 + 1);
                }
                for (const auto& point : points.GetArray())
                {
                    if (!numericValid) break;
                    numericValid = point.HasMember("stable_id") && point["stable_id"].IsUint()
                        && point.HasMember("strip_id") && point["strip_id"].IsUint()
                        && point.HasMember("strip_order") && point["strip_order"].IsUint()
                        && point.HasMember("position_m") && point["position_m"].IsArray()
                        && point.HasMember("surface_reference") && point["surface_reference"].IsObject();
                    if (!numericValid) break;
                    const unsigned id = point["stable_id"].GetUint();
                    const auto& pos = point["position_m"];
                    const auto& ref = point["surface_reference"];
                    numericValid = pos.Size() == 3 && id < vertices.Size()
                        && point["strip_id"].GetUint() == id / 21
                        && point["strip_order"].GetUint() == id % 21
                        && ref.HasMember("domain_id") && ref["domain_id"].IsUint()
                        && ref["domain_id"].GetUint() == 1
                        && ref.HasMember("u") && ref.HasMember("v");
                    if (!numericValid) break;
                    for (rapidjson::SizeType axis = 0; axis < 3; ++axis)
                    {
                        numericValid = numericValid && finite(pos[axis]);
                    }
                    // Cloth control points remain on the undisplaced base
                    // plane even when the body surface is curved.
                    const unsigned col = id % 21;
                    const unsigned row = id / 21;
                    const double lateral = -0.1 + static_cast<double>(col) * 0.01;
                    const double longitudinal = -0.05 + static_cast<double>(row) / 60.0;
                    const double clothX = matrixScenario == 3 ? 0.0 : lateral;
                    const double clothY = matrixScenario == 3 ? -lateral : 0.0;
                    numericValid = numericValid
                        && std::abs(pos[0].GetDouble() - clothX) < 1.0e-6
                        && std::abs(pos[1].GetDouble() - clothY) < 1.0e-6
                        && std::abs(pos[2].GetDouble() - longitudinal) < 1.0e-6;
                    numericValid = numericValid && finite(ref["u"]) && finite(ref["v"]);
                    if (!numericValid) break;
                    numericValid = std::abs(ref["u"].GetDouble() - static_cast<double>(id % 21) / 20.0) < 1.0e-6
                        && std::abs(ref["v"].GetDouble() - static_cast<double>(id / 21) / 6.0) < 1.0e-6;
                }
            }
            // OXC-009 semantic pattern gate: verify flags per StableId,
            // not merely their aggregate counts.
            bool patternValid = numericValid;
            if (patternValid)
            {
                const auto& points = doc["cloth"]["control_points"];
                for (const auto& point : points.GetArray())
                {
                    const unsigned id = point["stable_id"].GetUint();
                    const unsigned col = id % 21;
                    const bool expectedAnchor = matrixScenario == 3
                        ? col == 0 : (col == 0 || col == 20);
                    // SR-004/005 contact mask: seven rows of 12 points,
                    // comprising the first/last six columns of each strip.
                    const bool expectedContact = matrixScenario <= 3
                        || col < 6 || col >= 15;
                    patternValid = point["anchor"].GetBool() == expectedAnchor
                        && point["contact"].GetBool() == expectedContact;
                    if (!patternValid) break;
                }
            }
            AZ_Printf("SansaClothBackendProbe",
                "SANSA_O3DE|OXC-009.CPP_SEMANTIC_PATTERN|%s\\n",
                patternValid ? "PASS" : "FAIL");
            AZ_Printf("SansaClothBackendProbe",
                "SANSA_O3DE|OXC-009.CPP_NUMERIC_INTEGRITY|%s\\n", numericValid ? "PASS" : "FAIL");
            AZ_Printf("SansaClothBackendProbe",
                "SANSA_O3DE|OXC-009.CPP_STRUCTURE|%s|ANCHORS=%d|CONTACTS=%d\n",
                matrixValid ? "PASS" : "FAIL", matrixAnchors, matrixContacts);
            AZ_Printf("SansaClothBackendProbe",
                "SANSA_O3DE|OXC-009.CPP_PREACK_GATE|%s\\n",
                matrixValid && numericValid && patternValid ? "PASS" : "FAIL");
            if (matrixValid && numericValid && patternValid)
            {
                AZ_Printf("SansaClothBackendProbe",
                    "SANSA_O3DE|OXC-009.CPP_MATRIX_RESULT|PASS\\n");
                return AZStd::string::format("OXC-009|ACK|%d|147|240|147|%d|%d",
                    matrixScenario, matrixAnchors, matrixContacts);
            }
            AZ_Printf("SansaClothBackendProbe",
                "SANSA_O3DE|OXC-009.CPP_MATRIX_RESULT|FAIL\\n");
            return {};
        }
        int vertexCount = -1;
        int triangleCount = -1;
        int cpCount = -1;
        int anchorCount = 0;
        int contactCount = 0;
        if (valid)
        {
            const auto& body = doc["body_surface"];
            const auto& cloth = doc["cloth"];
            valid = body.HasMember("domain_id") && body["domain_id"].IsUint64()
                && body["domain_id"].GetUint64() == 1
                && body.HasMember("vertices") && body["vertices"].IsArray()
                && body.HasMember("triangles") && body["triangles"].IsArray()
                && cloth.HasMember("control_points") && cloth["control_points"].IsArray();
            if (valid)
            {
                const auto& vertices = body["vertices"];
                const auto& triangles = body["triangles"];
                const auto& points = cloth["control_points"];
                vertexCount = static_cast<int>(vertices.Size());
                triangleCount = static_cast<int>(triangles.Size());
                cpCount = static_cast<int>(points.Size());
                valid = vertexCount == 147 && triangleCount == 240 && cpCount == 147;
                for (const auto& point : points.GetArray())
                {
                    if (!point.IsObject() || !point.HasMember("anchor")
                        || !point["anchor"].IsBool() || !point.HasMember("contact")
                        || !point["contact"].IsBool())
                    {
                        valid = false;
                        break;
                    }
                    anchorCount += point["anchor"].GetBool() ? 1 : 0;
                    contactCount += point["contact"].GetBool() ? 1 : 0;
                }
                valid = valid && anchorCount == 14 && contactCount == 147;
            }
        }
        // OXC-003: validate every mapped body vertex, normal, UV and triangle.
        // Canonical Y/Z swap is applied once; triangle winding must be reversed.
        bool bodyValid = valid;
        if (bodyValid)
        {
            const auto& body = doc["body_surface"];
            const auto& vertices = body["vertices"];
            const auto& triangles = body["triangles"];
            auto finite = [](const rapidjson::Value& v) -> bool
            {
                return v.IsNumber() && std::isfinite(v.GetDouble());
            };
            auto close = [](double a, double b) -> bool
            {
                return std::abs(a - b) <= 1.0e-6;
            };
            for (rapidjson::SizeType i = 0; i < vertices.Size() && bodyValid; ++i)
            {
                const auto& vertex = vertices[i];
                if (!vertex.IsObject() || !vertex.HasMember("position_m")
                    || !vertex.HasMember("normal") || !vertex.HasMember("uv"))
                {
                    bodyValid = false;
                    break;
                }
                const auto& p = vertex["position_m"];
                const auto& n = vertex["normal"];
                const auto& uv = vertex["uv"];
                if (!p.IsArray() || p.Size() != 3 || !n.IsArray() || n.Size() != 3
                    || !uv.IsArray() || uv.Size() != 2)
                {
                    bodyValid = false;
                    break;
                }
                for (rapidjson::SizeType j = 0; j < 3; ++j)
                {
                    bodyValid = bodyValid && finite(p[j]) && finite(n[j]);
                }
                for (rapidjson::SizeType j = 0; j < 2; ++j)
                {
                    bodyValid = bodyValid && finite(uv[j]);
                }
                if (!bodyValid)
                {
                    break;
                }
                const int row = static_cast<int>(i) / 21;
                const int col = static_cast<int>(i) % 21;
                const double x = -0.1 + static_cast<double>(col) * 0.01;
                const double y = -0.05 + static_cast<double>(row) / 60.0;
                // O3DE coordinate: (canonical X, canonical Z, canonical Y).
                const double ox = p[0].GetDouble();
                const double oy = p[2].GetDouble();
                const double oz = p[1].GetDouble();
                const double nx = n[0].GetDouble();
                const double ny = n[2].GetDouble();
                const double nz = n[1].GetDouble();
                bodyValid = close(ox, x) && close(oy, y) && close(oz, 0.0)
                    && close(nx, 0.0) && close(ny, 0.0) && close(nz, 1.0)
                    && close(uv[0].GetDouble(), static_cast<double>(col) / 20.0)
                    && close(uv[1].GetDouble(), static_cast<double>(row) / 6.0);
            }
            for (rapidjson::SizeType i = 0; i < triangles.Size() && bodyValid; ++i)
            {
                const auto& tri = triangles[i];
                if (!tri.IsArray() || tri.Size() != 3 || !tri[0].IsUint()
                    || !tri[1].IsUint() || !tri[2].IsUint())
                {
                    bodyValid = false;
                    break;
                }
                const int row = static_cast<int>(i / 2) / 20;
                const int col = static_cast<int>(i / 2) % 20;
                const unsigned int v00 = static_cast<unsigned int>(row * 21 + col);
                const unsigned int v01 = v00 + 21;
                const unsigned int v11 = v01 + 1;
                const unsigned int v10 = v00 + 1;
                // Canonical triangle (a,b,c) maps to O3DE (a,c,b).
                const unsigned int a = tri[0].GetUint();
                const unsigned int b = tri[2].GetUint();
                const unsigned int d = tri[1].GetUint();
                bodyValid = (i % 2 == 0)
                    ? (a == v00 && b == v11 && d == v01)
                    : (a == v00 && b == v10 && d == v11);
            }
        }
        AZ_Printf("SansaClothBackendProbe",
            "SANSA_O3DE|OXC-003.CPP_RESULT|%s\n", bodyValid ? "PASS" : "FAIL");
        valid = valid && bodyValid;
        // OXC-004: validate each control point's stable identity, strip
        // position and surface reference against the committed SR-001 layout.
        bool cpValid = valid;
        int checkedCp = 0;
        if (cpValid)
        {
            const auto& points = doc["cloth"]["control_points"];
            const auto& bodyVertices = doc["body_surface"]["vertices"];
            auto near = [](double a, double b) -> bool
            {
                return std::isfinite(a) && std::isfinite(b)
                    && std::abs(a - b) <= 1.0e-6;
            };
            std::array<bool, 147> seenIds{};
            std::array<bool, 147> seenStripOrders{};
            for (const auto& point : points.GetArray())
            {
                if (!point.IsObject() || !point.HasMember("stable_id")
                    || !point.HasMember("strip_id") || !point.HasMember("strip_order")
                    || !point.HasMember("position_m") || !point.HasMember("surface_reference")
                    || !point["stable_id"].IsUint() || !point["strip_id"].IsUint()
                    || !point["strip_order"].IsUint())
                {
                    cpValid = false;
                    break;
                }
                const unsigned int id = point["stable_id"].GetUint();
                const unsigned int strip = point["strip_id"].GetUint();
                const unsigned int order = point["strip_order"].GetUint();
                if (id >= 147 || strip >= 7 || order >= 21 || seenIds[id]
                    || seenStripOrders[strip * 21 + order])
                {
                    cpValid = false;
                    break;
                }
                seenIds[id] = true;
                seenStripOrders[strip * 21 + order] = true;
                const auto& p = point["position_m"];
                const auto& ref = point["surface_reference"];
                const auto& bodyPos = bodyVertices[id]["position_m"];
                if (!p.IsArray() || p.Size() != 3 || !ref.IsObject()
                    || !ref.HasMember("domain_id") || !ref["domain_id"].IsUint()
                    || !ref.HasMember("u") || !ref.HasMember("v")
                    || !ref["u"].IsNumber() || !ref["v"].IsNumber())
                {
                    cpValid = false;
                    break;
                }
                cpValid = id == strip * 21 + order
                    && ref["domain_id"].GetUint() == 1
                    && near(ref["u"].GetDouble(), static_cast<double>(order) / 20.0)
                    && near(ref["v"].GetDouble(), static_cast<double>(strip) / 6.0);
                for (rapidjson::SizeType axis = 0; axis < 3 && cpValid; ++axis)
                {
                    cpValid = p[axis].IsNumber() && bodyPos[axis].IsNumber()
                        && near(p[axis].GetDouble(), bodyPos[axis].GetDouble());
                }
                if (!cpValid)
                {
                    break;
                }
                ++checkedCp;
            }
            for (bool seen : seenIds)
            {
                cpValid = cpValid && seen;
            }
            for (bool seen : seenStripOrders)
            {
                cpValid = cpValid && seen;
            }
        }
        AZ_Printf("SansaClothBackendProbe",
            "SANSA_O3DE|OXC-004.CPP_CHECKED_CP_COUNT|%d\n", checkedCp);
        AZ_Printf("SansaClothBackendProbe",
            "SANSA_O3DE|OXC-004.CPP_RESULT|%s\n", cpValid ? "PASS" : "FAIL");
        valid = valid && cpValid;
        // OXC-005: Anchor and Contact are independent input flags.
        // Contact-only points are unsupported here: no solver Support is inferred.
        bool semanticsValid = valid;
        int contactOnlyUnsupported = 0;
        int semanticAnchors = 0;
        int semanticContacts = 0;
        if (semanticsValid)
        {
            const auto& points = doc["cloth"]["control_points"];
            for (const auto& point : points.GetArray())
            {
                if (!point.IsObject() || !point.HasMember("stable_id")
                    || !point["stable_id"].IsUint() || !point.HasMember("anchor")
                    || !point["anchor"].IsBool() || !point.HasMember("contact")
                    || !point["contact"].IsBool())
                {
                    semanticsValid = false;
                    break;
                }
                const unsigned int id = point["stable_id"].GetUint();
                if (id >= 147)
                {
                    semanticsValid = false;
                    break;
                }
                const bool expectedAnchor = (id % 21 == 0 || id % 21 == 20);
                const bool anchor = point["anchor"].GetBool();
                const bool contact = point["contact"].GetBool();
                if (anchor != expectedAnchor || !contact)
                {
                    semanticsValid = false;
                    break;
                }
                semanticAnchors += anchor ? 1 : 0;
                semanticContacts += contact ? 1 : 0;
                // Unsupported is a classification of contact-only input,
                // not a claim about a computed contact/Support output.
                contactOnlyUnsupported += (contact && !anchor) ? 1 : 0;
            }
            semanticsValid = semanticsValid && semanticAnchors == 14
                && semanticContacts == 147 && contactOnlyUnsupported == 133;
        }
        AZ_Printf("SansaClothBackendProbe",
            "SANSA_O3DE|OXC-005.CPP_ANCHOR_COUNT|%d\n", semanticAnchors);
        AZ_Printf("SansaClothBackendProbe",
            "SANSA_O3DE|OXC-005.CPP_CONTACT_INPUT_COUNT|%d\n", semanticContacts);
        AZ_Printf("SansaClothBackendProbe",
            "SANSA_O3DE|OXC-005.CPP_CONTACT_ONLY_UNSUPPORTED_COUNT|%d\n", contactOnlyUnsupported);
        AZ_Printf("SansaClothBackendProbe",
            "SANSA_O3DE|OXC-005.CPP_RESULT|%s\n", semanticsValid ? "PASS" : "FAIL");
        valid = valid && semanticsValid;
        // OXC-006: the SR-001-C0-G0 input contract is zero gravity,
        // zero conformity and zero collision tolerance (meters).
        bool inputsValid = valid;
        if (inputsValid)
        {
            const auto& inputs = doc["inputs"];
            inputsValid = inputs.HasMember("world_gravity_m_per_s2")
                && inputs.HasMember("conformity")
                && inputs.HasMember("collision_tolerance_m");
            if (inputsValid)
            {
                const auto& gravity = inputs["world_gravity_m_per_s2"];
                const auto& conformity = inputs["conformity"];
                const auto& tolerance = inputs["collision_tolerance_m"];
                inputsValid = gravity.IsArray() && gravity.Size() == 3
                    && conformity.IsNumber() && tolerance.IsNumber()
                    && std::isfinite(conformity.GetDouble())
                    && std::isfinite(tolerance.GetDouble())
                    && conformity.GetDouble() == 0.0
                    && tolerance.GetDouble() == 0.0;
                if (inputsValid)
                {
                    for (const auto& component : gravity.GetArray())
                    {
                        if (!component.IsNumber()
                            || !std::isfinite(component.GetDouble())
                            || component.GetDouble() != 0.0)
                        {
                            inputsValid = false;
                            break;
                        }
                    }
                }
            }
        }
        AZ_Printf("SansaClothBackendProbe",
            "SANSA_O3DE|OXC-006.CPP_RESULT|%s\n", inputsValid ? "PASS" : "FAIL");
        valid = valid && inputsValid;
        AZ_Printf("SansaClothBackendProbe",
            "SANSA_O3DE|OXC-002.CPP_BODY_VERTEX_COUNT|%d\n", vertexCount);
        AZ_Printf("SansaClothBackendProbe",
            "SANSA_O3DE|OXC-002.CPP_BODY_TRIANGLE_COUNT|%d\n", triangleCount);
        AZ_Printf("SansaClothBackendProbe",
            "SANSA_O3DE|OXC-002.CPP_CP_COUNT|%d\n", cpCount);
        AZ_Printf("SansaClothBackendProbe",
            "SANSA_O3DE|OXC-002.CPP_ANCHOR_COUNT|%d\n", anchorCount);
        AZ_Printf("SansaClothBackendProbe",
            "SANSA_O3DE|OXC-002.CPP_CONTACT_COUNT|%d\n", contactCount);
        AZ_Printf("SansaClothBackendProbe",
            "SANSA_O3DE|OXC-002.CPP_RESULT|%s\n", valid ? "PASS" : "FAIL");
        return valid ? AZStd::string("OXC-002|ACK|147|240|147|14|147") : AZStd::string{};
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


    // OSR validation-only: independently calculate the SR-001-C0-G0 reference
    // stages in double precision. This is NOT an OXC fixture ACK.
    AZStd::string SystemComponent::ProbeSurfaceResponseJson(const AZStd::string& payload)
    {
        struct V
        {
            double x, y, z;
            V operator+(V b) const { return {x+b.x,y+b.y,z+b.z}; }
            V operator-(V b) const { return {x-b.x,y-b.y,z-b.z}; }
            V operator*(double s) const { return {x*s,y*s,z*s}; }
            V operator/(double s) const { return {x/s,y/s,z/s}; }
            double dot(V b) const { return x*b.x+y*b.y+z*b.z; }
            V cross(V b) const { return {y*b.z-z*b.y,z*b.x-x*b.z,x*b.y-y*b.x}; }
            V normalized() const
            {
                const double n = std::sqrt(dot(*this));
                return n > 0 && std::isfinite(n) ? (*this)/n : V{0,0,0};
            }
        };
        auto vec = [](const rapidjson::Value& v) -> V {
            return {v[0].GetDouble(),v[1].GetDouble(),v[2].GetDouble()};
        };
        auto validVec = [](const rapidjson::Value& v) {
            return v.IsArray() && v.Size()==3 && v[0].IsNumber() && v[1].IsNumber()
                && v[2].IsNumber() && std::isfinite(v[0].GetDouble())
                && std::isfinite(v[1].GetDouble()) && std::isfinite(v[2].GetDouble());
        };
        rapidjson::Document d;
        d.Parse(payload.c_str());
        if (d.HasParseError() || !d.IsObject() || !d.HasMember("case_id")
            || !d["case_id"].IsString()
            || AZStd::string(d["case_id"].GetString()).find("SR-00") != 0
            || !d.HasMember("format") || !d["format"].IsString()
            || AZStd::string(d["format"].GetString()) != "sansacloth.validation.fixture-exchange/0"
            || !d.HasMember("body_surface") || !d["body_surface"].IsObject()
            || !d.HasMember("cloth") || !d["cloth"].IsObject()
            || !d.HasMember("inputs") || !d["inputs"].IsObject())
            return {};
        const auto& body = d["body_surface"];
        const auto& cloth = d["cloth"];
        const auto& inputs = d["inputs"];
        if (!body.HasMember("domain_id") || !body["domain_id"].IsUint64()
            || !body.HasMember("vertices") || !body["vertices"].IsArray()
            || !body.HasMember("triangles") || !body["triangles"].IsArray()
            || !cloth.HasMember("control_points") || !cloth["control_points"].IsArray()
            || cloth["control_points"].Size()!=147
            || !inputs.HasMember("world_gravity_m_per_s2")
            || !validVec(inputs["world_gravity_m_per_s2"])
            || !inputs.HasMember("conformity") || !inputs["conformity"].IsNumber()
            || !inputs.HasMember("collision_tolerance_m") || !inputs["collision_tolerance_m"].IsNumber())
            return {};
        const double conformity = inputs["conformity"].GetDouble();
        const double tolerance = inputs["collision_tolerance_m"].GetDouble();
        if (!std::isfinite(conformity) || conformity < 0 || conformity > 1
            || !std::isfinite(tolerance) || tolerance < 0)
            return {};
        const V gravityDirection = vec(inputs["world_gravity_m_per_s2"]).normalized();
        const auto& vertices = body["vertices"];
        const auto& triangles = body["triangles"];
        struct CP { unsigned id, strip, order; V pos; double u,v; bool anchor,contact; };
        std::array<CP,147> points{};
        std::array<bool,147> seen{};
        for (const auto& item : cloth["control_points"].GetArray())
        {
            if (!item.IsObject() || !item.HasMember("stable_id") || !item["stable_id"].IsUint()
                || !item.HasMember("strip_id") || !item["strip_id"].IsUint()
                || !item.HasMember("strip_order") || !item["strip_order"].IsUint()
                || !item.HasMember("position_m") || !validVec(item["position_m"])
                || !item.HasMember("anchor") || !item["anchor"].IsBool()
                || !item.HasMember("contact") || !item["contact"].IsBool()
                || !item.HasMember("surface_reference") || !item["surface_reference"].IsObject())
                return {};
            const auto& ref = item["surface_reference"];
            if (!ref.HasMember("domain_id") || !ref["domain_id"].IsUint64()
                || ref["domain_id"].GetUint64()!=body["domain_id"].GetUint64()
                || !ref.HasMember("u") || !ref["u"].IsNumber()
                || !ref.HasMember("v") || !ref["v"].IsNumber()) return {};
            unsigned id=item["stable_id"].GetUint();
            if (id>=147 || seen[id]) return {};
            seen[id]=true;
            points[id]={id,item["strip_id"].GetUint(),item["strip_order"].GetUint(),
                vec(item["position_m"]),ref["u"].GetDouble(),ref["v"].GetDouble(),
                item["anchor"].GetBool(),item["contact"].GetBool()};
        }
        struct Q { V surface,normal; double separation; V rawNormal,delta; double normalLength,weights[3],dotTerms[3]; V trianglePositions[3],edge01,edge02; };
        auto query = [&](const CP& point,V position,Q& out) -> bool {
            for (const auto& tri : triangles.GetArray())
            {
                if (!tri.IsArray() || tri.Size()!=3) return false;
                unsigned idx[3]; V xyz[3]; double uv[3][2];
                for (int k=0;k<3;++k)
                {
                    if (!tri[k].IsUint() || (idx[k]=tri[k].GetUint())>=vertices.Size()) return false;
                    const auto& v=vertices[idx[k]];
                    if (!v.IsObject() || !v.HasMember("position_m") || !validVec(v["position_m"])
                        || !v.HasMember("uv") || !v["uv"].IsArray() || v["uv"].Size()!=2
                        || !v["uv"][0].IsNumber() || !v["uv"][1].IsNumber()) return false;
                    xyz[k]=vec(v["position_m"]);
                    uv[k][0]=v["uv"][0].GetDouble(); uv[k][1]=v["uv"][1].GetDouble();
                }
                double ax=uv[1][0]-uv[0][0],ay=uv[1][1]-uv[0][1];
                double bx=uv[2][0]-uv[0][0],by=uv[2][1]-uv[0][1];
                double px=point.u-uv[0][0],py=point.v-uv[0][1];
                double denom=ax*by-ay*bx;
                if (std::abs(denom)<=1e-15) continue;
                double w1=(px*by-py*bx)/denom,w2=(ax*py-ay*px)/denom,w0=1-w1-w2;
                if (w0 < -1e-12 || w1 < -1e-12 || w2 < -1e-12) continue;
                V rawNormal=(xyz[1]-xyz[0]).cross(xyz[2]-xyz[0]);
                const double normalLength=std::sqrt(rawNormal.dot(rawNormal));
                V normal=rawNormal.normalized();
                if (normal.dot(normal)==0) return false;
                V surface=xyz[0]*w0+xyz[1]*w1+xyz[2]*w2;
                V delta=position-surface;
                out={surface,normal,delta.dot(normal),rawNormal,delta,normalLength,
                    {w0,w1,w2},{delta.x*normal.x,delta.y*normal.y,delta.z*normal.z},
                    {xyz[0],xyz[1],xyz[2]},xyz[1]-xyz[0],xyz[2]-xyz[0]};
                return true;
            }
            return false;
        };
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> w(buffer);
        auto writeVec = [&](V v) { w.StartArray();w.Double(v.x);w.Double(v.y);w.Double(v.z);w.EndArray(); };
        w.StartObject();
        w.Key("format");w.String("sansacloth.validation.surface-response-result/0");
        w.Key("case_id");w.String(d["case_id"].GetString());
        w.Key("profile");w.StartObject();
        w.Key("characteristic_length_m");w.Double(0.1);
        w.Key("quasi_static_gravity_scale");w.Double(0.1);
        w.Key("conformity_reach_m");w.Double(0.02);
        const bool oneEdge = AZStd::string(d["case_id"].GetString()).find("SR-003-") == 0;
        w.Key("support_layout");w.String(oneEdge ? "OneEdge" : "BothEdges");
        w.EndObject();
        w.Key("control_points");w.StartArray();
        int supports=0,contacts=0;
        for (const auto& p : points)
        {
            // Both-edge strip bridge: anchor endpoints at strip orders 0 and 20.
            if (p.strip>=7 || p.order>=21 || p.id!=p.strip*21+p.order) return {};
            const CP& left=points[p.strip*21], &right=points[p.strip*21+20];
            if (!left.anchor || (!oneEdge && !right.anchor)) return {};
            V bridge = (left.anchor && right.anchor && !p.anchor)
                ? left.pos*(1.0-p.order/20.0)+right.pos*(p.order/20.0) : p.pos;
            const double t = p.order / 20.0;
            const double weight = oneEdge ? t : 4.0*t*(1.0-t);
            V gravity = p.anchor ? bridge : bridge + gravityDirection*(0.1*0.1*weight);
            Q afterGravity{};
            if (!query(p,gravity,afterGravity)) return {};
            V conform=gravity;
            if (!p.anchor && conformity > 0 && afterGravity.separation >= 0)
            {
                const double distanceWeight = std::max(0.0, std::min(1.0,
                    1.0-afterGravity.separation/0.02));
                const double effective = conformity*distanceWeight;
                conform = gravity*(1.0-effective)+afterGravity.surface*effective;
            }
            Q afterConformity{};
            if (!query(p,conform,afterConformity)) return {};
            V collision=conform;
            if (afterConformity.separation<tolerance)
                collision=collision+afterConformity.normal.normalized()*(tolerance-afterConformity.separation);
            Q final{};
            if (!query(p,collision,final)) return {};
            bool contact=final.separation<=tolerance;
            supports+=p.anchor;contacts+=contact;
            w.StartObject();
            w.Key("stable_id");w.Uint(p.id);
            w.Key("support");w.String(p.anchor?"Anchor":"Unsupported");
            w.Key("bridge_position_m");writeVec(bridge);
            w.Key("gravity_position_m");writeVec(gravity);
            w.Key("conformity_position_m");writeVec(conform);
            w.Key("collision_position_m");writeVec(collision);
            w.Key("final_position_m");writeVec(collision);
            w.Key("surface_position_m");writeVec(final.surface);
            w.Key("surface_normal");writeVec(final.normal);
            w.Key("separation_m");w.Double(final.separation);
            w.Key("contact");w.Bool(contact);
            w.Key("diagnostic_query");w.StartObject();
            w.Key("triangle_positions_m");w.StartArray();
            for (V value : final.trianglePositions) writeVec(value);
            w.EndArray();
            w.Key("edge_01_m");writeVec(final.edge01);
            w.Key("edge_02_m");writeVec(final.edge02);
            w.Key("barycentric_weights");w.StartArray();
            for (double value : final.weights) w.Double(value);
            w.EndArray();
            w.Key("raw_normal");writeVec(final.rawNormal);
            w.Key("normal_length");w.Double(final.normalLength);
            w.Key("delta_position_m");writeVec(final.delta);
            w.Key("separation_dot_terms");w.StartArray();
            for (double value : final.dotTerms) w.Double(value);
            w.EndArray();
            w.EndObject();
            w.EndObject();
        }
        w.EndArray();
        w.Key("aggregate");w.StartObject();
        w.Key("control_point_count");w.Uint(147);
        w.Key("support_count");w.Int(supports);
        w.Key("contact_count");w.Int(contacts);
        w.EndObject();w.EndObject();
        return AZStd::string(buffer.GetString());
    }

} // namespace SansaClothBackendProbeValidation
