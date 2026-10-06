using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEngine;

namespace SansaCloth.Validation.Editor
{
    /// <summary>
    /// Recreates a ready-to-run validation GameObject and wires copied baseline assets.
    /// Safe to run repeatedly, including after probe script GUIDs change on redeploy.
    /// </summary>
    internal static class ProbeSetup
    {
        private const string GameObjectName = "SansaClothBackendProbe";
        private const string FixtureFolder =
            "Assets/SansaCloth/Probe/Data/FixtureExchange/BasicV1";
        private const string ResultFolder =
            "Assets/SansaCloth/Probe/Data/SurfaceResponseResult/BasicV1";

        [MenuItem("SansaCloth/Probe/Setup")]
        internal static void Setup()
        {
            try
            {
                TextAsset[] fixtures = LoadJsonAssets(FixtureFolder);
                TextAsset[] results = LoadJsonAssets(ResultFolder);

                Require(fixtures.Length == 30, "Fixture Exchange asset count must be 30");
                Require(results.Length == 30, "SurfaceResponse result asset count must be 30");

                GameObject previous = GameObject.Find(GameObjectName);
                if (previous != null)
                {
                    Undo.DestroyObjectImmediate(previous);
                }

                var target = new GameObject(GameObjectName);
                Undo.RegisterCreatedObjectUndo(target, "Create SansaCloth Probe");

                SansaClothFixtureExchangeProbe single =
                    GetOrAdd<SansaClothFixtureExchangeProbe>(target);
                SansaClothFixtureExchangeMatrixProbe matrix =
                    GetOrAdd<SansaClothFixtureExchangeMatrixProbe>(target);
                SansaClothGravityResponseComparisonProbe gravity =
                    GetOrAdd<SansaClothGravityResponseComparisonProbe>(target);
                SansaClothConformityResponseComparisonProbe conformity =
                    GetOrAdd<SansaClothConformityResponseComparisonProbe>(target);

                TextAsset sr001C0G0 = fixtures.SingleOrDefault(
                    asset => asset.name == "SR-001-C0-G0"
                );
                Require(sr001C0G0 != null, "SR-001-C0-G0 Fixture Exchange asset is missing");

                SetObject(single, "fixtureExchangeJson", sr001C0G0);
                SetArray(matrix, "fixtureExchangeMatrixJson", fixtures);

                TextAsset[] c0Fixtures = fixtures
                    .Where(asset => IsC0Case(asset.name))
                    .OrderBy(asset => asset.name, StringComparer.Ordinal)
                    .ToArray();
                TextAsset[] c0Results = results
                    .Where(asset => IsC0Case(asset.name))
                    .OrderBy(asset => asset.name, StringComparer.Ordinal)
                    .ToArray();

                Require(c0Fixtures.Length == 10, "C0 Fixture Exchange asset count must be 10");
                Require(c0Results.Length == 10, "C0 result asset count must be 10");

                SetArray(gravity, "fixtureExchangeC0Json", c0Fixtures);
                SetArray(gravity, "surfaceResponseResultC0Json", c0Results);
                SetArray(conformity, "fixtureExchangeJson", fixtures);
                SetArray(conformity, "surfaceResponseResultJson", results);

                EditorUtility.SetDirty(target);
                Selection.activeGameObject = target;

                Log("SETUP.FIXTURE_EXCHANGE_COUNT", fixtures.Length);
                Log("SETUP.SURFACE_RESPONSE_RESULT_COUNT", results.Length);
                Log("SETUP.FXE-011-014", "READY");
                Log("SETUP.FXE-016", "READY");
                Log("SETUP.FXE-017B", "READY");
                Log("SETUP.FXE-017C", "READY");
                Log("SETUP.RESULT", "PASS");
            }
            catch (Exception exception)
            {
                Debug.LogError(
                    $"SANSA_PROBE|SETUP.RESULT|FAIL|{exception.Message}"
                );
                throw;
            }
        }

        [MenuItem("SansaCloth/Probe/Run FXE-011-014")]
        internal static void RunFxe011To014()
        {
            FindProbe<SansaClothFixtureExchangeProbe>().RunProbe();
        }

        [MenuItem("SansaCloth/Probe/Run FXE-016")]
        internal static void RunFxe016()
        {
            FindProbe<SansaClothFixtureExchangeMatrixProbe>().RunProbe();
        }

        [MenuItem("SansaCloth/Probe/Run FXE-017B")]
        internal static void RunFxe017B()
        {
            FindProbe<SansaClothGravityResponseComparisonProbe>().RunProbe();
        }

        [MenuItem("SansaCloth/Probe/Run FXE-017C")]
        internal static void RunFxe017C()
        {
            FindProbe<SansaClothConformityResponseComparisonProbe>().RunProbe();
        }

        [MenuItem("SansaCloth/Probe/Run All Ready Probes")]
        internal static void RunAllReady()
        {
            RunFxe011To014();
            RunFxe016();
            RunFxe017B();
            RunFxe017C();
        }

        private static T GetOrAdd<T>(GameObject target)
            where T : Component
        {
            T component = target.GetComponent<T>();
            if (component == null)
            {
                component = Undo.AddComponent<T>(target);
            }
            return component;
        }

        private static T FindProbe<T>()
            where T : Component
        {
            GameObject target = GameObject.Find(GameObjectName);
            Require(target != null, "Run SansaCloth > Probe > Setup first");
            T component = target.GetComponent<T>();
            Require(component != null, $"Probe component {typeof(T).Name} is missing");
            return component;
        }

        private static TextAsset[] LoadJsonAssets(string folder)
        {
            Require(AssetDatabase.IsValidFolder(folder), $"Asset folder is missing: {folder}");

            return AssetDatabase
                .FindAssets("t:TextAsset", new[] { folder })
                .Select(AssetDatabase.GUIDToAssetPath)
                .Where(path => string.Equals(
                    Path.GetExtension(path),
                    ".json",
                    StringComparison.OrdinalIgnoreCase
                ))
                .Select(AssetDatabase.LoadAssetAtPath<TextAsset>)
                .Where(asset => asset != null)
                .OrderBy(asset => asset.name, StringComparer.Ordinal)
                .ToArray();
        }

        private static bool IsC0Case(string caseId)
        {
            return caseId != null && caseId.Contains("-C0-");
        }

        private static void SetObject(
            UnityEngine.Object target,
            string propertyName,
            UnityEngine.Object value
        )
        {
            var serialized = new SerializedObject(target);
            SerializedProperty property = serialized.FindProperty(propertyName);
            Require(property != null, $"Serialized property is missing: {propertyName}");
            property.objectReferenceValue = value;
            serialized.ApplyModifiedProperties();
        }

        private static void SetArray(
            UnityEngine.Object target,
            string propertyName,
            IReadOnlyList<TextAsset> values
        )
        {
            var serialized = new SerializedObject(target);
            SerializedProperty property = serialized.FindProperty(propertyName);
            Require(property != null, $"Serialized property is missing: {propertyName}");
            property.arraySize = values.Count;
            for (int i = 0; i < values.Count; ++i)
            {
                property.GetArrayElementAtIndex(i).objectReferenceValue = values[i];
            }
            serialized.ApplyModifiedProperties();
        }

        private static void Require(bool condition, string message)
        {
            if (!condition)
            {
                throw new InvalidOperationException(message);
            }
        }

        private static void Log(string key, object value)
        {
            Debug.Log($"SANSA_PROBE|{key}|{value}");
        }
    }
}
