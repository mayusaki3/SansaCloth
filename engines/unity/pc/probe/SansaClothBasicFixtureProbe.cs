using System.Globalization;
using UnityEngine;

namespace SansaCloth.Validation
{
    /// <summary>
    /// BF-007 fixture-mapping probe for SR-001 through SR-005.
    /// Reproduces the Reference analytic fixture definitions in Unity float precision.
    /// </summary>
    public sealed class SansaClothBasicFixtureProbe : MonoBehaviour
    {
        private const int USamples = 21;
        private const int VSamples = 7;
        private const float WidthM = 0.20f;
        private const float DepthM = 0.10f;
        private const float FeatureWidthM = 0.10f;

        [ContextMenu("Run SansaCloth Basic Fixture Probe")]
        public void RunProbe()
        {
            ProbeFixture("SR-001.FLAT", FixtureKind.Flat, 0.0f, false, false);
            ProbeFixture("SR-002.CONVEX_UP", FixtureKind.Convex, 0.03f, false, false);
            ProbeFixture("SR-003.CONVEX_SIDE", FixtureKind.Convex, 0.03f, true, true);
            ProbeFixture(
                "SR-004.CONCAVE_SHALLOW",
                FixtureKind.Concave,
                0.02f,
                false,
                false
            );
            ProbeFixture(
                "SR-005.CONCAVE_DEEP",
                FixtureKind.Concave,
                0.05f,
                false,
                false
            );
        }

        private static void ProbeFixture(
            string key,
            FixtureKind kind,
            float magnitudeM,
            bool sideOrientation,
            bool oneEdgeAnchor
        )
        {
            Quaternion rotation = sideOrientation
                ? Quaternion.AngleAxis(-90.0f, Vector3.forward)
                : Quaternion.identity;

            int cpCount = 0;
            int anchorCount = 0;
            int vertexCount = 0;
            int triangleCount = (USamples - 1) * (VSamples - 1) * 2;
            float minLocalHeightM = float.PositiveInfinity;
            float maxLocalHeightM = float.NegativeInfinity;
            Vector3 centerPosition = default;
            Vector3 centerNormal = default;
            Vector3 firstTriangleNormal = default;

            Vector3[,] positions = new Vector3[VSamples, USamples];

            for (int vIndex = 0; vIndex < VSamples; ++vIndex)
            {
                float v = (float)vIndex / (VSamples - 1);
                for (int uIndex = 0; uIndex < USamples; ++uIndex)
                {
                    float u = (float)uIndex / (USamples - 1);
                    Vector3 localPosition = LocalPosition(kind, magnitudeM, u, v);
                    Vector3 worldPosition = rotation * localPosition;
                    positions[vIndex, uIndex] = worldPosition;

                    minLocalHeightM = Mathf.Min(minLocalHeightM, localPosition.y);
                    maxLocalHeightM = Mathf.Max(maxLocalHeightM, localPosition.y);
                    vertexCount++;
                    cpCount++;

                    bool anchor = oneEdgeAnchor
                        ? uIndex == 0
                        : uIndex == 0 || uIndex == USamples - 1;
                    if (anchor)
                    {
                        anchorCount++;
                    }

                    if (uIndex == USamples / 2 && vIndex == VSamples / 2)
                    {
                        centerPosition = worldPosition;
                        centerNormal = rotation * LocalNormal(kind, magnitudeM, u);
                    }
                }
            }

            Vector3 v00 = positions[0, 0];
            Vector3 v01 = positions[1, 0];
            Vector3 v11 = positions[1, 1];
            firstTriangleNormal = Vector3.Cross(v01 - v00, v11 - v00).normalized;

            LogInt($"{key}.VERTEX_COUNT", vertexCount);
            LogInt($"{key}.TRIANGLE_COUNT", triangleCount);
            LogInt($"{key}.CP_COUNT", cpCount);
            LogInt($"{key}.ANCHOR_COUNT", anchorCount);
            LogFloat($"{key}.LOCAL_MIN_HEIGHT_M", minLocalHeightM);
            LogFloat($"{key}.LOCAL_MAX_HEIGHT_M", maxLocalHeightM);
            LogVector($"{key}.CENTER_POSITION", centerPosition);
            LogVector($"{key}.CENTER_NORMAL", centerNormal);
            LogVector($"{key}.FIRST_TRIANGLE_NORMAL", firstTriangleNormal);
        }

        private static Vector3 LocalPosition(
            FixtureKind kind,
            float magnitudeM,
            float u,
            float v
        )
        {
            float x = (u - 0.5f) * WidthM;
            float z = (v - 0.5f) * DepthM;
            float feature = RaisedCosine(x, FeatureWidthM, magnitudeM);
            float y = kind switch
            {
                FixtureKind.Flat => 0.0f,
                FixtureKind.Convex => feature,
                FixtureKind.Concave => -feature,
                _ => 0.0f,
            };
            return new Vector3(x, y, z);
        }

        private static Vector3 LocalNormal(
            FixtureKind kind,
            float magnitudeM,
            float u
        )
        {
            float x = (u - 0.5f) * WidthM;
            float derivative = RaisedCosineDerivative(x, FeatureWidthM, magnitudeM);
            float dydx = kind switch
            {
                FixtureKind.Flat => 0.0f,
                FixtureKind.Convex => derivative,
                FixtureKind.Concave => -derivative,
                _ => 0.0f,
            };
            return new Vector3(-dydx, 1.0f, 0.0f).normalized;
        }

        private static float RaisedCosine(float x, float widthM, float heightM)
        {
            if (Mathf.Abs(x) > widthM * 0.5f)
            {
                return 0.0f;
            }

            return heightM * 0.5f * (
                1.0f + Mathf.Cos(2.0f * Mathf.PI * x / widthM)
            );
        }

        private static float RaisedCosineDerivative(
            float x,
            float widthM,
            float heightM
        )
        {
            if (Mathf.Abs(x) > widthM * 0.5f)
            {
                return 0.0f;
            }

            return -(heightM * Mathf.PI / widthM)
                * Mathf.Sin(2.0f * Mathf.PI * x / widthM);
        }

        private static void LogInt(string key, int value)
        {
            Debug.Log($"SANSA_BF|{key}|{value.ToString(CultureInfo.InvariantCulture)}");
        }

        private static void LogFloat(string key, float value)
        {
            Debug.Log($"SANSA_BF|{key}|{value.ToString("R", CultureInfo.InvariantCulture)}");
        }

        private static void LogVector(string key, Vector3 value)
        {
            Debug.Log(
                $"SANSA_BF|{key}|" +
                $"{value.x.ToString("R", CultureInfo.InvariantCulture)}," +
                $"{value.y.ToString("R", CultureInfo.InvariantCulture)}," +
                $"{value.z.ToString("R", CultureInfo.InvariantCulture)}"
            );
        }

        private enum FixtureKind
        {
            Flat,
            Convex,
            Concave,
        }
    }
}
