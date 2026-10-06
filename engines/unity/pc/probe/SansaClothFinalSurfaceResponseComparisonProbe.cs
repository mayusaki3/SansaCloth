using System;
using System.Collections.Generic;
using System.Globalization;
using UnityEngine;

namespace SansaCloth.Validation
{
    /// <summary>
    /// FXE-017D validation-only comparison of Collision and final SurfaceQuery
    /// against the Reference SurfaceResponse result baseline for all 30 cases.
    /// </summary>
    public sealed class SansaClothFinalSurfaceResponseComparisonProbe : MonoBehaviour
    {
        private const int ExpectedCaseCount = 30;
        private const double PositionToleranceM = 1.0e-6;
        private const double SeparationToleranceM = 1.0e-6;
        private const double NormalTolerance = 1.0e-5;

        [SerializeField]
        private TextAsset[] fixtureExchangeJson;

        [SerializeField]
        private TextAsset[] surfaceResponseResultJson;

        [ContextMenu("Run SansaCloth FXE-017D Final SurfaceResponse Comparison")]
        public void RunProbe()
        {
            try
            {
                Dictionary<string, FixtureExchangeDocument> fixtures =
                    LoadFixtures();
                Dictionary<string, SurfaceResponseResultDocument> references =
                    LoadReferences();

                double overallCollisionMaxM = 0.0;
                double overallFinalMaxM = 0.0;
                double overallSurfaceMaxM = 0.0;
                double overallNormalMax = 0.0;
                double overallSeparationMaxM = 0.0;
                int checkedContacts = 0;
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

                    double collisionMaxM = 0.0;
                    double finalMaxM = 0.0;
                    double surfaceMaxM = 0.0;
                    double normalMax = 0.0;
                    double separationMaxM = 0.0;

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

                        collisionMaxM = Math.Max(
                            collisionMaxM,
                            PositionDeviation(
                                actual.positionsM[i],
                                expected.collision_position_m
                            )
                        );
                        finalMaxM = Math.Max(
                            finalMaxM,
                            PositionDeviation(
                                actual.positionsM[i],
                                expected.final_position_m
                            )
                        );

                        ResolvedSurfaceQueryResult query = actual.finalQueries[i];
                        surfaceMaxM = Math.Max(
                            surfaceMaxM,
                            PositionDeviation(
                                query.surfacePositionM,
                                expected.surface_position_m
                            )
                        );
                        normalMax = Math.Max(
                            normalMax,
                            PositionDeviation(
                                query.surfaceNormal,
                                expected.surface_normal
                            )
                        );
                        separationMaxM = Math.Max(
                            separationMaxM,
                            Math.Abs(query.separationM - expected.separation_m)
                        );

                        double collisionToleranceM =
                            fixture.inputs.collisionToleranceM;
                        bool actualContact =
                            query.separationM
                            <= collisionToleranceM + SeparationToleranceM;
                        Require(
                            actualContact == expected.contact,
                            $"{caseId} contact mismatch at stable_id {stableId}: "
                                + $"Unity={actualContact}, Reference={expected.contact}, "
                                + $"separation={query.separationM:R}"
                        );
                        checkedContacts++;
                    }

                    Require(
                        collisionMaxM <= PositionToleranceM,
                        $"{caseId} Collision deviation {collisionMaxM:R} m exceeds tolerance"
                    );
                    Require(
                        finalMaxM <= PositionToleranceM,
                        $"{caseId} Final deviation {finalMaxM:R} m exceeds tolerance"
                    );
                    Require(
                        surfaceMaxM <= PositionToleranceM,
                        $"{caseId} Surface position deviation {surfaceMaxM:R} m exceeds tolerance"
                    );
                    Require(
                        normalMax <= NormalTolerance,
                        $"{caseId} Surface normal deviation {normalMax:R} exceeds tolerance"
                    );
                    Require(
                        separationMaxM <= SeparationToleranceM,
                        $"{caseId} Separation deviation {separationMaxM:R} m exceeds tolerance"
                    );

                    overallCollisionMaxM = Math.Max(
                        overallCollisionMaxM,
                        collisionMaxM
                    );
                    overallFinalMaxM = Math.Max(overallFinalMaxM, finalMaxM);
                    overallSurfaceMaxM = Math.Max(
                        overallSurfaceMaxM,
                        surfaceMaxM
                    );
                    overallNormalMax = Math.Max(overallNormalMax, normalMax);
                    overallSeparationMaxM = Math.Max(
                        overallSeparationMaxM,
                        separationMaxM
                    );
                    checkedCases++;

                    LogDouble(
                        $"FXE-017D.CASE.{caseId}.COLLISION_MAX_POSITION_DEVIATION_M",
                        collisionMaxM
                    );
                    LogDouble(
                        $"FXE-017D.CASE.{caseId}.FINAL_MAX_POSITION_DEVIATION_M",
                        finalMaxM
                    );
                    LogDouble(
                        $"FXE-017D.CASE.{caseId}.SURFACE_MAX_POSITION_DEVIATION_M",
                        surfaceMaxM
                    );
                    LogDouble(
                        $"FXE-017D.CASE.{caseId}.NORMAL_MAX_DEVIATION",
                        normalMax
                    );
                    LogDouble(
                        $"FXE-017D.CASE.{caseId}.SEPARATION_MAX_DEVIATION_M",
                        separationMaxM
                    );
                    Log($"FXE-017D.CASE.{caseId}", "PASS");
                }

                Require(
                    checkedCases == ExpectedCaseCount,
                    "FXE-017D checked case count mismatch"
                );
                Require(
                    checkedContacts == ExpectedCaseCount * 147,
                    "FXE-017D checked contact count mismatch"
                );

                LogInt("FXE-017D.CASE_COUNT", checkedCases);
                LogInt("FXE-017D.CONTACT_COUNT_CHECKED", checkedContacts);
                LogDouble(
                    "FXE-017D.COLLISION_MAX_POSITION_DEVIATION_M",
                    overallCollisionMaxM
                );
                LogDouble(
                    "FXE-017D.FINAL_MAX_POSITION_DEVIATION_M",
                    overallFinalMaxM
                );
                LogDouble(
                    "FXE-017D.SURFACE_MAX_POSITION_DEVIATION_M",
                    overallSurfaceMaxM
                );
                LogDouble(
                    "FXE-017D.NORMAL_MAX_DEVIATION",
                    overallNormalMax
                );
                LogDouble(
                    "FXE-017D.SEPARATION_MAX_DEVIATION_M",
                    overallSeparationMaxM
                );
                LogDouble(
                    "FXE-017D.POSITION_TOLERANCE_M",
                    PositionToleranceM
                );
                LogDouble(
                    "FXE-017D.NORMAL_TOLERANCE",
                    NormalTolerance
                );
                Log("FXE-017D.RESULT", "PASS");
            }
            catch (Exception exception)
            {
                Debug.LogError(
                    $"SANSA_FXE|FXE-017D.RESULT|FAIL|{exception.Message}"
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
