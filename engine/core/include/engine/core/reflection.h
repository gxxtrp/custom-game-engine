#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
#include <string>
#include <type_traits>
#include <span>
#include <string_view>

namespace engine::assets {
struct UUID;
}

namespace engine::core {

enum class FieldType : uint8_t {
    Bool,
    Int8,
    Int16,
    Int32,
    Int64,
    UInt8,
    UInt16,
    UInt32,
    UInt64,
    Float,
    Double,
    String,
    Vec2,
    Vec3,
    Vec4,
    Quat,
    Mat4,
    UUID,
    Enum,
    Custom
};

template <typename T>
struct CustomFieldTypeDeducer {
    static constexpr bool is_custom = false;
};

template <typename T>
[[nodiscard]] constexpr FieldType deduce_field_type() noexcept {
    using CleanT = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<CleanT, bool>) return FieldType::Bool;
    else if constexpr (std::is_same_v<CleanT, int8_t>) return FieldType::Int8;
    else if constexpr (std::is_same_v<CleanT, int16_t>) return FieldType::Int16;
    else if constexpr (std::is_same_v<CleanT, int32_t>) return FieldType::Int32;
    else if constexpr (std::is_same_v<CleanT, int64_t>) return FieldType::Int64;
    else if constexpr (std::is_same_v<CleanT, uint8_t>) return FieldType::UInt8;
    else if constexpr (std::is_same_v<CleanT, uint16_t>) return FieldType::UInt16;
    else if constexpr (std::is_same_v<CleanT, uint32_t>) return FieldType::UInt32;
    else if constexpr (std::is_same_v<CleanT, uint64_t>) return FieldType::UInt64;
    else if constexpr (std::is_same_v<CleanT, float>) return FieldType::Float;
    else if constexpr (std::is_same_v<CleanT, double>) return FieldType::Double;
    else if constexpr (std::is_same_v<CleanT, std::string>) return FieldType::String;
    else if constexpr (std::is_same_v<CleanT, core::Vec2>) return FieldType::Vec2;
    else if constexpr (std::is_same_v<CleanT, core::Vec3>) return FieldType::Vec3;
    else if constexpr (std::is_same_v<CleanT, core::Vec4>) return FieldType::Vec4;
    else if constexpr (std::is_same_v<CleanT, core::Quat>) return FieldType::Quat;
    else if constexpr (std::is_same_v<CleanT, core::Mat4>) return FieldType::Mat4;
    else if constexpr (std::is_same_v<CleanT, assets::UUID>) return FieldType::UUID;
    else if constexpr (std::is_enum_v<CleanT>) return FieldType::Enum;
    else if constexpr (CustomFieldTypeDeducer<CleanT>::is_custom) return CustomFieldTypeDeducer<CleanT>::type;
    else return FieldType::Custom;
}

template <typename T>
struct TypeReflector {
    static constexpr bool is_reflected = false;
};

template <typename T>
constexpr bool is_reflected_v = TypeReflector<std::remove_cvref_t<T>>::is_reflected;

template <typename T, typename TVisitor>
void reflect_visit(T& instance, TVisitor&& visitor) {
    using CleanT = std::remove_cvref_t<T>;
    if constexpr (is_reflected_v<CleanT>) {
        TypeReflector<CleanT>::visit_fields(instance, std::forward<TVisitor>(visitor));
    }
}

template <typename T, typename TVisitor>
void reflect_visit(const T& instance, TVisitor&& visitor) {
    using CleanT = std::remove_cvref_t<T>;
    if constexpr (is_reflected_v<CleanT>) {
        TypeReflector<CleanT>::visit_fields(instance, std::forward<TVisitor>(visitor));
    }
}

} // namespace engine::core

#define REFLECT_STRUCT_BEGIN(Type) \
    template <> \
    struct ::engine::core::TypeReflector<Type> { \
        using ReflectedType = Type; \
        static constexpr bool is_reflected = true; \
        static constexpr const char* name = #Type; \
        template <typename TVisitor> \
        static void visit_fields(ReflectedType& instance, TVisitor&& visitor) {

#define REFLECT_FIELD(Member, Label, Tooltip) \
            visitor.visit(#Member, instance.Member, Label, Tooltip);

#define REFLECT_STRUCT_END() \
        } \
        template <typename TVisitor> \
        static void visit_fields(const ReflectedType& instance, TVisitor&& visitor) { \
            visit_fields(const_cast<ReflectedType&>(instance), std::forward<TVisitor>(visitor)); \
        } \
    };
