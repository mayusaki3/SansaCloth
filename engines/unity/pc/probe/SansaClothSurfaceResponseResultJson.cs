using System;
using UnityEngine;

namespace SansaCloth.Validation
{
    /// <summary>
    /// Validation-only reader for Reference SurfaceResponse comparison results.
    /// This format is a validation artifact, not a Product serialization contract.
    /// </summary>
    internal static class SansaClothSurfaceResponseResultJson
    {
        internal const string Format =
            "sansacloth.validation.surface-response-result/0";

        internal static SurfaceResponseResultDocument Parse(string json)
        {
            if (string.IsNullOrEmpty(json))
            {
                throw new FixtureExchangeException(
                    "SurfaceResponse result JSON is empty"
                );
            }

            SurfaceResponseResultDocument document =
                JsonUtility.FromJson<SurfaceResponseResultDocument>(json);
            Validate(document);
            return document;
        }

        private static void Validate(SurfaceResponseResultDocument document)
        {
            Require(document != null, "SurfaceResponse result is null");
            Require(document.format == Format, "unsupported SurfaceResponse result format");
            Require(!string.IsNullOrEmpty(document.case_id), "result case_id is empty");
            Require(document.profile != null, "result profile is missing");
            RequireClose(
                document.profile.characteristic_length_m,
                0.10,
                "unexpected characteristic_length_m"
            );
            RequireClose(
                document.profile.quasi_static_gravity_scale,
                0.1,
                "unexpected quasi_static_gravity_scale"
            );
            RequireClose(
                document.profile.conformity_reach_m,
                0.02,
                "unexpected conformity_reach_m"
            );
            Require(
                document.profile.support_layout == "BothEdges"
                    || document.profile.support_layout == "OneEdge",
                "unexpected support_layout"
            );
            Require(
                document.control_points != null
                    && document.control_points.Length == 147,
                "result control point count must be 147"
            );

            var stableIds = new System.Collections.Generic.HashSet<long>();
            foreach (SurfaceResponseControlPointResult point in document.control_points)
            {
                Require(point != null, "result control point is null");
                Require(point.stable_id >= 0, "result stable_id must be non-negative");
                Require(
                    stableIds.Add(point.stable_id),
                    $"duplicate result stable_id {point.stable_id}"
                );
                Require(
                    point.support == "Anchor" || point.support == "Unsupported",
                    $"invalid support for stable_id {point.stable_id}"
                );
                RequireVector3(
                    point.bridge_position_m,
                    $"bridge_position_m for stable_id {point.stable_id}"
                );
                RequireVector3(
                    point.gravity_position_m,
                    $"gravity_position_m for stable_id {point.stable_id}"
                );
                RequireVector3(
                    point.conformity_position_m,
                    $"conformity_position_m for stable_id {point.stable_id}"
                );
            }
        }

        private static void RequireVector3(double[] value, string message)
        {
            Require(value != null && value.Length == 3, message);
            for (int i = 0; i < 3; ++i)
            {
                Require(
                    !double.IsNaN(value[i]) && !double.IsInfinity(value[i]),
                    message
                );
            }
        }

        private static void RequireClose(double actual, double expected, string message)
        {
            Require(
                !double.IsNaN(actual)
                    && !double.IsInfinity(actual)
                    && Math.Abs(actual - expected) <= 1.0e-12,
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
    }

    [Serializable]
    internal sealed class SurfaceResponseResultDocument
    {
        public string format;
        public string case_id;
        public SurfaceResponseResultProfile profile;
        public SurfaceResponseControlPointResult[] control_points;
    }

    [Serializable]
    internal sealed class SurfaceResponseResultProfile
    {
        public double characteristic_length_m;
        public double quasi_static_gravity_scale;
        public double conformity_reach_m;
        public string support_layout;
    }

    [Serializable]
    internal sealed class SurfaceResponseControlPointResult
    {
        public long stable_id;
        public string support;
        public double[] bridge_position_m;
        public double[] gravity_position_m;
        public double[] conformity_position_m;
    }
}
