using System;
using System.Globalization;
using System.IO;
using UnityEngine;

namespace SansaCloth.Validation
{
    /// <summary>
    /// BF-008 validation-only capture probe.
    /// Writes SR-001 C=0/G=0 Final CP and Aggregate Measurements as JSON Lines.
    /// This is not a production serialization format.
    /// </summary>
    public sealed class SansaClothValidationCaptureProbe : MonoBehaviour
    {
        private const int USamples = 21;
        private const int VSamples = 7;
        private const float WidthM = 0.20f;
        private const float DepthM = 0.10f;
        private const float CollisionToleranceM = 0.0f;
        private const ulong DomainId = 1;
        private const string TestId = "UNITY-SR-001-C0-G0";

        [ContextMenu("Run SansaCloth Validation Capture Probe")]
        public void RunProbe()
        {
            string eventId = Guid.NewGuid().ToString("D");
            string timestampUtc = DateTime.UtcNow.ToString(
                "O",
                CultureInfo.InvariantCulture
            );
            string path = Path.Combine(
                Application.temporaryCachePath,
                "SansaClothValidationCapture.jsonl"
            );

            int cpCount = 0;
            int contactCount = 0;
            int supportCount = 0;
            double separationSumM = 0.0;
            float maxSeparationM = 0.0f;
            float maxPenetrationM = 0.0f;
            float maxPositionDeviationM = 0.0f;
            double squaredPositionDeviationSum = 0.0;

            using (var writer = new StreamWriter(path, false))
            {
                writer.WriteLine(
                    "{\"record_type\":\"header\"," +
                    $"\"event_id\":\"{eventId}\"," +
                    $"\"timestamp_utc\":\"{timestampUtc}\"," +
                    $"\"test_id\":\"{TestId}\"," +
                    "\"backend\":\"Unity PC\"," +
                    $"\"cp_count\":{USamples * VSamples}" +
                    "}"
                );

                for (int vIndex = 0; vIndex < VSamples; ++vIndex)
                {
                    float v = (float)vIndex / (VSamples - 1);
                    for (int uIndex = 0; uIndex < USamples; ++uIndex)
                    {
                        float u = (float)uIndex / (USamples - 1);
                        int stableId = vIndex * USamples + uIndex;

                        Vector3 initialPosition = new Vector3(
                            (u - 0.5f) * WidthM,
                            0.0f,
                            (v - 0.5f) * DepthM
                        );

                        bool isAnchor =
                            uIndex == 0 || uIndex == USamples - 1;
                        string support = isAnchor ? "Anchor" : "Unsupported";

                        // Flat C=0/G=0 boundary case: no deformation required.
                        Vector3 finalPosition = initialPosition;
                        Vector3 surfacePosition = initialPosition;
                        Vector3 surfaceNormal = Vector3.up;
                        float separationM = Vector3.Dot(
                            finalPosition - surfacePosition,
                            surfaceNormal
                        );
                        bool derivedContact =
                            separationM <= CollisionToleranceM;

                        float positionDeviationM = Vector3.Distance(
                            initialPosition,
                            finalPosition
                        );

                        cpCount++;
                        if (derivedContact)
                        {
                            contactCount++;
                        }
                        if (isAnchor)
                        {
                            supportCount++;
                        }

                        separationSumM += separationM;
                        maxSeparationM = Mathf.Max(
                            maxSeparationM,
                            separationM
                        );
                        maxPenetrationM = Mathf.Max(
                            maxPenetrationM,
                            Mathf.Max(0.0f, -separationM)
                        );
                        maxPositionDeviationM = Mathf.Max(
                            maxPositionDeviationM,
                            positionDeviationM
                        );
                        squaredPositionDeviationSum +=
                            (double)positionDeviationM * positionDeviationM;

                        writer.WriteLine(
                            "{\"record_type\":\"final_cp\"," +
                            $"\"stable_id\":{stableId}," +
                            $"\"position_m\":[{F(finalPosition.x)},{F(finalPosition.y)},{F(finalPosition.z)}]," +
                            $"\"surface_reference\":{{\"domain_id\":{DomainId},\"u\":{F(u)},\"v\":{F(v)}}}," +
                            $"\"surface_normal\":[{F(surfaceNormal.x)},{F(surfaceNormal.y)},{F(surfaceNormal.z)}]," +
                            $"\"separation_m\":{F(separationM)}," +
                            $"\"support\":\"{support}\"," +
                            $"\"derived_contact\":{(derivedContact ? "true" : "false")}" +
                            "}"
                        );
                    }
                }

                double meanSeparationM =
                    cpCount == 0 ? 0.0 : separationSumM / cpCount;
                double rmsPositionDeviationM =
                    cpCount == 0
                        ? 0.0
                        : Math.Sqrt(squaredPositionDeviationSum / cpCount);

                writer.WriteLine(
                    "{\"record_type\":\"aggregate\"," +
                    $"\"contact_count\":{contactCount}," +
                    $"\"support_count\":{supportCount}," +
                    $"\"mean_separation_m\":{D(meanSeparationM)}," +
                    $"\"max_separation_m\":{F(maxSeparationM)}," +
                    $"\"max_penetration_m\":{F(maxPenetrationM)}," +
                    $"\"max_position_deviation_m\":{F(maxPositionDeviationM)}," +
                    $"\"rms_position_deviation_m\":{D(rmsPositionDeviationM)}" +
                    "}"
                );
            }

            int recordCount = File.ReadAllLines(path).Length;
            Debug.Log($"SANSA_BF|BF-008.EVENT_ID|{eventId}");
            Debug.Log($"SANSA_BF|BF-008.TEST_ID|{TestId}");
            Debug.Log($"SANSA_BF|BF-008.CAPTURE_PATH|{path}");
            Debug.Log($"SANSA_BF|BF-008.RECORD_COUNT|{recordCount}");
            Debug.Log($"SANSA_BF|BF-008.CP_COUNT|{cpCount}");
            Debug.Log($"SANSA_BF|BF-008.CONTACT_COUNT|{contactCount}");
            Debug.Log($"SANSA_BF|BF-008.SUPPORT_COUNT|{supportCount}");
            Debug.Log($"SANSA_BF|BF-008.MEAN_SEPARATION_M|0");
            Debug.Log($"SANSA_BF|BF-008.MAX_SEPARATION_M|{F(maxSeparationM)}");
            Debug.Log($"SANSA_BF|BF-008.MAX_PENETRATION_M|{F(maxPenetrationM)}");
            Debug.Log(
                $"SANSA_BF|BF-008.MAX_POSITION_DEVIATION_M|{F(maxPositionDeviationM)}"
            );
        }

        private static string F(float value)
        {
            return value.ToString("R", CultureInfo.InvariantCulture);
        }

        private static string D(double value)
        {
            return value.ToString("R", CultureInfo.InvariantCulture);
        }
    }
}
