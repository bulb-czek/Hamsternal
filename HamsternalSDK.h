/**
* @file HamsternalSDK.h
* @brief Hamsternal SDK Header
* 
* NOTE: The Hamsternal SDK is currently in BETA and is subject to change.
*
* Contributions to the Hamsternal Beta SDK are greatly appreciated! 
* If you would like to help improve this project, please feel free 
* to submit a pull request or open an issue.
*/

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <map>
#include <type_traits>
#include <variant>
#include <stdexcept>
#include <utility>

#define HAMSTERNAL_SDK_VERSION_MAJOR 1
#define HAMSTERNAL_SDK_VERSION_PATCH 0

struct PluginApiVersion
{
    uint32_t Major = HAMSTERNAL_SDK_VERSION_MAJOR;
    uint32_t Patch = HAMSTERNAL_SDK_VERSION_PATCH;
};

typedef PluginApiVersion (*GetPluginApiVersionFunc)();

inline bool IsPluginApiCompatible(const PluginApiVersion& pluginVersion, const PluginApiVersion& hostVersion)
{
    return pluginVersion.Major == hostVersion.Major;
}

#define REGISTER_PLUGIN(PluginClass) \
    extern "C" __declspec(dllexport) PluginApiVersion GetPluginApiVersion() \
    { \
        return PluginApiVersion { HAMSTERNAL_SDK_VERSION_MAJOR, HAMSTERNAL_SDK_VERSION_PATCH }; \
    } \
    extern "C" __declspec(dllexport) IPlugin* CreatePlugin() \
    { \
        return new PluginClass(); \
    } \
    extern "C" __declspec(dllexport) void DestroyPlugin(IPlugin* plugin) \
    { \
        delete plugin; \
    }

#define FIELD(x) Add(#x, x)

struct Color4;
struct Vector2;
struct Vector3;
struct Color3;

namespace Json
{
    std::string Serialize(int val);
    std::string Serialize(double val);
    std::string Serialize(bool val);
    std::string Serialize(const char* val);
    std::string Serialize(const std::string& val);
    template<typename T> std::string Serialize(const std::vector<T>& vec);

    inline std::string Serialize(int val)
    {
        return std::to_string(val);
    }

    inline std::string Serialize(double val)
    {
        return std::to_string(val);
    }

    inline std::string Serialize(float val)
    {
        return Serialize(static_cast<double>(val));
    }

    inline std::string Serialize(bool val)
    {
        return val ? "true" : "false";
    }

    inline std::string Serialize(const std::string& val)
    {
        std::string res = "\"";
        for (char c : val)
        {
            if (c == '"')
                res += "\\\"";
            else if (c == '\\')
                res += "\\\\";
            else if (c == '\n')
                res += "\\n";
            else
                res += c;
        }
        res += "\"";
        return res;
    }

    inline std::string Serialize(const char* val)
    {
        return Serialize(std::string(val));
    }

    template<typename T>
    inline std::string Serialize(const std::vector<T>& vec)
    {
        std::string res = "[";
        for (size_t i = 0; i < vec.size(); ++i)
        {
            res += Serialize(vec[i]);
            if (i < vec.size() - 1)
                res += ", ";
        }
        res += "]";
        return res;
    }

    class ObjectBuilder
    {
    private:
        std::string m_jsonStr = "{";
        bool m_first = true;

        static std::string CleanKey(const std::string& key)
        {
            if (key.size() > 2 && key[0] == 'm' && key[1] == '_')
                return key.substr(2);
            return key;
        }

    public:
        template<typename T>
        ObjectBuilder& Add(const std::string& key, const T& val)
        {
            if (!m_first)
                m_jsonStr += ", ";
            m_jsonStr += Serialize(CleanKey(key)) + ": " + Serialize(val);
            m_first = false;
            return *this;
        }

        std::string Build()
        {
            return m_jsonStr + "}";
        }
    };

    class ObjectReader
    {
    private:
        std::string m_json;

        bool FindValue(const std::string& key, std::string& value) const
        {
            const std::string quotedKey = Serialize(key);
            const size_t keyPosition = m_json.find(quotedKey);
            if (keyPosition == std::string::npos)
                return false;

            const size_t colonPosition = m_json.find(':', keyPosition + quotedKey.size());
            if (colonPosition == std::string::npos)
                return false;

            const size_t valueStart = m_json.find_first_not_of(" \t\r\n", colonPosition + 1);
            if (valueStart == std::string::npos)
                return false;

            size_t valueEnd;
            if (m_json[valueStart] == '"')
            {
                bool escaped = false;
                valueEnd = valueStart + 1;
                for (; valueEnd < m_json.size(); ++valueEnd)
                {
                    const char character = m_json[valueEnd];
                    if (escaped)
                        escaped = false;
                    else if (character == '\\')
                        escaped = true;
                    else if (character == '"')
                    {
                        ++valueEnd;
                        break;
                    }
                }
                if (valueEnd > m_json.size() || m_json[valueEnd - 1] != '"')
                    return false;
            }
            else
            {
                valueEnd = m_json.find_first_of(",}", valueStart);
                if (valueEnd == std::string::npos)
                    valueEnd = m_json.size();
                while (valueEnd > valueStart && (m_json[valueEnd - 1] == ' ' || m_json[valueEnd - 1] == '\t' || m_json[valueEnd - 1] == '\r' || m_json[valueEnd - 1] == '\n'))
                    --valueEnd;
            }

            if (valueEnd <= valueStart)
                return false;
            value = m_json.substr(valueStart, valueEnd - valueStart);
            return true;
        }

    public:
        explicit ObjectReader(const std::string& json) : m_json(json)
        {
        }

        bool Get(const std::string& key, bool& value) const
        {
            std::string rawValue;
            if (!FindValue(key, rawValue))
                return false;
            if (rawValue == "true")
                value = true;
            else if (rawValue == "false")
                value = false;
            else
                return false;
            return true;
        }

        bool Get(const std::string& key, int& value) const
        {
            std::string rawValue;
            if (!FindValue(key, rawValue))
                return false;

            try
            {
                size_t parsedLength = 0;
                const int parsedValue = std::stoi(rawValue, &parsedLength);
                if (parsedLength != rawValue.size())
                    return false;
                value = parsedValue;
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        bool Get(const std::string& key, float& value) const
        {
            std::string rawValue;
            if (!FindValue(key, rawValue))
                return false;

            try
            {
                size_t parsedLength = 0;
                const float parsedValue = std::stof(rawValue, &parsedLength);
                if (parsedLength != rawValue.size() || !std::isfinite(parsedValue))
                    return false;
                value = parsedValue;
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        bool Get(const std::string& key, double& value) const
        {
            std::string rawValue;
            if (!FindValue(key, rawValue))
                return false;

            try
            {
                size_t parsedLength = 0;
                const double parsedValue = std::stod(rawValue, &parsedLength);
                if (parsedLength != rawValue.size() || !std::isfinite(parsedValue))
                    return false;
                value = parsedValue;
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        bool Get(const std::string& key, std::string& value) const
        {
            std::string rawValue;
            if (!FindValue(key, rawValue) || rawValue.size() < 2 || rawValue.front() != '"' || rawValue.back() != '"')
                return false;

            std::string decoded;
            decoded.reserve(rawValue.size() - 2);
            for (size_t i = 1; i + 1 < rawValue.size(); ++i)
            {
                char character = rawValue[i];
                if (character == '\\')
                {
                    if (++i + 1 >= rawValue.size())
                        return false;
                    character = rawValue[i];
                    if (character == 'n')
                        character = '\n';
                    else if (character != '"' && character != '\\' && character != '/')
                        return false;
                }
                decoded += character;
            }
            value = std::move(decoded);
            return true;
        }

    };
}

struct Vector2
{
    float X;
    float Y;

    Vector2() : X(0.0f), Y(0.0f)
    {
    }

    Vector2(float x_, float y_) : X(x_), Y(y_)
    {
    }

    Vector2 operator+(const Vector2& other) const
    {
        return Vector2(X + other.X, Y + other.Y);
    }

    Vector2 operator-(const Vector2& other) const
    {
        return Vector2(X - other.X, Y - other.Y);
    }

    bool operator==(const Vector2& other) const
    {
        return X == other.X && Y == other.Y;
    }

    Vector2 operator*(float scalar) const
    {
        return Vector2(X * scalar, Y * scalar);
    }

    Vector2 operator/(float scalar) const
    {
        return Vector2(X / scalar, Y / scalar);
    }

    Vector2 operator-() const
    {
        return Vector2(-X, -Y);
    }

    Vector2 operator*(const Vector2& other) const
    {
        return Vector2(X * other.X, Y * other.Y);
    }

    std::string ToString() const
    {
        return std::to_string(X) + ", " + std::to_string(Y);
    }
};

struct Vector3
{
    float X;
    float Y;
    float Z;

    Vector3() : X(0.0f), Y(0.0f), Z(0.0f)
    {
    }

    Vector3(float x_, float y_, float z_) : X(x_), Y(y_), Z(z_)
    {
    }

    Vector3 operator+(const Vector3& other) const
    {
        return Vector3(X + other.X, Y + other.Y, Z + other.Z);
    }

    Vector3 operator-(const Vector3& other) const
    {
        return Vector3(X - other.X, Y - other.Y, Z - other.Z);
    }

    bool operator==(const Vector3& other) const
    {
        return X == other.X && Y == other.Y && Z == other.Z;
    }

    Vector3 operator*(float scalar) const
    {
        return Vector3(X * scalar, Y * scalar, Z * scalar);
    }

    Vector3 operator/(float scalar) const
    {
        return Vector3(X / scalar, Y / scalar, Z / scalar);
    }

    Vector3 operator-() const
    {
        return Vector3(-X, -Y, -Z);
    }

    Vector3 operator*(const Vector3& other) const
    {
        return Vector3(X * other.X, Y * other.Y, Z * other.Z);
    }

    float Magnitude() const
    {
        return sqrt(X * X + Y * Y + Z * Z);
    }

    Vector3 Normalize() const
    {
        float len = Magnitude();
        if (len == 0.0f)
            return Vector3(0, 0, 0);
        return *this / len;
    }

    static Vector3 Slerp(const Vector3& start, const Vector3& end, float alpha)
    {
        const Vector3 from = start.Normalize();
        const Vector3 to = end.Normalize();
        if (from.Magnitude() == 0.0f)
            return to;
        if (to.Magnitude() == 0.0f)
            return from;

        alpha = std::clamp(alpha, 0.0f, 1.0f);
        if (alpha == 0.0f)
            return from;
        if (alpha == 1.0f)
            return to;

        const float dot = std::clamp(from.Dot(to), -1.0f, 1.0f);
        if (dot > 0.9995f)
            return (from + (to - from) * alpha).Normalize();

        if (dot < -0.999999f)
        {
            Vector3 perpendicular = from.Cross(Vector3(0.0f, 1.0f, 0.0f));
            if (perpendicular.Magnitude() < 0.0001f)
                perpendicular = from.Cross(Vector3(1.0f, 0.0f, 0.0f));
            perpendicular = perpendicular.Normalize();
            const float angle = std::acos(-1.0f) * alpha;
            return (from * std::cos(angle) + perpendicular * std::sin(angle)).Normalize();
        }

        const float angle = std::acos(dot);
        const float inverseSinAngle = 1.0f / std::sin(angle);
        return (from * (std::sin((1.0f - alpha) * angle) * inverseSinAngle) + to * (std::sin(alpha * angle) * inverseSinAngle)).Normalize();
    }

    Vector3 Sign() const
    {
        return Vector3(
            (X > 0.0f ? 1.0f : (X < 0.0f ? -1.0f : 0.0f)),
            (Y > 0.0f ? 1.0f : (Y < 0.0f ? -1.0f : 0.0f)),
            (Z > 0.0f ? 1.0f : (Z < 0.0f ? -1.0f : 0.0f))
        );
    }

    Vector3 Cross(const Vector3& other) const
    {
        return Vector3(
            Y * other.Z - Z * other.Y,
            Z * other.X - X * other.Z,
            X * other.Y - Y * other.X
        );
    }

    float Dot(const Vector3& other) const
    {
        return X * other.X + Y * other.Y + Z * other.Z;
    }

    Vector3& operator+=(const Vector3& other)
    {
        X += other.X;
        Y += other.Y;
        Z += other.Z;
        return *this;
    }

    Vector3& operator-=(const Vector3& other)
    {
        X -= other.X;
        Y -= other.Y;
        Z -= other.Z;
        return *this;
    }

    std::string ToString() const
    {
        return std::to_string(X) + ", " + std::to_string(Y) + ", " + std::to_string(Z);
    }
};

struct Color3
{
    float R;
    float G;
    float B;

    Color3() : R(0.0f), G(0.0f), B(0.0f)
    {
    }

    Color3(float r_, float g_, float b_) : R(r_), G(g_), B(b_)
    {
    }

    std::string ToString() const
    {
        return std::to_string(R) + ", " + std::to_string(G) + ", " + std::to_string(B);
    }
};

struct ColorSequenceKeypoint
{
    Color3 Color;
    float Time;
    ColorSequenceKeypoint() : Color(0.0f, 0.0f, 0.0f), Time(0.0f)
    {
    }

    ColorSequenceKeypoint(const Color3& color, float time) : Color(color), Time(time)
    {
    }
};

struct ColorSequence
{
    std::vector<ColorSequenceKeypoint> Keypoints;

    ColorSequence() = default;
    ColorSequence(std::initializer_list<ColorSequenceKeypoint> keypoints) : Keypoints(keypoints)
    {
    }
    ColorSequence(const std::vector<ColorSequenceKeypoint>& keypoints) : Keypoints(keypoints)
    {
    }

    std::string ToString() const
    {
        std::string result = "ColorSequence: [";
        for (const auto& kp : Keypoints)
        {
            result += "{" + kp.Color.ToString() + ", " + std::to_string(kp.Time) + "}, ";
        }
        if (!Keypoints.empty())
            result.pop_back(), result.pop_back();
        result += "]";
        return result;
    }
};

struct AnimationTrack
{
    uintptr_t Address = 0;
    float Speed = 1.0f;
    float TimePosition = 0.0f;
    bool Looped = false;
    bool Playing = false;
    std::string AnimationId;

    AnimationTrack(
        uintptr_t address,
        float speed = 1.0f,
        float timePosition = 0.0f,
        bool looped = false,
        bool playing = false,
        std::string animationId = {})
        : Address(address),
          Speed(speed),
          TimePosition(timePosition),
          Looped(looped),
          Playing(playing),
          AnimationId(std::move(animationId))
    {
    }

    const std::string& GetAnimationId() const
    {
        return AnimationId;
    }
};

struct Matrix3
{
    float Data[9];

    static Matrix3 CreateLookAt(const Vector3& from, const Vector3& to)
    {
        Vector3 forward = (to - from).Normalize();
        Vector3 worldUp = fabsf(forward.Y) > 0.99f ? Vector3{ 1.0f, 0.0f, 0.0f } : Vector3{ 0.0f, 1.0f, 0.0f };
        Vector3 right = worldUp.Cross(forward).Normalize();
        Vector3 up = forward.Cross(right);
        Matrix3 rotation;

        rotation.Data[0] = -right.X;
        rotation.Data[1] = up.X;
        rotation.Data[2] = -forward.X;
        rotation.Data[3] = right.Y;
        rotation.Data[4] = up.Y;
        rotation.Data[5] = -forward.Y;
        rotation.Data[6] = -right.Z;
        rotation.Data[7] = up.Z;
        rotation.Data[8] = -forward.Z;

        return rotation;
    }

    static Matrix3 Lerp(const Matrix3& current, const Matrix3& target, float alpha)
    {
        Matrix3 result;
        for (int i = 0; i < 9; i++)
            result.Data[i] = current.Data[i] + (target.Data[i] - current.Data[i]) * alpha;
        return result;
    }
};

struct Matrix4
{
    float Data[16];

    static Matrix4 Multiply(const Matrix4& a, const Matrix4& b)
    {
        Matrix4 result{};
        for (int row = 0; row < 4; row++)
        {
            for (int col = 0; col < 4; col++)
            {
                float sum = 0.0f;
                for (int i = 0; i < 4; i++)
                    sum += a.Data[row * 4 + i] * b.Data[i * 4 + col];
                result.Data[row * 4 + col] = sum;
            }
        }
        return result;
    }

    Matrix4 operator*(const Matrix4& other) const
    {
        return Multiply(*this, other);
    }

    static Matrix4 Transpose(const Matrix4& m)
    {
        Matrix4 t{};
        for (int r = 0; r < 4; r++)
            for (int c = 0; c < 4; c++)
                t.Data[r * 4 + c] = m.Data[c * 4 + r];
        return t;
    }
};

struct CFrame
{
    Vector3 Position;
    Vector3 RightVector;
    Vector3 UpVector;
    Vector3 BackVector;

    CFrame() : Position(0.0f, 0.0f, 0.0f), RightVector(1.0f, 0.0f, 0.0f), UpVector(0.0f, 1.0f, 0.0f), BackVector(0.0f, 0.0f, 1.0f)
    {
    }

    explicit CFrame(const Vector3& pos) : Position(pos), RightVector(1.0f, 0.0f, 0.0f), UpVector(0.0f, 1.0f, 0.0f), BackVector(0.0f, 0.0f, 1.0f)
    {
    }

    CFrame(const Vector3& pos, const Vector3& right, const Vector3& up, const Vector3& back) : Position(pos), RightVector(right), UpVector(up), BackVector(back)
    {
    }

    Matrix3 GetRotationMatrix() const
    {
        Matrix3 rot;
        rot.Data[0] = RightVector.X;
        rot.Data[3] = RightVector.Y;
        rot.Data[6] = RightVector.Z;
        rot.Data[1] = UpVector.X;
        rot.Data[4] = UpVector.Y;
        rot.Data[7] = UpVector.Z;
        rot.Data[2] = BackVector.X;
        rot.Data[5] = BackVector.Y;
        rot.Data[8] = BackVector.Z;
        return rot;
    }

    static CFrame FromMatrix3(const Vector3& pos, const Matrix3& rot)
    {
        CFrame cf;
        cf.Position = pos;
        cf.RightVector = Vector3(rot.Data[0], rot.Data[3], rot.Data[6]);
        cf.UpVector = Vector3(rot.Data[1], rot.Data[4], rot.Data[7]);
        cf.BackVector = Vector3(rot.Data[2], rot.Data[5], rot.Data[8]);
        return cf;
    }
};

struct Color4
{
    float R;
    float G;
    float B;
    float A;

    Color4() : R(0), G(0), B(0), A(1)
    {
    }

    Color4(float r_, float g_, float b_, float a_ = 1.0f) : R(r_), G(g_), B(b_), A(a_)
    {
    }
};

using SettingsValue = std::variant<bool, int, float, double, std::string, Vector2, Vector3, Color4, Color3>;

class Settings
{
private:
    std::map<std::string, SettingsValue> m_settings;
    std::string m_json;

    template<typename T>
    static bool ReadValue(const Json::ObjectReader& reader, const std::string& key, T& value)
    {
        T parsedValue = value;
        if constexpr (std::is_same_v<T, Vector2>)
        {
            const std::string prefix = key + ".";
            if (!reader.Get(prefix + "X", parsedValue.X) || !reader.Get(prefix + "Y", parsedValue.Y))
                return false;
        }
        else if constexpr (std::is_same_v<T, Vector3>)
        {
            const std::string prefix = key + ".";
            if (!reader.Get(prefix + "X", parsedValue.X)
                || !reader.Get(prefix + "Y", parsedValue.Y)
                || !reader.Get(prefix + "Z", parsedValue.Z))
                return false;
        }
        else if constexpr (std::is_same_v<T, Color4>)
        {
            const std::string prefix = key + ".";
            if (!reader.Get(prefix + "R", parsedValue.R)
                || !reader.Get(prefix + "G", parsedValue.G)
                || !reader.Get(prefix + "B", parsedValue.B)
                || !reader.Get(prefix + "A", parsedValue.A))
                return false;
        }
        else if constexpr (std::is_same_v<T, Color3>)
        {
            const std::string prefix = key + ".";
            if (!reader.Get(prefix + "R", parsedValue.R)
                || !reader.Get(prefix + "G", parsedValue.G)
                || !reader.Get(prefix + "B", parsedValue.B))
                return false;
        }
        else
        {
            if (!reader.Get(key, parsedValue))
                return false;
        }
        value = std::move(parsedValue);
        return true;
    }

    template<typename T>
    static void WriteValue(Json::ObjectBuilder& builder, const std::string& key, const T& value)
    {
        if constexpr (std::is_same_v<T, Vector2>)
        {
            const std::string prefix = key + ".";
            builder.Add(prefix + "X", value.X);
            builder.Add(prefix + "Y", value.Y);
        }
        else if constexpr (std::is_same_v<T, Vector3>)
        {
            const std::string prefix = key + ".";
            builder.Add(prefix + "X", value.X);
            builder.Add(prefix + "Y", value.Y);
            builder.Add(prefix + "Z", value.Z);
        }
        else if constexpr (std::is_same_v<T, Color4>)
        {
            const std::string prefix = key + ".";
            builder.Add(prefix + "R", value.R);
            builder.Add(prefix + "G", value.G);
            builder.Add(prefix + "B", value.B);
            builder.Add(prefix + "A", value.A);
        }
        else if constexpr (std::is_same_v<T, Color3>)
        {
            const std::string prefix = key + ".";
            builder.Add(prefix + "R", value.R);
            builder.Add(prefix + "G", value.G);
            builder.Add(prefix + "B", value.B);
        }
        else
            builder.Add(key, value);
    }

public:
    Settings() = default;
    explicit Settings(const std::string& json) : m_json(json)
    {
    }

    template<typename T>
    void Add(const std::string& key, const T& value)
    {
        m_settings[key] = value;
    }

    template<typename T>
    T Get(const std::string& key) const
    {
        const auto iterator = m_settings.find(key);
        if (iterator != m_settings.end())
        {
            if (const T* value = std::get_if<T>(&iterator->second))
                return *value;
        }

        T value{};
        Get(key, value);
        return value;
    }

    template<typename T>
    bool Get(const std::string& key, T& value) const
    {
        const auto iterator = m_settings.find(key);
        if (iterator != m_settings.end())
        {
            if (const T* storedValue = std::get_if<T>(&iterator->second))
            {
                value = *storedValue;
                return true;
            }
            return false;
        }

        if (m_json.empty())
            return false;
        return ReadValue(Json::ObjectReader(m_json), key, value);
    }

    std::string Serialize() const
    {
        Json::ObjectBuilder builder;
        for (const auto& pair : m_settings)
        {
            std::visit([&builder, &pair](const auto& value)
            {
                using T = std::decay_t<decltype(value)>;
                WriteValue(builder, pair.first, value);
            }, pair.second);
        }
        return builder.Build();
    }

    void Deserialize(const std::string& json)
    {
        m_json = json;
        const Json::ObjectReader reader(json);
        for (auto& pair : m_settings)
        {
            std::visit([&reader, &pair](auto& value)
            {
                Settings::ReadValue(reader, pair.first, value);
            }, pair.second);
        }
    }
};

using AttrValue = std::variant<std::monostate, std::string, int, bool, double, Vector2, Vector3, Color4, Color3>;
using AttributeValue = std::variant<std::monostate, const char*, int, bool, double, Vector2, Vector3, Color4, Color3>;

enum class AttrType
{
    String,
    Bool,
    Int,
    Double,
    Vector2,
    Vector3,
    Color4,
    Color3
};

namespace RBLX
{
    class IInstance;
}

enum class KeyCode : uint32_t
{
    None = 0x00, LButton = 0x01, RButton = 0x02, Cancel = 0x03,
    MButton = 0x04, XButton1 = 0x05, XButton2 = 0x06, Backspace = 0x08,
    Tab = 0x09, Clear = 0x0C, Enter = 0x0D, Shift = 0x10,
    Control = 0x11, Alt = 0x12, Pause = 0x13, CapsLock = 0x14,
    Kana = 0x15, Hangul = 0x15, Junja = 0x17, Final = 0x18,
    Hanja = 0x19, Kanji = 0x19, Escape = 0x1B, Convert = 0x1C,
    NonConvert = 0x1D, Accept = 0x1E, ModeChange = 0x1F, Space = 0x20,
    PageUp = 0x21, PageDown = 0x22, End = 0x23, Home = 0x24,
    Left = 0x25, Up = 0x26, Right = 0x27, Down = 0x28,
    Select = 0x29, Print = 0x2A, Execute = 0x2B, Snapshot = 0x2C,
    Insert = 0x2D, Delete = 0x2E, Help = 0x2F,
    Key0 = 0x30, Key1 = 0x31, Key2 = 0x32, Key3 = 0x33,
    Key4 = 0x34, Key5 = 0x35, Key6 = 0x36, Key7 = 0x37,
    Key8 = 0x38, Key9 = 0x39,
    A = 0x41, B = 0x42, C = 0x43, D = 0x44, E = 0x45, F = 0x46,
    G = 0x47, H = 0x48, I = 0x49, J = 0x4A, K = 0x4B, L = 0x4C,
    M = 0x4D, N = 0x4E, O = 0x4F, P = 0x50, Q = 0x51, R = 0x52,
    S = 0x53, T = 0x54, U = 0x55, V = 0x56, W = 0x57, X = 0x58,
    Y = 0x59, Z = 0x5A, LWin = 0x5B, RWin = 0x5C, Apps = 0x5D,
    Sleep = 0x5F,
    NumPad0 = 0x60, NumPad1 = 0x61, NumPad2 = 0x62, NumPad3 = 0x63,
    NumPad4 = 0x64, NumPad5 = 0x65, NumPad6 = 0x66, NumPad7 = 0x67,
    NumPad8 = 0x68, NumPad9 = 0x69, Multiply = 0x6A, Add = 0x6B,
    Separator = 0x6C, Subtract = 0x6D, Decimal = 0x6E, Divide = 0x6F,
    F1 = 0x70, F2 = 0x71, F3 = 0x72, F4 = 0x73, F5 = 0x74, F6 = 0x75,
    F7 = 0x76, F8 = 0x77, F9 = 0x78, F10 = 0x79, F11 = 0x7A, F12 = 0x7B,
    F13 = 0x7C, F14 = 0x7D, F15 = 0x7E, F16 = 0x7F, F17 = 0x80, F18 = 0x81,
    F19 = 0x82, F20 = 0x83, F21 = 0x84, F22 = 0x85, F23 = 0x86, F24 = 0x87,
    NumLock = 0x90, ScrollLock = 0x91, LShift = 0xA0, RShift = 0xA1,
    LControl = 0xA2, RControl = 0xA3, LAlt = 0xA4, RAlt = 0xA5,
    BrowserBack = 0xA6, BrowserForward = 0xA7, BrowserRefresh = 0xA8,
    BrowserStop = 0xA9, BrowserSearch = 0xAA, BrowserFavorites = 0xAB,
    BrowserHome = 0xAC, VolumeMute = 0xAD, VolumeDown = 0xAE,
    VolumeUp = 0xAF, MediaNext = 0xB0, MediaPrev = 0xB1,
    MediaStop = 0xB2, MediaPlayPause = 0xB3, LaunchMail = 0xB4,
    LaunchMedia = 0xB5, LaunchApp1 = 0xB6, LaunchApp2 = 0xB7,
    Oem1 = 0xBA, OemPlus = 0xBB, OemComma = 0xBC, OemMinus = 0xBD,
    OemPeriod = 0xBE, Oem2 = 0xBF, Oem3 = 0xC0, Oem4 = 0xDB,
    Oem5 = 0xDC, Oem6 = 0xDD, Oem7 = 0xDE, Oem8 = 0xDF,
    Oem102 = 0xE2, ProcessKey = 0xE5, Packet = 0xE7, Attn = 0xF6,
    CrSel = 0xF7, ExSel = 0xF8, ErEof = 0xF9, Play = 0xFA,
    Zoom = 0xFB, NoName = 0xFC, Pa1 = 0xFD, OemClear = 0xFE
};

inline const char* KeyCodeToString(KeyCode key)
{
    switch (key)
    {
    case KeyCode::None: return "None";
    case KeyCode::LButton: return "LBUTTON";
    case KeyCode::RButton: return "RBUTTON";
    case KeyCode::Cancel: return "CANCEL";
    case KeyCode::MButton: return "MBUTTON";
    case KeyCode::XButton1: return "XBUTTON1";
    case KeyCode::XButton2: return "XBUTTON2";
    case KeyCode::Backspace: return "BACK";
    case KeyCode::Tab: return "TAB";
    case KeyCode::Clear: return "CLEAR";
    case KeyCode::Enter: return "ENTER";
    case KeyCode::Shift: return "SHIFT";
    case KeyCode::Control: return "CTRL";
    case KeyCode::Alt: return "ALT";
    case KeyCode::Pause: return "PAUSE";
    case KeyCode::CapsLock: return "CAPSLOCK";
    case KeyCode::Escape: return "ESC";
    case KeyCode::Space: return "SPACE";
    case KeyCode::PageUp: return "PAGEUP";
    case KeyCode::PageDown: return "PAGEDOWN";
    case KeyCode::End: return "END";
    case KeyCode::Home: return "HOME";
    case KeyCode::Left: return "LEFT";
    case KeyCode::Up: return "UP";
    case KeyCode::Right: return "RIGHT";
    case KeyCode::Down: return "DOWN";
    case KeyCode::Select: return "SELECT";
    case KeyCode::Print: return "PRINT";
    case KeyCode::Execute: return "EXECUTE";
    case KeyCode::Snapshot: return "SNAPSHOT";
    case KeyCode::Insert: return "INSERT";
    case KeyCode::Delete: return "DELETE";
    case KeyCode::Help: return "HELP";
    case KeyCode::Key0: return "0";
    case KeyCode::Key1: return "1";
    case KeyCode::Key2: return "2";
    case KeyCode::Key3: return "3";
    case KeyCode::Key4: return "4";
    case KeyCode::Key5: return "5";
    case KeyCode::Key6: return "6";
    case KeyCode::Key7: return "7";
    case KeyCode::Key8: return "8";
    case KeyCode::Key9: return "9";
    case KeyCode::A: return "A";
    case KeyCode::B: return "B";
    case KeyCode::C: return "C";
    case KeyCode::D: return "D";
    case KeyCode::E: return "E";
    case KeyCode::F: return "F";
    case KeyCode::G: return "G";
    case KeyCode::H: return "H";
    case KeyCode::I: return "I";
    case KeyCode::J: return "J";
    case KeyCode::K: return "K";
    case KeyCode::L: return "L";
    case KeyCode::M: return "M";
    case KeyCode::N: return "N";
    case KeyCode::O: return "O";
    case KeyCode::P: return "P";
    case KeyCode::Q: return "Q";
    case KeyCode::R: return "R";
    case KeyCode::S: return "S";
    case KeyCode::T: return "T";
    case KeyCode::U: return "U";
    case KeyCode::V: return "V";
    case KeyCode::W: return "W";
    case KeyCode::X: return "X";
    case KeyCode::Y: return "Y";
    case KeyCode::Z: return "Z";
    case KeyCode::LWin: return "LWIN";
    case KeyCode::RWin: return "RWIN";
    case KeyCode::Apps: return "APPS";
    case KeyCode::Sleep: return "SLEEP";
    case KeyCode::NumPad0: return "NUMPAD0";
    case KeyCode::NumPad1: return "NUMPAD1";
    case KeyCode::NumPad2: return "NUMPAD2";
    case KeyCode::NumPad3: return "NUMPAD3";
    case KeyCode::NumPad4: return "NUMPAD4";
    case KeyCode::NumPad5: return "NUMPAD5";
    case KeyCode::NumPad6: return "NUMPAD6";
    case KeyCode::NumPad7: return "NUMPAD7";
    case KeyCode::NumPad8: return "NUMPAD8";
    case KeyCode::NumPad9: return "NUMPAD9";
    case KeyCode::Multiply: return "MULTIPLY";
    case KeyCode::Add: return "ADD";
    case KeyCode::Separator: return "SEPARATOR";
    case KeyCode::Subtract: return "SUBTRACT";
    case KeyCode::Decimal: return "DECIMAL";
    case KeyCode::Divide: return "DIVIDE";
    case KeyCode::F1: return "F1";
    case KeyCode::F2: return "F2";
    case KeyCode::F3: return "F3";
    case KeyCode::F4: return "F4";
    case KeyCode::F5: return "F5";
    case KeyCode::F6: return "F6";
    case KeyCode::F7: return "F7";
    case KeyCode::F8: return "F8";
    case KeyCode::F9: return "F9";
    case KeyCode::F10: return "F10";
    case KeyCode::F11: return "F11";
    case KeyCode::F12: return "F12";
    case KeyCode::F13: return "F13";
    case KeyCode::F14: return "F14";
    case KeyCode::F15: return "F15";
    case KeyCode::F16: return "F16";
    case KeyCode::F17: return "F17";
    case KeyCode::F18: return "F18";
    case KeyCode::F19: return "F19";
    case KeyCode::F20: return "F20";
    case KeyCode::F21: return "F21";
    case KeyCode::F22: return "F22";
    case KeyCode::F23: return "F23";
    case KeyCode::F24: return "F24";
    case KeyCode::NumLock: return "NUMLOCK";
    case KeyCode::ScrollLock: return "SCROLL";
    case KeyCode::LShift: return "LSHIFT";
    case KeyCode::RShift: return "RSHIFT";
    case KeyCode::LControl: return "LCTRL";
    case KeyCode::RControl: return "RCTRL";
    case KeyCode::LAlt: return "LMENU";
    case KeyCode::RAlt: return "RMENU";
    case KeyCode::Oem1: return "OEM_1";
    case KeyCode::OemPlus: return "OEM_PLUS";
    case KeyCode::OemComma: return "OEM_COMMA";
    case KeyCode::OemMinus: return "OEM_MINUS";
    case KeyCode::OemPeriod: return "OEM_PERIOD";
    case KeyCode::Oem2: return "OEM_2";
    case KeyCode::Oem3: return "OEM_3";
    case KeyCode::Oem4: return "OEM_4";
    case KeyCode::Oem5: return "OEM_5";
    case KeyCode::Oem6: return "OEM_6";
    case KeyCode::Oem7: return "OEM_7";
    case KeyCode::Oem102: return "OEM_102";
    default: return "Unknown";
    }
}

inline KeyCode StringToKeyCode(const std::string& str)
{
    if (str.empty())
        return KeyCode::None;

    std::string upper;
    upper.reserve(str.size());
    for (char c : str)
        upper += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    if (upper == "NONE" || upper == "UNKNOWN") return KeyCode::None;
    if (upper == "CTRL" || upper == "CONTROL") return KeyCode::Control;
    if (upper == "SHIFT") return KeyCode::Shift;
    if (upper == "ALT" || upper == "MENU") return KeyCode::Alt;
    if (upper == "RETURN") return KeyCode::Enter;
    if (upper == "ESCAPE") return KeyCode::Escape;
    if (upper == "BACKSPACE") return KeyCode::Backspace;
    if (upper == "CAPS") return KeyCode::CapsLock;

    for (uint32_t val = 1; val < 256; ++val)
    {
        KeyCode code = static_cast<KeyCode>(val);
        const char* name = KeyCodeToString(code);
        if (name && std::string(name) != "Unknown" && upper == name)
            return code;
    }
    return KeyCode::None;
}

typedef bool (*GetIsMenuOpenFunc)();
typedef RBLX::IInstance* (*GetGameFunc)();
typedef uint64_t (*GetPlaceIdFunc)();
typedef uint64_t (*GetGameIdFunc)();
typedef const char* (*GetJobIdFunc)();
typedef const char* (*GetServerIPFunc)();
typedef RBLX::IInstance* (*GetLocalPlayerFunc)();
typedef RBLX::IInstance* (*GetLocalCharacterFunc)();
typedef std::string (*DecompileFunc)(const std::vector<uint8_t>& bytecode);
typedef std::string (*DecompileAddressFunc)(uintptr_t address);
typedef std::string (*GetHttpRawTextFunc)(const std::string& url, const std::string& headers);
typedef std::string (*GetHttpsTextFunc)(const std::wstring& url, const std::wstring& headers);
typedef std::string (*PostHttpsTextFunc)(const std::wstring& url, const std::string& body);
typedef bool (*IsKeyDownFunc)(KeyCode key, bool ignoreForeground);
typedef bool (*WasKeyDownFunc)(KeyCode key, bool ignoreForeground);
typedef bool (*KeyPressedFunc)(KeyCode key, bool ignoreForeground);
typedef bool (*KeyReleasedFunc)(KeyCode key, bool ignoreForeground);
typedef KeyCode (*GetKeyPressedFunc)();
typedef void (*SimulateKeyFunc)(KeyCode key);
typedef void (*SimulateKeyPressAsyncFunc)(KeyCode key, int pressTime);

enum class MouseButton : uint32_t
{
    Left, Right, Middle
};

typedef void (*SimulateMouseMoveFunc)(int x, int y);
typedef void (*SimulateMouseMoveRelativeFunc)(int dx, int dy);
typedef void (*SimulateMouseButtonFunc)(MouseButton button);
typedef void (*SimulateMouseHoldFunc)(MouseButton button, int holdTime);
typedef bool (*ReadRawFunc)(uintptr_t address, void* buffer, size_t size);
typedef bool (*WriteBytesFunc)(uintptr_t address, const void* buffer, size_t size);
typedef std::string (*ReadStringFunc)(uintptr_t address);
typedef void (*WriteStringFunc)(uintptr_t address, const std::string& value);

using FFlagValue = std::variant<std::monostate, bool, int32_t, double, std::string>;
typedef bool (*SetFFlagFunc)(const std::string& fflag, const FFlagValue& value);
typedef FFlagValue (*GetFFlagFunc)(const std::string& fflag);
typedef bool (*WorldToScreenFunc)(const Vector3& worldPos, Vector2& outScreenPos, float& outDepth);
typedef size_t (*WorldToScreenBatchFunc)(const Vector3* worldPoints, Vector2* outScreenPos, float* outDepths, size_t count);
typedef Vector3 (*GetCameraRelativeDirectionFunc)();
typedef void (*AimCameraAtFunc)(uintptr_t cameraAddress, uintptr_t targetAddress, float smoothSpeed);
typedef bool (*GetMousePositionFunc)(Vector2& outPosition);
typedef bool (*GetWindowSizeFunc)(Vector2& outSize);
typedef void (*PluginLogFunc)(const char* message);
typedef void (*PluginNotifyFunc)(const char* message, float messageLifetime);
typedef bool (*PlayAudioFileFunc)(const std::string& filePath);
typedef bool (*PlayAudioMemoryFunc)(const std::vector<uint8_t>& audioData);
typedef void (*StopAudioFunc)();

struct HamsternalGlobals;

class IDrawingCanvas
{
public:
    virtual ~IDrawingCanvas() = default;

    virtual void DrawLine(Vector2 start, Vector2 end, Color4 color, float thickness = 1.0f) = 0;
    virtual void DrawRect(Vector2 pos, Vector2 size, Color4 color, float thickness = 1.0f) = 0;
    virtual void DrawFilledRect(Vector2 pos, Vector2 size, Color4 color) = 0;
    virtual void DrawCircle(Vector2 center, float radius, Color4 color, float thickness = 1.0f, int segments = 32) = 0;
    virtual void DrawFilledCircle(Vector2 center, float radius, Color4 color, int segments = 32) = 0;
    virtual void DrawText(Vector2 pos, const char* text, Color4 color, float fontSize = 14.0f) = 0;
    virtual void DrawTriangle(Vector2 p1, Vector2 p2, Vector2 p3, Color4 color, float thickness = 1.0f) = 0;
    virtual void DrawFilledTriangle(Vector2 p1, Vector2 p2, Vector2 p3, Color4 color) = 0;
    virtual void DrawQuad(Vector2 p1, Vector2 p2, Vector2 p3, Vector2 p4, Color4 color, float thickness = 1.0f) = 0;
    virtual void DrawFilledQuad(Vector2 p1, Vector2 p2, Vector2 p3, Vector2 p4, Color4 color) = 0;
    virtual void DrawPolyline(const Vector2* points, int numPoints, Color4 color, bool closed, float thickness = 1.0f) = 0;
    virtual void DrawConvexPolyFilled(const Vector2* points, int numPoints, Color4 color) = 0;
    virtual bool BeginWindow(const char* name, bool* open = nullptr) = 0;
    virtual void EndWindow() = 0;
    virtual bool Button(const char* label, Vector2 size = {0, 0}) = 0;
    virtual bool Checkbox(const char* label, bool* v) = 0;
    virtual bool SliderFloat(const char* label, float* v, float vMin, float vMax) = 0;
    virtual bool SliderInt(const char* label, int* v, int vMin, int vMax) = 0;
    virtual bool ColorEdit4(const char* label, Color4* col) = 0;
    virtual void Label(const char* text) = 0;
    virtual void Label(const char* text, Color4 color) = 0;
    virtual void Separator() = 0;
    virtual bool InputText(const char* label, char* buffer, size_t bufferSize) = 0;
    virtual bool InputInt(const char* label, int* v, int step = 1, int stepFast = 100) = 0;
    virtual bool InputFloat(const char* label, float* v, float step = 0.0f, float stepFast = 0.0f, const char* format = "%.3f") = 0;
    virtual bool InputDouble(const char* label, double* v, double step = 0.0, double stepFast = 0.0, const char* format = "%.6f") = 0;
    virtual bool Keybind(const char* label, KeyCode* key, Vector2 size = {0, 0}) = 0;
    virtual bool Keybind(const char* label, int* key, Vector2 size = {0, 0}) = 0;
    virtual bool Keybind(const char* label, char* buffer, size_t bufferSize, Vector2 size = {0, 0}) = 0;
    virtual bool InputKeybind(const char* label, KeyCode* key, Vector2 size = {0, 0})
    {
        return Keybind(label, key, size);
    }
    virtual bool InputKeybind(const char* label, int* key, Vector2 size = {0, 0})
    {
        return Keybind(label, key, size);
    }
    virtual bool InputKeybind(const char* label, char* buffer, size_t bufferSize, Vector2 size = {0, 0})
    {
        return Keybind(label, buffer, bufferSize, size);
    }
    virtual bool Dropdown(const char* label, int* currentItem, const char* const items[], int itemsCount, int popupMaxHeightInItems = -1) = 0;
    virtual bool BeginTabBar(const char* id) = 0;
    virtual void EndTabBar() = 0;
    virtual bool BeginTabItem(const char* label) = 0;
    virtual void EndTabItem() = 0;
    virtual Vector2 MeasureText(const char* text, float fontSize = 14.0f) const = 0;
    virtual bool DrawImage(const char* path, Vector2 pos, Vector2 size, Color4 tint = Color4(1.0f, 1.0f, 1.0f, 1.0f))
    {
        return false;
    }
    virtual bool DrawImageFromMemory(const void* data, size_t dataSize, Vector2 pos, Vector2 size, Color4 tint = Color4(1.0f, 1.0f, 1.0f, 1.0f))
    {
        return false;
    }
    virtual void ReleaseImageFromMemory(const void* data, size_t dataSize) = 0;
    virtual bool BeginPanel(const char* id, const char* title = nullptr)
    {
        return true;
    }
    virtual void EndPanel() = 0;
    virtual void Indent(float = 0.0f) = 0;
    virtual void Unindent(float = 0.0f) = 0;
    virtual void SameLine(float = 0.0f, float = -1.0f) = 0;
    virtual bool CollapsingHeader(const char* label, bool defaultOpen = false) = 0;
    virtual bool BeginScrollablePanel(const char* id, Vector2 size = {0, 0}, bool border = true) = 0;
    virtual void EndScrollablePanel() = 0;
    virtual bool TreeNode(const char* label, bool selected = false, bool leaf = false, bool defaultOpen = false, bool* clicked = nullptr) = 0;
    virtual void TreePop() = 0;
    virtual bool DrawInlineImageFromMemory(const void*, size_t, Vector2)
    {
        return false;
    }
};

struct HamsternalGlobals
{
    GetIsMenuOpenFunc IsHamsternalMenuOpen;
    GetGameFunc GetGame;
    GetLocalPlayerFunc GetLocalPlayer;
    GetLocalCharacterFunc GetLocalCharacter;
    IDrawingCanvas* Canvas;
    DecompileFunc Decompile;
    DecompileAddressFunc DecompileAddress;
    GetHttpRawTextFunc GetHttpRawText;
    GetHttpsTextFunc GetHttpsText;
    PostHttpsTextFunc PostHttpsText;
    IsKeyDownFunc IsKeyDown;
    WasKeyDownFunc WasKeyDown;
    KeyPressedFunc KeyPressed;
    KeyReleasedFunc KeyReleased;
    GetKeyPressedFunc GetKeyPressed;
    SimulateKeyFunc SimulateKeyDown;
    SimulateKeyFunc SimulateKeyUp;
    SimulateKeyPressAsyncFunc SimulateKeyPressAsync;
    SimulateMouseMoveFunc SimulateMouseMove;
    SimulateMouseMoveRelativeFunc SimulateMouseMoveRelative;
    SimulateMouseButtonFunc SimulateMouseClick;
    SimulateMouseButtonFunc SimulateMouseButtonDown;
    SimulateMouseButtonFunc SimulateMouseButtonUp;
    SimulateMouseHoldFunc SimulateMouseHold;
    ReadRawFunc ReadRaw;
    WriteBytesFunc WriteBytes;
    ReadStringFunc ReadString;
    WriteStringFunc WriteString;
    SetFFlagFunc SetFFlag;
    GetFFlagFunc GetFFlag;
    WorldToScreenFunc WorldToScreen;
    WorldToScreenBatchFunc WorldToScreenBatch;
    GetCameraRelativeDirectionFunc GetCameraRelativeDirection;
    PluginLogFunc Log;
    AimCameraAtFunc AimCameraAt;
    GetMousePositionFunc GetMousePosition;
    PluginNotifyFunc Notify;
    PlayAudioFileFunc PlayAudioFile;
    PlayAudioMemoryFunc PlayAudioMemory;
    StopAudioFunc StopAudio;
    GetPlaceIdFunc GetPlaceId;
    GetGameIdFunc GetGameId;
    GetJobIdFunc GetJobId;
    GetServerIPFunc GetServerIP;
    GetWindowSizeFunc GetWindowSize;
};

enum class HamsternalError
{
    Success,
    UnknownError,
    IncompatibleApiVersion
};

struct PluginOptions
{
    PluginOptions() = default;
    bool ReloadOnServerHop = true;
};

struct IPluginSettings
{
    virtual ~IPluginSettings() = default;
    virtual void LoadSettings(const std::string& json)
    {
    }
    virtual Settings SaveSettings() const
    {
        return {};
    }
};

class IPlugin : public IPluginSettings
{
protected:
    const HamsternalGlobals* HG = nullptr;

public:
    virtual ~IPlugin() = default;

    virtual PluginOptions GetOptions() const
    {
        return {};
    }

    virtual HamsternalError OnInit()
    {
        return HamsternalError::Success;
    }

    HamsternalError Init(const HamsternalGlobals* globals, const PluginApiVersion& version)
    {
        HG = globals;
        return OnInit();
    }

    virtual void OnExecute()
    {
    }

    virtual void OnCharacterAdded(RBLX::IInstance* character)
    {
    }

    virtual void OnUnload()
    {
    }

    virtual void OnVisualUpdate(IDrawingCanvas* canvas)
    {
    }

    RBLX::IInstance* GetGame() const
    {
        return HG && HG->GetGame ? HG->GetGame() : nullptr;
    }

    uint64_t GetPlaceId() const
    {
        return HG && HG->GetPlaceId ? HG->GetPlaceId() : 0;
    }

    uint64_t GetGameId() const
    {
        return HG && HG->GetGameId ? HG->GetGameId() : 0;
    }

    std::string GetJobId() const
    {
        const char* value = HG && HG->GetJobId ? HG->GetJobId() : nullptr;
        return value ? value : "";
    }

    std::string GetServerIP() const
    {
        const char* value = HG && HG->GetServerIP ? HG->GetServerIP() : nullptr;
        return value ? value : "";
    }

    Vector2 GetWindowSize() const
    {
        Vector2 size;
        if (HG && HG->GetWindowSize)
            HG->GetWindowSize(size);
        return size;
    }

    RBLX::IInstance* GetLocalPlayer() const
    {
        return HG && HG->GetLocalPlayer ? HG->GetLocalPlayer() : nullptr;
    }

    RBLX::IInstance* GetLocalCharacter() const
    {
        return HG && HG->GetLocalCharacter ? HG->GetLocalCharacter() : nullptr;
    }

    void Log(const std::string& message) const
    {
        if (HG && HG->Log)
            HG->Log(message.c_str());
    }

    void Notify(const std::string& message, float messageLifetime = 3.0f) const
    {
        if (HG && HG->Notify)
            HG->Notify(message.c_str(), messageLifetime);
    }

    bool PlayAudioFromFile(const std::string& filePath) const
    {
        return HG && HG->PlayAudioFile ? HG->PlayAudioFile(filePath) : false;
    }

    bool PlayAudioFromMemory(const std::vector<uint8_t>& audioData) const
    {
        return HG && HG->PlayAudioMemory ? HG->PlayAudioMemory(audioData) : false;
    }

    bool PlayAudioFromMemory(const std::string& audioData) const
    {
        return PlayAudioFromMemory(std::vector<uint8_t>(audioData.begin(), audioData.end()));
    }

    void StopAudio() const
    {
        if (HG && HG->StopAudio)
            HG->StopAudio();
    }

    bool IsKeyDown(KeyCode key, bool ignoreForeground = false) const
    {
        return HG && HG->IsKeyDown ? HG->IsKeyDown(key, ignoreForeground) : false;
    }

    bool WasKeyDown(KeyCode key, bool ignoreForeground = false) const
    {
        return HG && HG->WasKeyDown ? HG->WasKeyDown(key, ignoreForeground) : false;
    }

    bool KeyPressed(KeyCode key, bool ignoreForeground = false) const
    {
        return HG && HG->KeyPressed ? HG->KeyPressed(key, ignoreForeground) : false;
    }

    bool KeyReleased(KeyCode key, bool ignoreForeground = false) const
    {
        return HG && HG->KeyReleased ? HG->KeyReleased(key, ignoreForeground) : false;
    }

    KeyCode GetKeyPressed() const
    {
        return HG && HG->GetKeyPressed ? HG->GetKeyPressed() : KeyCode::None;
    }

    void SimulateKeyDown(KeyCode key) const
    {
        if (HG && HG->SimulateKeyDown)
            HG->SimulateKeyDown(key);
    }

    void SimulateKeyUp(KeyCode key) const
    {
        if (HG && HG->SimulateKeyUp)
            HG->SimulateKeyUp(key);
    }

    void SimulateKeyPressAsync(KeyCode key, int pressTime = 10) const
    {
        if (HG && HG->SimulateKeyPressAsync)
            HG->SimulateKeyPressAsync(key, pressTime);
    }

    void SetMousePosition(int x, int y) const
    {
        if (HG && HG->SimulateMouseMove)
            HG->SimulateMouseMove(x, y);
    }

    void SimulateMouseMoveRelative(int dx, int dy) const
    {
        if (HG && HG->SimulateMouseMoveRelative)
            HG->SimulateMouseMoveRelative(dx, dy);
    }

    void SimulateMouseClick(MouseButton button) const
    {
        if (HG && HG->SimulateMouseClick)
            HG->SimulateMouseClick(button);
    }

    void SimulateMouseButtonDown(MouseButton button) const
    {
        if (HG && HG->SimulateMouseButtonDown)
            HG->SimulateMouseButtonDown(button);
    }

    void SimulateMouseButtonUp(MouseButton button) const
    {
        if (HG && HG->SimulateMouseButtonUp)
            HG->SimulateMouseButtonUp(button);
    }

    void SimulateMouseHold(MouseButton button, int holdTime) const
    {
        if (HG && HG->SimulateMouseHold)
            HG->SimulateMouseHold(button, holdTime);
    }

    template <typename T>
    T Read(uintptr_t address) const
    {
        T value{};
        if (HG && HG->ReadRaw)
            HG->ReadRaw(address, &value, sizeof(T));
        return value;
    }

    template <typename T>
    bool Write(uintptr_t address, const T& value) const
    {
        if (HG && HG->WriteBytes)
            return HG->WriteBytes(address, &value, sizeof(T));
        return false;
    }

    bool ReadRaw(uintptr_t address, void* buffer, size_t size) const
    {
        return HG && HG->ReadRaw ? HG->ReadRaw(address, buffer, size) : false;
    }

    bool WriteBytes(uintptr_t address, const void* buffer, size_t size) const
    {
        return HG && HG->WriteBytes ? HG->WriteBytes(address, buffer, size) : false;
    }

    std::string ReadString(uintptr_t address) const
    {
        if (HG && HG->ReadString)
            return HG->ReadString(address);
        return {};
    }

    void WriteString(uintptr_t address, const std::string& value) const
    {
        if (HG && HG->WriteString)
            HG->WriteString(address, value);
    }

    bool SetFFlag(const std::string& fflag, bool value) const
    {
        return HG && HG->SetFFlag ? HG->SetFFlag(fflag, FFlagValue{value}) : false;
    }

    bool SetFFlag(const std::string& fflag, int32_t value) const
    {
        return HG && HG->SetFFlag ? HG->SetFFlag(fflag, FFlagValue{value}) : false;
    }

    bool SetFFlag(const std::string& fflag, double value) const
    {
        return HG && HG->SetFFlag ? HG->SetFFlag(fflag, FFlagValue{value}) : false;
    }

    bool SetFFlag(const std::string& fflag, const std::string& value) const
    {
        return HG && HG->SetFFlag ? HG->SetFFlag(fflag, FFlagValue{value}) : false;
    }

    bool GetFFlag(const std::string& fflag, bool& outValue) const
    {
        if (!HG || !HG->GetFFlag)
            return false;

        const auto value = HG->GetFFlag(fflag);
        if (const auto* typed = std::get_if<bool>(&value))
        {
            outValue = *typed;
            return true;
        }
        return false;
    }

    bool GetFFlag(const std::string& fflag, int32_t& outValue) const
    {
        if (!HG || !HG->GetFFlag)
            return false;

        const auto value = HG->GetFFlag(fflag);
        if (const auto* typed = std::get_if<int32_t>(&value))
        {
            outValue = *typed;
            return true;
        }
        return false;
    }

    bool GetFFlag(const std::string& fflag, double& outValue) const
    {
        if (!HG || !HG->GetFFlag)
            return false;

        const auto value = HG->GetFFlag(fflag);
        if (const auto* typed = std::get_if<double>(&value))
        {
            outValue = *typed;
            return true;
        }
        return false;
    }

    bool GetFFlag(const std::string& fflag, std::string& outValue) const
    {
        if (!HG || !HG->GetFFlag)
            return false;

        const auto value = HG->GetFFlag(fflag);
        if (const auto* typed = std::get_if<std::string>(&value))
        {
            outValue = *typed;
            return true;
        }
        return false;
    }

    std::string Decompile(const std::vector<uint8_t>& bytecode) const
    {
        if (HG && HG->Decompile)
            return HG->Decompile(bytecode);
        return "FAILED_TO_DECOMPILE";
    }

    std::string Decompile(uintptr_t address) const
    {
        if (HG && HG->DecompileAddress)
            return HG->DecompileAddress(address);
        return "FAILED_TO_DECOMPILE";
    }

    std::string GetHttpRawText(const std::string& url, const std::string& headers = "") const
    {
        if (HG && HG->GetHttpRawText)
            return HG->GetHttpRawText(url, headers);
        return "EMPTY_RESPONSE";
    }

    std::string GetHttpsText(const std::wstring& url, const std::wstring& headers = L"") const
    {
        if (HG && HG->GetHttpsText)
            return HG->GetHttpsText(url, headers);
        return "EMPTY_RESPONSE";
    }

    std::string PostHttpsText(const std::wstring& url, const std::string& body) const
    {
        if (HG && HG->PostHttpsText)
            return HG->PostHttpsText(url, body);
        return "EMPTY_RESPONSE";
    }

    bool WorldToScreen(const Vector3& worldPos, Vector2& outScreenPos, float& outDepth) const
    {
        return HG && HG->WorldToScreen ? HG->WorldToScreen(worldPos, outScreenPos, outDepth) : false;
    }

    size_t WorldToScreenBatch(const std::vector<Vector3>& worldPoints, std::vector<Vector2>& outScreenPos, std::vector<float>& outDepths) const
    {
        if (!HG || !HG->WorldToScreenBatch || worldPoints.empty())
            return 0;

        outScreenPos.resize(worldPoints.size());
        outDepths.resize(worldPoints.size());
        return HG->WorldToScreenBatch(worldPoints.data(), outScreenPos.data(), outDepths.data(), worldPoints.size());
    }

    size_t WorldToScreenBatch(const Vector3* worldPoints, Vector2* outScreenPos, float* outDepths, size_t count) const
    {
        return HG && HG->WorldToScreenBatch && worldPoints && outScreenPos && outDepths && count > 0 ? HG->WorldToScreenBatch(worldPoints, outScreenPos, outDepths, count) : 0;
    }

    Vector3 GetCameraRelativeDirection() const
    {
        return HG && HG->GetCameraRelativeDirection ? HG->GetCameraRelativeDirection() : Vector3();
    }
};

namespace RBLX
{
    class IInstance
    {
    public:
        virtual ~IInstance() = default;

        virtual std::vector<std::unique_ptr<IInstance>> GetChildren() = 0;
        virtual std::unique_ptr<IInstance> FindFirstChild(const char* name) = 0;
        virtual std::unique_ptr<IInstance> GetParent() = 0;
        virtual bool SetParent(uintptr_t targetAddress) = 0;
        virtual std::unique_ptr<IInstance> FindFirstChildOfClass(const char* className) = 0;
        virtual uintptr_t GetAddress() const = 0;
        virtual bool IsValid() const = 0;
        virtual void GetName(char* buffer, size_t maxLength) const = 0;
        virtual void GetClass(char* buffer, size_t maxLength) const = 0;
        virtual AttributeValue GetAttribute(const char* name, AttrType type) = 0;
        virtual void SetAttribute(const char* name, AttrType type, const AttributeValue& value) = 0;
        virtual size_t GetAttributeList(size_t index, char* buffer, size_t maxLength) = 0;

        virtual Vector3 GetVelocity() = 0;
        virtual void SetVelocity(Vector3 velocity) = 0;
        virtual void SetPosition(Vector3 position) = 0;
        virtual Vector3 GetPosition() = 0;
        virtual bool GetCanCollide() = 0;
        virtual void SetCanCollide(bool state) = 0;
        virtual bool GetAnchored() = 0;
        virtual float GetTransparency() = 0;
        virtual void SetTransparency(float transparency) = 0;
        virtual CFrame GetCFrame() = 0;
        virtual void SetCFrame(CFrame cf) = 0;
        virtual Color3 GetBasePartColor() = 0;
        virtual void SetBasePartColor(Color3 color) = 0;

        virtual Color3 GetAtmosphereColor() = 0;
        virtual void SetAtmosphereColor(Color3 color) = 0;
        virtual Color3 GetDecay() = 0;
        virtual void SetDecay(Color3 decay) = 0;
        virtual float GetDensity() = 0;
        virtual void SetDensity(float density) = 0;
        virtual float GetGlare() = 0;
        virtual void SetGlare(float glare) = 0;
        virtual float GetHaze() = 0;
        virtual void SetHaze(float haze) = 0;
        virtual float GetOffset() = 0;
        virtual void SetOffset(float offset) = 0;

        virtual Vector3 GetCameraPosition() = 0;
        virtual void SetCameraPosition(Vector3 position) = 0;
        virtual Matrix3 GetCameraRotation() = 0;
        virtual void SetCameraRotation(Matrix3 rotation) = 0;
        virtual std::unique_ptr<IInstance> GetCameraSubject() = 0;
        virtual void SetCameraSubject(uintptr_t targetAddress) = 0;
        virtual int GetCameraTypeID() = 0;
        virtual void SetCameraTypeID(int type) = 0;
        virtual float GetCameraFOV() = 0;
        virtual void SetCameraFOV(float fov) = 0;
        virtual Vector2 GetViewportSize() = 0;
        virtual void SetViewportSize(Vector2 size) = 0;
        virtual float GetImagePlaneDepth() = 0;
        virtual void SetImagePlaneDepth(float depth) = 0;

        virtual float GetHealth() = 0;
        virtual void SetHealth(float health) = 0;
        virtual float GetMaxHealth() = 0;
        virtual void SetMaxHealth(float maxHealth) = 0;
        virtual float GetWalkSpeed() = 0;
        virtual void SetWalkSpeed(float walkSpeed) = 0;
        virtual float GetJumpPower() = 0;
        virtual void SetJumpPower(float jumpPower) = 0;
        virtual float GetJumpHeight() = 0;
        virtual void SetJumpHeight(float jumpHeight) = 0;
        virtual Vector3 GetMoveDirection() = 0;
        virtual Vector3 GetCameraOffset() = 0;
        virtual void SetCameraOffset(Vector3 offset) = 0;
        virtual float GetHealthDisplayDistance() = 0;
        virtual void SetHealthDisplayDistance(float distance) = 0;
        virtual float GetHipHeight() = 0;
        virtual void SetHipHeight(float hipHeight) = 0;
        virtual float GetMaxSlopeAngle() = 0;
        virtual void SetMaxSlopeAngle(float maxSlopeAngle) = 0;
        virtual float GetNameDisplayDistance() = 0;
        virtual void SetNameDisplayDistance(float distance) = 0;
        virtual int GetNameOcclusionID() = 0;
        virtual void SetNameOcclusionID(int id) = 0;
        virtual int GetRigTypeID() = 0;
        virtual bool GetAutoJumpEnabled() = 0;
        virtual void SetAutoJumpEnabled(bool state) = 0;
        virtual bool GetUseJumpPower() = 0;
        virtual void SetUseJumpPower(bool state) = 0;
        virtual bool GetEvaluateStateMachine() = 0;
        virtual void SetEvaluateStateMachine(bool state) = 0;
        virtual bool GetAutoRotate() = 0;
        virtual void SetAutoRotate(bool state) = 0;
        virtual bool GetAutomaticScalingEnabled() = 0;
        virtual void SetAutomaticScalingEnabled(bool state) = 0;
        virtual bool GetBreakJointsOnDeath() = 0;
        virtual void SetBreakJointsOnDeath(bool state) = 0;
        virtual bool GetPlatformStand() = 0;
        virtual void SetPlatformStand(bool state) = 0;
        virtual bool GetIsWalking() = 0;
        virtual int GetFloorMaterialID() = 0;
        virtual void SetFloorMaterialID(int id) = 0;
        virtual int GetHumanoidStateID() = 0;
        virtual void SetHumanoidStateID(int id) = 0;

        virtual std::unique_ptr<IInstance> GetCharacter() = 0;

        virtual uint32_t GetUpdateTypeID() = 0;
        virtual void SetUpdateTypeID(uint32_t id) = 0;
        virtual uint32_t GetSensedMaterialID() = 0;
        virtual void SetSensedMaterialID(uint32_t id) = 0;

        virtual void GetSoundID(char* buffer, size_t maxLength) const = 0;
        virtual void SetSoundID(const char* soundId) = 0;
        virtual float GetSoundPlaybackSpeed() = 0;
        virtual void SetSoundPlaybackSpeed(float speed) = 0;
        virtual float GetSoundRollOffMaxDistance() = 0;
        virtual void SetSoundRollOffMaxDistance(float distance) = 0;
        virtual float GetSoundRollOffMinDistance() = 0;
        virtual void SetSoundRollOffMinDistance(float distance) = 0;
        virtual float GetSoundVolume() = 0;
        virtual void SetSoundVolume(float volume) = 0;
        virtual bool GetSoundLooped() = 0;

        virtual void GetStringValue(char* buffer, size_t maxLength) const = 0;
        virtual void SetStringValue(const char* value) = 0;
        virtual double GetDoubleValue() = 0;
        virtual void SetDoubleValue(double value) = 0;
        virtual int GetIntValue() = 0;
        virtual void SetIntValue(int value) = 0;
        virtual bool GetBoolValue() = 0;
        virtual void SetBoolValue(bool value) = 0;
        virtual double GetDoubleConstrainedValue() = 0;
        virtual void SetDoubleConstrainedValue(double value) = 0;
        virtual double GetDoubleConstrainedMaxValue() = 0;
        virtual void SetDoubleConstrainedMaxValue(double value) = 0;
        virtual double GetDoubleConstrainedMinValue() = 0;
        virtual void SetDoubleConstrainedMinValue(double value) = 0;

        virtual std::vector<uint8_t> GetBytecode(bool decompress = true) = 0;
        virtual size_t Decompile(char* buffer, size_t bufferSize) = 0;

        virtual uint64_t GetUserId() = 0;

        virtual size_t GetGuiText(char* buffer, size_t bufferSize) = 0;

        virtual ColorSequence GetParticleColor() = 0;
        // No visual changes will occur if this is called on a particle emitter that is already emitting particles.
        // Visual changes will be applied after the game updates the particle emitter's properties or reparents it.
        virtual void SetParticleColor(ColorSequence color) = 0;
        virtual float GetParticleBrightness() = 0;
        // No visual changes will occur if this is called on a particle emitter that is already emitting particles.
        // Visual changes will be applied after the game updates the particle emitter's properties or reparents it.
        virtual void SetParticleBrightness(float brightness) = 0;
        virtual float GetParticleLightEmission() = 0;
        // No visual changes will occur if this is called on a particle emitter that is already emitting particles.
        // Visual changes will be applied after the game updates the particle emitter's properties or reparents it.
        virtual void SetParticleLightEmission(float lightEmission) = 0;
        virtual float GetParticleLightInfluence() = 0;
        // No visual changes will occur if this is called on a particle emitter that is already emitting particles.
        // Visual changes will be applied after the game updates the particle emitter's properties or reparents it.
        virtual void SetParticleLightInfluence(float lightInfluence) = 0;
        virtual size_t GetParticleTexture(char* buffer, size_t bufferSize) = 0;
        // No visual changes will occur if this is called on a particle emitter that is already emitting particles.
        // Visual changes will be applied after the game updates the particle emitter's properties or reparents it.
        virtual void SetParticleTexture(const char* texture) = 0;
        virtual float GetParticleZOffset() = 0;
        // No visual changes will occur if this is called on a particle emitter that is already emitting particles.
        // Visual changes will be applied after the game updates the particle emitter's properties or reparents it.
        virtual void SetParticleZOffset(float zOffset) = 0;
        virtual Vector2 GetParticleLifetime() = 0;
        // No visual changes will occur if this is called on a particle emitter that is already emitting particles.
        // Visual changes will be applied after the game updates the particle emitter's properties or reparents it.
        virtual void SetParticleLifetime(Vector2 lifetime) = 0;
        virtual float GetParticleRate() = 0;
        // No visual changes will occur if this is called on a particle emitter that is already emitting particles.
        // Visual changes will be applied after the game updates the particle emitter's properties or reparents it.
        virtual void SetParticleRate(float rate) = 0;
        virtual float GetParticleRotation() = 0;
        // No visual changes will occur if this is called on a particle emitter that is already emitting particles.
        // Visual changes will be applied after the game updates the particle emitter's properties or reparents it.
        virtual void SetParticleRotation(float rotation) = 0;
        virtual float GetParticleRotSpeed() = 0;
        // No visual changes will occur if this is called on a particle emitter that is already emitting particles.
        // Visual changes will be applied after the game updates the particle emitter's properties or reparents it.
        virtual void SetParticleRotSpeed(float rotSpeed) = 0;
        virtual float GetParticleSpeed() = 0;
        // No visual changes will occur if this is called on a particle emitter that is already emitting particles.
        // Visual changes will be applied after the game updates the particle emitter's properties or reparents it.
        virtual void SetParticleSpeed(float speed) = 0;
        virtual Vector2 GetParticleSpreadAngle() = 0;
        // No visual changes will occur if this is called on a particle emitter that is already emitting particles.
        // Visual changes will be applied after the game updates the particle emitter's properties or reparents it.
        virtual void SetParticleSpreadAngle(Vector2 spreadAngle) = 0;
        virtual Vector3 GetParticleAcceleration() = 0;
        // No visual changes will occur if this is called on a particle emitter that is already emitting particles.
        // Visual changes will be applied after the game updates the particle emitter's properties or reparents it.
        virtual void SetParticleAcceleration(Vector3 acceleration) = 0;
        virtual float GetParticleDrag() = 0;
        // No visual changes will occur if this is called on a particle emitter that is already emitting particles.
        // Visual changes will be applied after the game updates the particle emitter's properties or reparents it.
        virtual void SetParticleDrag(float drag) = 0;
        virtual float GetParticleTimeScale() = 0;
        // No visual changes will occur if this is called on a particle emitter that is already emitting particles.
        // Visual changes will be applied after the game updates the particle emitter's properties or reparents it.
        virtual void SetParticleTimeScale(float timeScale) = 0;
        virtual float GetParticleVelocityInheritance() = 0;
        // No visual changes will occur if this is called on a particle emitter that is already emitting particles.
        // Visual changes will be applied after the game updates the particle emitter's properties or reparents it.
        virtual void SetParticleVelocityInheritance(float velocityInheritance) = 0;

        virtual Vector2 GetAbsolutePosition() = 0;
        virtual Vector2 GetAbsoluteSize() = 0;
        virtual float GetAbsoluteRotation() = 0;
        virtual std::vector<::AnimationTrack> GetActiveAnimationTracks() = 0;
        virtual size_t GetAnimationId(char* buffer, size_t bufferSize) = 0;
        virtual void SetAnimationId(const char* animationId) = 0;
        virtual bool SetName(const char* newName) = 0;
    };
}

namespace Wrapper
{
    using namespace RBLX;

    class Instance
    {
    private:
        IInstance* m_ptr = nullptr;
        std::unique_ptr<IInstance> m_owner;

        template<typename Getter>
        static std::string ReadBoundaryString(Getter getter)
        {
            const size_t length = getter(nullptr, 0);
            std::string value(length + 1, '\0');
            if (length != 0)
                getter(value.data(), value.size());
            value.resize(length);
            return value;
        }

        template<typename T>
        static constexpr AttrType GetAttributeType()
        {
            if constexpr (std::is_same_v<T, std::string>) return AttrType::String;
            else if constexpr (std::is_same_v<T, bool>) return AttrType::Bool;
            else if constexpr (std::is_same_v<T, int>) return AttrType::Int;
            else if constexpr (std::is_same_v<T, double>) return AttrType::Double;
            else if constexpr (std::is_same_v<T, Vector2>) return AttrType::Vector2;
            else if constexpr (std::is_same_v<T, Vector3>) return AttrType::Vector3;
            else if constexpr (std::is_same_v<T, Color4>) return AttrType::Color4;
            else return AttrType::Color3;
        }

    public:
        Instance() = default;

        explicit Instance(IInstance* ptr) : m_ptr(ptr)
        {
        }

        explicit Instance(std::unique_ptr<IInstance>&& ptr) : m_ptr(ptr.get()), m_owner(std::move(ptr))
        {
        }

        Instance(Instance&& other) noexcept : m_ptr(other.m_ptr), m_owner(std::move(other.m_owner))
        {
            other.m_ptr = nullptr;
        }

        Instance& operator=(Instance&& other) noexcept
        {
            if (this != &other)
            {
                m_owner = std::move(other.m_owner);
                m_ptr = other.m_ptr;
                other.m_ptr = nullptr;
            }
            return *this;
        }

        Instance(const Instance&) = delete;
        Instance& operator=(const Instance&) = delete;

        std::vector<Instance> GetChildren() const
        {
            if (!m_ptr)
                return {};

            auto rawChildren = m_ptr->GetChildren();
            std::vector<Instance> wrappedChildren;
            wrappedChildren.reserve(rawChildren.size());

            for (auto& child : rawChildren)
                wrappedChildren.emplace_back(std::move(child));

            return wrappedChildren;
        }

        std::vector<::AnimationTrack> GetActiveAnimationTracks() const
        {
            return m_ptr ? m_ptr->GetActiveAnimationTracks() : std::vector<::AnimationTrack>{};
        }

        std::string GetAnimationId() const
        {
            return m_ptr
                ? ReadBoundaryString([this](char* buffer, size_t size) {
                    return m_ptr->GetAnimationId(buffer, size);
                })
                : std::string{};
        }

        void SetAnimationId(const std::string& animationId)
        {
            if (m_ptr)
                m_ptr->SetAnimationId(animationId.c_str());
        }

        std::vector<std::string> GetAttributeList()
        {
            std::vector<std::string> names;
            if (!m_ptr)
                return names;

            const size_t count = m_ptr->GetAttributeList(static_cast<size_t>(-1), nullptr, 0);
            names.reserve(count);
            for (size_t index = 0; index < count; ++index)
            {
                const size_t length = m_ptr->GetAttributeList(index, nullptr, 0);
                std::string name(length + 1, '\0');
                if (length != 0)
                    m_ptr->GetAttributeList(index, name.data(), name.size());
                name.resize(length);
                names.push_back(std::move(name));
            }
            return names;
        }

        Instance FindFirstChild(std::string name) const
        {
            if (!m_ptr)
                return Instance();
            auto child = m_ptr->FindFirstChild(name.c_str());
            return child ? Instance(std::move(child)) : Instance();
        }

        Instance FindFirstChildOfClass(std::string className) const
        {
            if (!m_ptr)
                return Instance();
            auto child = m_ptr->FindFirstChildOfClass(className.c_str());
            return child ? Instance(std::move(child)) : Instance();
        }

        Instance GetService(std::string name) const
        {
            if (!m_ptr)
                return Instance();
            auto service = m_ptr->FindFirstChildOfClass(name.c_str());
            return service ? Instance(std::move(service)) : Instance();
        }

        std::string GetName() const
        {
            if (!m_ptr)
                return "NO_NAME";
            char buf[256];
            m_ptr->GetName(buf, sizeof(buf));
            return std::string(buf);
        }

        bool SetName(const std::string& newName)
        {
            return m_ptr && m_ptr->SetName(newName.c_str());
        }

        std::string GetClass() const
        {
            if (!m_ptr)
                return "NO_CLASS";
            char buf[256];
            m_ptr->GetClass(buf, sizeof(buf));
            return std::string(buf);
        }

        uintptr_t GetAddress() const
        {
            return m_ptr ? m_ptr->GetAddress() : 0;
        }

        Instance GetParent()
        {
            if (!m_ptr)
                return Instance();
            auto parent = m_ptr->GetParent();
            return parent ? Instance(std::move(parent)) : Instance();
        }

        bool SetParent(const Instance& target)
        {
            return m_ptr ? m_ptr->SetParent(target.GetAddress()) : false;
        }

        bool SetParent(const Instance* target)
        {
            return (m_ptr && target) ? m_ptr->SetParent(target->GetAddress()) : false;
        }

        bool SetParent(uintptr_t targetAddress)
        {
            return m_ptr ? m_ptr->SetParent(targetAddress) : false;
        }

        bool IsValid() const
        {
            return m_ptr ? m_ptr->IsValid() : false;
        }

        Vector3 GetVelocity()
        {
            return m_ptr ? m_ptr->GetVelocity() : Vector3();
        }

        Vector3 GetPosition()
        {
            return m_ptr ? m_ptr->GetPosition() : Vector3();
        }

        void SetPosition(Vector3 position)
        {
            if (!m_ptr)
                return;
            m_ptr->SetPosition(position);
        }

        void SetVelocity(Vector3 velocity)
        {
            if (!m_ptr)
                return;
            m_ptr->SetVelocity(velocity);
        }

        bool GetCanCollide()
        {
            return m_ptr ? m_ptr->GetCanCollide() : false;
        }

        bool IsAnchored()
        {
            return m_ptr ? m_ptr->GetAnchored() : false;
        }

        bool GetAnchored()
        {
            return m_ptr ? m_ptr->GetAnchored() : false;
        }

        void SetCanCollide(bool state)
        {
            if (!m_ptr)
                return;
            m_ptr->SetCanCollide(state);
        }

        float GetTransparency()
        {
            return m_ptr ? m_ptr->GetTransparency() : 0.0f;
        }

        void SetTransparency(float transparency)
        {
            if (!m_ptr)
                return;
            m_ptr->SetTransparency(transparency);
        }

        CFrame GetCFrame()
        {
            return m_ptr ? m_ptr->GetCFrame() : CFrame();
        }

        void SetCFrame(CFrame cf)
        {
            if (!m_ptr)
                return;
            m_ptr->SetCFrame(cf);
        }

        Color3 GetBasePartColor()
        {
            return m_ptr ? m_ptr->GetBasePartColor() : Color3();
        }

        void SetBasePartColor(Color3 color)
        {
            if (!m_ptr)
                return;
            m_ptr->SetBasePartColor(color);
        }

        Color3 GetAtmosphereColor()
        {
            return m_ptr ? m_ptr->GetAtmosphereColor() : Color3();
        }

        void SetAtmosphereColor(Color3 color)
        {
            if (!m_ptr)
                return;
            m_ptr->SetAtmosphereColor(color);
        }

        Color3 GetDecay()
        {
            return m_ptr ? m_ptr->GetDecay() : Color3();
        }

        void SetDecay(Color3 decay)
        {
            if (!m_ptr)
                return;
            m_ptr->SetDecay(decay);
        }

        float GetDensity()
        {
            return m_ptr ? m_ptr->GetDensity() : 0.0f;
        }

        void SetDensity(float density)
        {
            if (!m_ptr)
                return;
            m_ptr->SetDensity(density);
        }

        float GetGlare()
        {
            return m_ptr ? m_ptr->GetGlare() : 0.0f;
        }

        void SetGlare(float glare)
        {
            if (!m_ptr)
                return;
            m_ptr->SetGlare(glare);
        }

        float GetHaze()
        {
            return m_ptr ? m_ptr->GetHaze() : 0.0f;
        }

        void SetHaze(float haze)
        {
            if (!m_ptr)
                return;
            m_ptr->SetHaze(haze);
        }

        float GetOffset()
        {
            return m_ptr ? m_ptr->GetOffset() : 0.0f;
        }

        void SetOffset(float offset)
        {
            if (!m_ptr)
                return;
            m_ptr->SetOffset(offset);
        }

        Vector3 GetCameraPosition()
        {
            return m_ptr ? m_ptr->GetCameraPosition() : Vector3();
        }

        void SetCameraPosition(Vector3 position)
        {
            if (!m_ptr)
                return;
            m_ptr->SetCameraPosition(position);
        }

        Matrix3 GetCameraRotation()
        {
            return m_ptr ? m_ptr->GetCameraRotation() : Matrix3();
        }

        void SetCameraRotation(Matrix3 rotation)
        {
            if (!m_ptr)
                return;
            m_ptr->SetCameraRotation(rotation);
        }

        Instance GetCameraSubject()
        {
            if (!m_ptr)
                return Instance();
            auto subject = m_ptr->GetCameraSubject();
            return subject ? Instance(std::move(subject)) : Instance();
        }

        void SetCameraSubject(const Instance& target)
        {
            if (!m_ptr)
                return;
            m_ptr->SetCameraSubject(target.GetAddress());
        }

        void SetCameraSubject(const Instance* target)
        {
            if (!m_ptr || !target)
                return;
            m_ptr->SetCameraSubject(target->GetAddress());
        }

        void SetCameraSubject(uintptr_t targetAddress)
        {
            if (!m_ptr)
                return;
            m_ptr->SetCameraSubject(targetAddress);
        }

        int GetCameraTypeID()
        {
            return m_ptr ? m_ptr->GetCameraTypeID() : 0;
        }

        void SetCameraTypeID(int type)
        {
            if (!m_ptr)
                return;
            m_ptr->SetCameraTypeID(type);
        }

        int GetCameraType()
        {
            return GetCameraTypeID();
        }

        void SetCameraType(int type)
        {
            SetCameraTypeID(type);
        }

        float GetCameraFOV()
        {
            return m_ptr ? m_ptr->GetCameraFOV() : 0.0f;
        }

        void SetCameraFOV(float fov)
        {
            if (!m_ptr)
                return;
            m_ptr->SetCameraFOV(fov);
        }

        float GetFieldOfView()
        {
            return GetCameraFOV();
        }

        void SetFieldOfView(float fov)
        {
            SetCameraFOV(fov);
        }

        Vector2 GetViewportSize()
        {
            return m_ptr ? m_ptr->GetViewportSize() : Vector2();
        }

        void SetViewportSize(Vector2 size)
        {
            if (!m_ptr)
                return;
            m_ptr->SetViewportSize(size);
        }

        float GetImagePlaneDepth()
        {
            return m_ptr ? m_ptr->GetImagePlaneDepth() : 0.0f;
        }

        void SetImagePlaneDepth(float depth)
        {
            if (!m_ptr)
                return;
            m_ptr->SetImagePlaneDepth(depth);
        }

        Vector2 GetAbsolutePosition()
        {
            return m_ptr ? m_ptr->GetAbsolutePosition() : Vector2();
        }

        Vector2 GetAbsoluteSize()
        {
            return m_ptr ? m_ptr->GetAbsoluteSize() : Vector2();
        }

        float GetAbsoluteRotation()
        {
            return m_ptr ? m_ptr->GetAbsoluteRotation() : 0.0f;
        }

        float GetHealth()
        {
            return m_ptr ? m_ptr->GetHealth() : 0.0f;
        }

        void SetHealth(float health)
        {
            if (!m_ptr)
                return;
            m_ptr->SetHealth(health);
        }

        float GetMaxHealth()
        {
            return m_ptr ? m_ptr->GetMaxHealth() : 0.0f;
        }

        void SetMaxHealth(float maxHealth)
        {
            if (!m_ptr)
                return;
            m_ptr->SetMaxHealth(maxHealth);
        }

        float GetWalkSpeed()
        {
            return m_ptr ? m_ptr->GetWalkSpeed() : 0.0f;
        }

        void SetWalkSpeed(float walkSpeed)
        {
            if (!m_ptr)
                return;
            m_ptr->SetWalkSpeed(walkSpeed);
        }

        float GetJumpPower()
        {
            return m_ptr ? m_ptr->GetJumpPower() : 0.0f;
        }

        void SetJumpPower(float jumpPower)
        {
            if (!m_ptr)
                return;
            m_ptr->SetJumpPower(jumpPower);
        }

        float GetJumpHeight()
        {
            return m_ptr ? m_ptr->GetJumpHeight() : 0.0f;
        }

        void SetJumpHeight(float jumpHeight)
        {
            if (!m_ptr)
                return;
            m_ptr->SetJumpHeight(jumpHeight);
        }

        Vector3 GetMoveDirection()
        {
            return m_ptr ? m_ptr->GetMoveDirection() : Vector3();
        }

        Vector3 GetCameraOffset()
        {
            return m_ptr ? m_ptr->GetCameraOffset() : Vector3();
        }

        void SetCameraOffset(Vector3 offset)
        {
            if (!m_ptr)
                return;
            m_ptr->SetCameraOffset(offset);
        }

        float GetHealthDisplayDistance()
        {
            return m_ptr ? m_ptr->GetHealthDisplayDistance() : 0.0f;
        }

        void SetHealthDisplayDistance(float distance)
        {
            if (!m_ptr)
                return;
            m_ptr->SetHealthDisplayDistance(distance);
        }

        float GetHipHeight()
        {
            return m_ptr ? m_ptr->GetHipHeight() : 0.0f;
        }

        void SetHipHeight(float hipHeight)
        {
            if (!m_ptr)
                return;
            m_ptr->SetHipHeight(hipHeight);
        }

        float GetMaxSlopeAngle()
        {
            return m_ptr ? m_ptr->GetMaxSlopeAngle() : 0.0f;
        }

        void SetMaxSlopeAngle(float maxSlopeAngle)
        {
            if (!m_ptr)
                return;
            m_ptr->SetMaxSlopeAngle(maxSlopeAngle);
        }

        float GetNameDisplayDistance()
        {
            return m_ptr ? m_ptr->GetNameDisplayDistance() : 0.0f;
        }

        void SetNameDisplayDistance(float distance)
        {
            if (!m_ptr)
                return;
            m_ptr->SetNameDisplayDistance(distance);
        }

        int GetNameOcclusionID()
        {
            return m_ptr ? m_ptr->GetNameOcclusionID() : 0;
        }

        void SetNameOcclusionID(int id)
        {
            if (!m_ptr)
                return;
            m_ptr->SetNameOcclusionID(id);
        }

        int GetRigTypeID()
        {
            return m_ptr ? m_ptr->GetRigTypeID() : 0;
        }

        bool GetAutoJumpEnabled()
        {
            return m_ptr ? m_ptr->GetAutoJumpEnabled() : false;
        }

        void SetAutoJumpEnabled(bool state)
        {
            if (!m_ptr)
                return;
            m_ptr->SetAutoJumpEnabled(state);
        }

        bool GetUseJumpPower()
        {
            return m_ptr ? m_ptr->GetUseJumpPower() : false;
        }

        void SetUseJumpPower(bool state)
        {
            if (!m_ptr)
                return;
            m_ptr->SetUseJumpPower(state);
        }

        bool GetEvaluateStateMachine()
        {
            return m_ptr ? m_ptr->GetEvaluateStateMachine() : false;
        }

        void SetEvaluateStateMachine(bool state)
        {
            if (!m_ptr)
                return;
            m_ptr->SetEvaluateStateMachine(state);
        }

        bool GetAutoRotate()
        {
            return m_ptr ? m_ptr->GetAutoRotate() : false;
        }

        void SetAutoRotate(bool state)
        {
            if (!m_ptr)
                return;
            m_ptr->SetAutoRotate(state);
        }

        bool GetAutomaticScalingEnabled()
        {
            return m_ptr ? m_ptr->GetAutomaticScalingEnabled() : false;
        }

        void SetAutomaticScalingEnabled(bool state)
        {
            if (!m_ptr)
                return;
            m_ptr->SetAutomaticScalingEnabled(state);
        }

        bool GetBreakJointsOnDeath()
        {
            return m_ptr ? m_ptr->GetBreakJointsOnDeath() : false;
        }

        void SetBreakJointsOnDeath(bool state)
        {
            if (!m_ptr)
                return;
            m_ptr->SetBreakJointsOnDeath(state);
        }

        bool GetPlatformStand()
        {
            return m_ptr ? m_ptr->GetPlatformStand() : false;
        }

        void SetPlatformStand(bool state)
        {
            if (!m_ptr)
                return;
            m_ptr->SetPlatformStand(state);
        }

        bool GetIsWalking()
        {
            return m_ptr ? m_ptr->GetIsWalking() : false;
        }

        int GetFloorMaterialID()
        {
            return m_ptr ? m_ptr->GetFloorMaterialID() : 0;
        }

        void SetFloorMaterialID(int id)
        {
            if (!m_ptr)
                return;
            m_ptr->SetFloorMaterialID(id);
        }

        int GetHumanoidStateID()
        {
            return m_ptr ? m_ptr->GetHumanoidStateID() : 0;
        }

        void SetHumanoidStateID(int id)
        {
            if (!m_ptr)
                return;
            m_ptr->SetHumanoidStateID(id);
        }

        Instance GetCharacter()
        {
            if (!m_ptr)
                return Instance();
            auto charInst = m_ptr->GetCharacter();
            return charInst ? Instance(std::move(charInst)) : Instance();
        }

        uint64_t GetUserId()
        {
            return m_ptr ? m_ptr->GetUserId() : 0;
        }

        uint32_t GetUpdateTypeID()
        {
            return m_ptr ? m_ptr->GetUpdateTypeID() : 0;
        }

        void SetUpdateTypeID(uint32_t id)
        {
            if (!m_ptr)
                return;
            m_ptr->SetUpdateTypeID(id);
        }

        uint32_t GetSensedMaterialID()
        {
            return m_ptr ? m_ptr->GetSensedMaterialID() : 0;
        }

        void SetSensedMaterialID(uint32_t id)
        {
            if (!m_ptr)
                return;
            m_ptr->SetSensedMaterialID(id);
        }

        std::string GetSoundID() const
        {
            if (!m_ptr)
                return "";
            char buf[512];
            m_ptr->GetSoundID(buf, sizeof(buf));
            return std::string(buf);
        }

        void SetSoundID(const std::string& soundId)
        {
            if (!m_ptr)
                return;
            m_ptr->SetSoundID(soundId.c_str());
        }

        float GetSoundPlaybackSpeed()
        {
            return m_ptr ? m_ptr->GetSoundPlaybackSpeed() : 0.0f;
        }

        void SetSoundPlaybackSpeed(float speed)
        {
            if (!m_ptr)
                return;
            m_ptr->SetSoundPlaybackSpeed(speed);
        }

        float GetPlaybackSpeed()
        {
            return GetSoundPlaybackSpeed();
        }

        void SetPlaybackSpeed(float speed)
        {
            SetSoundPlaybackSpeed(speed);
        }

        float GetSoundRollOffMaxDistance()
        {
            return m_ptr ? m_ptr->GetSoundRollOffMaxDistance() : 0.0f;
        }

        void SetSoundRollOffMaxDistance(float distance)
        {
            if (!m_ptr)
                return;
            m_ptr->SetSoundRollOffMaxDistance(distance);
        }

        float GetRollOffMaxDistance()
        {
            return GetSoundRollOffMaxDistance();
        }

        void SetRollOffMaxDistance(float distance)
        {
            SetSoundRollOffMaxDistance(distance);
        }

        float GetSoundRollOffMinDistance()
        {
            return m_ptr ? m_ptr->GetSoundRollOffMinDistance() : 0.0f;
        }

        void SetSoundRollOffMinDistance(float distance)
        {
            if (!m_ptr)
                return;
            m_ptr->SetSoundRollOffMinDistance(distance);
        }

        float GetRollOffMinDistance()
        {
            return GetSoundRollOffMinDistance();
        }

        void SetRollOffMinDistance(float distance)
        {
            SetSoundRollOffMinDistance(distance);
        }

        float GetSoundVolume()
        {
            return m_ptr ? m_ptr->GetSoundVolume() : 0.0f;
        }

        void SetSoundVolume(float volume)
        {
            if (!m_ptr)
                return;
            m_ptr->SetSoundVolume(volume);
        }

        float GetVolume()
        {
            return GetSoundVolume();
        }

        void SetVolume(float volume)
        {
            SetSoundVolume(volume);
        }

        bool GetSoundLooped()
        {
            return m_ptr ? m_ptr->GetSoundLooped() : false;
        }

        std::string GetStringValue() const
        {
            if (!m_ptr)
                return "";
            char buf[512];
            m_ptr->GetStringValue(buf, sizeof(buf));
            return std::string(buf);
        }

        std::string GetGuiText() const
        {
            return m_ptr
                ? ReadBoundaryString([this](char* buffer, size_t size) {
                    return m_ptr->GetGuiText(buffer, size);
                })
                : std::string{};
        }

        void SetStringValue(const std::string& value)
        {
            if (!m_ptr)
                return;
            m_ptr->SetStringValue(value.c_str());
        }

        double GetDoubleValue()
        {
            return m_ptr ? m_ptr->GetDoubleValue() : 0.0;
        }

        void SetDoubleValue(double value)
        {
            if (!m_ptr)
                return;
            m_ptr->SetDoubleValue(value);
        }

        int GetIntValue()
        {
            return m_ptr ? m_ptr->GetIntValue() : 0;
        }

        void SetIntValue(int value)
        {
            if (!m_ptr)
                return;
            m_ptr->SetIntValue(value);
        }

        bool GetBoolValue()
        {
            return m_ptr ? m_ptr->GetBoolValue() : false;
        }

        void SetBoolValue(bool value)
        {
            if (!m_ptr)
                return;
            m_ptr->SetBoolValue(value);
        }

        double GetDoubleConstrainedValue()
        {
            return m_ptr ? m_ptr->GetDoubleConstrainedValue() : 0.0;
        }

        void SetDoubleConstrainedValue(double value)
        {
            if (!m_ptr)
                return;
            m_ptr->SetDoubleConstrainedValue(value);
        }

        double GetDoubleConstrainedMaxValue()
        {
            return m_ptr ? m_ptr->GetDoubleConstrainedMaxValue() : 0.0;
        }

        void SetDoubleConstrainedMaxValue(double value)
        {
            if (!m_ptr)
                return;
            m_ptr->SetDoubleConstrainedMaxValue(value);
        }

        double GetDoubleConstrainedMinValue()
        {
            return m_ptr ? m_ptr->GetDoubleConstrainedMinValue() : 0.0;
        }

        void SetDoubleConstrainedMinValue(double value)
        {
            if (!m_ptr)
                return;
            m_ptr->SetDoubleConstrainedMinValue(value);
        }

        std::vector<uint8_t> GetBytecode()
        {
            return m_ptr ? m_ptr->GetBytecode() : std::vector<uint8_t>{};
        }

        std::string Decompile() const
        {
            return m_ptr
                ? ReadBoundaryString([this](char* buffer, size_t size) {
                    return m_ptr->Decompile(buffer, size);
                })
                : std::string{};
        }

        ColorSequence GetParticleColor()
        {
            return m_ptr ? m_ptr->GetParticleColor() : ColorSequence();
        }

        void SetParticleColor(ColorSequence color)
        {
            if (!m_ptr)
                return;
            m_ptr->SetParticleColor(color);
        }

        float GetParticleBrightness()
        {
            return m_ptr ? m_ptr->GetParticleBrightness() : 0.0f;
        }

        void SetParticleBrightness(float brightness)
        {
            if (!m_ptr)
                return;
            m_ptr->SetParticleBrightness(brightness);
        }

        float GetParticleLightEmission()
        {
            return m_ptr ? m_ptr->GetParticleLightEmission() : 0.0f;
        }

        void SetParticleLightEmission(float lightEmission)
        {
            if (!m_ptr)
                return;
            m_ptr->SetParticleLightEmission(lightEmission);
        }

        float GetParticleLightInfluence()
        {
            return m_ptr ? m_ptr->GetParticleLightInfluence() : 0.0f;
        }

        void SetParticleLightInfluence(float lightInfluence)
        {
            if (!m_ptr)
                return;
            m_ptr->SetParticleLightInfluence(lightInfluence);
        }

        std::string GetParticleTexture()
        {
            return m_ptr
                ? ReadBoundaryString([this](char* buffer, size_t size) {
                    return m_ptr->GetParticleTexture(buffer, size);
                })
                : std::string{};
        }

        void SetParticleTexture(const std::string& texture)
        {
            if (!m_ptr)
                return;
            m_ptr->SetParticleTexture(texture.c_str());
        }

        float GetParticleZOffset()
        {
            return m_ptr ? m_ptr->GetParticleZOffset() : 0.0f;
        }

        void SetParticleZOffset(float zOffset)
        {
            if (!m_ptr)
                return;
            m_ptr->SetParticleZOffset(zOffset);
        }

        Vector2 GetParticleLifetime()
        {
            return m_ptr ? m_ptr->GetParticleLifetime() : Vector2();
        }

        void SetParticleLifetime(Vector2 lifetime)
        {
            if (!m_ptr)
                return;
            m_ptr->SetParticleLifetime(lifetime);
        }

        float GetParticleRate()
        {
            return m_ptr ? m_ptr->GetParticleRate() : 0.0f;
        }

        void SetParticleRate(float rate)
        {
            if (!m_ptr)
                return;
            m_ptr->SetParticleRate(rate);
        }

        float GetParticleRotation()
        {
            return m_ptr ? m_ptr->GetParticleRotation() : 0.0f;
        }

        void SetParticleRotation(float rotation)
        {
            if (!m_ptr)
                return;
            m_ptr->SetParticleRotation(rotation);
        }

        float GetParticleRotSpeed()
        {
            return m_ptr ? m_ptr->GetParticleRotSpeed() : 0.0f;
        }

        void SetParticleRotSpeed(float rotSpeed)
        {
            if (!m_ptr)
                return;
            m_ptr->SetParticleRotSpeed(rotSpeed);
        }

        float GetParticleSpeed()
        {
            return m_ptr ? m_ptr->GetParticleSpeed() : 0.0f;
        }

        void SetParticleSpeed(float speed)
        {
            if (!m_ptr)
                return;
            m_ptr->SetParticleSpeed(speed);
        }

        Vector2 GetParticleSpreadAngle()
        {
            return m_ptr ? m_ptr->GetParticleSpreadAngle() : Vector2();
        }

        void SetParticleSpreadAngle(Vector2 spreadAngle)
        {
            if (!m_ptr)
                return;
            m_ptr->SetParticleSpreadAngle(spreadAngle);
        }

        Vector3 GetParticleAcceleration()
        {
            return m_ptr ? m_ptr->GetParticleAcceleration() : Vector3();
        }

        void SetParticleAcceleration(Vector3 acceleration)
        {
            if (!m_ptr)
                return;
            m_ptr->SetParticleAcceleration(acceleration);
        }

        float GetParticleDrag()
        {
            return m_ptr ? m_ptr->GetParticleDrag() : 0.0f;
        }

        void SetParticleDrag(float drag)
        {
            if (!m_ptr)
                return;
            m_ptr->SetParticleDrag(drag);
        }

        float GetParticleTimeScale()
        {
            return m_ptr ? m_ptr->GetParticleTimeScale() : 0.0f;
        }

        void SetParticleTimeScale(float timeScale)
        {
            if (!m_ptr)
                return;
            m_ptr->SetParticleTimeScale(timeScale);
        }

        float GetParticleVelocityInheritance()
        {
            return m_ptr ? m_ptr->GetParticleVelocityInheritance() : 0.0f;
        }

        void SetParticleVelocityInheritance(float velocityInheritance)
        {
            if (!m_ptr)
                return;
            m_ptr->SetParticleVelocityInheritance(velocityInheritance);
        }

        AttrValue GetAttribute(const char* name, AttrType type) const
        {
            if (!m_ptr)
                return std::monostate{};

            if (type == AttrType::String)
            {
                const AttributeValue value = m_ptr->GetAttribute(name, type);
                const auto* stringValue = std::get_if<const char*>(&value);
                if (!stringValue || !*stringValue)
                    return std::monostate{};
                return std::string(*stringValue);
            }

            const AttributeValue value = m_ptr->GetAttribute(name, type);
            return std::visit([](const auto& item) -> AttrValue {
                return AttrValue{item};
            }, value);
        }

        void SetAttribute(const char* name, AttrValue value)
        {
            if (!m_ptr)
                return;

            if (const auto* stringValue = std::get_if<std::string>(&value))
            {
                m_ptr->SetAttribute(
                    name, AttrType::String, AttributeValue{stringValue->c_str()});
                return;
            }

            std::visit([this, name](const auto& item) {
                using T = std::decay_t<decltype(item)>;
                if constexpr (!std::is_same_v<T, std::string> && !std::is_same_v<T, std::monostate>)
                {
                    m_ptr->SetAttribute(
                        name, GetAttributeType<T>(), AttributeValue{item});
                }
            }, value);
        }

        template<typename T>
        T GetAttributeAs(const char* name) const
        {
            if (!m_ptr)
                return T{};

            if constexpr(std::is_same_v<T, std::string>)
            {
                AttrValue val = GetAttribute(name, AttrType::String);
                if (std::holds_alternative<std::string>(val))
                    return std::get<std::string>(val);
            }
            else if constexpr(std::is_same_v<T, bool>)
            {
                AttrValue val = GetAttribute(name, AttrType::Bool);
                if (std::holds_alternative<bool>(val))
                    return std::get<bool>(val);
            }
            else if constexpr(std::is_same_v<T, int>)
            {
                AttrValue val = GetAttribute(name, AttrType::Int);
                if (std::holds_alternative<int>(val))
                    return std::get<int>(val);
            }
            else if constexpr(std::is_same_v<T, double>)
            {
                AttrValue val = GetAttribute(name, AttrType::Double);
                if (std::holds_alternative<double>(val))
                    return std::get<double>(val);
            }
            else if constexpr(std::is_same_v<T, Vector2>)
            {
                AttrValue val = GetAttribute(name, AttrType::Vector2);
                if (std::holds_alternative<Vector2>(val))
                    return std::get<Vector2>(val);
            }
            else if constexpr(std::is_same_v<T, Vector3>)
            {
                AttrValue val = GetAttribute(name, AttrType::Vector3);
                if (std::holds_alternative<Vector3>(val))
                    return std::get<Vector3>(val);
            }
            else if constexpr(std::is_same_v<T, Color4>)
            {
                AttrValue val = GetAttribute(name, AttrType::Color4);
                if (std::holds_alternative<Color4>(val))
                    return std::get<Color4>(val);
            }
            return T{};
        }

        template<typename T>
        void SetAttributeAs(const char* name, const T& value)
        {
            if (!m_ptr)
                return;
            if constexpr (std::is_same_v<std::decay_t<T>, const char*> || std::is_same_v<std::decay_t<T>, char*>)
                SetAttribute(name, AttrValue{std::string(value)});
            else if constexpr (std::is_convertible_v<T, std::string> && !std::is_same_v<std::decay_t<T>, bool>)
                SetAttribute(name, AttrValue{std::string(value)});
            else
                SetAttribute(name, AttrValue{value});
        }

        IInstance* Get() const
        {
            return m_ptr;
        }

        explicit operator bool() const
        {
            return m_ptr != nullptr;
        }
    };
}