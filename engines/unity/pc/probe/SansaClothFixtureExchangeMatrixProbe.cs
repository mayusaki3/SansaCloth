using System;
using System.Collections.Generic;
using System.Globalization;
using UnityEngine;

namespace SansaCloth.Validation
{
    /// <summary>
    /// FXE-016 validation-only importer for the complete Basic v1 30-case matrix.
    /// Assign exactly the 30 Reference-generated JSON TextAssets.
    /// </summary>
    public sealed class SansaClothFixtureExchangeMatrixProbe : MonoBehaviour
    {
        private const int ExpectedCaseCount = 30;
        private const int ExpectedScenarioCaseCount = 6;
        private const int ExpectedVertexCount = 147;
        private const int ExpectedTriangleCount = 240;
        private const int ExpectedControlPointCount = 147;
        private const double NumericTolerance = 1.0e-12;
        private const double Sr003GeometryTolerance = 1.0e-9;

        [SerializeField]
        private TextAsset[] fixtureExchangeMatrixJson;

        [ContextMenu("Run SansaCloth Fixture Exchange Matrix Probe")]
        public void RunProbe()
        {
            try
            {
                Require(
                    fixtureExchangeMatrixJson != null
                        && fixtureExchangeMatrixJson.Length == ExpectedCaseCount,
                    $"Assign exactly {ExpectedCaseCount} Basic v1 JSON TextAssets."
                );

                var documents = new Dictionary<string, FixtureExchangeDocument>();
                foreach (TextAsset asset in fixtureExchangeMatrixJson)
                {
                    Require(asset != null, "Matrix contains an unassigned TextAsset.");
                    FixtureExchangeDocument document =
                        SansaClothFixtureExchangeJson.Parse(asset.text);
                    Require(
                        documents.AddIfAbsent(document.caseId, document),
                        $"duplicate case_id {document.caseId}"
                    );
                }

                Require(
                    documents.Count == ExpectedCaseCount,
                    "FXE-016 unique case count mismatch"
                );

                string[] scenarios =
                {
                    "SR-001",
                    "SR-002",
                    "SR-003",
                    "SR-004",
                    "SR-005",
                };
                string[] conformities = { "C0", "C05", "C1" };
                string[] gravities = { "G0", "G1" };

                foreach (string scenario in scenarios)
                {
                    FixtureExchangeDocument baseline = null;
                    int scenarioCount = 0;

                    foreach (string gravity in gravities)
                    {
                        foreach (string conformity in conformities)
                        {
                            string caseId =
                                $"{scenario}-{conformity}-{gravity}";
                            Require(
                                documents.TryGetValue(
                                    caseId,
                                    out FixtureExchangeDocument document
                                ),
                                $"missing matrix case {caseId}"
                            );
                            ValidateCommonCounts(document);
                            ValidateInputs(
                                document,
                                conformity,
                                gravity
                            );

                            if (baseline == null)
                            {
                                baseline = document;
                            }
                            else
                            {
                                RequireScenarioSemanticInvariant(
                                    baseline,
                                    document
                                );
                            }

                            scenarioCount++;
                            Log($"FXE-016.CASE.{caseId}", "PASS");
                        }
                    }

                    Require(
                        scenarioCount == ExpectedScenarioCaseCount,
                        $"{scenario} case count mismatch"
                    );
                    ValidateScenario(scenario, baseline);
                    LogInt(
                        $"FXE-016.SCENARIO.{scenario}.CASE_COUNT",
                        scenarioCount
                    );
                    Log(
                        $"FXE-016.SCENARIO.{scenario}.RESULT",
                        "PASS"
                    );
                }

                LogInt("FXE-016.CASE_COUNT", documents.Count);
                LogInt("FXE-016.SCENARIO_COUNT", scenarios.Length);
                Log("FXE-016.SR003_BAKED_CONVEX_SIDE", "PASS");
                Log("FXE-016.RESULT", "PASS");
            }
            catch (Exception exception)
            {
                Debug.LogError($"SANSA_FXE|FXE-016.RESULT|FAIL|{exception.Message}");
                throw;
            }
        }

        private static void ValidateCommonCounts(
            FixtureExchangeDocument document
        )
        {
            Require(
                document.bodySurface.vertices.Length == ExpectedVertexCount,
                $"{document.caseId} body vertex count mismatch"
            );
            Require(
                document.bodySurface.triangles.Length == ExpectedTriangleCount,
                $"{document.caseId} triangle count mismatch"
            );
            Require(
                document.cloth.controlPoints.Length
                    == ExpectedControlPointCount,
                $"{document.caseId} control point count mismatch"
            );
        }

        private static void ValidateInputs(
            FixtureExchangeDocument document,
            string conformityId,
            string gravityId
        )
        {
            double expectedConformity;
            switch (conformityId)
            {
                case "C0":
                    expectedConformity = 0.0;
                    break;
                case "C05":
                    expectedConformity = 0.5;
                    break;
                case "C1":
                    expectedConformity = 1.0;
                    break;
                default:
                    throw new FixtureExchangeException(
                        $"unknown conformity id {conformityId}"
                    );
            }

            double[] expectedGravity =
                gravityId == "G0"
                    ? new[] { 0.0, 0.0, 0.0 }
                    : new[] { 0.0, -9.80665, 0.0 };

            RequireClose(
                document.inputs.conformity,
                expectedConformity,
                $"{document.caseId} conformity mismatch"
            );
            RequireArrayClose(
                document.inputs.worldGravityMPerS2,
                expectedGravity,
                $"{document.caseId} gravity mismatch"
            );
            RequireClose(
                document.inputs.collisionToleranceM,
                0.0,
                $"{document.caseId} collision tolerance mismatch"
            );
        }

        private static void ValidateScenario(
            string scenario,
            FixtureExchangeDocument document
        )
        {
            int anchorCount = 0;
            foreach (ControlPoint point in document.cloth.controlPoints)
            {
                if (point.anchor)
                {
                    anchorCount++;
                }
            }

            int expectedAnchorCount = scenario == "SR-003" ? 7 : 14;
            Require(
                anchorCount == expectedAnchorCount,
                $"{scenario} anchor count mismatch"
            );

            if (scenario != "SR-003")
            {
                return;
            }

            BodyVertex center = document.bodySurface.vertices[73];
            RequireArrayClose(
                center.positionM,
                new[] { 0.03, 0.0, 0.0 },
                "SR-003 baked center position mismatch",
                Sr003GeometryTolerance
            );
            RequireArrayClose(
                center.normal,
                new[] { 1.0, 0.0, 0.0 },
                "SR-003 baked center normal mismatch",
                Sr003GeometryTolerance
            );
        }

        private static void RequireScenarioSemanticInvariant(
            FixtureExchangeDocument expected,
            FixtureExchangeDocument actual
        )
        {
            Require(
                expected.coordinate.lengthUnit == actual.coordinate.lengthUnit
                    && expected.coordinate.gravityUnit
                        == actual.coordinate.gravityUnit
                    && expected.coordinate.x == actual.coordinate.x
                    && expected.coordinate.y == actual.coordinate.y
                    && expected.coordinate.z == actual.coordinate.z,
                $"{actual.caseId} coordinate changed within scenario"
            );

            BodySurface expectedBody = expected.bodySurface;
            BodySurface actualBody = actual.bodySurface;
            Require(
                expectedBody.domainId == actualBody.domainId,
                $"{actual.caseId} body domain changed within scenario"
            );
            Require(
                expectedBody.vertices.Length == actualBody.vertices.Length
                    && expectedBody.triangles.Length
                        == actualBody.triangles.Length,
                $"{actual.caseId} body size changed within scenario"
            );

            for (int i = 0; i < expectedBody.vertices.Length; ++i)
            {
                RequireArrayClose(
                    expectedBody.vertices[i].positionM,
                    actualBody.vertices[i].positionM,
                    $"{actual.caseId} body position {i} changed"
                );
                RequireArrayClose(
                    expectedBody.vertices[i].normal,
                    actualBody.vertices[i].normal,
                    $"{actual.caseId} body normal {i} changed"
                );
                RequireArrayClose(
                    expectedBody.vertices[i].uv,
                    actualBody.vertices[i].uv,
                    $"{actual.caseId} body UV {i} changed"
                );
            }

            for (int i = 0; i < expectedBody.triangles.Length; ++i)
            {
                int[] left = expectedBody.triangles[i];
                int[] right = actualBody.triangles[i];
                Require(
                    left.Length == right.Length
                        && left[0] == right[0]
                        && left[1] == right[1]
                        && left[2] == right[2],
                    $"{actual.caseId} triangle {i} changed"
                );
            }

            ControlPoint[] expectedPoints = expected.cloth.controlPoints;
            ControlPoint[] actualPoints = actual.cloth.controlPoints;
            Require(
                expectedPoints.Length == actualPoints.Length,
                $"{actual.caseId} cloth size changed within scenario"
            );

            for (int i = 0; i < expectedPoints.Length; ++i)
            {
                ControlPoint left = expectedPoints[i];
                ControlPoint right = actualPoints[i];
                Require(
                    left.stableId == right.stableId
                        && left.stripId == right.stripId
                        && left.stripOrder == right.stripOrder
                        && left.anchor == right.anchor
                        && left.contact == right.contact,
                    $"{actual.caseId} cloth semantics {i} changed"
                );
                RequireArrayClose(
                    left.positionM,
                    right.positionM,
                    $"{actual.caseId} cloth position {i} changed"
                );
                Require(
                    left.surfaceReference.domainId
                        == right.surfaceReference.domainId,
                    $"{actual.caseId} SurfaceReference domain {i} changed"
                );
                RequireClose(
                    left.surfaceReference.u,
                    right.surfaceReference.u,
                    $"{actual.caseId} SurfaceReference U {i} changed"
                );
                RequireClose(
                    left.surfaceReference.v,
                    right.surfaceReference.v,
                    $"{actual.caseId} SurfaceReference V {i} changed"
                );
            }

            RequireClose(
                expected.inputs.collisionToleranceM,
                actual.inputs.collisionToleranceM,
                $"{actual.caseId} collision tolerance changed within scenario"
            );
        }

        private static void RequireArrayClose(
            double[] actual,
            double[] expected,
            string message,
            double tolerance = NumericTolerance
        )
        {
            Require(
                actual != null
                    && expected != null
                    && actual.Length == expected.Length,
                message
            );
            for (int i = 0; i < actual.Length; ++i)
            {
                Require(
                    Math.Abs(actual[i] - expected[i]) <= tolerance,
                    $"{message} at component {i}"
                );
            }
        }

        private static void RequireClose(
            double actual,
            double expected,
            string message
        )
        {
            Require(
                Math.Abs(actual - expected) <= NumericTolerance,
                message
            );
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
    }

    internal static class SansaClothDictionaryExtensions
    {
        internal static bool AddIfAbsent<TKey, TValue>(
            this Dictionary<TKey, TValue> dictionary,
            TKey key,
            TValue value
        )
        {
            if (dictionary.ContainsKey(key))
            {
                return false;
            }
            dictionary.Add(key, value);
            return true;
        }
    }
}
