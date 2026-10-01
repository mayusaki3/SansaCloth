using System;
using System.Collections.Generic;
using UnityEngine;

namespace SansaCloth.Validation
{
    /// <summary>
    /// Validation-only SurfaceQuery over resolved Fixture Exchange mesh data.
    /// SurfaceReference is resolved from domain_id + UV; triangle identity is not persisted.
    /// </summary>
    internal sealed class SansaClothResolvedMeshSurfaceQuery
    {
        private const float BarycentricEpsilon = 1.0e-6f;
        private const float DegenerateEpsilon = 1.0e-8f;

        private readonly ulong domainId;
        private readonly Vector3[] vertices;
        private readonly Vector2[] uvs;
        private readonly int[][] triangles;

        internal SansaClothResolvedMeshSurfaceQuery(BodySurface surface)
        {
            if (surface == null)
            {
                throw new FixtureExchangeException("SurfaceQuery body surface is null");
            }

            domainId = surface.domainId;
            vertices = new Vector3[surface.vertices.Length];
            uvs = new Vector2[surface.vertices.Length];
            for (int i = 0; i < surface.vertices.Length; ++i)
            {
                vertices[i] = ToVector3(surface.vertices[i].positionM);
                uvs[i] = new Vector2(
                    (float)surface.vertices[i].uv[0],
                    (float)surface.vertices[i].uv[1]
                );
            }
            triangles = surface.triangles;
        }

        internal ResolvedSurfaceQueryResult Query(
            Vector3 currentPositionM,
            SurfaceReferenceData reference
        )
        {
            if (reference == null)
            {
                throw new FixtureExchangeException("SurfaceReference is null");
            }
            if (reference.domainId != domainId)
            {
                throw new FixtureExchangeException(
                    $"SurfaceReference domain {reference.domainId} does not match {domainId}"
                );
            }

            Vector2 uv = new Vector2((float)reference.u, (float)reference.v);
            for (int triangleIndex = 0; triangleIndex < triangles.Length; ++triangleIndex)
            {
                int[] triangle = triangles[triangleIndex];
                int i0 = triangle[0];
                int i1 = triangle[1];
                int i2 = triangle[2];

                if (
                    !TryBarycentric(
                        uv,
                        uvs[i0],
                        uvs[i1],
                        uvs[i2],
                        out Vector3 barycentric
                    )
                )
                {
                    continue;
                }

                Vector3 surfacePosition =
                    vertices[i0] * barycentric.x
                    + vertices[i1] * barycentric.y
                    + vertices[i2] * barycentric.z;
                Vector3 surfaceNormal = Vector3.Cross(
                    vertices[i1] - vertices[i0],
                    vertices[i2] - vertices[i0]
                ).normalized;
                if (surfaceNormal.sqrMagnitude <= 0.0f)
                {
                    throw new FixtureExchangeException(
                        $"Resolved triangle {triangleIndex} is degenerate"
                    );
                }

                float separationM = Vector3.Dot(
                    currentPositionM - surfacePosition,
                    surfaceNormal
                );
                return new ResolvedSurfaceQueryResult(
                    triangleIndex,
                    surfacePosition,
                    surfaceNormal,
                    separationM
                );
            }

            throw new FixtureExchangeException(
                $"SurfaceReference ({reference.u},{reference.v}) was not resolved in domain {domainId}"
            );
        }

        private static bool TryBarycentric(
            Vector2 p,
            Vector2 a,
            Vector2 b,
            Vector2 c,
            out Vector3 barycentric
        )
        {
            Vector2 v0 = b - a;
            Vector2 v1 = c - a;
            Vector2 v2 = p - a;
            float denominator = v0.x * v1.y - v1.x * v0.y;

            if (Mathf.Abs(denominator) <= DegenerateEpsilon)
            {
                barycentric = default;
                return false;
            }

            float w1 = (v2.x * v1.y - v1.x * v2.y) / denominator;
            float w2 = (v0.x * v2.y - v2.x * v0.y) / denominator;
            float w0 = 1.0f - w1 - w2;
            barycentric = new Vector3(w0, w1, w2);
            return w0 >= -BarycentricEpsilon
                && w1 >= -BarycentricEpsilon
                && w2 >= -BarycentricEpsilon;
        }

        internal static Vector3 ToVector3(double[] value)
        {
            return new Vector3((float)value[0], (float)value[1], (float)value[2]);
        }
    }

    internal readonly struct ResolvedSurfaceQueryResult
    {
        internal ResolvedSurfaceQueryResult(
            int triangleIndex,
            Vector3 surfacePositionM,
            Vector3 surfaceNormal,
            float separationM
        )
        {
            this.triangleIndex = triangleIndex;
            this.surfacePositionM = surfacePositionM;
            this.surfaceNormal = surfaceNormal;
            this.separationM = separationM;
        }

        internal readonly int triangleIndex;
        internal readonly Vector3 surfacePositionM;
        internal readonly Vector3 surfaceNormal;
        internal readonly float separationM;
    }
}
