/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * String builder utility functions
  */

#pragma once

#ifdef __cplusplus

extern "C++" {

#include <utility>
#include <tuple>
#include <string_view>
#include <type_traits>

#pragma warning(push)
// MSVC is imagining things
#pragma warning(disable : 4305)

template<typename T, bool null_terminate = true, typename S = void, std::enable_if_t<std::is_same_v<S, std::basic_string<T>> || std::is_same_v<S, std::basic_string_view<T>>, bool> = true, typename ... StrsT>
static constexpr TH_FORCEINLINE T* build_str(T* buffer, const S& cur_str, StrsT&&... next_strs);
template<typename T, bool null_terminate = true, typename P = void, typename L = void, std::enable_if_t<std::is_pointer_v<P> && std::is_same_v<std::remove_cv_t<std::remove_pointer_t<P>>, T> && std::is_integral_v<L> && !std::is_same_v<L, T>, bool> = true, typename ... StrsT>
static constexpr TH_FORCEINLINE T* build_str(T* buffer, P cur_str, L cur_len, StrsT&&... next_strs);
template<typename T, bool null_terminate = true, typename P = void, std::enable_if_t<std::is_pointer_v<P> && std::is_same_v<std::remove_cv_t<std::remove_pointer_t<P>>, T>, bool> = true, typename ... StrsT>
static constexpr TH_FORCEINLINE T* build_str(T* buffer, P cur_str, StrsT&&... next_strs);
template<typename T, bool null_terminate = true, size_t N = 0, typename L = void, std::enable_if_t<std::is_integral_v<L> && !std::is_same_v<L, T>, bool> = true, typename ... StrsT>
static constexpr TH_FORCEINLINE T* build_str(T* buffer, const T(&cur_str)[N], L cur_len, StrsT&&... next_strs);
template<typename T, bool null_terminate = true, size_t N = 0, typename ... StrsT>
static constexpr TH_FORCEINLINE T* build_str(T* buffer, const T(&cur_str)[N], StrsT&&... next_strs);
template<typename T, bool null_terminate = true, typename ... StrsT>
static constexpr TH_FORCEINLINE T* build_str(T* buffer, T cur_char, StrsT&&... next_strs);
template<typename T, bool null_terminate = true, typename S = void, typename L = void, std::enable_if_t<(std::is_same_v<S, std::basic_string<T>> || std::is_same_v <S, std::basic_string_view<T>>) && std::is_integral_v<L> && !std::is_same_v<L, T>, bool> = true, typename ... StrsT>
static constexpr TH_FORCEINLINE T* build_str(T* buffer, const S& cur_str, L cur_len, StrsT&&... next_strs) {
    //buffer += cur_str.copy(buffer, cur_len);
    for (size_t i = 0; i < cur_len; ++i) {
        buffer[i] = cur_str.data()[i];
    }
    buffer += cur_len;
    if constexpr (sizeof...(StrsT)) {
        return build_str<T, null_terminate>(buffer, std::forward<StrsT>(next_strs)...);
    } else {
        if constexpr (null_terminate) {
            *buffer = (T)0;
        }
        return buffer;
    }
}
template<typename T, bool null_terminate, typename S, std::enable_if_t<std::is_same_v<S, std::basic_string<T>> || std::is_same_v<S, std::basic_string_view<T>>, bool>, typename ... StrsT>
static constexpr TH_FORCEINLINE T* build_str(T* buffer, const S& cur_str, StrsT&&... next_strs) {
    //buffer += cur_str.copy(buffer, cur_str.length());
    size_t cur_len = cur_str.length();
    for (size_t i = 0; i < cur_len; ++i) {
        buffer[i] = cur_str.data()[i];
    }
    buffer += cur_len;
    if constexpr (sizeof...(StrsT)) {
        return build_str<T, null_terminate>(buffer, std::forward<StrsT>(next_strs)...);
    } else {
        if constexpr (null_terminate) {
            *buffer = (T)0;
        }
        return buffer;
    }
}
template<typename T, bool null_terminate, typename P, typename L, std::enable_if_t<std::is_pointer_v<P> && std::is_same_v<std::remove_cv_t<std::remove_pointer_t<P>>, T> && std::is_integral_v<L> && !std::is_same_v<L, T>, bool>, typename ... StrsT>
static constexpr TH_FORCEINLINE T* build_str(T* buffer, P cur_str, L cur_len, StrsT&&... next_strs) {
    for (size_t i = 0; i < (size_t)cur_len; ++i) {
        buffer[i] = cur_str[i];
    }
    buffer += cur_len;
    if constexpr (sizeof...(StrsT)) {
        return build_str<T, null_terminate>(buffer, std::forward<StrsT>(next_strs)...);
    } else {
        if constexpr (null_terminate) {
            *buffer = (T)0;
        }
        return buffer;
    }
}
template<typename T, bool null_terminate, typename P, std::enable_if_t<std::is_pointer_v<P> && std::is_same_v<std::remove_cv_t<std::remove_pointer_t<P>>, T>, bool>, typename ... StrsT>
static constexpr TH_FORCEINLINE T* build_str(T* buffer, P cur_str, StrsT&&... next_strs) {
    size_t length;
    if constexpr (std::is_same_v<T, char>) {
        length = strlen(cur_str);
    } else if constexpr (std::is_same_v<T, wchar_t>) {
        length = wcslen(cur_str);
    }
    for (size_t i = 0; i < length; ++i) {
        buffer[i] = cur_str[i];
    }
    buffer += length;
    if constexpr (sizeof...(StrsT)) {
        return build_str<T, null_terminate>(buffer, std::forward<StrsT>(next_strs)...);
    } else {
        if constexpr (null_terminate) {
            *buffer = (T)0;
        }
        return buffer;
    }
}
template<typename T, bool null_terminate, size_t N, typename L, std::enable_if_t<std::is_integral_v<L> && !std::is_same_v<L, T>, bool>, typename ... StrsT>
static constexpr TH_FORCEINLINE T* build_str(T* buffer, const T(&cur_str)[N], L cur_len, StrsT&&... next_strs) {
    //memcpy(buffer, cur_str, cur_len * sizeof(T));
    for (size_t i = 0; i < cur_len; ++i) {
        buffer[i] = cur_str[i];
    }
    buffer += cur_len;
    if constexpr (sizeof...(StrsT)) {
        return build_str<T, null_terminate>(buffer, std::forward<StrsT>(next_strs)...);
    } else {
        if constexpr (null_terminate) {
            *buffer = (T)0;
        }
        return buffer;
    }
}
template<typename T, bool null_terminate, size_t N, typename ... StrsT>
static constexpr TH_FORCEINLINE T* build_str(T* buffer, const T(&cur_str)[N], StrsT&&... next_strs) {
    // Assume this is coming from a string literal
    //memcpy(buffer, cur_str, sizeof(T[N - 1]));
    for (size_t i = 0; i < N - 1; ++i) {
        buffer[i] = cur_str[i];
    }
    buffer += N - 1;
    if constexpr (sizeof...(StrsT)) {
        return build_str<T, null_terminate>(buffer, std::forward<StrsT>(next_strs)...);
    } else {
        if constexpr (null_terminate) {
            *buffer = (T)0;
        }
        return buffer;
    }
}
template<typename T, bool null_terminate, typename ... StrsT>
static constexpr TH_FORCEINLINE T* build_str(T* buffer, T cur_char, StrsT&&... next_strs) {
    *buffer++ = cur_char;
    if constexpr (sizeof...(StrsT)) {
        return build_str<T, null_terminate>(buffer, std::forward<StrsT>(next_strs)...);
    } else {
        if constexpr (null_terminate) {
            *buffer = (T)0;
        }
        return buffer;
    }
}

template<typename T, typename S, std::enable_if_t<std::is_same_v<S, std::basic_string<T>> || std::is_same_v<S, std::basic_string_view<T>>, bool> = true, typename ... StrsT>
static constexpr TH_FORCEINLINE size_t alloc_str_calc_len(size_t length, const S& cur_str, StrsT&&... next_strs);
template<typename T, typename P, typename L, std::enable_if_t<std::is_pointer_v<P> && std::is_same_v<std::remove_cv_t<std::remove_pointer_t<P>>, T> && std::is_integral_v<L> && !std::is_same_v<L, T>, bool> = true, typename ... StrsT>
static constexpr TH_FORCEINLINE size_t alloc_str_calc_len(size_t length, P cur_str, L cur_len, StrsT&&... next_strs);
template<typename T, size_t N, typename L, std::enable_if_t<std::is_integral_v<L> && !std::is_same_v<L, T>, bool> = true, typename ... StrsT>
static constexpr TH_FORCEINLINE size_t alloc_str_calc_len(size_t length, const T(&cur_str)[N], L cur_len, StrsT&&... next_strs);
template<typename T, size_t N, typename ... StrsT>
static constexpr TH_FORCEINLINE size_t alloc_str_calc_len(size_t length, const T(&cur_str)[N], StrsT&&... next_strs);
template<typename T, typename ... StrsT>
static constexpr TH_FORCEINLINE size_t alloc_str_calc_len(size_t length, T cur_char, StrsT&&... next_strs);
template<typename T, typename S, typename L, std::enable_if_t<(std::is_same_v<S, std::basic_string<T>> || std::is_same_v<S, std::basic_string_view<T>>) && std::is_integral_v<L> && !std::is_same_v<L, T>, bool>, typename ... StrsT>
static constexpr TH_FORCEINLINE size_t alloc_str_calc_len(size_t length, const S& cur_str, L cur_len, StrsT&&... next_strs) {
    length += cur_len;
    if constexpr (sizeof...(StrsT)) {
        return alloc_str_calc_len<T>(length, std::forward<StrsT>(next_strs)...);
    } else {
        return length;
    }
}
template<typename T, typename S, std::enable_if_t<std::is_same_v<S, std::basic_string<T>> || std::is_same_v<S, std::basic_string_view<T>>, bool>, typename ... StrsT>
static constexpr TH_FORCEINLINE size_t alloc_str_calc_len(size_t length, const S& cur_str, StrsT&&... next_strs) {
    length += cur_str.length();
    if constexpr (sizeof...(StrsT)) {
        return alloc_str_calc_len<T>(length, std::forward<StrsT>(next_strs)...);
    } else {
        return length;
    }
}
template<typename T, typename P, typename L, std::enable_if_t<std::is_pointer_v<P> && std::is_same_v<std::remove_cv_t<std::remove_pointer_t<P>>, T> && std::is_integral_v<L> && !std::is_same_v<L, T>, bool>, typename ... StrsT>
static constexpr TH_FORCEINLINE size_t alloc_str_calc_len(size_t length, P cur_str, L cur_len, StrsT&&... next_strs) {
    length += cur_len;
    if constexpr (sizeof...(StrsT)) {
        return alloc_str_calc_len<T>(length, std::forward<StrsT>(next_strs)...);
    } else {
        return length;
    }
}
template<typename T, size_t N, typename L, std::enable_if_t<std::is_integral_v<L> && !std::is_same_v<L, T>, bool>, typename ... StrsT>
static constexpr TH_FORCEINLINE size_t alloc_str_calc_len(size_t length, const T(&cur_str)[N], L cur_len, StrsT&&... next_strs) {
    length += cur_len;
    if constexpr (sizeof...(StrsT)) {
        return alloc_str_calc_len<T>(length, std::forward<StrsT>(next_strs)...);
    } else {
        return length;
    }
}
template<typename T, size_t N, typename ... StrsT>
static constexpr TH_FORCEINLINE size_t alloc_str_calc_len(size_t length, const T(&cur_str)[N], StrsT&&... next_strs) {
    length += N - 1; // Assume this is coming from a string literal
    if constexpr (sizeof...(StrsT)) {
        return alloc_str_calc_len<T>(length, std::forward<StrsT>(next_strs)...);
    } else {
        return length;
    }
}
template<typename T, typename ... StrsT>
static constexpr TH_FORCEINLINE size_t alloc_str_calc_len(size_t length, T cur_char, StrsT&&... next_strs) {
    ++length;
    if constexpr (sizeof...(StrsT)) {
        return alloc_str_calc_len<T>(length, std::forward<StrsT>(next_strs)...);
    } else {
        return length;
    }
}

template<typename T, bool null_terminate = true, typename = void>
struct AllocStrHelper;
template<typename T, bool null_terminate, typename ... StrsT>
struct AllocStrHelper<T, null_terminate, std::tuple<StrsT...>> {
    using TupleStrs = std::tuple<StrsT...>;

private:
    using TupleStrs2 = std::tuple<StrsT..., void>;

    template<typename>
    struct inner2;
    template<size_t ... idx>
    struct inner2<std::integer_sequence<size_t, idx...>> {
        using TupleArgs = std::tuple<
            std::conditional_t<
                std::is_pointer_v<std::remove_reference_t<std::tuple_element_t<idx, TupleStrs2>>> &&
                std::is_same_v<std::remove_cv_t<std::remove_pointer_t<std::remove_reference_t<std::tuple_element_t<idx, TupleStrs2>>>>, T> && (
                    (idx >= sizeof...(StrsT) - 1) ||
                    !std::is_integral_v<std::remove_reference_t<std::tuple_element_t<idx + 1, TupleStrs2>>> ||
                    std::is_same_v<T, std::remove_reference_t<std::tuple_element_t<idx + 1, TupleStrs2>>>
                ),
                std::basic_string_view<T>,
                std::tuple_element_t<idx, TupleStrs>
            >...
        >;
    };

public:

    using TupleArgs = typename inner2<std::index_sequence_for<StrsT...>>::TupleArgs;

    static inline constexpr bool requires_implicit_strlen = !std::is_same_v<TupleStrs, TupleArgs>;

private:

    static constexpr TH_FORCEINLINE void build_vla_dummy(T*) {}

    template<typename>
    struct inner;
    template<typename ... ArgsT>
    struct inner<std::tuple<ArgsT...>> {
        static constexpr TH_FORCEINLINE auto build_vla_impl2(size_t& length, ArgsT&&... strs) {
            size_t length_val = alloc_str_calc_len<T>(0, std::forward<ArgsT>(strs)...);
            if constexpr (null_terminate) {
                ++length_val;
            }
            length_val *= sizeof(T);
            length = length_val;
            if constexpr (requires_implicit_strlen) {
                return [&](T* buffer) {
                    build_str<T, null_terminate>(buffer, std::forward<const ArgsT>(strs)...);
                };
            } else {
                return &build_vla_dummy;
            }
        }

        static constexpr TH_FORCEINLINE auto build_vla_impl(size_t& length, StrsT&&... strs) {
            return build_vla_impl2(length, std::forward<ArgsT>(strs)...);
        }

        static constexpr TH_FORCEINLINE auto alloc_str_impl2(ArgsT&&... strs) {
            size_t length = alloc_str_calc_len<T>(0, std::forward<ArgsT>(strs)...);
            if constexpr (null_terminate) {
                ++length;
            }
            length *= sizeof(T);
            T* ret = (T*)malloc(length);
            if (ret) {
                if constexpr (null_terminate) {
                    ret[length] = (T)0;
                }
                build_str<T, false>(ret, std::forward<ArgsT>(strs)...);
            }
            return ret;
        }

        static constexpr TH_FORCEINLINE auto alloc_str_impl(StrsT&&... strs) {
            return alloc_str_impl2(std::forward<ArgsT>(strs)...);
        }
    };

public:

    static constexpr TH_FORCEINLINE auto build_vla(size_t& length, StrsT&&... strs) {
        return inner<TupleArgs>::build_vla_impl(length, std::forward<StrsT>(strs)...);
    }

    static constexpr TH_FORCEINLINE T* alloc_str(StrsT&&... strs) {
        return inner<TupleArgs>::alloc_str_impl(std::forward<StrsT>(strs)...);
    }
};

template<typename T, bool null_terminate = true, typename ... StrsT>
TH_CALLER_FREE static constexpr TH_FORCEINLINE T* alloc_str(StrsT&&... strs) {
    return AllocStrHelper<T, null_terminate, StrsT...>::alloc_str(std::forward<StrsT>(strs)...);
}

#define BUILD_VLA(type, name, ...) \
    size_t name##_len; \
    auto name##_build = AllocStrHelper<type, false, decltype(std::forward_as_tuple(__VA_ARGS__))>::build_vla(name##_len, __VA_ARGS__); \
    VLA(type, name, name##_len); \
    if constexpr (!AllocStrHelper<type, false, decltype(std::forward_as_tuple(__VA_ARGS__))>::requires_implicit_strlen) { \
        ::build_str<type, false>(name, __VA_ARGS__); \
    } else { \
        name##_build(name); \
    } \
    do; while (0)
    

#define BUILD_VLA_STR(type, name, ...) \
    size_t name##_len; \
    auto name##_build = AllocStrHelper<type, true, decltype(std::forward_as_tuple(__VA_ARGS__))>::build_vla(name##_len, __VA_ARGS__); \
    VLA(type, name, name##_len); \
    if constexpr (!AllocStrHelper<type, true, decltype(std::forward_as_tuple(__VA_ARGS__))>::requires_implicit_strlen) { \
        ::build_str<type, true>(name, __VA_ARGS__); \
    } else { \
        name##_build(name); \
    } \
    do; while (0)

#define STRDUP_VLA(type, name, str) \
    size_t name##_len = 0; \
    if constexpr (std::is_same_v<type, char>) { \
        name##_len = strlen((const char*)str) + 1; \
    } else if constexpr (std::is_same_v<type, wchar_t>) { \
        name##_len = (wcslen((const wchar_t*)str) + 1) * sizeof(wchar_t); \
    } \
    VLA(type, name, name##_len); \
    memcpy(name, str, name##_len)

#define FORMAT_VLA_STR(type, name, format, ...) \
    size_t name##_len = snprintf(NULL, 0, (format), __VA_ARGS__); \
    VLA(type, name, name##_len); \
    (void)sprintf(name, (format), __VA_ARGS__)

#pragma warning(pop)
}
#endif
