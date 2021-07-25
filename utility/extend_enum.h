#ifdef ENUM_FILE

#define ENUM_BEGIN(name) \
class name \
{ \
  public: \
    enum InnerEnum : uint32_t {
#define ENUM_MEMBER(name, member) member,
#define ENUM_END(name) \
        name##_MAX_, \
        name##_INVALID_ = UINT32_MAX \
    }; \
 \
    name() : v_(name##_INVALID_) {} \
 \
    name(const InnerEnum& v) : v_(v) {} \
 \
    constexpr auto operator<=>(const name&) const = default; \
 \
    constexpr auto operator<=>(const name::InnerEnum& e) const { return v_ <=> e; }; \
 \
    inline const char* ToString() const; \
 \
    constexpr auto ToUInt() const { return v_; } \
 \
    constexpr static size_t Count() { return name##_MAX_; } \
 \
    inline static const std::array<name, name##_MAX_>& Members(); \
 \
    inline static std::optional<name> Parse(const std::string_view& str); \
 \
    explicit operator uint32_t() const { return static_cast<uint32_t>(v_); } \
 \
    template <typename OS> \
    friend inline decltype(auto) operator<<(OS& os, const name& e) { return os << e.ToString(); } \
 \
  private: \
    InnerEnum v_; \
 \
};
#include ENUM_FILE
#undef ENUM_BEGIN
#undef ENUM_MEMBER
#undef ENUM_END

#define ENUM_BEGIN(name) \
inline const char* name::ToString() const \
{ \
    static const std::array<const char*, name::name##_MAX_> strings = {
#define ENUM_MEMBER(name, member) #member,
#define ENUM_END(name) \
    }; \
    return strings[v_]; \
}
#include ENUM_FILE
#undef ENUM_BEGIN
#undef ENUM_MEMBER
#undef ENUM_END

#define ENUM_BEGIN(name) \
inline const std::array<name, name::name##_MAX_>& name::Members() \
{ \
    static std::array<name, name::name##_MAX_> members = {
#define ENUM_MEMBER(name, member) name::member,
#define ENUM_END(name) \
    }; \
    return members; \
}
#include ENUM_FILE
#undef ENUM_BEGIN
#undef ENUM_MEMBER
#undef ENUM_END

#define ENUM_BEGIN(name) \
inline std::optional<name> name::Parse(const std::string_view& str) \
{ \
    static std::map<std::string_view, name> parser = {
#define ENUM_MEMBER(name, member)  { #member, name::member },
#define ENUM_END(name) \
    }; \
    const auto it = parser.find(str); \
    if (it == parser.end()) { \
        return std::nullopt; \
    } \
    return it->second; \
}
#include ENUM_FILE
#undef ENUM_BEGIN
#undef ENUM_MEMBER
#undef ENUM_END

#undef ENUM_FILE
#endif
