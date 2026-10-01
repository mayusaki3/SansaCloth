using System;
using System.Collections.Generic;
using UnityEngine;

namespace SansaCloth.Validation
{
    /// <summary>
    /// Validation-only SurfaceResponse path for imported SR-001 C=0/G=0.
    /// Stages mirror the Reference ordering:
    /// Support -> Bridge -> Gravity -> Query -> Conformity -> Query -> Collision -> Query.
    /// Non-zero gravity/conformity are rejected until their backend behavior is validated.
    /// </summary>
    internal static class SansaClothImportedSurfaceResponse
    {
        private const double InputZeroTolerance = 1.0e-15;

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
            if (Math.Abs(document.inputs.conformity) > InputZeroTolerance)
            {
                throw new FixtureExchangeException(
                    "FXE-014 only validates Conformity=0"
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

            // Gravity=0: Reference gravity stage is identity.
            // Conformity=0: Reference conformity stage is identity.
            // Query before collision, then apply collision tolerance.
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
                positions,
                support,
                finalQueries
            );
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
            Vector3[] positionsM,
            bool[] support,
            ResolvedSurfaceQueryResult[] finalQueries
        )
        {
            this.initialPositionsM = initialPositionsM;
            this.positionsM = positionsM;
            this.support = support;
            this.finalQueries = finalQueries;
        }

        internal readonly Vector3[] initialPositionsM;
        internal readonly Vector3[] positionsM;
        internal readonly bool[] support;
        internal readonly ResolvedSurfaceQueryResult[] finalQueries;
    }
}
