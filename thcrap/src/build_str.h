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


// Template to identify whether or not a type is valid for.
// 
// signed/unsigned char are distinct types from char, but are
// not included since they're intended more for numeric usage.

template<typename T>
struct is_char_type : std::false_type {};
template<>
struct is_char_type<char> : std::true_type {};
template<>
struct is_char_type<wchar_t> : std::true_type {};
#if CPP20
template<>
struct is_char_type<char8_t> : std::true_type {};
#endif
template<>
struct is_char_type<char16_t> : std::true_type {};
template<>
struct is_char_type<char32_t> : std::true_type {};

template<typename T>
static inline constexpr bool is_char_type_v = is_char_type<T>::value;

// Template to check whether or not two types can represent the
// same text without re-encoding a string.

template<typename T1, typename T2>
struct is_compatible_char_type : std::conditional_t<is_char_type_v<T1> && is_char_type_v<T2> && sizeof(T1) == sizeof(T2), std::true_type, std::false_type> {};

template<typename T1, typename T2>
static inline constexpr bool is_compatible_char_type_v = is_compatible_char_type<T1, T2>::value;

// Special template to check if two string views are compatible
// since otherwise detecting this is a pain.

template<typename T, typename V, typename C = void*, bool = is_compatible_char_type_v<T, C>>
struct is_compatible_string_view : std::false_type {};

template<typename T, typename C>
struct is_compatible_string_view<T, std::basic_string_view<C>, typename std::basic_string_view<C>::value_type, true> : std::true_type {};

template<typename T, typename V>
static inline constexpr bool is_compatible_string_view_v = is_compatible_string_view<T, V>::value;

// Convenience template for detecting string/string_view

template<typename T, typename C>
struct is_cpp_string_like : std::conditional_t<std::is_same_v<T, std::basic_string<C>> || std::is_same_v<T, std::basic_string_view<C>>, std::true_type, std::false_type> {};

template<typename T, typename C>
static inline constexpr bool is_cpp_string_like_v = is_cpp_string_like<T, C>::value;

// Template to extract the character type of a string argument.
// Used for deducing whether all types in a parameter pack match.

template<typename T>
struct string_char_type {
    using type = std::remove_cv_t<std::remove_pointer_t<T>>;
};
template<typename C>
struct string_char_type<std::basic_string<C>> {
    using type = C;
};
template<typename C>
struct string_char_type<std::basic_string_view<C>> {
    using type = C;
};
template<typename C, size_t N>
struct string_char_type<C[N]> {
    using type = std::remove_cv_t<C>;
};
template<typename T>
using string_char_type_t = typename string_char_type<T>::type;

template<typename ... Args>
struct string_args_compatible {
private:
    template<typename C, typename T, typename ... TArgs>
    static constexpr auto types_match() {
        if constexpr (std::is_integral_v<T> && !is_char_type_v<T>) {
            if constexpr (!std::is_same_v<C, void>) {
                if constexpr ((bool)sizeof...(TArgs)) {
                    return types_match<C, TArgs...>();
                }
                return static_cast<C>(1);
            }
            else {
                return false;
            }
        }
        else {
            using char_t = string_char_type_t<T>;
            if constexpr (!std::is_same_v<C, void>) {
                if constexpr (is_compatible_char_type_v<C, char_t>) {
                    if constexpr ((bool)sizeof...(TArgs)) {
                        return types_match<C, TArgs...>();
                    }
                    return static_cast<C>(1);
                }
                return static_cast<C>(0);
            }
            else {
                if constexpr ((bool)sizeof...(TArgs)) {
                    return types_match<char_t, TArgs...>();
                }
                return static_cast<char_t>(1);
            }
        }
    }
public:
    using type = decltype(types_match<void, Args...>());
    static constexpr bool value = (bool)types_match<void, Args...>();
};

template<typename ... Args>
using string_args_compatible_t = typename string_args_compatible<Args...>::type;

template<typename ... Args>
static inline constexpr bool string_args_compatible_v = string_args_compatible<Args...>::value;

// constexpr functions for calculating string length

template<typename T, typename S = void, std::enable_if_t<is_cpp_string_like_v<S, T>, bool> = true>
static constexpr TH_FORCEINLINE size_t length_str(const S& str) {
    return str.length();
}

template<typename T, size_t N>
static constexpr TH_FORCEINLINE size_t length_str(const T(&str)[N]) {
    return N - 1;
}

template<typename T, std::enable_if_t<is_char_type_v<T>, bool> = true>
static constexpr TH_FORCEINLINE size_t length_str(const T* str) {
    if constexpr (is_compatible_char_type_v<T, char>) {
        return strlen((const char*)str);
    }
    else if constexpr (is_compatible_char_type_v<T, wchar_t>) {
        return wcslen((const wchar_t*)str);
    }
    else {
        const T* str_start = str;
        while (*str) ++str;
        return PtrDiffStrlen(str, str_start);
    }
}

template<typename T, std::enable_if_t<is_char_type_v<T>, bool> = true>
static constexpr TH_FORCEINLINE size_t length_str(T str) {
    return 1;
}

// Functions for concatenating compatible characters within a buffer.

template<typename T, bool null_terminate = true, typename S = void, typename C = S::value_type, std::enable_if_t<is_compatible_char_type_v<T, C> && is_cpp_string_like_v<S, C>, bool> = true, typename ... StrsT>
static constexpr TH_FORCEINLINE T* build_str(T* buffer, const S& cur_str, StrsT&&... next_strs);
template<typename T, bool null_terminate = true, typename P = void, typename L = void, std::enable_if_t<std::is_pointer_v<P> && is_compatible_char_type_v<std::remove_cv_t<std::remove_pointer_t<P>>, T> && std::is_integral_v<L> && !is_char_type_v<L>, bool> = true, typename ... StrsT>
static constexpr TH_FORCEINLINE T* build_str(T* buffer, P cur_str, L cur_len, StrsT&&... next_strs);
template<typename T, bool null_terminate = true, typename P = void, std::enable_if_t<std::is_pointer_v<P> && is_compatible_char_type_v<std::remove_cv_t<std::remove_pointer_t<P>>, T>, bool> = true, typename ... StrsT>
static constexpr TH_FORCEINLINE T* build_str(T* buffer, P cur_str, StrsT&&... next_strs);
template<typename T, bool null_terminate = true, typename C = T, size_t N = 0, typename L = void, std::enable_if_t<is_compatible_char_type_v<T, C> && std::is_integral_v<L> && !is_char_type_v<L>, bool> = true, typename ... StrsT>
static constexpr TH_FORCEINLINE T* build_str(T* buffer, const C(&cur_str)[N], L cur_len, StrsT&&... next_strs);
template<typename T, bool null_terminate = true, typename C = T, size_t N = 0, std::enable_if_t<is_compatible_char_type_v<T, C>, bool> = true, typename ... StrsT>
static constexpr TH_FORCEINLINE T* build_str(T* buffer, const C(&cur_str)[N], StrsT&&... next_strs);
template<typename T, bool null_terminate = true, typename C = T, std::enable_if_t<is_compatible_char_type_v<T, C>, bool> = true, typename ... StrsT>
static constexpr TH_FORCEINLINE T* build_str(T* buffer, C cur_char, StrsT&&... next_strs);
template<typename T, bool null_terminate = true, typename S = void, typename C = S::value_type, typename L = void, std::enable_if_t<is_compatible_char_type_v<T, C> && is_cpp_string_like_v<S, C> && std::is_integral_v<L> && !is_char_type_v<L>, bool> = true, typename ... StrsT>
static constexpr TH_FORCEINLINE T* build_str(T* buffer, const S& cur_str, L cur_len, StrsT&&... next_strs) {
    for (size_t i = 0; i < cur_len; ++i) {
        buffer[i] = (T)cur_str.data()[i];
    }
    buffer += cur_len;
    if constexpr ((bool)sizeof...(StrsT)) {
        return build_str<T, null_terminate>(buffer, std::forward<StrsT>(next_strs)...);
    } else {
        if constexpr (null_terminate) {
            *buffer = (T)0;
        }
        return buffer;
    }
}
template<typename T, bool null_terminate, typename S, typename C, std::enable_if_t<is_compatible_char_type_v<T, C> && is_cpp_string_like_v<S, C>, bool>, typename ... StrsT>
static constexpr TH_FORCEINLINE T* build_str(T* buffer, const S& cur_str, StrsT&&... next_strs) {
    size_t cur_len = cur_str.length();
    for (size_t i = 0; i < cur_len; ++i) {
        buffer[i] = (T)cur_str.data()[i];
    }
    buffer += cur_len;
    if constexpr ((bool)sizeof...(StrsT)) {
        return build_str<T, null_terminate>(buffer, std::forward<StrsT>(next_strs)...);
    } else {
        if constexpr (null_terminate) {
            *buffer = (T)0;
        }
        return buffer;
    }
}
template<typename T, bool null_terminate, typename P, typename L, std::enable_if_t<std::is_pointer_v<P> && is_compatible_char_type_v<std::remove_cv_t<std::remove_pointer_t<P>>, T> && std::is_integral_v<L> && !is_char_type_v<L>, bool>, typename ... StrsT>
static constexpr TH_FORCEINLINE T* build_str(T* buffer, P cur_str, L cur_len, StrsT&&... next_strs) {
    for (size_t i = 0; i < (size_t)cur_len; ++i) {
        buffer[i] = (T)cur_str[i];
    }
    buffer += cur_len;
    if constexpr ((bool)sizeof...(StrsT)) {
        return build_str<T, null_terminate>(buffer, std::forward<StrsT>(next_strs)...);
    } else {
        if constexpr (null_terminate) {
            *buffer = (T)0;
        }
        return buffer;
    }
}
template<typename T, bool null_terminate, typename P, std::enable_if_t<std::is_pointer_v<P> && is_compatible_char_type_v<std::remove_cv_t<std::remove_pointer_t<P>>, T>, bool>, typename ... StrsT>
static constexpr TH_FORCEINLINE T* build_str(T* buffer, P cur_str, StrsT&&... next_strs) {
    size_t length = length_str(cur_str);
    for (size_t i = 0; i < length; ++i) {
        buffer[i] = (T)cur_str[i];
    }
    buffer += length;
    if constexpr ((bool)sizeof...(StrsT)) {
        return build_str<T, null_terminate>(buffer, std::forward<StrsT>(next_strs)...);
    } else {
        if constexpr (null_terminate) {
            *buffer = (T)0;
        }
        return buffer;
    }
}
template<typename T, bool null_terminate, typename C, size_t N, typename L, std::enable_if_t<is_compatible_char_type_v<T, C> && std::is_integral_v<L> && !is_char_type_v<L>, bool>, typename ... StrsT>
static constexpr TH_FORCEINLINE T* build_str(T* buffer, const C(&cur_str)[N], L cur_len, StrsT&&... next_strs) {
    for (size_t i = 0; i < cur_len; ++i) {
        buffer[i] = (T)cur_str[i];
    }
    buffer += cur_len;
    if constexpr ((bool)sizeof...(StrsT)) {
        return build_str<T, null_terminate>(buffer, std::forward<StrsT>(next_strs)...);
    } else {
        if constexpr (null_terminate) {
            *buffer = (T)0;
        }
        return buffer;
    }
}
template<typename T, bool null_terminate, typename C, size_t N, std::enable_if_t<is_compatible_char_type_v<T, C>, bool>, typename ... StrsT>
static constexpr TH_FORCEINLINE T* build_str(T* buffer, const C(&cur_str)[N], StrsT&&... next_strs) {
    // Assume this is coming from a string literal
    for (size_t i = 0; i < N - 1; ++i) {
        buffer[i] = (T)cur_str[i];
    }
    buffer += N - 1;
    if constexpr ((bool)sizeof...(StrsT)) {
        return build_str<T, null_terminate>(buffer, std::forward<StrsT>(next_strs)...);
    } else {
        if constexpr (null_terminate) {
            *buffer = (T)0;
        }
        return buffer;
    }
}
template<typename T, bool null_terminate, typename C, std::enable_if_t<is_compatible_char_type_v<T, C>, bool>, typename ... StrsT>
static constexpr TH_FORCEINLINE T* build_str(T* buffer, C cur_char, StrsT&&... next_strs) {
    *buffer++ = (T)cur_char;
    if constexpr ((bool)sizeof...(StrsT)) {
        return build_str<T, null_terminate>(buffer, std::forward<StrsT>(next_strs)...);
    } else {
        if constexpr (null_terminate) {
            *buffer = (T)0;
        }
        return buffer;
    }
}

// Functions for calculating the buffer size required to hold all arguments when concatenated.

template<typename T, typename S, typename C = S::value_type, std::enable_if_t<is_compatible_char_type_v<T, C> && is_cpp_string_like_v<S, C>, bool> = true, typename ... StrsT>
static constexpr TH_FORCEINLINE size_t alloc_str_calc_len(size_t length, const S& cur_str, StrsT&&... next_strs);
template<typename T, typename P, typename L, std::enable_if_t<std::is_pointer_v<P> && is_compatible_char_type_v<std::remove_cv_t<std::remove_pointer_t<P>>, T> && std::is_integral_v<L> && !is_char_type_v<L>, bool> = true, typename ... StrsT>
static constexpr TH_FORCEINLINE size_t alloc_str_calc_len(size_t length, P cur_str, L cur_len, StrsT&&... next_strs);
template<typename T, typename C, size_t N, typename L, std::enable_if_t<is_compatible_char_type_v<T, C> && std::is_integral_v<L> && !is_char_type_v<L>, bool> = true, typename ... StrsT>
static constexpr TH_FORCEINLINE size_t alloc_str_calc_len(size_t length, const C(&cur_str)[N], L cur_len, StrsT&&... next_strs);
template<typename T, typename C, size_t N, std::enable_if_t<is_compatible_char_type_v<T, C>, bool> = true, typename ... StrsT>
static constexpr TH_FORCEINLINE size_t alloc_str_calc_len(size_t length, const C(&cur_str)[N], StrsT&&... next_strs);
template<typename T, typename C, std::enable_if_t<is_compatible_char_type_v<T, C>, bool> = true, typename ... StrsT>
static constexpr TH_FORCEINLINE size_t alloc_str_calc_len(size_t length, C cur_char, StrsT&&... next_strs);
template<typename T, typename S, typename L, typename C = S::value_type, std::enable_if_t<is_compatible_char_type_v<T, C> && is_cpp_string_like_v<S, C> && std::is_integral_v<L> && !is_char_type_v<L>, bool>, typename ... StrsT>
static constexpr TH_FORCEINLINE size_t alloc_str_calc_len(size_t length, const S& cur_str, L cur_len, StrsT&&... next_strs) {
    length += cur_len;
    if constexpr ((bool)sizeof...(StrsT)) {
        return alloc_str_calc_len<T>(length, std::forward<StrsT>(next_strs)...);
    } else {
        return length;
    }
}
template<typename T, typename S, typename C, std::enable_if_t<is_compatible_char_type_v<T, C> && is_cpp_string_like_v<S, C>, bool>, typename ... StrsT>
static constexpr TH_FORCEINLINE size_t alloc_str_calc_len(size_t length, const S& cur_str, StrsT&&... next_strs) {
    length += cur_str.length();
    if constexpr ((bool)sizeof...(StrsT)) {
        return alloc_str_calc_len<T>(length, std::forward<StrsT>(next_strs)...);
    } else {
        return length;
    }
}
template<typename T, typename P, typename L, std::enable_if_t<std::is_pointer_v<P> && is_compatible_char_type_v<std::remove_cv_t<std::remove_pointer_t<P>>, T> && std::is_integral_v<L> && !is_char_type_v<L>, bool>, typename ... StrsT>
static constexpr TH_FORCEINLINE size_t alloc_str_calc_len(size_t length, P cur_str, L cur_len, StrsT&&... next_strs) {
    length += cur_len;
    if constexpr ((bool)sizeof...(StrsT)) {
        return alloc_str_calc_len<T>(length, std::forward<StrsT>(next_strs)...);
    } else {
        return length;
    }
}
template<typename T, typename C, size_t N, typename L, std::enable_if_t<is_compatible_char_type_v<T, C> && std::is_integral_v<L> && !is_char_type_v<L>, bool>, typename ... StrsT>
static constexpr TH_FORCEINLINE size_t alloc_str_calc_len(size_t length, const T(&cur_str)[N], L cur_len, StrsT&&... next_strs) {
    length += cur_len;
    if constexpr ((bool)sizeof...(StrsT)) {
        return alloc_str_calc_len<T>(length, std::forward<StrsT>(next_strs)...);
    } else {
        return length;
    }
}
template<typename T, typename C, size_t N, std::enable_if_t<is_compatible_char_type_v<T, C>, bool>, typename ... StrsT>
static constexpr TH_FORCEINLINE size_t alloc_str_calc_len(size_t length, const C(&cur_str)[N], StrsT&&... next_strs) {
    length += N - 1; // Assume this is coming from a string literal
    if constexpr ((bool)sizeof...(StrsT)) {
        return alloc_str_calc_len<T>(length, std::forward<StrsT>(next_strs)...);
    } else {
        return length;
    }
}
template<typename T, typename C, std::enable_if_t<is_compatible_char_type_v<T, C>, bool>, typename ... StrsT>
static constexpr TH_FORCEINLINE size_t alloc_str_calc_len(size_t length, C cur_char, StrsT&&... next_strs) {
    ++length;
    if constexpr ((bool)sizeof...(StrsT)) {
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
                is_compatible_char_type_v<std::remove_cv_t<std::remove_pointer_t<std::remove_reference_t<std::tuple_element_t<idx, TupleStrs2>>>>, T> && (
                    (idx >= sizeof...(StrsT) - 1) ||
                    !std::is_integral_v<std::remove_reference_t<std::tuple_element_t<idx + 1, TupleStrs2>>> ||
                    is_char_type_v<std::remove_reference_t<std::tuple_element_t<idx + 1, TupleStrs2>>>
                ),
                std::basic_string_view<std::remove_cv_t<std::remove_pointer_t<std::remove_reference_t<std::tuple_element_t<idx, TupleStrs2>>>>>,
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

#if COMPILER_IS_REAL_MSVC
        template<typename ... Args2>
        static constexpr TH_FORCEINLINE auto build_vla_data_impl3(Args2&&... strs) {
            return [=](T* buffer) {
                build_str<T, null_terminate>(buffer, std::forward<const ArgsT>(strs)...);
            };
        }
#endif

        static constexpr TH_FORCEINLINE auto build_vla_impl2(size_t& length, ArgsT&&... strs) {
            size_t length_val = alloc_str_calc_len<T>(0, std::forward<ArgsT>(strs)...);
            if constexpr (null_terminate) {
                ++length_val;
            }
            length_val *= sizeof(T);
            length = length_val;
            if constexpr (requires_implicit_strlen) {
#if COMPILER_IS_REAL_MSVC
                // MSVC generates insanely stupid code
                // if [=] capture is used without this.
                //
                // The intent is to still pass everything
                // by reference unless the argument is an
                // implicitly created string_view, since
                // those need to be captured by value to
                // avoid a dangling reference when returning.
                return build_vla_data_impl3(
                    std::forward<std::conditional_t<
                        is_compatible_string_view_v<T, ArgsT>,
                        std::remove_reference_t<ArgsT>,
                        std::reference_wrapper<std::remove_reference_t<ArgsT>>
                    >>(strs)...
                );
#else
                return [=](T* buffer) {
                    build_str<T, null_terminate>(buffer, std::forward<const ArgsT>(strs)...);
                };
#endif
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

    template<std::enable_if_t<string_args_compatible_v<T, std::remove_reference_t<StrsT>...>, bool> = true>
    static constexpr TH_FORCEINLINE auto build_vla(size_t& length, StrsT&&... strs) {
        return inner<TupleArgs>::build_vla_impl(length, std::forward<StrsT>(strs)...);
    }

    static constexpr TH_FORCEINLINE T* alloc_str(StrsT&&... strs) {
        return inner<TupleArgs>::alloc_str_impl(std::forward<StrsT>(strs)...);
    }
};

// Functions for allocating a new string

// Allocate a string concatenating all of the argument strings.
template<typename T, bool null_terminate = true, typename ... StrsT, std::enable_if_t<string_args_compatible_v<T, std::remove_reference_t<StrsT>...>, bool> = true>
TH_CALLER_FREE static constexpr TH_FORCEINLINE T* alloc_str(StrsT&&... strs) {
    return AllocStrHelper<T, null_terminate, std::tuple<StrsT&&...>>::alloc_str(std::forward<StrsT>(strs)...);
}

// Allocate a string concatenating all of the argument strings,
// deducing the returned character type from the arguments.
template<bool null_terminate = true, typename ... StrsT, std::enable_if_t<string_args_compatible_v<std::remove_reference_t<StrsT>...>, bool> = true>
TH_CALLER_FREE static constexpr TH_FORCEINLINE auto alloc_str(StrsT&&... strs) {
    return AllocStrHelper<string_args_compatible_t<std::remove_reference_t<StrsT>...>, null_terminate, std::tuple<StrsT&&...>>::alloc_str(std::forward<StrsT>(strs)...);
}

// Wrappers for allocating a string within a VLA

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
    size_t name##_len = (length_str(str) + 1) * sizeof(type); \
    VLA(type, name, name##_len); \
    memcpy(name, str, name##_len)

#define FORMAT_VLA_STR(type, name, format, ...) \
    size_t name##_len = snprintf(NULL, 0, (format), __VA_ARGS__); \
    VLA(type, name, name##_len); \
    (void)sprintf(name, (format), __VA_ARGS__)

}
#endif
