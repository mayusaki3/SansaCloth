using System.Globalization;
using UnityEngine;

namespace SansaCloth.Validation
{
    /// <summary>
    /// Minimal Unity PC probe for Runtime Backend Feasibility BF-001/BF-002.
    /// This does not implement the SansaCloth solver.
    /// </summary>
    public sealed class SansaClothCoordinateProbe : MonoBehaviour
    {
        [ContextMenu("Run SansaCloth Coordinate Probe")]
        public void RunProbe()
        {
            Vector3 x = Vector3.right;
            Vector3 y = Vector3.up;
            Vector3 z = Vector3.forward;

            LogVector("UBF-001.UNITY_X", x);
            LogScalar("UBF-002.CLOTH_WIDTH_WORLD_UNIT", 0.20f);
            LogVector("CBF-001.BASIS_X", x);
            LogVector("CBF-001.BASIS_Y", y);
            LogVector("CBF-001.BASIS_Z", z);
            LogVector("CBF-003.NORMAL_Y", y);
            LogVector("CBF-004.WORLD_GRAVITY", Physics.gravity);

            Quaternion plus90Z = Quaternion.AngleAxis(90.0f, z);
            Quaternion minus90Z = Quaternion.AngleAxis(-90.0f, z);
            LogVector("CBF-005.PLUS_90_Z_X", plus90Z * x);
            LogVector("CBF-005.MINUS_90_Z_X", minus90Z * x);

            Vector3 v00 = new Vector3(-0.10f, 0.0f, -0.05f);
            Vector3 v01 = new Vector3(-0.10f, 0.0f, 0.05f);
            Vector3 v11 = new Vector3(0.10f, 0.0f, 0.05f);
            Vector3 triangleNormal = Vector3.Cross(v01 - v00, v11 - v00).normalized;
            LogVector("CBF-006.V00_V01_V11_NORMAL", triangleNormal);

            LogVector("CBF-007.CROSS_X_Y", Vector3.Cross(x, y));
            LogVector("CBF-007.CROSS_Y_Z", Vector3.Cross(y, z));
            LogVector("CBF-007.CROSS_Z_X", Vector3.Cross(z, x));

            Quaternion bodyRotation = Quaternion.AngleAxis(90.0f, z);
            LogVector("CBF-008.BODY_ROTATED_X", bodyRotation * x);
            LogVector("CBF-008.WORLD_GRAVITY_UNCHANGED", Physics.gravity);
            LogVector("CBF-008.BODY_ROTATED_GRAVITY_REFERENCE", bodyRotation * Physics.gravity);
        }

        private static void LogScalar(string key, float value)
        {
            Debug.Log($"SANSA_BF|{key}|{value.ToString("R", CultureInfo.InvariantCulture)}");
        }

        private static void LogVector(string key, Vector3 value)
        {
            Debug.Log(
                $"SANSA_BF|{key}|{value.x.ToString("R", CultureInfo.InvariantCulture)}," +
                $"{value.y.ToString("R", CultureInfo.InvariantCulture)}," +
                $"{value.z.ToString("R", CultureInfo.InvariantCulture)}"
            );
        }
    }
}
