using System;
using System.Collections.Generic;
using System.Globalization;
using UnityEngine;

namespace SansaCloth.Validation
{
    /// <summary>
    /// FXE-017C validation-only comparison of Unity Conformity Response
    /// against the Reference SurfaceResponse result baseline for all 30 cases.
    /// </summary>
    public sealed class SansaClothConformityResponseComparisonProbe : MonoBehaviour
    {
        private const int ExpectedCaseCount = 30;
        private const double PositionToleranceM = 1.0e-6;

        [SerializeField]
        private TextAsset[] fixtureExchangeJson;

        [SerializeField]
        private TextAsset[] surfaceResponseResultJson;

        [ContextMenu("Run SansaCloth FXE-017C Conformity Comparison")]
        public void RunProbe()
        {
            try
            {
                Dictionary<string, FixtureExchangeDocument> fixtures =
                    LoadFixtures();
                Dictionary<string, SurfaceResponseResultDocument> references =
                    LoadReferences();

                double overallGravityMaxM = 0.0;
                double overallConformityMaxM = 0.0;
                int checkedCases = 0;

                foreach (
                    KeyValuePair<string, FixtureExchangeDocument> entry in fixtures
                )
                {
                    string caseId = entry.Key;
                    FixtureExchangeDocument fixture = entry.Value;
                    Require(
                        references.TryGetValue(
                            caseId,
                            out SurfaceResponseResultDocument reference
                        ),
                        $"missing SurfaceResponse result case {caseId}"
                    );

                    string expectedLayout = caseId.StartsWith(
                        "SR-003-",
                        StringComparison.Ordinal
                    )
                        ? "OneEdge"
                        : "BothEdges";
                    Require(
                        reference.profile.support_layout == expectedLayout,
                        $"{caseId} support layout mismatch"
                    );

                    var surfaceQuery =
                        new SansaClothResolvedMeshSurfaceQuery(
                            fixture.bodySurface
                        );
                    ImportedSurfaceResponseResult actual =
                        SansaClothImportedSurfaceResponse.SolveValidationProfile(
                            fixture,
                            surfaceQuery
                        );
                    Dictionary<long, SurfaceResponseControlPointResult>
                        referenceByStableId = IndexReference(reference);

                    double gravityMaxM = 0.0;
                    double conformityMaxM = 0.0;
                    for (
                        int i = 0;
                        i < fixture.cloth.controlPoints.Length;
                        ++i
                    )
                    {
                        ControlPoint point = fixture.cloth.controlPoints[i];
                        Require(
                            point.stableId <= (ulong)long.MaxValue,
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

                        gravityMaxM = Math.Max(
                            gravityMaxM,
                            PositionDeviation(
                                actual.gravityPositionsM[i],
                                expected.gravity_position_m
                            )
                        );
                        conformityMaxM = Math.Max(
                            conformityMaxM,
                            PositionDeviation(
                                actual.conformityPositionsM[i],
                                expected.conformity_position_m
                            )
                        );
                    }

                    Require(
                        gravityMaxM <= PositionToleranceM,
                        $"{caseId} Gravity deviation {gravityMaxM:R} m exceeds tolerance"
                    );
                    Require(
                        conformityMaxM <= PositionToleranceM,
                        $"{caseId} Conformity deviation {conformityMaxM:R} m exceeds tolerance"
                    );

                    overallGravityMaxM = Math.Max(
                        overallGravityMaxM,
                        gravityMaxM
                    );
                    overallConformityMaxM = Math.Max(
                        overallConformityMaxM,
                        conformityMaxM
                    );
                    checkedCases++;

                    LogDouble(
                        $"FXE-017C.CASE.{caseId}.GRAVITY_MAX_POSITION_DEVIATION_M",
                        gravityMaxM
                    );
                    LogDouble(
                        $"FXE-017C.CASE.{caseId}.CONFORMITY_MAX_POSITION_DEVIATION_M",
                        conformityMaxM
                    );
                    Log($"FXE-017C.CASE.{caseId}", "PASS");
                }

                Require(
                    checkedCases == ExpectedCaseCount,
                    "FXE-017C checked case count mismatch"
                );
                Require(
                    references.Count == ExpectedCaseCount,
                    "FXE-017C Reference result count mismatch"
                );

                LogInt("FXE-017C.CASE_COUNT", checkedCases);
                LogDouble(
                    "FXE-017C.GRAVITY_MAX_POSITION_DEVIATION_M",
                    overallGravityMaxM
                );
                LogDouble(
                    "FXE-017C.CONFORMITY_MAX_POSITION_DEVIATION_M",
                    overallConformityMaxM
                );
                LogDouble(
                    "FXE-017C.POSITION_TOLERANCE_M",
                    PositionToleranceM
                );
                Log("FXE-017C.RESULT", "PASS");
            }
            catch (Exception exception)
            {
                Debug.LogError(
                    $"SANSA_FXE|FXE-017C.RESULT|FAIL|{exception.Message}"
                );
                throw;
            }
        }

        private Dictionary<string, FixtureExchangeDocument> LoadFixtures()
        {
            Require(
                fixtureExchangeJson != null
                    && fixtureExchangeJson.Length == ExpectedCaseCount,
                $"Assign exactly {ExpectedCaseCount} Fixture Exchange JSON TextAssets."
            );

            var result = new SortedDictionary<string, FixtureExchangeDocument>(
                StringComparer.Ordinal
            );
            foreach (TextAsset asset in fixtureExchangeJson)
            {
                Require(asset != null, "Fixture Exchange array has an empty slot");
                FixtureExchangeDocument document =
                    SansaClothFixtureExchangeJson.Parse(asset.text);
                Require(
                    !result.ContainsKey(document.caseId),
                    $"duplicate Fixture Exchange case_id {document.caseId}"
                );
                result.Add(document.caseId, document);
            }
            return new Dictionary<string, FixtureExchangeDocument>(result);
        }

        private Dictionary<string, SurfaceResponseResultDocument> LoadReferences()
        {
            Require(
                surfaceResponseResultJson != null
                    && surfaceResponseResultJson.Length == ExpectedCaseCount,
                $"Assign exactly {ExpectedCaseCount} SurfaceResponse result JSON TextAssets."
            );

            var result =
                new Dictionary<string, SurfaceResponseResultDocument>();
            foreach (TextAsset asset in surfaceResponseResultJson)
            {
                Require(asset != null, "SurfaceResponse result array has an empty slot");
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
