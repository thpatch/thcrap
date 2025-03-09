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
#if !(defined(_MSC_VER) && !defined(_LDSUPPORT)) || defined(TH_X64)

// Somehow it's actually defined, so this can
// just be a typedef and nothing else is needed.
typedef long double LongDouble80;

#else
#include <stdint.h>

// It isn't supported, so time to make it work anyway
// with C++ template shenanigans and other BS.

typedef struct LongDouble80 LongDouble80;

#ifdef __cplusplus
extern "C++" {

template <typename T>
inline constexpr bool is_valid_x87_type = std::is_same_v<T, LongDouble80> || std::is_arithmetic_v<T>;

#define sfinae_enable(...) std::enable_if_t<(__VA_ARGS__), bool> = true

#define ValidFPValue(T) \
	(std::is_same_v<T, LongDouble80> || std::is_arithmetic_v<T>)

#define ValidFPArithmetic(T1, T2) \
	((std::is_same_v<T1, LongDouble80> || std::is_same_v<T2, LongDouble80>) && \
	ValidFPValue(T1) && ValidFPValue(T2))

// Janky looking workaround to determine which integer size
// is being used without caring about how the compiler defines
// any of the fundamental integer types.
// Normally int/long are considered different types despite
// being the same size.
#define IsAnyIntBelow16(T) \
	(sizeof(T) < sizeof(int16_t))
#define IsSigned16(T) \
	(sizeof(T) >= sizeof(int16_t) && sizeof(T) < sizeof(int32_t) && std::is_signed_v<T>)
#define IsSigned32(T) \
	(sizeof(T) >= sizeof(int32_t) && sizeof(T) < sizeof(int64_t) && std::is_signed_v<T>)
#define IsSigned64(T) \
	(sizeof(T) == sizeof(int64_t) && std::is_signed_v<T>)
#define IsUnsigned16(T) \
	(sizeof(T) >= sizeof(uint16_t) && sizeof(T) < sizeof(uint32_t) && std::is_unsigned_v<T>)
#define IsUnsigned32(T) \
	(sizeof(T) >= sizeof(uint32_t) && sizeof(T) < sizeof(uint64_t) && std::is_unsigned_v<T>)
#define IsUnsigned64(T) \
	(sizeof(T) == sizeof(uint64_t) && std::is_unsigned_v<T>)

#define IsValidMemOpInt(T) \
	(IsSigned16(T) || IsSigned32(T))
#define IsValidMemOpFloat(T) \
	(std::is_same_v<T, float> || std::is_same_v<T, double>)
#define IsValidMemOp(T) \
	(IsValidMemOpInt(T) || IsValidMemOpFloat(T))

#define ValidFPMemOp(T1, T2) \
	(std::is_same_v<T1, LongDouble80> && IsValidMemOp(T2)) || (std::is_same_v<T2, LongDouble80> && IsValidMemOp(T1))

	template <typename T, sfinae_enable(is_valid_x87_type<T>)>
	TH_FORCEINLINE void LoadToX87(T t_val) {
		if constexpr (std::is_same_v<T, LongDouble80>) {
			__asm FLD TBYTE PTR[t_val];
		} else if constexpr (std::is_floating_point_v<T>) {
			__asm FLD[t_val];
		} else if constexpr (std::is_integral_v<T>) {
			if constexpr (IsAnyIntBelow16(T) || IsUnsigned16(T)) {
				int32_t temp = (int32_t)t_val;
				__asm FILD DWORD PTR[temp];
			} else if constexpr (IsSigned16(T) || IsSigned32(T) || IsSigned64(T)) {
				__asm FILD[t_val];
			} else if constexpr (IsUnsigned32(T)) {
				int64_t temp = (int64_t)t_val;
				__asm FILD QWORD PTR[temp];
			} else if constexpr (IsUnsigned64(T)) {
				__asm FILD QWORD PTR[t_val];
				if (((int32_t*)&t_val)[1] < 0) {
					static constexpr float_t MAGIC_VALUE_A = 0x1.0p64;
					__asm FADD[MAGIC_VALUE_A];
				}
			} else {
				static_assert(0, "Invalid integer size");
			}
		} else {
			static_assert(0, "Invalid arithmetic type");
		}
	}

	template <typename T, sfinae_enable(ValidFPValue(T))>
	TH_FORCEINLINE T ReadFromX87(void) {
		T t_val;
		if constexpr (std::is_same_v<T, LongDouble80>) {
			__asm {
				FLD ST(0);
				FSTP TBYTE PTR[t_val];
			}
		} else if constexpr (std::is_floating_point_v<T>) {
			__asm FST[t_val];
		} else if constexpr (std::is_integral_v<T>) {
			if constexpr (IsAnyIntBelow16(T) || IsUnsigned16(T)) {
				int32_t temp;
				__asm FIST DWORD PTR[temp];
				t_val = (T)temp;
			} else if constexpr (IsSigned16(T) || IsSigned32(T)) {
				__asm FIST[t_val];
			} else if constexpr (IsSigned64(T)) {
				__asm {
					FLD ST(0);
					FSTP QWORD PTR[t_val];
				}
			} else if constexpr (IsUnsigned32(T)) {
				int64_t temp;
				__asm {
					FLD ST(0);
					FISTP QWORD PTR[temp];
				}
				t_val = (T)temp;
			} else if constexpr (IsUnsigned64(T)) {
				static constexpr float_t MAGIC_VALUE_A = 0x1.0p64;
				static constexpr float_t MAGIC_VALUE_B = 0x1.0p63;
				__asm {
					FLD[MAGIC_VALUE_B];
					FUCOMIP ST, ST(1);
					FLD ST(0);
					JA RequireU64;
					FSUB[MAGIC_VALUE_A];
				RequireU64:
					FISTP QWORD PTR[t_val];
				}
			} else {
				static_assert(0, "Invalid integer size");
			}
		} else {
			static_assert(0, "Invalid arithmetic type");
		}
		return t_val;
	}

	template <typename T, sfinae_enable(ValidFPValue(T))>
	TH_FORCEINLINE T StoreFromX87(void) {
		T t_val;
		if constexpr (std::is_same_v<T, LongDouble80>) {
			__asm FSTP TBYTE PTR[t_val];
		} else if constexpr (std::is_floating_point_v<T>) {
			__asm FSTP[t_val];
		} else if constexpr (std::is_integral_v<T>) {
			if constexpr (IsAnyIntBelow16(T) || IsUnsigned16(T)) {
				int32_t temp;
				__asm FISTP DWORD PTR[temp];
				t_val = (T)temp;
			} else if constexpr (IsSigned16(T) || IsSigned32(T) || IsSigned64(T)) {
				__asm FISTP[t_val];
			} else if constexpr (IsUnsigned32(T)) {
				int64_t temp;
				__asm FISTP QWORD PTR[temp];
				t_val = (T)temp;
			} else if constexpr (IsUnsigned64(T)) {
				static constexpr float_t MAGIC_VALUE_A = 0x1.0p64;
				static constexpr float_t MAGIC_VALUE_B = 0x1.0p63;
				__asm {
					FLD[MAGIC_VALUE_B];
					FUCOMIP ST, ST(1);
					JA RequireU64;
					FSUB[MAGIC_VALUE_A];
				RequireU64:
					FISTP QWORD PTR[t_val];
				}
			} else {
				static_assert(0, "Invalid integer size");
			}
		} else {
			static_assert(0, "Invalid arithmetic type");
		}
		return t_val;
	}

// LongDouble80NonModLVal Destructor cleanup
#define AddX87() __asm { __asm FADD ST(0), ST(1) }
#define SubX87() __asm { __asm FSUB ST(0), ST(1) }
#define MulX87() __asm { __asm FMUL ST(0), ST(1) }
#define DivX87() __asm { __asm FDIV ST(0), ST(1) }

#define AddRX87 AddX87
#define SubRX87() __asm { __asm FSUBR ST(0), ST(1) }
#define MulRX87 MulX87
#define DivRX87() __asm { __asm FDIVR ST(0), ST(1) }

#define AddMemFX87(value) __asm { __asm FADD[value] }
#define AddMemIX87(value) __asm { __asm FIADD[value] }
#define SubMemFX87(value) __asm { __asm FSUB[value] }
#define SubMemIX87(value) __asm { __asm FISUB[value] }
#define MulMemFX87(value) __asm { __asm FMUL[value] }
#define MulMemIX87(value) __asm { __asm FIMUL[value] }
#define DivMemFX87(value) __asm { __asm FDIV[value] }
#define DivMemIX87(value) __asm { __asm FIDIV[value] }

#define AddRMemFX87 AddMemFX87
#define AddRMemIX87 AddMemIX87
#define SubRMemFX87(value) __asm { __asm FSUBR[value] }
#define SubRMemIX87(value) __asm { __asm FISUBR[value] }
#define MulRMemFX87 MulMemFX87
#define MulRMemIX87 MulMemIX87
#define DivRMemFX87(value) __asm { __asm FDIVR[value] }
#define DivRMemIX87(value) __asm { __asm FIDIVR[value] }

// Built-in cleanup
#define AddPX87() __asm { __asm FADDP ST(1), ST }
#define SubPX87() __asm { __asm FSUBP ST(1), ST }
#define MulPX87() __asm { __asm FMULP ST(1), ST }
#define DivPX87() __asm { __asm FDIVP ST(1), ST }

#define AddRPX87 AddPX87
#define SubRPX87() __asm { __asm FSUBRP ST(1), ST }
#define MulRPX87 MulPX87
#define DivRPX87() __asm { __asm FDIVRP ST(1), ST }

// Built-in cleanup
#define PAddX87() __asm { __asm FSTP ST(1) AddX87() }
#define PSubX87() __asm { __asm FSTP ST(1) SubX87() }
#define PMulX87() __asm { __asm FSTP ST(1) MulX87() }
#define PDivX87() __asm { __asm FSTP ST(1) DivX87() }

#define IncX87() __asm { __asm FLD1 AddPX87() }
#define DecX87() __asm { __asm FLD1 SubPX87() }

#define ComIX87(cond, ret) __asm { __asm FUCOMI ST, ST(1) __asm SET ## cond BYTE PTR [ret] }
#define ComIPX87(cond, ret) __asm { __asm FUCOMIP ST, ST(1) __asm SET ## cond BYTE PTR [ret] }
#define ComIPPX87(cond, ret) __asm { __asm FUCOMIP ST, ST(1) __asm SET ## cond BYTE PTR [ret] __asm FSTP ST(0) }

#define X87OneOp(op, val) \
		LoadToX87(val); \
		op ## X87();

#define X87Op(op, lh, rh) \
		LoadToX87(rh); \
		X87OneOp(op, lh);

#define X87PossibleMemOneOp(op, val) \
		if constexpr (IsValidMemOpInt(decltype(val))) { \
			op ## MemIX87(val); \
		} \
		else if constexpr (IsValidMemOpFloat(decltype(val))) { \
			op ## MemFX87(val); \
		} \
		else { \
			X87OneOp(op ## P, val); \
		}

#define X87PossibleMemOp(op, lh, rh) \
		if constexpr (ValidFPMemOp(decltype(lh),decltype(rh))) { \
			if constexpr (std::is_same_v<LongDouble80,decltype(lh)>) { \
				LoadToX87(lh); \
				X87PossibleMemOneOp(op, rh); \
			} else if constexpr (std::is_same_v<LongDouble80,decltype(rh)>) {\
				LoadToX87(rh); \
				X87PossibleMemOneOp(op ## R, lh); \
			} \
		} else { \
			X87Op(op ## P, lh, rh); \
		}

#define X87OneComSetCC(op, cond, ret, val) \
		LoadToX87(val); \
		op ## X87(cond, ret);

#define X87ComSetCC(op, cond, ret, lh, rh) \
		LoadToX87(rh); \
		X87OneComSetCC(op, cond, ret, lh);

#define X87() static_assert(0, "X87 Error: No op specified!")

	struct LongDouble80NonModLVal {

		TH_FORCEINLINE LongDouble80NonModLVal() noexcept {};
		TH_FORCEINLINE LongDouble80NonModLVal(const LongDouble80NonModLVal&) noexcept = delete;
		TH_FORCEINLINE LongDouble80NonModLVal& operator=(const LongDouble80NonModLVal&) noexcept = delete;
		TH_FORCEINLINE LongDouble80NonModLVal(LongDouble80NonModLVal&&) noexcept = delete;
		TH_FORCEINLINE LongDouble80NonModLVal& operator=(LongDouble80NonModLVal&&) noexcept = delete;

		template<typename T = void, sfinae_enable(ValidFPValue(T))>
		TH_FORCEINLINE operator T() noexcept {
			if constexpr (std::is_same_v<T, LongDouble80>) {
				__asm FLD ST(0);
			}
			return StoreFromX87<T, false>();
		}

#ifdef ALLOW_UNSAFE_LONG_DOUBLE_OPS
#define X87LValChain
#else
#define X87LValChain const
#endif

		TH_FORCEINLINE X87LValChain LongDouble80NonModLVal& operator+(const LongDouble80NonModLVal& right) {
			AddX87();
			return right;
		}

		TH_FORCEINLINE X87LValChain LongDouble80NonModLVal& operator-(const LongDouble80NonModLVal& right) {
			SubX87();
			return right;
		}

		TH_FORCEINLINE X87LValChain LongDouble80NonModLVal& operator*(const LongDouble80NonModLVal& right) {
			MulX87();
			return right;
		}

		TH_FORCEINLINE X87LValChain LongDouble80NonModLVal& operator/(const LongDouble80NonModLVal& right) {
			DivX87();
			return right;
		}

		TH_FORCEINLINE bool operator==(const LongDouble80NonModLVal& right) {
			bool _ret;
			ComIX87(E, _ret);
			return _ret;
		}

		TH_FORCEINLINE bool operator!=(const LongDouble80NonModLVal& right) {
			bool _ret;
			ComIX87(NE, _ret);
			return _ret;
		}

		TH_FORCEINLINE bool operator<(const LongDouble80NonModLVal& right) {
			bool _ret;
			ComIX87(B, _ret);
			return _ret;
		}

		TH_FORCEINLINE bool operator<=(const LongDouble80NonModLVal& right) {
			bool _ret;
			ComIX87(BE, _ret);
			return _ret;
		}

		TH_FORCEINLINE bool operator>(const LongDouble80NonModLVal& right) {
			bool _ret;
			ComIX87(A, _ret);
			return _ret;
		}

		TH_FORCEINLINE bool operator>=(const LongDouble80NonModLVal& right) {
			bool _ret;
			ComIX87(AE, _ret);
			return _ret;
		}

		TH_FORCEINLINE ~LongDouble80NonModLVal(void) {
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

		template <typename T, sfinae_enable(std::is_arithmetic_v<T>)>
		TH_FORCEINLINE constexpr LongDouble80(T right) {
			*this = right;
		}

		template <typename... T, sfinae_enable(sizeof...(T) <= 10 && (... && std::is_integral_v<T>))>
		TH_FORCEINLINE constexpr LongDouble80(T... right) {
			this->ld = { (unsigned char)right... };
		}

		TH_FORCEINLINE constexpr LongDouble80(_LDOUBLE right) : ld(right) {}

#endif

		TH_FORCEINLINE operator bool() {
			bool _ret;
			__asm {
				FUCOMI ST, ST(0);
				SETZ BYTE PTR[_ret];
			}
			return _ret;
		}

		template<typename T, sfinae_enable(std::is_arithmetic_v<T>)>
		TH_FORCEINLINE operator T() {
			LoadToX87(*this);
			return StoreFromX87<T>();
		}

		template<typename T, sfinae_enable(std::is_arithmetic_v<T>)>
		TH_FORCEINLINE LongDouble80 operator=(T right) {
			LoadToX87(right);
			return *this = StoreFromX87<LongDouble80>();
		}

		TH_FORCEINLINE LongDouble80 operator=(_LDOUBLE right) {
			this->ld = right;
			return *this;
		}

		TH_FORCEINLINE LongDouble80 operator=(const LongDouble80NonModLVal& right) {
			return *this = ReadFromX87<LongDouble80>();
		}

		TH_FORCEINLINE LongDouble80 operator+() {
			return *this;
		}

		TH_FORCEINLINE LongDouble80 operator-() {
			LongDouble80 _ret = *this;
			_ret.ld.ld[9] ^= (unsigned char)0b10000000; // Flip sign bit
			return _ret;
		}

		TH_FORCEINLINE LongDouble80 operator+=(LongDouble80NonModLVal& right) {
			X87OneOp(Add, *this);
			return *this = StoreFromX87<LongDouble80>();
		}

		template<typename T, sfinae_enable(ValidFPValue(T))>
		TH_FORCEINLINE LongDouble80 operator+=(T right) {
			X87Op(AddP, *this, right);
			return *this = StoreFromX87<LongDouble80>();
		}

		TH_FORCEINLINE LongDouble80 operator-=(LongDouble80NonModLVal& right) {
			X87OneOp(Sub, *this);
			return *this = StoreFromX87<LongDouble80>();
		}

		template<typename T, sfinae_enable(ValidFPValue(T))>
		TH_FORCEINLINE LongDouble80 operator-=(T right) {
			X87Op(SubP, *this, right);
			return *this = StoreFromX87<LongDouble80>();
		}

		TH_FORCEINLINE LongDouble80 operator*=(LongDouble80NonModLVal& right) {
			X87OneOp(Mul, *this);
			return *this = StoreFromX87<LongDouble80>();
		}

		template<typename T, sfinae_enable(ValidFPValue(T))>
		TH_FORCEINLINE LongDouble80 operator*=(T right) {
			X87Op(MulP, *this, right);
			return *this = StoreFromX87<LongDouble80>();
		}

		TH_FORCEINLINE LongDouble80 operator/=(LongDouble80NonModLVal& right) {
			X87OneOp(Div, *this);
			return *this = StoreFromX87<LongDouble80>();
		}

		template<typename T, sfinae_enable(ValidFPValue(T))>
		TH_FORCEINLINE LongDouble80 operator/=(T right) {
			X87Op(DivP, *this, right);
			return *this = StoreFromX87<LongDouble80>();
		}

		TH_FORCEINLINE LongDouble80 operator++() {
			X87OneOp(Inc, *this);
			return *this = StoreFromX87<LongDouble80>();
		}

		TH_FORCEINLINE LongDouble80 operator++(int) {
			LongDouble80 ret = *this;
			++*this;
			return ret;
		}

		TH_FORCEINLINE LongDouble80 operator--() {
			X87OneOp(Dec, *this);
			return *this = StoreFromX87<LongDouble80>();
		}

		TH_FORCEINLINE LongDouble80 operator--(int) {
			LongDouble80 ret = *this;
			--*this;
			return ret;
		}

#endif // ifdef __cplusplus

// Still need to end the C version of the struct, so...
	};

#ifdef __cplusplus

	template<typename T1, typename T2, sfinae_enable(ValidFPArithmetic(T1, T2))>
	TH_FORCEINLINE LongDouble80NonModLVal operator+(T1 left, T2 right) {
		X87PossibleMemOp(Add, left, right);
		return {};
	}

	template <typename T, sfinae_enable(ValidFPValue(T))>
	TH_FORCEINLINE LongDouble80NonModLVal& operator+(LongDouble80NonModLVal& left, T right) {
		//X87OneOp(AddP, right);
		X87PossibleMemOneOp(Add, right);
		return left;
	}

	template <typename T, sfinae_enable(ValidFPValue(T))>
	TH_FORCEINLINE LongDouble80NonModLVal& operator+(T left, LongDouble80NonModLVal& right) {
		X87OneOp(AddP, left);
		return right;
	}

	template<typename T1, typename T2, sfinae_enable(std::is_same_v<T1, LongDouble80> && std::is_same_v<T2, bool>)>
	TH_FORCEINLINE LongDouble80& operator+(LongDouble80 left, bool right) {
		return right ? ++left : left;
	}

	template<typename T1, typename T2, sfinae_enable(ValidFPArithmetic(T1, T2))>
	TH_FORCEINLINE LongDouble80NonModLVal operator-(T1 left, T2 right) {
		X87PossibleMemOp(Sub, left, right);
		return {};
	}

	template <typename T, sfinae_enable(ValidFPValue(T))>
	TH_FORCEINLINE LongDouble80NonModLVal& operator-(LongDouble80NonModLVal& left, T right) {
		X87OneOp(SubP, right);
		return left;
	}

	template <typename T, sfinae_enable(ValidFPValue(T))>
	TH_FORCEINLINE LongDouble80NonModLVal& operator-(T left, LongDouble80NonModLVal& right) {
		X87OneOp(SubP, left);
		return right;
	}

	template<typename T1, typename T2, sfinae_enable(std::is_same_v<T1, LongDouble80> && std::is_same_v<T2, bool>)>
	TH_FORCEINLINE LongDouble80& operator-(LongDouble80 left, bool right) {
		return right ? --left : left;
	}

	template<typename T1, typename T2, sfinae_enable(ValidFPArithmetic(T1, T2))>
	TH_FORCEINLINE LongDouble80NonModLVal operator*(T1 left, T2 right) {
		X87PossibleMemOp(Mul, left, right);
		return {};
	}

	template <typename T, sfinae_enable(ValidFPValue(T))>
	TH_FORCEINLINE LongDouble80NonModLVal& operator*(LongDouble80NonModLVal& left, T right) {
		X87OneOp(MulP, right);
		return left;
	}

	template <typename T, sfinae_enable(ValidFPValue(T))>
	TH_FORCEINLINE LongDouble80NonModLVal& operator*(T left, LongDouble80NonModLVal& right) {
		X87OneOp(MulP, left);
		return right;
	}

	template<typename T1, typename T2, sfinae_enable(std::is_same_v<T1, LongDouble80> && std::is_same_v<T2, bool>)>
	TH_FORCEINLINE LongDouble80& operator*(LongDouble80 left, bool right) {
		return right ? left : left = _LDOUBLE{ 0 };
	}

	template<typename T1, typename T2, sfinae_enable(ValidFPArithmetic(T1, T2))>
	TH_FORCEINLINE LongDouble80NonModLVal operator/(T1 left, T2 right) {
		X87PossibleMemOp(Div, left, right);
		return {};
	}

	template <typename T, sfinae_enable(ValidFPValue(T))>
	TH_FORCEINLINE LongDouble80NonModLVal& operator/(LongDouble80NonModLVal& left, T right) {
		X87OneOp(DivP, right);
		return left;
	}

	template <typename T, sfinae_enable(ValidFPValue(T))>
	TH_FORCEINLINE LongDouble80NonModLVal& operator/(T left, LongDouble80NonModLVal& right) {
		X87OneOp(DivP, left);
		return right;
	}

	template<typename T1, typename T2, sfinae_enable(std::is_same_v<T1, LongDouble80> && std::is_same_v<T2, bool>)>
	TH_FORCEINLINE LongDouble80& operator/(LongDouble80 left, bool right) {
		return right ? left : left = _LDOUBLE{ 0, 0, 0, 0, 0, 0, 0, 0x80, 0xFF, 0x7F };
	}

	template<typename T1, typename T2, sfinae_enable(ValidFPArithmetic(T1, T2))>
	TH_FORCEINLINE bool operator==(T1 left, T2 right) {
		bool _ret;
		X87ComSetCC(ComIPP, E, _ret, left, right);
		return _ret;
	}

	template <typename T, sfinae_enable(ValidFPValue(T))>
	TH_FORCEINLINE bool operator==(LongDouble80NonModLVal& left, T right) {
		bool _ret;
		X87OneComSetCC(ComIPP, E, _ret, right);
		return _ret;
	}

	template <typename T, sfinae_enable(ValidFPValue(T))>
	TH_FORCEINLINE bool operator==(T left, LongDouble80NonModLVal& right) {
		bool _ret;
		X87OneComSetCC(ComIPP, E, _ret, left);
		return _ret;
	}

	template<>
	TH_FORCEINLINE bool operator==(LongDouble80 left, LongDouble80 right) {
		return memcmp(&left, &right, sizeof(LongDouble80)) == 0;
	}

	template<typename T1, typename T2, sfinae_enable(ValidFPArithmetic(T1, T2))>
	TH_FORCEINLINE bool operator!=(T1 left, T2 right) {
		bool _ret;
		X87ComSetCC(ComIPP, NE, _ret, left, right);
		return _ret;
	}

	template <typename T, sfinae_enable(ValidFPValue(T))>
	TH_FORCEINLINE bool operator!=(LongDouble80NonModLVal& left, T right) {
		bool _ret;
		X87OneComSetCC(ComIPP, NE, _ret, right);
		return _ret;
	}

	template <typename T, sfinae_enable(ValidFPValue(T))>
	TH_FORCEINLINE bool operator!=(T left, LongDouble80NonModLVal& right) {
		bool _ret;
		X87OneComSetCC(ComIPP, NE, _ret, left);
		return _ret;
	}

	template<>
	TH_FORCEINLINE bool operator!=(LongDouble80 left, LongDouble80 right) {
		return memcmp(&left, &right, sizeof(LongDouble80)) != 0;
	}

	template<typename T1, typename T2, sfinae_enable(ValidFPArithmetic(T1, T2))>
	TH_FORCEINLINE bool operator<(T1 left, T2 right) {
		bool _ret;
		X87ComSetCC(ComIPP, B, _ret, left, right);
		return _ret;
	}

	template <typename T, sfinae_enable(ValidFPValue(T))>
	TH_FORCEINLINE bool operator<(LongDouble80NonModLVal& left, T right) {
		bool _ret;
		X87OneComSetCC(ComIPP, B, _ret, right);
		return _ret;
	}

	template <typename T, sfinae_enable(ValidFPValue(T))>
	TH_FORCEINLINE bool operator<(T left, LongDouble80NonModLVal& right) {
		bool _ret;
		X87OneComSetCC(ComIPP, AE, _ret, left);
		return _ret;
	}

	template<typename T1, typename T2, sfinae_enable(ValidFPArithmetic(T1, T2))>
	TH_FORCEINLINE bool operator<=(T1 left, T2 right) {
		bool _ret;
		X87ComSetCC(ComIPP, BE, _ret, left, right);
		return _ret;
	}

	template <typename T, sfinae_enable(ValidFPValue(T))>
	TH_FORCEINLINE bool operator<=(LongDouble80NonModLVal& left, T right) {
		bool _ret;
		X87OneComSetCC(ComIPP, BE, _ret, right);
		return _ret;
	}

	template <typename T, sfinae_enable(ValidFPValue(T))>
	TH_FORCEINLINE bool operator<=(T left, LongDouble80NonModLVal& right) {
		bool _ret;
		X87OneComSetCC(ComIPP, A, _ret, left);
		return _ret;
	}

	template<typename T1, typename T2, sfinae_enable(ValidFPArithmetic(T1, T2))>
	TH_FORCEINLINE bool operator>(T1 left, T2 right) {
		bool _ret;
		X87ComSetCC(ComIPP, A, _ret, left, right);
		return _ret;
	}

	template <typename T, sfinae_enable(ValidFPValue(T))>
	TH_FORCEINLINE bool operator>(LongDouble80NonModLVal& left, T right) {
		bool _ret;
		X87OneComSetCC(ComIPP, A, _ret, right);
		return _ret;
	}

	template <typename T, sfinae_enable(ValidFPValue(T))>
	TH_FORCEINLINE bool operator>(T left, LongDouble80NonModLVal& right) {
		bool _ret;
		X87OneComSetCC(ComIPP, BE, _ret, left);
		return _ret;
	}

	template<typename T1, typename T2, sfinae_enable(ValidFPArithmetic(T1, T2))>
	TH_FORCEINLINE bool operator>=(T1 left, T2 right) {
		bool _ret;
		X87ComSetCC(ComIPP, AE, _ret, left, right);
		return _ret;
	}

	template <typename T, sfinae_enable(ValidFPValue(T))>
	TH_FORCEINLINE bool operator>=(LongDouble80NonModLVal& left, T right) {
		bool _ret;
		X87OneComSetCC(ComIPP, AE, _ret, right);
		return _ret;
	}

	template <typename T, sfinae_enable(ValidFPValue(T))>
	TH_FORCEINLINE bool operator>=(T left, LongDouble80NonModLVal& right) {
		bool _ret;
		X87OneComSetCC(ComIPP, B, _ret, left);
		return _ret;
	}

#undef IsAnyIntBelow16
#undef IsSigned16
#undef IsSigned32
#undef IsSigned64
#undef IsUnsigned16
#undef IsUnsigned32
#undef IsUnsigned64

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
#undef ValidFPMemOp

#undef IsValidMemOpInt
#undef IsValidMemOpFloat
#undef IsValidMemOp

#endif // ifdef __cplusplus

#endif // if !(defined(_MSC_VER) && !defined(_LDSUPPORT)) || defined(TH_X64)

#ifdef __cplusplus
extern "C++" {

	TH_FORCEINLINE LongDouble80 operator"" _ld(unsigned long long int value) {
		LongDouble80 ret;
#pragma warning(suppress : 4244) // On x64 there's not much we can do about this
		return ret = value;
	}

	TH_FORCEINLINE LongDouble80 operator"" _ld(long double value) {
		LongDouble80 ret;
		return ret = value;
	}
}
#endif
