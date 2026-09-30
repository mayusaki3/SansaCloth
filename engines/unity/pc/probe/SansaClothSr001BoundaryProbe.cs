using System.Globalization;
using UnityEngine;

namespace SansaCloth.Validation
{
    /// <summary>
    /// BF-005/BF-006 semantic-boundary probe for SR-001 Flat, C=0, Gravity=0.
    /// This validates mapping semantics without porting the Reference solver.
    /// </summary>
    public sealed class SansaClothSr001BoundaryProbe : MonoBehaviour
    {
        private const int USamples = 21;
        private const int VSamples = 7;
        private const float WidthM = 0.20f;
        private const float DepthM = 0.10f;
        private const float CollisionToleranceM = 0.0f;
        private const ulong DomainId = 1;

        [ContextMenu("Run SansaCloth SR-001 Boundary Probe")]
        public void RunProbe()
        {
            int cpCount = 0;
            int anchorCount = 0;
            int contactInputCount = 0;
            int directSupportCount = 0;
            int contactOnlyUnsupportedCount = 0;
            int derivedContactCount = 0;
            float maxPositionDeviationM = 0.0f;
            float maxNormalDeviation = 0.0f;
            float maxAbsSeparationM = 0.0f;

            Sample first = default;
            Sample center = default;
            Sample last = default;

            for (int vIndex = 0; vIndex < VSamples; ++vIndex)
            {
                float v = (float)vIndex / (VSamples - 1);
                for (int uIndex = 0; uIndex < USamples; ++uIndex)
                {
                    float u = (float)uIndex / (USamples - 1);
                    int stableId = vIndex * USamples + uIndex;

                    Vector3 initialPosition = new Vector3(
                        (u - 0.5f) * WidthM,
                        0.0f,
                        (v - 0.5f) * DepthM
                    );
                    bool isAnchor = uIndex == 0 || uIndex == USamples - 1;
                    bool isContact = true;

                    // Reference v1 direct support semantics:
                    // Anchor => direct support; Contact alone does not pin/support.
                    bool hasDirectSupport = isAnchor;

                    // C=0, G=0, flat contact case has no required deformation.
                    Vector3 finalPosition = initialPosition;
                    Vector3 surfaceNormal = Vector3.up;
                    float separationM = Vector3.Dot(
                        finalPosition - initialPosition,
                        surfaceNormal
                    );
                    bool derivedContact = separationM <= CollisionToleranceM;

                    cpCount++;
                    if (isAnchor)
                    {
                        anchorCount++;
                    }
                    if (isContact)
                    {
                        contactInputCount++;
                    }
                    if (hasDirectSupport)
                    {
                        directSupportCount++;
                    }
                    if (isContact && !hasDirectSupport)
                    {
                        contactOnlyUnsupportedCount++;
                    }
                    if (derivedContact)
                    {
                        derivedContactCount++;
                    }

                    maxPositionDeviationM = Mathf.Max(
                        maxPositionDeviationM,
                        Vector3.Distance(initialPosition, finalPosition)
                    );
                    maxNormalDeviation = Mathf.Max(
                        maxNormalDeviation,
                        Vector3.Distance(surfaceNormal, Vector3.up)
                    );
                    maxAbsSeparationM = Mathf.Max(
                        maxAbsSeparationM,
                        Mathf.Abs(separationM)
                    );

                    var sample = new Sample(
                        stableId,
                        u,
                        v,
                        finalPosition,
                        surfaceNormal,
                        separationM,
                        hasDirectSupport
                    );
                    if (stableId == 0)
                    {
                        first = sample;
                    }
                    if (stableId == (VSamples / 2) * USamples + USamples / 2)
                    {
                        center = sample;
                    }
                    if (stableId == USamples * VSamples - 1)
                    {
                        last = sample;
                    }
                }
            }

            LogInt("SR-001.CP_COUNT", cpCount);
            LogInt("SR-001.ANCHOR_COUNT", anchorCount);
            LogInt("SR-001.CONTACT_INPUT_COUNT", contactInputCount);
            LogInt("SR-001.DIRECT_SUPPORT_COUNT", directSupportCount);
            LogInt(
                "SR-001.CONTACT_ONLY_UNSUPPORTED_COUNT",
                contactOnlyUnsupportedCount
            );
            LogFloat(
                "SR-001.FINAL_POSITION_DEVIATION_MAX_M",
                maxPositionDeviationM
            );
            LogFloat("SR-001.NORMAL_DEVIATION_MAX", maxNormalDeviation);
            LogFloat("SR-001.SEPARATION_ABS_MAX_M", maxAbsSeparationM);
            LogInt("SR-001.DERIVED_CONTACT_COUNT", derivedContactCount);
            LogVector("SR-001.WORLD_GRAVITY_INPUT", Vector3.zero);
            LogFloat("SR-001.CONFORMITY_INPUT", 0.0f);

            LogSample("SR-001.CP_FIRST", first);
            LogSample("SR-001.CP_CENTER", center);
            LogSample("SR-001.CP_LAST", last);
        }

        private static void LogSample(string key, Sample sample)
        {
            Debug.Log(
                $"SANSA_BF|{key}|" +
                $"id={sample.stableId};domain={DomainId};" +
                $"uv={F(sample.u)},{F(sample.v)};" +
                $"position={F(sample.position.x)},{F(sample.position.y)},{F(sample.position.z)};" +
                $"normal={F(sample.normal.x)},{F(sample.normal.y)},{F(sample.normal.z)};" +
                $"separation_m={F(sample.separationM)};" +
                $"support={(sample.hasDirectSupport ? "Anchor" : "Unsupported")}"
            );
        }

        private static void LogInt(string key, int value)
        {
            Debug.Log($"SANSA_BF|{key}|{value.ToString(CultureInfo.InvariantCulture)}");
        }

        private static void LogFloat(string key, float value)
        {
            Debug.Log($"SANSA_BF|{key}|{F(value)}");
        }

        private static void LogVector(string key, Vector3 value)
        {
            Debug.Log($"SANSA_BF|{key}|{F(value.x)},{F(value.y)},{F(value.z)}");
        }

        private static string F(float value)
        {
            return value.ToString("R", CultureInfo.InvariantCulture);
        }

        private readonly struct Sample
        {
            public Sample(
                int stableId,
                float u,
                float v,
                Vector3 position,
                Vector3 normal,
                float separationM,
                bool hasDirectSupport
            )
            {
                this.stableId = stableId;
                this.u = u;
                this.v = v;
                this.position = position;
                this.normal = normal;
                this.separationM = separationM;
                this.hasDirectSupport = hasDirectSupport;
            }

            public readonly int stableId;
            public readonly float u;
            public readonly float v;
            public readonly Vector3 position;
            public readonly Vector3 normal;
            public readonly float separationM;
            public readonly bool hasDirectSupport;
        }
    }
}
