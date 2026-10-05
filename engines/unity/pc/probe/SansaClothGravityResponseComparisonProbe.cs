using System;
using System.Collections.Generic;
using System.Globalization;
using UnityEngine;

namespace SansaCloth.Validation
{
    /// <summary>
    /// FXE-017B validation-only comparison of Unity C0 Gravity Response
    /// against the Reference SurfaceResponse result baseline.
    /// Assign the ten C0 Fixture Exchange JSON files and matching ten result JSON files.
    /// </summary>
    public sealed class SansaClothGravityResponseComparisonProbe : MonoBehaviour
    {
        private const int ExpectedCaseCount = 10;
        private const double PositionToleranceM = 1.0e-6;

        [SerializeField]
        private TextAsset[] fixtureExchangeC0Json;

        [SerializeField]
        private TextAsset[] surfaceResponseResultC0Json;

        [ContextMenu("Run SansaCloth FXE-017B Gravity Comparison")]
        public void RunProbe()
        {
            try
            {
                Dictionary<string, FixtureExchangeDocument> fixtures =
                    LoadFixtures();
                Dictionary<string, SurfaceResponseResultDocument> references =
                    LoadReferences();

                string[] scenarios =
                {
                    "SR-001",
                    "SR-002",
                    "SR-003",
                    "SR-004",
                    "SR-005",
                };
                string[] gravities = { "G0", "G1" };

                double overallBridgeMaxM = 0.0;
                double overallGravityMaxM = 0.0;
                int checkedCases = 0;

                foreach (string scenario in scenarios)
                {
                    foreach (string gravity in gravities)
                    {
                        string caseId = $"{scenario}-C0-{gravity}";
                        Require(
                            fixtures.TryGetValue(
                                caseId,
                                out FixtureExchangeDocument fixture
                            ),
                            $"missing Fixture Exchange case {caseId}"
                        );
                        Require(
                            references.TryGetValue(
                                caseId,
                                out SurfaceResponseResultDocument reference
                            ),
                            $"missing SurfaceResponse result case {caseId}"
                        );

                        Require(
                            Math.Abs(fixture.inputs.conformity) <= 1.0e-15,
                            $"{caseId} must have Conformity=0"
                        );
                        string expectedLayout =
                            scenario == "SR-003" ? "OneEdge" : "BothEdges";
                        Require(
                            reference.profile.support_layout == expectedLayout,
                            $"{caseId} support layout mismatch"
                        );

                        var surfaceQuery =
                            new SansaClothResolvedMeshSurfaceQuery(
                                fixture.bodySurface
                            );
                        ImportedSurfaceResponseResult actual =
                            SansaClothImportedSurfaceResponse.SolveC0(
                                fixture,
                                surfaceQuery
                            );

                        Dictionary<long, SurfaceResponseControlPointResult>
                            referenceByStableId = IndexReference(reference);

                        double bridgeMaxM = 0.0;
                        double gravityMaxM = 0.0;
                        for (
                            int i = 0;
                            i < fixture.cloth.controlPoints.Length;
                            ++i
                        )
                        {
                            ControlPoint point =
                                fixture.cloth.controlPoints[i];
                            Require(
                                point.stableId <= long.MaxValue,
                                $"{caseId} stable_id is too large"
                            );
                            long stableId = (long)point.stableId;
                            Require(
                                referenceByStableId.TryGetValue(
                                    stableId,
                                    out SurfaceResponseControlPointResult expected
                                ),
                                $"{caseId} missing result stable_id {stableId}"
                            );

                            string expectedSupport =
                                actual.support[i] ? "Anchor" : "Unsupported";
                            Require(
                                expected.support == expectedSupport,
                                $"{caseId} support mismatch at stable_id {stableId}"
                            );

                            bridgeMaxM = Math.Max(
                                bridgeMaxM,
                                PositionDeviation(
                                    actual.bridgePositionsM[i],
                                    expected.bridge_position_m
                                )
                            );
                            gravityMaxM = Math.Max(
                                gravityMaxM,
                                PositionDeviation(
                                    actual.gravityPositionsM[i],
                                    expected.gravity_position_m
                                )
                            );
                        }

                        Require(
                            bridgeMaxM <= PositionToleranceM,
                            $"{caseId} Bridge deviation {bridgeMaxM:R} m exceeds tolerance"
                        );
                        Require(
                            gravityMaxM <= PositionToleranceM,
                            $"{caseId} Gravity deviation {gravityMaxM:R} m exceeds tolerance"
                        );

                        overallBridgeMaxM = Math.Max(
                            overallBridgeMaxM,
                            bridgeMaxM
                        );
                        overallGravityMaxM = Math.Max(
                            overallGravityMaxM,
                            gravityMaxM
                        );
                        checkedCases++;

                        LogDouble(
                            $"FXE-017B.CASE.{caseId}.BRIDGE_MAX_POSITION_DEVIATION_M",
                            bridgeMaxM
                        );
                        LogDouble(
                            $"FXE-017B.CASE.{caseId}.GRAVITY_MAX_POSITION_DEVIATION_M",
                            gravityMaxM
                        );
                        Log($"FXE-017B.CASE.{caseId}", "PASS");
                    }
                }

                Require(
                    checkedCases == ExpectedCaseCount,
                    "FXE-017B checked case count mismatch"
                );
                LogInt("FXE-017B.CASE_COUNT", checkedCases);
                LogDouble(
                    "FXE-017B.BRIDGE_MAX_POSITION_DEVIATION_M",
                    overallBridgeMaxM
                );
                LogDouble(
                    "FXE-017B.GRAVITY_MAX_POSITION_DEVIATION_M",
                    overallGravityMaxM
                );
                LogDouble(
                    "FXE-017B.POSITION_TOLERANCE_M",
                    PositionToleranceM
                );
                Log("FXE-017B.RESULT", "PASS");
            }
            catch (Exception exception)
            {
                Debug.LogError(
                    $"SANSA_FXE|FXE-017B.RESULT|FAIL|{exception.Message}"
                );
                throw;
            }
        }

        private Dictionary<string, FixtureExchangeDocument> LoadFixtures()
        {
            Require(
                fixtureExchangeC0Json != null
                    && fixtureExchangeC0Json.Length == ExpectedCaseCount,
                $"Assign exactly {ExpectedCaseCount} C0 Fixture Exchange JSON TextAssets."
            );

            var result = new Dictionary<string, FixtureExchangeDocument>();
            foreach (TextAsset asset in fixtureExchangeC0Json)
            {
                Require(asset != null, "C0 Fixture Exchange array has an empty slot");
                FixtureExchangeDocument document =
                    SansaClothFixtureExchangeJson.Parse(asset.text);
                Require(
                    result.AddIfAbsent(document.caseId, document),
                    $"duplicate Fixture Exchange case_id {document.caseId}"
                );
            }
            return result;
        }

        private Dictionary<string, SurfaceResponseResultDocument> LoadReferences()
        {
            Require(
                surfaceResponseResultC0Json != null
                    && surfaceResponseResultC0Json.Length == ExpectedCaseCount,
                $"Assign exactly {ExpectedCaseCount} C0 SurfaceResponse result JSON TextAssets."
            );

            var result =
                new Dictionary<string, SurfaceResponseResultDocument>();
            foreach (TextAsset asset in surfaceResponseResultC0Json)
            {
                Require(asset != null, "C0 result array has an empty slot");
                SurfaceResponseResultDocument document =
                    SansaClothSurfaceResponseResultJson.Parse(asset.text);
                Require(
                    !result.ContainsKey(document.case_id),
                    $"duplicate SurfaceResponse result case_id {document.case_id}"
                );
                result.Add(document.case_id, document);
            }
            return result;
        }

        private static Dictionary<long, SurfaceResponseControlPointResult>
            IndexReference(SurfaceResponseResultDocument document)
        {
            var result =
                new Dictionary<long, SurfaceResponseControlPointResult>();
            foreach (
                SurfaceResponseControlPointResult point in document.control_points
            )
            {
                Require(
                    !result.ContainsKey(point.stable_id),
                    $"{document.case_id} duplicate stable_id {point.stable_id}"
                );
                result.Add(point.stable_id, point);
            }
            return result;
        }

        private static double PositionDeviation(
            Vector3 actual,
            double[] expected
        )
        {
            double dx = actual.x - expected[0];
            double dy = actual.y - expected[1];
            double dz = actual.z - expected[2];
            return Math.Sqrt(dx * dx + dy * dy + dz * dz);
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
    }
}
