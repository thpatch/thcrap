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
				__asm FILD QWORD PTR[t_val];
				if (*(int64_t*)&t_val < 0) {
					static constexpr double UINT_MAX_PLUS1 = 4294967296.0;
					__asm FADD QWORD PTR[UINT_MAX_PLUS1];
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

// LongDouble80NonModLVal Destructor cleanup
#define AddX87() __asm { __asm FADD ST(0), ST(1) }
#define SubX87() __asm { __asm FSUB ST(0), ST(1) }
#define MulX87() __asm { __asm FMUL ST(0), ST(1) }
#define DivX87() __asm { __asm FDIV ST(0), ST(1) }

// Built-in cleanup
#define AddPX87() __asm { __asm FADDP ST(1), ST }
#define SubPX87() __asm { __asm FSUBP ST(1), ST }
#define MulPX87() __asm { __asm FMULP ST(1), ST }
#define DivPX87() __asm { __asm FDIVP ST(1), ST }

// Built-in cleanup
#define PAddX87() __asm { __asm FSTP ST(1) AddX87() }
#define PSubX87() __asm { __asm FSTP ST(1) SubX87() }
#define PMulX87() __asm { __asm FSTP ST(1) MulX87() }
#define PDivX87() __asm { __asm FSTP ST(1) DivX87() }

#define IncX87() __asm { __asm FLD1 AddPX87() }
#define DecX87() __asm { __asm FLD1 SubPX87() }

#define ComIX87(cond, ret) __asm { __asm FCOMI ST, ST(1) __asm SET ## cond BYTE PTR [ret] }
#define ComIPX87(cond, ret) __asm { __asm FCOMIP ST, ST(1) __asm SET ## cond BYTE PTR [ret] }
#define ComIPPX87(cond, ret) __asm { __asm FCOMIP ST, ST(1) __asm SET ## cond BYTE PTR [ret] __asm FSTP ST(0) }

#define X87OneOp(op, val) \
		LoadToX87(val); \
		op ## X87();

#define X87Op(op, lh, rh) \
		LoadToX87(rh); \
		X87OneOp(op, lh);

#define X87OneComSetCC(op, cond, ret, val) \
		LoadToX87(val); \
		op ## X87(cond, ret);

#define X87ComSetCC(op, cond, ret, lh, rh) \
		LoadToX87(rh); \
		X87OneComSetCC(op, cond, ret, lh);

#define X87() static_assert(0, "X87 Error: No op specified!")

	struct LongDouble80NonModLVal {

		__forceinline LongDouble80NonModLVal() noexcept {};
		__forceinline LongDouble80NonModLVal(const LongDouble80NonModLVal&) noexcept = delete;
		__forceinline LongDouble80NonModLVal& operator=(const LongDouble80NonModLVal&) noexcept = delete;
		__forceinline LongDouble80NonModLVal(LongDouble80NonModLVal&&) noexcept = delete;
		__forceinline LongDouble80NonModLVal& operator=(LongDouble80NonModLVal&&) noexcept = delete;

		template<typename T = void, typename = std::enable_if_t<ValidFPValue(T), T>>
		__forceinline operator T() noexcept {
			if constexpr (std::is_same_v<T, LongDouble80>) {
				__asm FLD ST(0);
			}
			return StoreFromX87<T>();
		}

#ifdef ALLOW_UNSAFE_LONG_DOUBLE_OPS
#define X87LValChain
#else
#define X87LValChain const
#endif

		__forceinline X87LValChain LongDouble80NonModLVal& operator+(LongDouble80NonModLVal& right) {
			AddX87();
			return right;
		}

		__forceinline X87LValChain LongDouble80NonModLVal& operator-(const LongDouble80NonModLVal& right) {
			SubX87();
			return right;
		}

		__forceinline X87LValChain LongDouble80NonModLVal& operator*(const LongDouble80NonModLVal& right) {
			MulX87();
			return right;
		}

		__forceinline X87LValChain LongDouble80NonModLVal& operator/(const LongDouble80NonModLVal& right) {
			DivX87();
			return right;
		}

		__forceinline bool operator==(const LongDouble80NonModLVal& right) {
			bool _ret;
			ComIX87(E, _ret);
			return _ret;
		}

		__forceinline bool operator!=(const LongDouble80NonModLVal& right) {
			bool _ret;
			ComIX87(NE, _ret);
			return _ret;
		}

		__forceinline bool operator<(const LongDouble80NonModLVal& right) {
			bool _ret;
			ComIX87(B, _ret);
			return _ret;
		}

		__forceinline bool operator<=(const LongDouble80NonModLVal& right) {
			bool _ret;
			ComIX87(BE, _ret);
			return _ret;
		}

		__forceinline bool operator>(const LongDouble80NonModLVal& right) {
			bool _ret;
			ComIX87(A, _ret);
			return _ret;
		}

		__forceinline bool operator>=(const LongDouble80NonModLVal& right) {
			bool _ret;
			ComIX87(AE, _ret);
			return _ret;
		}

		__forceinline ~LongDouble80NonModLVal(void) {
			__asm FSTP ST(0);
		}
	};

#endif // ifdef __cplusplus

// Make sure that the struct is still visible without
// C++ so that the compiler doesn't complain about
// incompatible linkage crap.
	struct LongDouble80 {

		_LDOUBLE ld;

#ifdef __cplusplus

#ifdef INCLUDE_LONG_DOUBLE_CONSTRUCTORS

		LongDouble80() = default;

		template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>, T>>
		__forceinline constexpr LongDouble80(T right) {
			*this = right;
		}

		template <typename... T, typename = std::enable_if_t<sizeof...(T) <= 10 && (... && std::is_integral_v<T>)>>
		__forceinline constexpr LongDouble80(T... right) {
			this->ld = { (unsigned char)right... };
		}

		__forceinline constexpr LongDouble80(_LDOUBLE right) : ld(right) {}

#endif

		__forceinline operator bool() {
			bool _ret;
			X87OneComSetCC(ComIP, Z, _ret, *this);
			return _ret;
		}

		template<typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>, T>>
		__forceinline operator T() {
			LoadToX87(*this);
			T ret = StoreFromX87<T>();
			return ret;
		}

		template<typename T>
		__forceinline std::enable_if_t<std::is_arithmetic_v<T>, LongDouble80> operator=(T right) {
			LoadToX87(right);
			return *this = StoreFromX87<LongDouble80>();;
		}

		__forceinline LongDouble80 operator=(_LDOUBLE right) {
			this->ld = right;
			return *this;
		}

		__forceinline LongDouble80 operator+() {
			return *this;
		}

		__forceinline LongDouble80 operator-() {
			LongDouble80 _ret = *this;
			_ret.ld.ld[9] ^= (unsigned char)0b10000000; // Flip sign bit
			return _ret;
		}

		__forceinline LongDouble80 operator+=(LongDouble80NonModLVal& right) {
			X87OneOp(Add, *this);
			return *this = StoreFromX87<LongDouble80>();
		}

		template<typename T>
		__forceinline std::enable_if_t<ValidFPValue(T), LongDouble80> operator+=(T right) {
			X87Op(AddP, *this, right);
			return *this = StoreFromX87<LongDouble80>();
		}

		__forceinline LongDouble80 operator-=(LongDouble80NonModLVal& right) {
			X87OneOp(Sub, *this);
			return *this = StoreFromX87<LongDouble80>();
		}

		template<typename T>
		__forceinline std::enable_if_t<ValidFPValue(T), LongDouble80> operator-=(T right) {
			X87Op(SubP, *this, right);
			return *this = StoreFromX87<LongDouble80>();
		}

		__forceinline LongDouble80 operator*=(LongDouble80NonModLVal& right) {
			X87OneOp(Mul, *this);
			return *this = StoreFromX87<LongDouble80>();
		}

		template<typename T>
		__forceinline std::enable_if_t<ValidFPValue(T), LongDouble80> operator*=(T right) {
			X87Op(MulP, *this, right);
			return *this = StoreFromX87<LongDouble80>();
		}

		__forceinline LongDouble80 operator/=(LongDouble80NonModLVal& right) {
			X87OneOp(Div, *this);
			return *this = StoreFromX87<LongDouble80>();
		}

		template<typename T>
		__forceinline std::enable_if_t<ValidFPValue(T), LongDouble80> operator/=(T right) {
			X87Op(DivP, *this, right);
			return *this = StoreFromX87<LongDouble80>();
		}

		__forceinline LongDouble80 operator++() {
			X87OneOp(Inc, *this);
			return *this = StoreFromX87<LongDouble80>();
		}

		__forceinline LongDouble80 operator++(int) {
			LongDouble80 ret = *this;
			++*this;
			return ret;
		}

		__forceinline LongDouble80 operator--() {
			X87OneOp(Dec, *this);
			return *this = StoreFromX87<LongDouble80>();
		}

		__forceinline LongDouble80 operator--(int) {
			LongDouble80 ret = *this;
			--*this;
			return ret;
		}

#endif // ifdef __cplusplus

// Still need to end the C version of the struct, so...
	};

#ifdef __cplusplus

	template<typename T1, typename T2>
	__forceinline std::enable_if_t<ValidFPArithmetic(T1, T2), LongDouble80NonModLVal> operator+(T1 left, T2 right) {
		X87Op(AddP, left, right);
		return {};
	}

	template <typename T>
	__forceinline std::enable_if_t<ValidFPValue(T), LongDouble80NonModLVal&> operator+(LongDouble80NonModLVal& left, T right) {
		X87OneOp(AddP, right);
		return left;
	}

	template <typename T>
	__forceinline std::enable_if_t<ValidFPValue(T), LongDouble80NonModLVal&> operator+(T left, LongDouble80NonModLVal& right) {
		X87OneOp(AddP, left);
		return right;
	}

	template<typename T1, typename T2>
	__forceinline std::enable_if_t<std::is_same_v<T1, LongDouble80> && std::is_same_v<T2, bool>, LongDouble80&> operator+(LongDouble80 left, bool right) {
		return right ? ++left : left;
	}

	template<typename T1, typename T2>
	__forceinline std::enable_if_t<ValidFPArithmetic(T1, T2), LongDouble80NonModLVal> operator-(T1 left, T2 right) {
		X87Op(SubP, left, right);
		return {};
	}

	template <typename T>
	__forceinline std::enable_if_t<ValidFPValue(T), LongDouble80NonModLVal&> operator-(LongDouble80NonModLVal& left, T right) {
		X87OneOp(SubP, right);
		return left;
	}

	template <typename T>
	__forceinline std::enable_if_t<ValidFPValue(T), LongDouble80NonModLVal&> operator-(T left, LongDouble80NonModLVal& right) {
		X87OneOp(SubP, left);
		return right;
	}

	template<typename T1, typename T2>
	__forceinline std::enable_if_t<std::is_same_v<T1, LongDouble80> && std::is_same_v<T2, bool>, LongDouble80&> operator-(LongDouble80 left, bool right) {
		return right ? --left : left;
	}

	template<typename T1, typename T2>
	__forceinline std::enable_if_t<ValidFPArithmetic(T1, T2), LongDouble80NonModLVal> operator*(T1 left, T2 right) {
		X87Op(MulP, left, right);
		return {};
	}

	template <typename T>
	__forceinline std::enable_if_t<ValidFPValue(T), LongDouble80NonModLVal&> operator*(LongDouble80NonModLVal& left, T right) {
		X87OneOp(MulP, right);
		return left;
	}

	template <typename T>
	__forceinline std::enable_if_t<ValidFPValue(T), LongDouble80NonModLVal&> operator*(T left, LongDouble80NonModLVal& right) {
		X87OneOp(MulP, left);
		return right;
	}

	template<typename T1, typename T2>
	__forceinline std::enable_if_t<std::is_same_v<T1, LongDouble80> && std::is_same_v<T2, bool>, LongDouble80&> operator*(LongDouble80 left, bool right) {
		return right ? left : left = _LDOUBLE{ 0 };
	}

	template<typename T1, typename T2>
	__forceinline std::enable_if_t<ValidFPArithmetic(T1, T2), LongDouble80NonModLVal> operator/(T1 left, T2 right) {
		X87Op(DivP, left, right);
		return {};
	}

	template <typename T>
	__forceinline std::enable_if_t<ValidFPValue(T), LongDouble80NonModLVal&> operator/(LongDouble80NonModLVal& left, T right) {
		X87OneOp(DivP, right);
		return left;
	}

	template <typename T>
	__forceinline std::enable_if_t<ValidFPValue(T), LongDouble80NonModLVal&> operator/(T left, LongDouble80NonModLVal& right) {
		X87OneOp(DivP, left);
		return right;
	}

	template<typename T1, typename T2>
	__forceinline std::enable_if_t<std::is_same_v<T1, LongDouble80> && std::is_same_v<T2, bool>, LongDouble80&> operator/(LongDouble80 left, bool right) {
		return right ? left : left = _LDOUBLE{ 0, 0, 0, 0, 0, 0, 0, 0x80, 0xFF, 0x7F };
	}

	template<typename T1, typename T2>
	__forceinline std::enable_if_t<ValidFPArithmetic(T1, T2), bool> operator==(T1 left, T2 right) {
		bool _ret;
		X87ComSetCC(ComIPP, E, _ret, left, right);
		return _ret;
	}

	template <typename T>
	__forceinline std::enable_if_t<ValidFPValue(T), bool> operator==(LongDouble80NonModLVal& left, T right) {
		bool _ret;
		X87OneComSetCC(ComIPP, E, _ret, right);
		return _ret;
	}

	template <typename T>
	__forceinline std::enable_if_t<ValidFPValue(T), bool> operator==(T left, LongDouble80NonModLVal& right) {
		bool _ret;
		X87OneComSetCC(ComIPP, E, _ret, left);
		return _ret;
	}

	template<>
	__forceinline bool operator==(LongDouble80 left, LongDouble80 right) {
		return memcmp(&left, &right, sizeof(LongDouble80)) == 0;
	}

	template<typename T1, typename T2>
	__forceinline std::enable_if_t<ValidFPArithmetic(T1, T2), bool> operator!=(T1 left, T2 right) {
		bool _ret;
		X87ComSetCC(ComIPP, NE, _ret, left, right);
		return _ret;
	}

	template <typename T>
	__forceinline std::enable_if_t<ValidFPValue(T), bool> operator!=(LongDouble80NonModLVal& left, T right) {
		bool _ret;
		X87OneComSetCC(ComIPP, NE, _ret, right);
		return _ret;
	}

	template <typename T>
	__forceinline std::enable_if_t<ValidFPValue(T), bool> operator!=(T left, LongDouble80NonModLVal& right) {
		bool _ret;
		X87OneComSetCC(ComIPP, NE, _ret, left);
		return _ret;
	}

	template<>
	__forceinline bool operator!=(LongDouble80 left, LongDouble80 right) {
		return memcmp(&left, &right, sizeof(LongDouble80)) != 0;
	}

	template<typename T1, typename T2>
	__forceinline std::enable_if_t<ValidFPArithmetic(T1, T2), bool> operator<(T1 left, T2 right) {
		bool _ret;
		X87ComSetCC(ComIPP, B, _ret, left, right);
		return _ret;
	}

	template <typename T>
	__forceinline std::enable_if_t<ValidFPValue(T), bool> operator<(LongDouble80NonModLVal& left, T right) {
		bool _ret;
		X87OneComSetCC(ComIPP, B, _ret, right);
		return _ret;
	}

	template <typename T>
	__forceinline std::enable_if_t<ValidFPValue(T), bool> operator<(T left, LongDouble80NonModLVal& right) {
		bool _ret;
		X87OneComSetCC(ComIPP, AE, _ret, left);
		return _ret;
	}

	template<typename T1, typename T2>
	__forceinline std::enable_if_t<ValidFPArithmetic(T1, T2), bool> operator<=(T1 left, T2 right) {
		bool _ret;
		X87ComSetCC(ComIPP, BE, _ret, left, right);
		return _ret;
	}

	template <typename T>
	__forceinline std::enable_if_t<ValidFPValue(T), bool> operator<=(LongDouble80NonModLVal& left, T right) {
		bool _ret;
		X87OneComSetCC(ComIPP, BE, _ret, right);
		return _ret;
	}

	template <typename T>
	__forceinline std::enable_if_t<ValidFPValue(T), bool> operator<=(T left, LongDouble80NonModLVal& right) {
		bool _ret;
		X87OneComSetCC(ComIPP, A, _ret, left);
		return _ret;
	}

	template<typename T1, typename T2>
	__forceinline std::enable_if_t<ValidFPArithmetic(T1, T2), bool> operator>(T1 left, T2 right) {
		bool _ret;
		X87ComSetCC(ComIPP, A, _ret, left, right);
		return _ret;
	}

	template <typename T>
	__forceinline std::enable_if_t<ValidFPValue(T), bool> operator>(LongDouble80NonModLVal& left, T right) {
		bool _ret;
		X87OneComSetCC(ComIPP, A, _ret, right);
		return _ret;
	}

	template <typename T>
	__forceinline std::enable_if_t<ValidFPValue(T), bool> operator>(T left, LongDouble80NonModLVal& right) {
		bool _ret;
		X87OneComSetCC(ComIPP, BE, _ret, left);
		return _ret;
	}

	template<typename T1, typename T2>
	__forceinline std::enable_if_t<ValidFPArithmetic(T1, T2), bool> operator>=(T1 left, T2 right) {
		bool _ret;
		X87ComSetCC(ComIPP, AE, _ret, left, right);
		return _ret;
	}

	template <typename T>
	__forceinline std::enable_if_t<ValidFPValue(T), bool> operator>=(LongDouble80NonModLVal& left, T right) {
		bool _ret;
		X87OneComSetCC(ComIPP, AE, _ret, right);
		return _ret;
	}

	template <typename T>
	__forceinline std::enable_if_t<ValidFPValue(T), bool> operator>=(T left, LongDouble80NonModLVal& right) {
		bool _ret;
		X87OneComSetCC(ComIPP, B, _ret, left);
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
//	inline LongDouble80& __CRTDECL fabsld(LongDouble80& _X) {
//		_X[9] &= '\x7F';
//		return _X;
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
#undef AddPX87
#undef SubX87
#undef SubPX87
#undef MulX87
#undef MulPX87
#undef DivX87
#undef DivPX87
#undef IncX87
#undef DecX87
#undef ComIX87
#undef ComIPX87
#undef ComIPPX87
#undef X87Op
#undef X87OneOp
#undef X87ComSetCC
#undef X87OneComSetCC
#undef X87

#undef ValidFPArithmetic
#undef ValidFPValue

#endif // ifdef __cplusplus

#endif // if !(defined(_MSC_VER) && !defined(_LDSUPPORT))

#ifdef __cplusplus
extern "C++" {

	__forceinline LongDouble80 operator"" _ld(unsigned long long int value) {
		LongDouble80 ret;
		return ret = value;
	}

	__forceinline LongDouble80 operator"" _ld(long double value) {
		LongDouble80 ret;
		return ret = value;
	}
}
#endif
