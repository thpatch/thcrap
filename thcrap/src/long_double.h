/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Long double support for MSVC.
  */

#pragma once

// Check whether long double is supported natively
#if !(defined(_MSC_VER) && !defined(_LDSUPPORT))

// Somehow it's actually defined, so this can
// just be a typedef and nothing else is needed.
typedef long double LongDouble80;

#else

// It isn't supported, so time to make it work anyway
// with C++ template shenanigans and other BS.

typedef struct LongDouble80 LongDouble80;

#ifdef __cplusplus
extern "C++" {

#define ValidFPValue(T) \
	(std::is_same_v<T, LongDouble80> || std::is_arithmetic_v<T>)

#define ValidFPArithmetic(T1, T2) \
	((std::is_same_v<T1, LongDouble80> || std::is_same_v<T2, LongDouble80>) && \
	ValidFPValue(T1) && ValidFPValue(T2))

// Janky looking workaround to determine which integer size
// is being used without caring about how the compiler defines
// any of the fundamental integer types.
#define IsAnyIntBelow16(T) \
	(sizeof(T) < sizeof(int16_t))
#define IsSigned16OrHigher(T) \
	(sizeof(T) >= sizeof(int16_t) && std::is_signed_v<T>)
#define IsUnsigned16(T) \
	(sizeof(T) == sizeof(uint16_t) && std::is_unsigned_v<T>)
#define IsUnsigned32(T) \
	(sizeof(T) == sizeof(uint32_t) && std::is_unsigned_v<T>)
#define IsUnsigned64(T) \
	(sizeof(T) == sizeof(uint64_t) && std::is_unsigned_v<T>)

	template <typename T>
	__forceinline std::enable_if_t<ValidFPValue(T), void> LoadToX87(T t_val) {
		if constexpr (std::is_same_v<T, LongDouble80>) {
			__asm FLD TBYTE PTR[t_val];
		}
		else if constexpr (std::is_floating_point_v<T>) {
			__asm FLD[t_val];
		}
		else if constexpr (std::is_integral_v<T>) {
			if constexpr (IsAnyIntBelow16(T) || IsUnsigned16(T)) {
				int32_t temp = (int32_t)t_val;
				__asm FILD DWORD PTR[temp];
			}
			else if constexpr (IsSigned16OrHigher(T)) {
				__asm FILD[t_val];
			}
			else if constexpr (IsUnsigned32(T)) {
				int64_t temp = (int64_t)t_val;
				__asm FILD QWORD PTR[temp];
			}
			else if constexpr (IsUnsigned64(T)) {
				if (t_val <= INT64_MAX) {
					__asm FILD QWORD PTR[t_val];
				}
				else {
					uint64_t temp;
					t_val -= (temp = INT64_MAX);
					__asm {
						FILD QWORD PTR[temp];
						FILD QWORD PTR[t_val];
						FADDP ST(1), ST;
					}
				}
			}
			else {
				static_assert(0, "Invalid integer size");
			}
		}
		else {
			static_assert(0, "Invalid arithmetic type");
		}
	}

	template <typename T>
	__forceinline std::enable_if_t<ValidFPValue(T), T> StoreFromX87(void) {
		T t_val;
		if constexpr (std::is_same_v<T, LongDouble80>) {
			__asm FSTP TBYTE PTR[t_val];
		}
		else if constexpr (std::is_floating_point_v<T>) {
			__asm FSTP[t_val];
		}
		else if constexpr (std::is_integral_v<T>) {
			if constexpr (IsAnyIntBelow16(T) || IsUnsigned16(T)) {
				int32_t temp;
				__asm FISTP DWORD PTR[temp];
				t_val = (T)temp;
			}
			else if constexpr (IsSigned16OrHigher(T)) {
				__asm FISTP[t_val];
			}
			else if constexpr (IsUnsigned32(T)) {
				int64_t temp;
				__asm FISTP QWORD PTR[temp];
				t_val = (T)temp;
			}
			else if constexpr (IsUnsigned64(T)) {
				static constexpr double DBL_INT64_MAX = (double)INT64_MAX;
				t_val = UINT64_MAX;
				LoadToX87(DBL_INT64_MAX);
				__asm {
					FCOMI ST, ST(1);
					JNA RequireU64;
					FSTP ST(0);
					FISTP QWORD PTR[t_val];
				RequireU64:
				}
				if (t_val == UINT64_MAX) {
					__asm {
						FSUBP ST(1), ST;
						FISTP QWORD PTR[t_val];
					}
					t_val += INT64_MAX;
				}
			}
			else {
				static_assert(0, "Invalid integer size");
			}
		}
		else {
			static_assert(0, "Invalid arithmetic type");
		}
		return t_val;
	}

#undef IsAnyIntBelow16
#undef IsSigned16OrHigher
#undef IsUnsigned16
#undef IsUnsigned32
#undef IsUnsigned64

#endif // ifdef __cplusplus

// Make sure that the struct is still visible without
// C++ so that the compiler doesn't complain about
// incompatible linkage crap.
	struct LongDouble80 {

		_LDOUBLE ld;

#ifdef __cplusplus

#define AddX87() __asm FADDP ST(1), ST;

#define SubX87() __asm FSUBP ST(1), ST;

#define MulX87() __asm FMULP ST(1), ST;

#define DivX87() __asm FDIVP ST(1), ST;

#define CompX87() __asm FCOMIP ST, ST(1); __asm FSTP ST(0);

#define X87Op(op, lh, rh) \
		LoadToX87(lh); \
		LoadToX87(rh); \
		op ## X87(); \

		__forceinline operator bool() {
			bool _ret;
			LoadToX87(*this);
			__asm {
				FCOMIP ST, ST(0);
				SETZ BYTE PTR[_ret];
			}
			return _ret;
		}

		template<typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>, T>>
		__forceinline operator T() {
			LoadToX87(*this);
			return StoreFromX87<T>();
		}

		template<typename T>
		__forceinline std::enable_if_t<std::is_arithmetic_v<T>, LongDouble80> operator=(T right) {
			LoadToX87(right);
			return *this = StoreFromX87<LongDouble80>();
		}

		__forceinline LongDouble80 operator+() {
			return *this;
		}

		__forceinline LongDouble80 operator-() {
			LoadToX87(*this);
			__asm FCHS;
			return StoreFromX87<LongDouble80>();
		}

		template<typename T>
		__forceinline std::enable_if_t<ValidFPValue(T), LongDouble80> operator+=(T right) {
			return *this = *this + right;
		}

		template<typename T>
		__forceinline std::enable_if_t<ValidFPValue(T), LongDouble80> operator-=(T right) {
			return *this = *this - right;
		}

		template<typename T>
		__forceinline std::enable_if_t<ValidFPValue(T), LongDouble80> operator*=(T right) {
			return *this = *this * right;
		}

		template<typename T>
		__forceinline std::enable_if_t<ValidFPValue(T), LongDouble80> operator/=(T right) {
			return *this = *this / right;
		}

		__forceinline LongDouble80 operator++() {
			__asm FLD1;
			LoadToX87(*this);
			AddX87();
			return *this = StoreFromX87<LongDouble80>();
		}

		__forceinline LongDouble80 operator++(int) {
			LongDouble80 ret = *this;
			this->operator++();
			return ret;
		}

		__forceinline LongDouble80 operator--() {
			__asm FLD1;
			LoadToX87(*this);
			SubX87();
			return *this = StoreFromX87<LongDouble80>();
		}

		__forceinline LongDouble80 operator--(int) {
			LongDouble80 ret = *this;
			this->operator--();
			return ret;
		}

#endif // ifdef __cplusplus

// Still need to end the C version of the struct, so...
	};

#ifdef __cplusplus

	template<typename T1, typename T2>
	__forceinline std::enable_if_t<ValidFPArithmetic(T1, T2), LongDouble80> operator+(T1 left, T2 right) {
		X87Op(Add, left, right);
		return StoreFromX87<LongDouble80>();
	}

	template<>
	__forceinline LongDouble80 operator+(LongDouble80 left, bool right) {
		return right ? ++left : left;
	}

	template<typename T1, typename T2>
	__forceinline std::enable_if_t<ValidFPArithmetic(T1, T2), LongDouble80> operator-(T1 left, T2 right) {
		X87Op(Sub, left, right);
		return StoreFromX87<LongDouble80>();
	}

	template<>
	__forceinline LongDouble80 operator-(LongDouble80 left, bool right) {
		return right ? --left : left;
	}

	template<typename T1, typename T2>
	__forceinline std::enable_if_t<ValidFPArithmetic(T1, T2), LongDouble80> operator*(T1 left, T2 right) {
		X87Op(Mul, left, right);
		return StoreFromX87<LongDouble80>();
	}

	template<>
	__forceinline LongDouble80 operator*(LongDouble80 left, bool right) {
		return right ? left : left = { 0 };
	}

	template<typename T1, typename T2>
	__forceinline std::enable_if_t<ValidFPArithmetic(T1, T2), LongDouble80> operator/(T1 left, T2 right) {
		X87Op(Div, left, right);
		return StoreFromX87<LongDouble80>();
	}

	template<>
	__forceinline LongDouble80 operator/(LongDouble80 left, bool right) {
		return right ? left : left = { 0, 0, 0, 0, 0, 0, 0, 0x80, 0xFF, 0x7F };
	}

	template<typename T1, typename T2>
	__forceinline std::enable_if_t<ValidFPArithmetic(T1, T2), bool> operator==(T1 left, T2 right) {
		bool _ret;
		X87Op(Comp, left, right);
		__asm SETE BYTE PTR[_ret];
		return _ret;
	}

	template<>
	__forceinline bool operator==(LongDouble80 left, LongDouble80 right) {
		return memcmp(&left, &right, sizeof(LongDouble80)) == 0;
	}

	template<typename T1, typename T2>
	__forceinline std::enable_if_t<ValidFPArithmetic(T1, T2), bool> operator!=(T1 left, T2 right) {
		bool _ret;
		X87Op(Comp, left, right);
		__asm SETNE BYTE PTR[_ret];
		return _ret;
	}

	template<>
	__forceinline bool operator!=(LongDouble80 left, LongDouble80 right) {
		return memcmp(&left, &right, sizeof(LongDouble80)) != 0;
	}

	template<typename T1, typename T2>
	__forceinline std::enable_if_t<ValidFPArithmetic(T1, T2), bool> operator<(T1 left, T2 right) {
		bool _ret;
		X87Op(Comp, left, right);
		__asm SETB BYTE PTR[_ret];
		return _ret;
	}

	template<typename T1, typename T2>
	__forceinline std::enable_if_t<ValidFPArithmetic(T1, T2), bool> operator<=(T1 left, T2 right) {
		bool _ret;
		X87Op(Comp, left, right);
		__asm SETBE BYTE PTR[_ret];
		return _ret;
	}

	template<typename T1, typename T2>
	__forceinline std::enable_if_t<ValidFPArithmetic(T1, T2), bool> operator>(T1 left, T2 right) {
		bool _ret;
		X87Op(Comp, left, right);
		__asm SETA BYTE PTR[_ret];
		return _ret;
	}

	template<typename T1, typename T2>
	__forceinline std::enable_if_t<ValidFPArithmetic(T1, T2), bool> operator>=(T1 left, T2 right) {
		bool _ret;
		X87Op(Comp, left, right);
		__asm SETAE BYTE PTR[_ret];
		return _ret;
	}

	template<typename T>
	__forceinline std::enable_if_t<std::is_integral_v<T>, T> operator<<(T left, LongDouble80 right) {
		return left << (T)right;
	}
	template<typename T>
	__forceinline std::enable_if_t<std::is_integral_v<T>, T> operator<<=(T left, LongDouble80 right) {
		return left <<= (T)right;
	}

	template<typename T>
	__forceinline std::enable_if_t<std::is_integral_v<T>, T> operator>>(T left, LongDouble80 right) {
		return left >> (T)right;
	}
	template<typename T>
	__forceinline std::enable_if_t<std::is_integral_v<T>, T> operator>>=(T left, LongDouble80 right) {
		return left >>= (T)right;
	}
} // extern "C++"

//extern "C" {
//
//#define fmodl fmodld
//	inline LongDouble80 __CRTDECL fmodld(LongDouble80 _X, LongDouble80 _Y) {
//		LoadToX87(_X);
//		LoadToX87(_Y);
//		__asm {
//			FPREM;
//			FSTP ST(1);
//		}
//		return StoreFromX87<LongDouble80>();
//	}
//
//#define remainderl remainderld
//	inline LongDouble80 __CRTDECL remainderld(LongDouble80 _X, LongDouble80 _Y) {
//		LoadToX87(_X);
//		LoadToX87(_Y);
//		__asm {
//			FPREM1;
//			FSTP ST(1);
//		}
//		return StoreFromX87<LongDouble80>();
//	}
//
//#define fabsl fabsld
//	inline LongDouble80 __CRTDECL fabsld(LongDouble80 _X) {
//		LoadToX87(_X);
//		__asm FABS;
//		return StoreFromX87<LongDouble80>();
//	}
//
//#define sinl sinld
//	inline LongDouble80 __CRTDECL sinld(LongDouble80 _X) {
//		LoadToX87(_X);
//		__asm FSIN;
//		return StoreFromX87<LongDouble80>();
//	}
//
//#define cosl cosld
//	inline LongDouble80 __CRTDECL cosld(LongDouble80 _X) {
//		LoadToX87(_X);
//		__asm FCOS;
//		return StoreFromX87<LongDouble80>();
//	}
//
//#define tanl tanld
//	inline LongDouble80 __CRTDECL tanld(LongDouble80 _X) {
//		LoadToX87(_X);
//		__asm {
//			FPTAN;
//			FSTP ST(0);
//		}
//		return StoreFromX87<LongDouble80>();
//	}
//
//#define sqrtl sqrtld
//	inline LongDouble80 __CRTDECL sqrtld(LongDouble80 _X) {
//		LoadToX87(_X);
//		__asm FSQRT;
//		return StoreFromX87<LongDouble80>();
//	}
//
//} // extern "C"

#undef AddX87
#undef SubX87
#undef MulX87
#undef DivX87
#undef CompX87
#undef X87Op

#undef ValidFPArithmetic
#undef ValidFPValue

#endif // ifdef __cplusplus

#endif // if !(defined(_MSC_VER) && !defined(_LDSUPPORT))
