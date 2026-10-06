using System;
using System.Collections.Generic;
using UnityEngine;

namespace SansaCloth.Validation
{
    /// <summary>
    /// Validation-only SurfaceResponse path for imported Fixture Exchange cases.
    /// Stages mirror the Reference ordering:
    /// Support -> Bridge -> Gravity -> Query -> Conformity -> Query -> Collision -> Query.
    /// FXE-017B validates Gravity Response for C=0 cases.
    /// FXE-017C adds Reference-v1 Conformity Response for the full 30-case matrix.
    /// </summary>
    internal static class SansaClothImportedSurfaceResponse
    {
        private const double InputZeroTolerance = 1.0e-15;
        private const float ReferenceCharacteristicLengthM = 0.10f;
        private const float ReferenceQuasiStaticGravityScale = 0.1f;
        private const float ReferenceConformityReachM = 0.02f;

        internal static ImportedSurfaceResponseResult SolveSr001C0G0(
            FixtureExchangeDocument document,
            SansaClothResolvedMeshSurfaceQuery surfaceQuery
        )
        {
            if (document == null || surfaceQuery == null)
            {
                throw new FixtureExchangeException(
                    "Imported SurfaceResponse requires document and SurfaceQuery"
                );
            }

            RequireZeroInput(
                document.inputs.worldGravityMPerS2,
                "FXE-014 only validates Gravity=0"
            );
            return SolveC0(document, surfaceQuery);
        }

        internal static ImportedSurfaceResponseResult SolveC0(
            FixtureExchangeDocument document,
            SansaClothResolvedMeshSurfaceQuery surfaceQuery
        )
        {
            if (document == null || surfaceQuery == null)
            {
                throw new FixtureExchangeException(
                    "Imported SurfaceResponse requires document and SurfaceQuery"
                );
            }
            if (Math.Abs(document.inputs.conformity) > InputZeroTolerance)
            {
                throw new FixtureExchangeException(
                    "FXE-017B only validates Conformity=0"
                );
            }

            return SolveValidationProfile(document, surfaceQuery);
        }

        internal static ImportedSurfaceResponseResult SolveValidationProfile(
            FixtureExchangeDocument document,
            SansaClothResolvedMeshSurfaceQuery surfaceQuery
        )
        {
            if (document == null || surfaceQuery == null)
            {
                throw new FixtureExchangeException(
                    "Imported SurfaceResponse requires document and SurfaceQuery"
                );
            }

            ControlPoint[] points = document.cloth.controlPoints;
            int count = points.Length;
            var initial = new Vector3[count];
            var positions = new Vector3[count];
            var support = new bool[count];

            var indexByStableId = new Dictionary<ulong, int>();
            var strips = new SortedDictionary<ulong, SortedDictionary<ulong, int>>();

            for (int i = 0; i < count; ++i)
            {
                ControlPoint point = points[i];
                initial[i] = SansaClothResolvedMeshSurfaceQuery.ToVector3(
                    point.positionM
                );
                positions[i] = initial[i];
                support[i] = point.anchor;

                if (indexByStableId.ContainsKey(point.stableId))
                {
                    throw new FixtureExchangeException(
                        $"duplicate stable_id {point.stableId}"
                    );
                }
                indexByStableId.Add(point.stableId, i);

                if (
                    !strips.TryGetValue(
                        point.stripId,
                        out SortedDictionary<ulong, int> strip
                    )
                )
                {
                    strip = new SortedDictionary<ulong, int>();
                    strips.Add(point.stripId, strip);
                }
                if (strip.ContainsKey(point.stripOrder))
                {
                    throw new FixtureExchangeException(
                        $"duplicate strip/order {point.stripId}/{point.stripOrder}"
                    );
                }
                strip.Add(point.stripOrder, i);
            }

            // Bridge: interpolate only when both strip endpoints have direct support.
            foreach (
                KeyValuePair<ulong, SortedDictionary<ulong, int>> stripEntry in strips
            )
            {
                var orderedIndices = new List<int>(stripEntry.Value.Values);
                if (orderedIndices.Count < 2)
                {
                    continue;
                }

                int firstIndex = orderedIndices[0];
                int lastIndex = orderedIndices[orderedIndices.Count - 1];
                if (!support[firstIndex] || !support[lastIndex])
                {
                    continue;
                }

                Vector3 first = initial[firstIndex];
                Vector3 last = initial[lastIndex];
                float denominator = orderedIndices.Count - 1;
                for (int order = 1; order < orderedIndices.Count - 1; ++order)
                {
                    positions[orderedIndices[order]] = Vector3.LerpUnclamped(
                        first,
                        last,
                        order / denominator
                    );
                }
            }

            Vector3[] bridgePositions = (Vector3[])positions.Clone();

            ApplyReferenceGravity(
                document,
                strips,
                support,
                positions
            );
            Vector3[] gravityPositions = (Vector3[])positions.Clone();

            var afterGravityQueries = new ResolvedSurfaceQueryResult[count];
            for (int i = 0; i < count; ++i)
            {
                afterGravityQueries[i] = surfaceQuery.Query(
                    positions[i],
                    points[i].surfaceReference
                );
            }

            ApplyReferenceConformity(
                document,
                support,
                positions,
                afterGravityQueries
            );
            Vector3[] conformityPositions = (Vector3[])positions.Clone();

            var beforeCollision = new ResolvedSurfaceQueryResult[count];
            for (int i = 0; i < count; ++i)
            {
                beforeCollision[i] = surfaceQuery.Query(
                    positions[i],
                    points[i].surfaceReference
                );
            }

            float collisionToleranceM =
                (float)document.inputs.collisionToleranceM;
            for (int i = 0; i < count; ++i)
            {
                ResolvedSurfaceQueryResult query = beforeCollision[i];
                if (query.separationM >= collisionToleranceM)
                {
                    continue;
                }

                float correctionM =
                    collisionToleranceM - query.separationM;
                positions[i] += query.surfaceNormal * correctionM;
            }

            var finalQueries = new ResolvedSurfaceQueryResult[count];
            for (int i = 0; i < count; ++i)
            {
                finalQueries[i] = surfaceQuery.Query(
                    positions[i],
                    points[i].surfaceReference
                );
            }

            return new ImportedSurfaceResponseResult(
                initial,
                bridgePositions,
                gravityPositions,
                conformityPositions,
                positions,
                support,
                finalQueries
            );
        }

        private static void ApplyReferenceGravity(
            FixtureExchangeDocument document,
            SortedDictionary<ulong, SortedDictionary<ulong, int>> strips,
            bool[] support,
            Vector3[] positions
        )
        {
            Vector3 gravity = SansaClothResolvedMeshSurfaceQuery.ToVector3(
                document.inputs.worldGravityMPerS2
            );
            if (gravity.sqrMagnitude <= 0.0f)
            {
                return;
            }

            Vector3 direction = gravity.normalized;
            bool oneEdge = document.caseId.StartsWith(
                "SR-003-",
                StringComparison.Ordinal
            );

            foreach (
                KeyValuePair<ulong, SortedDictionary<ulong, int>> stripEntry in strips
            )
            {
                var orderedIndices = new List<int>(stripEntry.Value.Values);
                int lastOrder = orderedIndices.Count - 1;
                if (lastOrder <= 0)
                {
                    continue;
                }

                for (int order = 0; order <= lastOrder; ++order)
                {
                    int index = orderedIndices[order];
                    if (support[index])
                    {
                        continue;
                    }

                    float t = (float)order / lastOrder;
                    float supportWeight = oneEdge
                        ? t
                        : 4.0f * t * (1.0f - t);
                    float displacementM =
                        ReferenceCharacteristicLengthM
                        * ReferenceQuasiStaticGravityScale
                        * supportWeight;
                    positions[index] += direction * displacementM;
                }
            }
        }

        private static void ApplyReferenceConformity(
            FixtureExchangeDocument document,
            bool[] support,
            Vector3[] positions,
            ResolvedSurfaceQueryResult[] queries
        )
        {
            float conformity = (float)document.inputs.conformity;
            if (conformity <= 0.0f || ReferenceConformityReachM <= 0.0f)
            {
                return;
            }

            for (int i = 0; i < positions.Length; ++i)
            {
                if (support[i])
                {
                    continue;
                }

                float separationM = queries[i].separationM;
                if (
                    float.IsNaN(separationM)
                    || float.IsInfinity(separationM)
                    || separationM < 0.0f
                )
                {
                    continue;
                }

                float distanceWeight = Mathf.Clamp01(
                    1.0f - separationM / ReferenceConformityReachM
                );
                float effective = conformity * distanceWeight;
                positions[i] = Vector3.LerpUnclamped(
                    positions[i],
                    queries[i].surfacePositionM,
                    effective
                );
            }
        }

        private static void RequireZeroInput(double[] value, string message)
        {
            if (
                value == null
                || value.Length != 3
                || Math.Abs(value[0]) > InputZeroTolerance
                || Math.Abs(value[1]) > InputZeroTolerance
                || Math.Abs(value[2]) > InputZeroTolerance
            )
            {
                throw new FixtureExchangeException(message);
            }
        }
    }

    internal sealed class ImportedSurfaceResponseResult
    {
        internal ImportedSurfaceResponseResult(
            Vector3[] initialPositionsM,
            Vector3[] bridgePositionsM,
            Vector3[] gravityPositionsM,
            Vector3[] conformityPositionsM,
            Vector3[] positionsM,
            bool[] support,
            ResolvedSurfaceQueryResult[] finalQueries
        )
        {
            this.initialPositionsM = initialPositionsM;
            this.bridgePositionsM = bridgePositionsM;
            this.gravityPositionsM = gravityPositionsM;
            this.conformityPositionsM = conformityPositionsM;
            this.positionsM = positionsM;
            this.support = support;
            this.finalQueries = finalQueries;
        }

        internal readonly Vector3[] initialPositionsM;
        internal readonly Vector3[] bridgePositionsM;
        internal readonly Vector3[] gravityPositionsM;
        internal readonly Vector3[] conformityPositionsM;
        internal readonly Vector3[] positionsM;
        internal readonly bool[] support;
        internal readonly ResolvedSurfaceQueryResult[] finalQueries;
    }
}
