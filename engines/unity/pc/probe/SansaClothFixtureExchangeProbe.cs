using System;
using System.Collections.Generic;
using System.Globalization;
using UnityEngine;

namespace SansaCloth.Validation
{
    /// <summary>
    /// FXE-011/012/013 Unity PC validation probe.
    /// Imports resolved Reference fixture data instead of rebuilding analytic fixtures in Unity.
    /// </summary>
    public sealed class SansaClothFixtureExchangeProbe : MonoBehaviour
    {
        private const int ExpectedVertexCount = 147;
        private const int ExpectedTriangleCount = 240;
        private const int ExpectedControlPointCount = 147;
        private const int ExpectedAnchorCount = 14;
        private const int ExpectedContactCount = 147;
        private const int ExpectedStripCount = 7;
        private const int ExpectedStripLength = 21;
        private const double ExpectedWidthM = 0.20;
        private const double ExpectedDepthM = 0.10;
        private const double DoubleTolerance = 1.0e-12;
        private const float UnityTolerance = 1.0e-6f;

        [SerializeField]
        private TextAsset fixtureExchangeJson;

        [ContextMenu("Run SansaCloth Fixture Exchange Probe")]
        public void RunProbe()
        {
            if (fixtureExchangeJson == null)
            {
                Debug.LogError(
                    "SANSA_FXE|ERROR|Assign SR-001-C0-G0.json to Fixture Exchange Json."
                );
                return;
            }

            try
            {
                FixtureExchangeDocument document =
                    SansaClothFixtureExchangeJson.Parse(fixtureExchangeJson.text);

                RunFxe011(document);
                RunFxe012(document);
                RunFxe013(fixtureExchangeJson.text);
            }
            catch (Exception error)
            {
                Debug.LogError(
                    $"SANSA_FXE|ERROR|{error.GetType().Name}:{error.Message}"
                );
            }
        }

        private static void RunFxe011(FixtureExchangeDocument document)
        {
            Require(document.caseId == "SR-001-C0-G0", "unexpected case_id");
            Require(
                document.bodySurface.vertices.Length == ExpectedVertexCount,
                "FXE-011 vertex count mismatch"
            );
            Require(
                document.bodySurface.triangles.Length == ExpectedTriangleCount,
                "FXE-011 triangle count mismatch"
            );
            Require(
                document.cloth.controlPoints.Length == ExpectedControlPointCount,
                "FXE-011 control point count mismatch"
            );

            double minX = double.PositiveInfinity;
            double maxX = double.NegativeInfinity;
            double minZ = double.PositiveInfinity;
            double maxZ = double.NegativeInfinity;
            double maxAbsY = 0.0;
            double maxNormalDeviation = 0.0;
            int centerCount = 0;
            BodyVertex center = null;

            for (int i = 0; i < document.bodySurface.vertices.Length; ++i)
            {
                BodyVertex vertex = document.bodySurface.vertices[i];
                minX = Math.Min(minX, vertex.positionM[0]);
                maxX = Math.Max(maxX, vertex.positionM[0]);
                minZ = Math.Min(minZ, vertex.positionM[2]);
                maxZ = Math.Max(maxZ, vertex.positionM[2]);
                maxAbsY = Math.Max(maxAbsY, Math.Abs(vertex.positionM[1]));
                maxNormalDeviation = Math.Max(
                    maxNormalDeviation,
                    Distance3(vertex.normal, new[] { 0.0, 1.0, 0.0 })
                );

                if (
                    Math.Abs(vertex.uv[0] - 0.5) <= DoubleTolerance
                    && Math.Abs(vertex.uv[1] - 0.5) <= DoubleTolerance
                )
                {
                    center = vertex;
                    centerCount++;
                }
            }

            Require(
                Math.Abs((maxX - minX) - ExpectedWidthM) <= DoubleTolerance,
                "FXE-011 width mismatch"
            );
            Require(
                Math.Abs((maxZ - minZ) - ExpectedDepthM) <= DoubleTolerance,
                "FXE-011 depth mismatch"
            );
            Require(maxAbsY <= DoubleTolerance, "FXE-011 flat height mismatch");
            Require(
                maxNormalDeviation <= DoubleTolerance,
                "FXE-011 body normal mismatch"
            );
            Require(centerCount == 1 && center != null, "FXE-011 center vertex missing");
            Require(
                Distance3(center.positionM, new[] { 0.0, 0.0, 0.0 })
                    <= DoubleTolerance,
                "FXE-011 center position mismatch"
            );

            ValidateBasicTopology(document);
            ValidateUnityMeshMapping(document);

            Log("FXE-011.RESULT", "PASS");
            LogInt("FXE-011.VERTEX_COUNT", document.bodySurface.vertices.Length);
            LogInt("FXE-011.TRIANGLE_COUNT", document.bodySurface.triangles.Length);
            LogInt(
                "FXE-011.CP_COUNT",
                document.cloth.controlPoints.Length
            );
            LogDouble("FXE-011.WIDTH_M", maxX - minX);
            LogDouble("FXE-011.DEPTH_M", maxZ - minZ);
            LogDouble("FXE-011.MAX_ABS_HEIGHT_M", maxAbsY);
            LogDouble("FXE-011.MAX_NORMAL_DEVIATION", maxNormalDeviation);
        }

        private static void RunFxe012(FixtureExchangeDocument document)
        {
            int anchorCount = 0;
            int contactCount = 0;
            int directSupportCount = 0;
            int contactOnlyUnsupportedCount = 0;
            var stableIds = new HashSet<ulong>();
            var strips = new Dictionary<ulong, SortedSet<ulong>>();

            for (int i = 0; i < document.cloth.controlPoints.Length; ++i)
            {
                ControlPoint point = document.cloth.controlPoints[i];
                Require(
                    stableIds.Add(point.stableId),
                    "FXE-012 duplicate stable_id"
                );
                if (!strips.TryGetValue(point.stripId, out SortedSet<ulong> orders))
                {
                    orders = new SortedSet<ulong>();
                    strips.Add(point.stripId, orders);
                }
                Require(
                    orders.Add(point.stripOrder),
                    "FXE-012 duplicate strip/order"
                );

                if (point.anchor)
                {
                    anchorCount++;
                    directSupportCount++;
                }
                if (point.contact)
                {
                    contactCount++;
                    if (!point.anchor)
                    {
                        contactOnlyUnsupportedCount++;
                    }
                }

                Require(
                    point.surfaceReference.domainId
                        == document.bodySurface.domainId,
                    "FXE-012 SurfaceReference domain mismatch"
                );
            }

            Require(
                stableIds.Count == ExpectedControlPointCount,
                "FXE-012 stable ID count mismatch"
            );
            for (ulong id = 0; id < ExpectedControlPointCount; ++id)
            {
                Require(stableIds.Contains(id), $"FXE-012 missing stable_id {id}");
            }

            Require(strips.Count == ExpectedStripCount, "FXE-012 strip count mismatch");
            for (ulong stripId = 0; stripId < ExpectedStripCount; ++stripId)
            {
                Require(
                    strips.TryGetValue(stripId, out SortedSet<ulong> orders),
                    $"FXE-012 missing strip {stripId}"
                );
                Require(
                    orders.Count == ExpectedStripLength,
                    $"FXE-012 strip {stripId} length mismatch"
                );
                for (ulong order = 0; order < ExpectedStripLength; ++order)
                {
                    Require(
                        orders.Contains(order),
                        $"FXE-012 strip {stripId} missing order {order}"
                    );
                }
            }

            Require(anchorCount == ExpectedAnchorCount, "FXE-012 anchor count mismatch");
            Require(
                contactCount == ExpectedContactCount,
                "FXE-012 contact input count mismatch"
            );
            Require(
                directSupportCount == ExpectedAnchorCount,
                "FXE-012 direct support count mismatch"
            );
            Require(
                contactOnlyUnsupportedCount
                    == ExpectedContactCount - ExpectedAnchorCount,
                "FXE-012 contact-only unsupported count mismatch"
            );
            Require(
                Distance3(
                    document.inputs.worldGravityMPerS2,
                    new[] { 0.0, 0.0, 0.0 }
                ) <= DoubleTolerance,
                "FXE-012 gravity input mismatch"
            );
            Require(
                Math.Abs(document.inputs.conformity) <= DoubleTolerance,
                "FXE-012 conformity input mismatch"
            );
            Require(
                Math.Abs(document.inputs.collisionToleranceM) <= DoubleTolerance,
                "FXE-012 collision tolerance mismatch"
            );

            Log("FXE-012.RESULT", "PASS");
            LogInt("FXE-012.ANCHOR_COUNT", anchorCount);
            LogInt("FXE-012.CONTACT_INPUT_COUNT", contactCount);
            LogInt("FXE-012.DIRECT_SUPPORT_COUNT", directSupportCount);
            LogInt(
                "FXE-012.CONTACT_ONLY_UNSUPPORTED_COUNT",
                contactOnlyUnsupportedCount
            );
            LogInt("FXE-012.STRIP_COUNT", strips.Count);
            LogInt("FXE-012.STRIP_LENGTH", ExpectedStripLength);
            LogVector(
                "FXE-012.WORLD_GRAVITY_M_PER_S2",
                document.inputs.worldGravityMPerS2
            );
            LogDouble("FXE-012.CONFORMITY", document.inputs.conformity);
            LogDouble(
                "FXE-012.COLLISION_TOLERANCE_M",
                document.inputs.collisionToleranceM
            );
        }

        private static void RunFxe013(string validJson)
        {
            int rejected = 0;
            rejected += RejectMutation(
                validJson,
                "FORMAT",
                document => document.format = "sansacloth.validation.fixture-exchange/999"
            );
            rejected += RejectMutation(
                validJson,
                "DOMAIN",
                document =>
                    document.cloth.controlPoints[0].surfaceReference.domainId = 999
            );
            rejected += RejectMutation(
                validJson,
                "UV",
                document => document.cloth.controlPoints[0].surfaceReference.u = 1.1
            );
            rejected += RejectMutation(
                validJson,
                "TRIANGLE_INDEX",
                document =>
                    document.bodySurface.triangles[0][0] =
                        document.bodySurface.vertices.Length
            );
            rejected += RejectMutation(
                validJson,
                "STABLE_ID",
                document =>
                    document.cloth.controlPoints[1].stableId =
                        document.cloth.controlPoints[0].stableId
            );
            rejected += RejectMutation(
                validJson,
                "STRIP_ORDER",
                document =>
                {
                    document.cloth.controlPoints[1].stripId =
                        document.cloth.controlPoints[0].stripId;
                    document.cloth.controlPoints[1].stripOrder =
                        document.cloth.controlPoints[0].stripOrder;
                }
            );
            rejected += RejectMutation(
                validJson,
                "NORMAL",
                document => document.bodySurface.vertices[0].normal =
                    new[] { 0.0, 0.0, 0.0 }
            );
            rejected += RejectMutation(
                validJson,
                "CONFORMITY",
                document => document.inputs.conformity = 1.1
            );
            rejected += RejectMutation(
                validJson,
                "COLLISION_TOLERANCE",
                document => document.inputs.collisionToleranceM = -0.001
            );

            const int expectedRejected = 9;
            Require(
                rejected == expectedRejected,
                $"FXE-013 rejection count mismatch: {rejected}/{expectedRejected}"
            );
            Log("FXE-013.RESULT", "PASS");
            LogInt("FXE-013.REJECTED_CASE_COUNT", rejected);
        }

        private static int RejectMutation(
            string validJson,
            string key,
            Action<FixtureExchangeDocument> mutate
        )
        {
            FixtureExchangeDocument document =
                SansaClothFixtureExchangeJson.Parse(validJson);
            mutate(document);
            try
            {
                SansaClothFixtureExchangeJson.Validate(document);
            }
            catch (FixtureExchangeException)
            {
                Log($"FXE-013.{key}", "REJECTED");
                return 1;
            }

            throw new FixtureExchangeException(
                $"FXE-013 {key} invalid data was accepted"
            );
        }

        private static void ValidateBasicTopology(FixtureExchangeDocument document)
        {
            int expectedTriangle = 0;
            for (int v = 0; v < ExpectedStripCount - 1; ++v)
            {
                for (int u = 0; u < ExpectedStripLength - 1; ++u)
                {
                    int v00 = v * ExpectedStripLength + u;
                    int v10 = v00 + 1;
                    int v01 = v00 + ExpectedStripLength;
                    int v11 = v01 + 1;

                    RequireTriangle(
                        document.bodySurface.triangles[expectedTriangle++],
                        v00,
                        v01,
                        v11
                    );
                    RequireTriangle(
                        document.bodySurface.triangles[expectedTriangle++],
                        v00,
                        v11,
                        v10
                    );
                }
            }
            Require(
                expectedTriangle == ExpectedTriangleCount,
                "FXE-011 topology count mismatch"
            );
        }

        private static void ValidateUnityMeshMapping(FixtureExchangeDocument document)
        {
            var positions = new Vector3[document.bodySurface.vertices.Length];
            for (int i = 0; i < positions.Length; ++i)
            {
                positions[i] = ToUnity(document.bodySurface.vertices[i].positionM);
            }

            int[] first = document.bodySurface.triangles[0];
            Vector3 v00 = positions[first[0]];
            Vector3 v01 = positions[first[1]];
            Vector3 v11 = positions[first[2]];
            Vector3 normal = Vector3.Cross(v01 - v00, v11 - v00).normalized;
            Require(
                Vector3.Distance(normal, Vector3.up) <= UnityTolerance,
                "FXE-011 Unity first-triangle normal mismatch"
            );
        }

        private static void RequireTriangle(
            int[] actual,
            int a,
            int b,
            int c
        )
        {
            Require(
                actual.Length == 3
                    && actual[0] == a
                    && actual[1] == b
                    && actual[2] == c,
                "FXE-011 fixed-diagonal topology mismatch"
            );
        }

        private static Vector3 ToUnity(double[] value)
        {
            return new Vector3((float)value[0], (float)value[1], (float)value[2]);
        }

        private static double Distance3(double[] a, double[] b)
        {
            double x = a[0] - b[0];
            double y = a[1] - b[1];
            double z = a[2] - b[2];
            return Math.Sqrt(x * x + y * y + z * z);
        }

        private static void Require(bool condition, string message)
        {
            if (!condition)
            {
                throw new FixtureExchangeException(message);
            }
        }

        private static void Log(string key, string value)
        {
            Debug.Log($"SANSA_FXE|{key}|{value}");
        }

        private static void LogInt(string key, int value)
        {
            Log(key, value.ToString(CultureInfo.InvariantCulture));
        }

        private static void LogDouble(string key, double value)
        {
            Log(key, value.ToString("R", CultureInfo.InvariantCulture));
        }

        private static void LogVector(string key, double[] value)
        {
            Log(
                key,
                $"{value[0].ToString("R", CultureInfo.InvariantCulture)}," +
                $"{value[1].ToString("R", CultureInfo.InvariantCulture)}," +
                $"{value[2].ToString("R", CultureInfo.InvariantCulture)}"
            );
        }
    }
}
