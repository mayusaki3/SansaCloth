using System;
using System.Collections.Generic;
using System.Globalization;

namespace SansaCloth.Validation
{
    /// <summary>
    /// Validation-only reader for sansacloth.validation.fixture-exchange/0.
    /// It intentionally has no dependency on a Unity/product serialization package.
    /// </summary>
    internal static class SansaClothFixtureExchangeJson
    {
        internal const string Format = "sansacloth.validation.fixture-exchange/0";

        internal static FixtureExchangeDocument Parse(string json)
        {
            object rootValue = new JsonReader(json).ReadDocument();
            var root = RequireObject(rootValue, "$");
            RequireKeys(
                root,
                "$",
                "format",
                "case_id",
                "coordinate",
                "body_surface",
                "cloth",
                "inputs"
            );

            var document = new FixtureExchangeDocument
            {
                format = RequireString(root, "format", "$"),
                caseId = RequireString(root, "case_id", "$"),
                coordinate = ParseCoordinate(Require(root, "coordinate", "$")),
                bodySurface = ParseBodySurface(Require(root, "body_surface", "$")),
                cloth = ParseCloth(Require(root, "cloth", "$")),
                inputs = ParseInputs(Require(root, "inputs", "$")),
            };
            Validate(document);
            return document;
        }

        internal static void Validate(FixtureExchangeDocument document)
        {
            Check(document != null, "document is null");
            Check(document.format == Format, "unsupported format");
            Check(!string.IsNullOrEmpty(document.caseId), "case_id must not be empty");
            Check(document.coordinate != null, "coordinate is missing");
            Check(
                document.coordinate.lengthUnit == "m"
                    && document.coordinate.gravityUnit == "m/s^2"
                    && document.coordinate.x == "right"
                    && document.coordinate.y == "up"
                    && document.coordinate.z == "forward",
                "unsupported coordinate declaration"
            );

            Check(document.bodySurface != null, "body_surface is missing");
            Check(
                document.bodySurface.vertices != null
                    && document.bodySurface.vertices.Length > 0,
                "body surface must not be empty"
            );
            Check(
                document.bodySurface.triangles != null,
                "body surface triangles are missing"
            );

            for (int i = 0; i < document.bodySurface.vertices.Length; ++i)
            {
                BodyVertex vertex = document.bodySurface.vertices[i];
                Check(vertex != null, $"body vertex {i} is null");
                CheckFinite3(vertex.positionM, $"body vertex {i} position");
                CheckFinite3(vertex.normal, $"body vertex {i} normal");
                double normalLengthSquared =
                    vertex.normal[0] * vertex.normal[0]
                    + vertex.normal[1] * vertex.normal[1]
                    + vertex.normal[2] * vertex.normal[2];
                Check(
                    normalLengthSquared > 0.0 && IsFinite(normalLengthSquared),
                    $"body vertex {i} normal is invalid"
                );
                CheckUv(vertex.uv, $"body vertex {i} UV");
            }

            int vertexCount = document.bodySurface.vertices.Length;
            for (int i = 0; i < document.bodySurface.triangles.Length; ++i)
            {
                int[] triangle = document.bodySurface.triangles[i];
                Check(
                    triangle != null && triangle.Length == 3,
                    $"triangle {i} must have three indices"
                );
                for (int j = 0; j < 3; ++j)
                {
                    Check(
                        triangle[j] >= 0 && triangle[j] < vertexCount,
                        $"triangle {i} vertex index is out of range"
                    );
                }
            }

            Check(document.cloth != null, "cloth is missing");
            Check(
                document.cloth.controlPoints != null
                    && document.cloth.controlPoints.Length > 0,
                "cloth control points must not be empty"
            );

            var stableIds = new HashSet<ulong>();
            var stripOrders = new HashSet<string>();
            for (int i = 0; i < document.cloth.controlPoints.Length; ++i)
            {
                ControlPoint point = document.cloth.controlPoints[i];
                Check(point != null, $"control point {i} is null");
                Check(
                    stableIds.Add(point.stableId),
                    $"duplicate stable_id {point.stableId}"
                );
                string stripKey = $"{point.stripId}:{point.stripOrder}";
                Check(
                    stripOrders.Add(stripKey),
                    $"duplicate strip/order {point.stripId}/{point.stripOrder}"
                );
                CheckFinite3(
                    point.positionM,
                    $"control point {point.stableId} position"
                );
                Check(
                    point.surfaceReference != null,
                    $"control point {point.stableId} SurfaceReference is missing"
                );
                Check(
                    point.surfaceReference.domainId == document.bodySurface.domainId,
                    $"control point {point.stableId} references an unknown domain"
                );
                Check(
                    IsFinite(point.surfaceReference.u)
                        && IsFinite(point.surfaceReference.v)
                        && InUnitRange(point.surfaceReference.u)
                        && InUnitRange(point.surfaceReference.v),
                    $"control point {point.stableId} SurfaceReference UV is invalid"
                );
            }

            Check(document.inputs != null, "inputs are missing");
            CheckFinite3(document.inputs.worldGravityMPerS2, "world gravity");
            Check(
                IsFinite(document.inputs.conformity)
                    && InUnitRange(document.inputs.conformity),
                "conformity must be finite and in [0,1]"
            );
            Check(
                IsFinite(document.inputs.collisionToleranceM)
                    && document.inputs.collisionToleranceM >= 0.0,
                "collision tolerance must be finite and non-negative"
            );
        }

        private static Coordinate ParseCoordinate(object value)
        {
            var obj = RequireObject(value, "$.coordinate");
            RequireKeys(
                obj,
                "$.coordinate",
                "length_unit",
                "gravity_unit",
                "x",
                "y",
                "z"
            );
            return new Coordinate
            {
                lengthUnit = RequireString(obj, "length_unit", "$.coordinate"),
                gravityUnit = RequireString(obj, "gravity_unit", "$.coordinate"),
                x = RequireString(obj, "x", "$.coordinate"),
                y = RequireString(obj, "y", "$.coordinate"),
                z = RequireString(obj, "z", "$.coordinate"),
            };
        }

        private static BodySurface ParseBodySurface(object value)
        {
            var obj = RequireObject(value, "$.body_surface");
            RequireKeys(obj, "$.body_surface", "domain_id", "vertices", "triangles");
            List<object> vertexValues = RequireArray(
                Require(obj, "vertices", "$.body_surface"),
                "$.body_surface.vertices"
            );
            var vertices = new BodyVertex[vertexValues.Count];
            for (int i = 0; i < vertices.Length; ++i)
            {
                var vertex = RequireObject(
                    vertexValues[i],
                    $"$.body_surface.vertices[{i}]"
                );
                RequireKeys(
                    vertex,
                    $"$.body_surface.vertices[{i}]",
                    "position_m",
                    "normal",
                    "uv"
                );
                vertices[i] = new BodyVertex
                {
                    positionM = RequireDoubleArray(
                        Require(
                            vertex,
                            "position_m",
                            $"$.body_surface.vertices[{i}]"
                        ),
                        3,
                        $"$.body_surface.vertices[{i}].position_m"
                    ),
                    normal = RequireDoubleArray(
                        Require(vertex, "normal", $"$.body_surface.vertices[{i}]"),
                        3,
                        $"$.body_surface.vertices[{i}].normal"
                    ),
                    uv = RequireDoubleArray(
                        Require(vertex, "uv", $"$.body_surface.vertices[{i}]"),
                        2,
                        $"$.body_surface.vertices[{i}].uv"
                    ),
                };
            }

            List<object> triangleValues = RequireArray(
                Require(obj, "triangles", "$.body_surface"),
                "$.body_surface.triangles"
            );
            var triangles = new int[triangleValues.Count][];
            for (int i = 0; i < triangles.Length; ++i)
            {
                triangles[i] = RequireIntArray(
                    triangleValues[i],
                    3,
                    $"$.body_surface.triangles[{i}]"
                );
            }

            return new BodySurface
            {
                domainId = RequireUlong(obj, "domain_id", "$.body_surface"),
                vertices = vertices,
                triangles = triangles,
            };
        }

        private static Cloth ParseCloth(object value)
        {
            var obj = RequireObject(value, "$.cloth");
            RequireKeys(obj, "$.cloth", "control_points");
            List<object> pointValues = RequireArray(
                Require(obj, "control_points", "$.cloth"),
                "$.cloth.control_points"
            );
            var points = new ControlPoint[pointValues.Count];
            for (int i = 0; i < points.Length; ++i)
            {
                string path = $"$.cloth.control_points[{i}]";
                var point = RequireObject(pointValues[i], path);
                RequireKeys(
                    point,
                    path,
                    "stable_id",
                    "strip_id",
                    "strip_order",
                    "position_m",
                    "surface_reference",
                    "anchor",
                    "contact"
                );
                points[i] = new ControlPoint
                {
                    stableId = RequireUlong(point, "stable_id", path),
                    stripId = RequireUlong(point, "strip_id", path),
                    stripOrder = RequireUlong(point, "strip_order", path),
                    positionM = RequireDoubleArray(
                        Require(point, "position_m", path),
                        3,
                        $"{path}.position_m"
                    ),
                    surfaceReference = ParseSurfaceReference(
                        Require(point, "surface_reference", path),
                        $"{path}.surface_reference"
                    ),
                    anchor = RequireBool(point, "anchor", path),
                    contact = RequireBool(point, "contact", path),
                };
            }
            return new Cloth { controlPoints = points };
        }

        private static SurfaceReferenceData ParseSurfaceReference(
            object value,
            string path
        )
        {
            var obj = RequireObject(value, path);
            RequireKeys(obj, path, "domain_id", "u", "v");
            return new SurfaceReferenceData
            {
                domainId = RequireUlong(obj, "domain_id", path),
                u = RequireDouble(obj, "u", path),
                v = RequireDouble(obj, "v", path),
            };
        }

        private static Inputs ParseInputs(object value)
        {
            var obj = RequireObject(value, "$.inputs");
            RequireKeys(
                obj,
                "$.inputs",
                "world_gravity_m_per_s2",
                "conformity",
                "collision_tolerance_m"
            );
            return new Inputs
            {
                worldGravityMPerS2 = RequireDoubleArray(
                    Require(obj, "world_gravity_m_per_s2", "$.inputs"),
                    3,
                    "$.inputs.world_gravity_m_per_s2"
                ),
                conformity = RequireDouble(obj, "conformity", "$.inputs"),
                collisionToleranceM = RequireDouble(
                    obj,
                    "collision_tolerance_m",
                    "$.inputs"
                ),
            };
        }

        private static object Require(
            Dictionary<string, object> obj,
            string key,
            string path
        )
        {
            Check(obj.TryGetValue(key, out object value), $"{path}.{key} is missing");
            return value;
        }

        private static Dictionary<string, object> RequireObject(
            object value,
            string path
        )
        {
            Check(
                value is Dictionary<string, object>,
                $"{path} must be an object"
            );
            return (Dictionary<string, object>)value;
        }

        private static List<object> RequireArray(object value, string path)
        {
            Check(value is List<object>, $"{path} must be an array");
            return (List<object>)value;
        }

        private static string RequireString(
            Dictionary<string, object> obj,
            string key,
            string path
        )
        {
            object value = Require(obj, key, path);
            Check(value is string, $"{path}.{key} must be a string");
            return (string)value;
        }

        private static bool RequireBool(
            Dictionary<string, object> obj,
            string key,
            string path
        )
        {
            object value = Require(obj, key, path);
            Check(value is bool, $"{path}.{key} must be a bool");
            return (bool)value;
        }

        private static double RequireDouble(
            Dictionary<string, object> obj,
            string key,
            string path
        )
        {
            object value = Require(obj, key, path);
            Check(value is double, $"{path}.{key} must be a number");
            double result = (double)value;
            Check(IsFinite(result), $"{path}.{key} must be finite");
            return result;
        }

        private static ulong RequireUlong(
            Dictionary<string, object> obj,
            string key,
            string path
        )
        {
            double value = RequireDouble(obj, key, path);
            Check(
                value >= 0.0
                    && value <= ulong.MaxValue
                    && Math.Truncate(value) == value,
                $"{path}.{key} must be a non-negative integer"
            );
            return checked((ulong)value);
        }

        private static double[] RequireDoubleArray(
            object value,
            int count,
            string path
        )
        {
            List<object> array = RequireArray(value, path);
            Check(array.Count == count, $"{path} must contain {count} values");
            var result = new double[count];
            for (int i = 0; i < count; ++i)
            {
                Check(array[i] is double, $"{path}[{i}] must be a number");
                result[i] = (double)array[i];
                Check(IsFinite(result[i]), $"{path}[{i}] must be finite");
            }
            return result;
        }

        private static int[] RequireIntArray(object value, int count, string path)
        {
            double[] values = RequireDoubleArray(value, count, path);
            var result = new int[count];
            for (int i = 0; i < count; ++i)
            {
                Check(
                    values[i] >= 0.0
                        && values[i] <= int.MaxValue
                        && Math.Truncate(values[i]) == values[i],
                    $"{path}[{i}] must be a non-negative integer"
                );
                result[i] = checked((int)values[i]);
            }
            return result;
        }

        private static void RequireKeys(
            Dictionary<string, object> obj,
            string path,
            params string[] keys
        )
        {
            var allowed = new HashSet<string>(keys);
            foreach (string key in obj.Keys)
            {
                Check(allowed.Contains(key), $"{path}.{key} is unknown");
            }
            foreach (string key in keys)
            {
                Check(obj.ContainsKey(key), $"{path}.{key} is missing");
            }
        }

        private static void CheckFinite3(double[] value, string path)
        {
            Check(value != null && value.Length == 3, $"{path} must have 3 values");
            for (int i = 0; i < 3; ++i)
            {
                Check(IsFinite(value[i]), $"{path}[{i}] must be finite");
            }
        }

        private static void CheckUv(double[] value, string path)
        {
            Check(value != null && value.Length == 2, $"{path} must have 2 values");
            Check(
                IsFinite(value[0])
                    && IsFinite(value[1])
                    && InUnitRange(value[0])
                    && InUnitRange(value[1]),
                $"{path} is invalid"
            );
        }

        private static bool InUnitRange(double value)
        {
            return value >= 0.0 && value <= 1.0;
        }

        private static bool IsFinite(double value)
        {
            return !double.IsNaN(value) && !double.IsInfinity(value);
        }

        private static void Check(bool condition, string message)
        {
            if (!condition)
            {
                throw new FixtureExchangeException(message);
            }
        }

        private sealed class JsonNumber
        {
            internal JsonNumber(string token)
            {
                this.token = token;
            }

            internal readonly string token;
        }

        private sealed class JsonReader
        {
            private readonly string text;
            private int index;

            internal JsonReader(string text)
            {
                this.text = text ?? throw new ArgumentNullException(nameof(text));
            }

            internal object ReadDocument()
            {
                SkipWhitespace();
                object value = ReadValue();
                SkipWhitespace();
                if (index != text.Length)
                {
                    Fail("unexpected trailing content");
                }
                return value;
            }

            private object ReadValue()
            {
                SkipWhitespace();
                if (index >= text.Length)
                {
                    Fail("unexpected end of JSON");
                }

                char c = text[index];
                if (c == '{')
                {
                    return ReadObject();
                }
                if (c == '[')
                {
                    return ReadArray();
                }
                if (c == '"')
                {
                    return ReadString();
                }
                if (c == 't')
                {
                    ReadLiteral("true");
                    return true;
                }
                if (c == 'f')
                {
                    ReadLiteral("false");
                    return false;
                }
                if (c == 'n')
                {
                    ReadLiteral("null");
                    return null;
                }
                if (c == '-' || (c >= '0' && c <= '9'))
                {
                    return ReadNumber();
                }

                Fail($"unexpected character '{c}'");
                return null;
            }

            private Dictionary<string, object> ReadObject()
            {
                Expect('{');
                var result = new Dictionary<string, object>();
                SkipWhitespace();
                if (TryConsume('}'))
                {
                    return result;
                }

                while (true)
                {
                    SkipWhitespace();
                    if (index >= text.Length || text[index] != '"')
                    {
                        Fail("object key must be a string");
                    }
                    string key = ReadString();
                    if (result.ContainsKey(key))
                    {
                        Fail($"duplicate object key '{key}'");
                    }
                    SkipWhitespace();
                    Expect(':');
                    result.Add(key, ReadValue());
                    SkipWhitespace();
                    if (TryConsume('}'))
                    {
                        return result;
                    }
                    Expect(',');
                }
            }

            private List<object> ReadArray()
            {
                Expect('[');
                var result = new List<object>();
                SkipWhitespace();
                if (TryConsume(']'))
                {
                    return result;
                }

                while (true)
                {
                    result.Add(ReadValue());
                    SkipWhitespace();
                    if (TryConsume(']'))
                    {
                        return result;
                    }
                    Expect(',');
                }
            }

            private string ReadString()
            {
                Expect('"');
                var chars = new System.Text.StringBuilder();
                while (index < text.Length)
                {
                    char c = text[index++];
                    if (c == '"')
                    {
                        return chars.ToString();
                    }
                    if (c == '\\')
                    {
                        if (index >= text.Length)
                        {
                            Fail("unterminated escape");
                        }
                        char escaped = text[index++];
                        switch (escaped)
                        {
                            case '"':
                            case '\\':
                            case '/':
                                chars.Append(escaped);
                                break;
                            case 'b':
                                chars.Append('\b');
                                break;
                            case 'f':
                                chars.Append('\f');
                                break;
                            case 'n':
                                chars.Append('\n');
                                break;
                            case 'r':
                                chars.Append('\r');
                                break;
                            case 't':
                                chars.Append('\t');
                                break;
                            case 'u':
                                chars.Append(ReadUnicodeEscape());
                                break;
                            default:
                                Fail($"invalid escape '\\{escaped}'");
                                break;
                        }
                    }
                    else
                    {
                        if (c < 0x20)
                        {
                            Fail("control character in string");
                        }
                        chars.Append(c);
                    }
                }
                Fail("unterminated string");
                return null;
            }

            private char ReadUnicodeEscape()
            {
                if (index + 4 > text.Length)
                {
                    Fail("incomplete unicode escape");
                }
                string hex = text.Substring(index, 4);
                index += 4;
                if (
                    !ushort.TryParse(
                        hex,
                        NumberStyles.HexNumber,
                        CultureInfo.InvariantCulture,
                        out ushort value
                    )
                )
                {
                    Fail("invalid unicode escape");
                }
                return (char)value;
            }

            private double ReadNumber()
            {
                int start = index;
                if (TryConsume('-'))
                {
                }

                if (TryConsume('0'))
                {
                    if (index < text.Length && char.IsDigit(text[index]))
                    {
                        Fail("leading zero in number");
                    }
                }
                else
                {
                    ReadDigits(true);
                }

                if (TryConsume('.'))
                {
                    ReadDigits(true);
                }

                if (TryConsume('e') || TryConsume('E'))
                {
                    if (!TryConsume('+'))
                    {
                        TryConsume('-');
                    }
                    ReadDigits(true);
                }

                string token = text.Substring(start, index - start);
                if (
                    !double.TryParse(
                        token,
                        NumberStyles.Float,
                        CultureInfo.InvariantCulture,
                        out double value
                    )
                    || !IsFinite(value)
                )
                {
                    Fail($"invalid number '{token}'");
                }
                return value;
            }

            private void ReadDigits(bool requireOne)
            {
                int start = index;
                while (index < text.Length && char.IsDigit(text[index]))
                {
                    index++;
                }
                if (requireOne && index == start)
                {
                    Fail("expected digit");
                }
            }

            private void ReadLiteral(string literal)
            {
                if (
                    index + literal.Length > text.Length
                    || string.CompareOrdinal(text, index, literal, 0, literal.Length) != 0
                )
                {
                    Fail($"expected '{literal}'");
                }
                index += literal.Length;
            }

            private void SkipWhitespace()
            {
                while (
                    index < text.Length
                    && (
                        text[index] == ' '
                        || text[index] == '\t'
                        || text[index] == '\r'
                        || text[index] == '\n'
                    )
                )
                {
                    index++;
                }
            }

            private bool TryConsume(char expected)
            {
                if (index < text.Length && text[index] == expected)
                {
                    index++;
                    return true;
                }
                return false;
            }

            private void Expect(char expected)
            {
                if (!TryConsume(expected))
                {
                    Fail($"expected '{expected}'");
                }
            }

            private void Fail(string message)
            {
                throw new FixtureExchangeException(
                    $"JSON parse error at offset {index}: {message}"
                );
            }
        }
    }

    internal sealed class FixtureExchangeException : Exception
    {
        internal FixtureExchangeException(string message)
            : base(message) { }
    }

    internal sealed class FixtureExchangeDocument
    {
        internal string format;
        internal string caseId;
        internal Coordinate coordinate;
        internal BodySurface bodySurface;
        internal Cloth cloth;
        internal Inputs inputs;
    }

    internal sealed class Coordinate
    {
        internal string lengthUnit;
        internal string gravityUnit;
        internal string x;
        internal string y;
        internal string z;
    }

    internal sealed class BodySurface
    {
        internal ulong domainId;
        internal BodyVertex[] vertices;
        internal int[][] triangles;
    }

    internal sealed class BodyVertex
    {
        internal double[] positionM;
        internal double[] normal;
        internal double[] uv;
    }

    internal sealed class Cloth
    {
        internal ControlPoint[] controlPoints;
    }

    internal sealed class ControlPoint
    {
        internal ulong stableId;
        internal ulong stripId;
        internal ulong stripOrder;
        internal double[] positionM;
        internal SurfaceReferenceData surfaceReference;
        internal bool anchor;
        internal bool contact;
    }

    internal sealed class SurfaceReferenceData
    {
        internal ulong domainId;
        internal double u;
        internal double v;
    }

    internal sealed class Inputs
    {
        internal double[] worldGravityMPerS2;
        internal double conformity;
        internal double collisionToleranceM;
    }
}
