using System;
using System.Collections.Generic;
using System.Globalization;
using UnityEngine;

namespace SansaCloth.Validation
{
    /// <summary>
    /// Minimal single-domain SurfaceReference / SurfaceQuery probe for BF-003/BF-004.
    /// This is validation code, not the production Runtime Backend.
    /// </summary>
    public sealed class SansaClothSurfaceQueryProbe : MonoBehaviour
    {
        private const ulong DomainId = 1;

        [ContextMenu("Run SansaCloth Surface Query Probe")]
        public void RunProbe()
        {
            Mesh mesh = BuildFixtureMesh();
            try
            {
                var uvs = new List<Vector2>();
                mesh.GetUVs(0, uvs);
                int[] triangles = mesh.triangles;
                Vector3[] vertices = mesh.vertices;

                Vector2 referenceUv = new Vector2(0.25f, 0.75f);
                QueryResult localQuery = QuerySingleDomain(
                    vertices,
                    uvs,
                    triangles,
                    referenceUv,
                    Matrix4x4.identity,
                    null
                );

                Vector3 currentPosition =
                    localQuery.surfacePosition + localQuery.surfaceNormal * 0.01f;
                localQuery = QuerySingleDomain(
                    vertices,
                    uvs,
                    triangles,
                    referenceUv,
                    Matrix4x4.identity,
                    currentPosition
                );

                LogScalar("BF-003.DOMAIN_ID", DomainId);
                LogVector2("BF-003.SURFACE_REFERENCE_UV", referenceUv);
                LogScalar("BF-003.RESOLVED_TRIANGLE", localQuery.triangleIndex);
                LogVector("BF-004.IDENTITY_SURFACE_POSITION", localQuery.surfacePosition);
                LogVector("BF-004.IDENTITY_SURFACE_NORMAL", localQuery.surfaceNormal);
                LogScalar("BF-004.IDENTITY_SEPARATION_M", localQuery.separation);

                Matrix4x4 transformed = Matrix4x4.TRS(
                    new Vector3(0.30f, 0.20f, -0.10f),
                    Quaternion.AngleAxis(-90.0f, Vector3.forward),
                    Vector3.one
                );
                QueryResult transformedSurface = QuerySingleDomain(
                    vertices,
                    uvs,
                    triangles,
                    referenceUv,
                    transformed,
                    null
                );
                Vector3 transformedCurrent =
                    transformedSurface.surfacePosition + transformedSurface.surfaceNormal * 0.01f;
                QueryResult transformedQuery = QuerySingleDomain(
                    vertices,
                    uvs,
                    triangles,
                    referenceUv,
                    transformed,
                    transformedCurrent
                );

                LogScalar("BF-003.TRANSFORMED_DOMAIN_ID", DomainId);
                LogVector2("BF-003.TRANSFORMED_SURFACE_REFERENCE_UV", referenceUv);
                LogScalar(
                    "BF-003.TRANSFORMED_RESOLVED_TRIANGLE",
                    transformedQuery.triangleIndex
                );
                LogVector(
                    "BF-004.TRANSFORMED_SURFACE_POSITION",
                    transformedQuery.surfacePosition
                );
                LogVector(
                    "BF-004.TRANSFORMED_SURFACE_NORMAL",
                    transformedQuery.surfaceNormal
                );
                LogScalar(
                    "BF-004.TRANSFORMED_SEPARATION_M",
                    transformedQuery.separation
                );
            }
            finally
            {
                DestroyImmediate(mesh);
            }
        }

        private static Mesh BuildFixtureMesh()
        {
            var mesh = new Mesh { name = "SansaCloth BF-003/BF-004 Fixture" };
            mesh.vertices = new[]
            {
                new Vector3(-0.10f, 0.0f, -0.05f), // V00
                new Vector3(-0.10f, 0.0f,  0.05f), // V01
                new Vector3( 0.10f, 0.0f, -0.05f), // V10
                new Vector3( 0.10f, 0.0f,  0.05f), // V11
            };
            mesh.uv = new[]
            {
                new Vector2(0.0f, 0.0f),
                new Vector2(0.0f, 1.0f),
                new Vector2(1.0f, 0.0f),
                new Vector2(1.0f, 1.0f),
            };
            mesh.triangles = new[]
            {
                0, 1, 3,
                0, 3, 2,
            };
            return mesh;
        }

        private static QueryResult QuerySingleDomain(
            IReadOnlyList<Vector3> vertices,
            IReadOnlyList<Vector2> uvs,
            IReadOnlyList<int> triangles,
            Vector2 uv,
            Matrix4x4 localToWorld,
            Vector3? currentPosition
        )
        {
            for (int triangle = 0; triangle < triangles.Count; triangle += 3)
            {
                int i0 = triangles[triangle];
                int i1 = triangles[triangle + 1];
                int i2 = triangles[triangle + 2];

                if (!TryBarycentric(
                    uv,
                    uvs[i0],
                    uvs[i1],
                    uvs[i2],
                    out Vector3 barycentric))
                {
                    continue;
                }

                Vector3 localSurface =
                    vertices[i0] * barycentric.x +
                    vertices[i1] * barycentric.y +
                    vertices[i2] * barycentric.z;
                Vector3 localNormal = Vector3.Cross(
                    vertices[i1] - vertices[i0],
                    vertices[i2] - vertices[i0]
                ).normalized;

                Vector3 worldSurface = localToWorld.MultiplyPoint3x4(localSurface);
                Vector3 worldNormal = localToWorld.MultiplyVector(localNormal).normalized;
                float separation = currentPosition.HasValue
                    ? Vector3.Dot(currentPosition.Value - worldSurface, worldNormal)
                    : 0.0f;

                return new QueryResult(
                    triangle / 3,
                    worldSurface,
                    worldNormal,
                    separation
                );
            }

            throw new InvalidOperationException(
                $"SurfaceReference ({uv.x}, {uv.y}) was not found in domain {DomainId}."
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

            if (Mathf.Abs(denominator) <= 1.0e-8f)
            {
                barycentric = default;
                return false;
            }

            float w1 = (v2.x * v1.y - v1.x * v2.y) / denominator;
            float w2 = (v0.x * v2.y - v2.x * v0.y) / denominator;
            float w0 = 1.0f - w1 - w2;
            const float epsilon = 1.0e-6f;

            barycentric = new Vector3(w0, w1, w2);
            return w0 >= -epsilon && w1 >= -epsilon && w2 >= -epsilon;
        }

        private static void LogScalar(string key, float value)
        {
            Debug.Log($"SANSA_BF|{key}|{value.ToString("R", CultureInfo.InvariantCulture)}");
        }

        private static void LogScalar(string key, ulong value)
        {
            Debug.Log($"SANSA_BF|{key}|{value.ToString(CultureInfo.InvariantCulture)}");
        }

        private static void LogScalar(string key, int value)
        {
            Debug.Log($"SANSA_BF|{key}|{value.ToString(CultureInfo.InvariantCulture)}");
        }

        private static void LogVector(string key, Vector3 value)
        {
            Debug.Log(
                $"SANSA_BF|{key}|{value.x.ToString("R", CultureInfo.InvariantCulture)}," +
                $"{value.y.ToString("R", CultureInfo.InvariantCulture)}," +
                $"{value.z.ToString("R", CultureInfo.InvariantCulture)}"
            );
        }

        private static void LogVector2(string key, Vector2 value)
        {
            Debug.Log(
                $"SANSA_BF|{key}|{value.x.ToString("R", CultureInfo.InvariantCulture)}," +
                $"{value.y.ToString("R", CultureInfo.InvariantCulture)}"
            );
        }

        private readonly struct QueryResult
        {
            public QueryResult(
                int triangleIndex,
                Vector3 surfacePosition,
                Vector3 surfaceNormal,
                float separation
            )
            {
                this.triangleIndex = triangleIndex;
                this.surfacePosition = surfacePosition;
                this.surfaceNormal = surfaceNormal;
                this.separation = separation;
            }

            public readonly int triangleIndex;
            public readonly Vector3 surfacePosition;
            public readonly Vector3 surfaceNormal;
            public readonly float separation;
        }
    }
}
