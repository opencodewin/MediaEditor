#include <limits>
#include <cstring>
#include <algorithm>
#include "MathUtils.h"

namespace SimdOpt
{
SIMD_SCOPE_BEGIN(INTRIN_MODE)
SIMD_DEFINE_SCOPVARS(INTRIN_MODE)

template<typename _Tp> struct V_TypeTraits
{
};

#define SIMD_DEF_TYPE_TRAITS(type, int_type_, uint_type_, abs_type_, w_type_, q_type_, sum_type_) \
    template<> struct V_TypeTraits<type> \
    { \
        typedef type value_type; \
        typedef int_type_ int_type; \
        typedef abs_type_ abs_type; \
        typedef uint_type_ uint_type; \
        typedef w_type_ w_type; \
        typedef q_type_ q_type; \
        typedef sum_type_ sum_type; \
    \
        static inline int_type reinterpret_int(type x) \
        { \
            union { type l; int_type i; } v; \
            v.l = x; \
            return v.i; \
        } \
    \
        static inline type reinterpret_from_int(int_type x) \
        { \
            union { type l; int_type i; } v; \
            v.i = x; \
            return v.l; \
        } \
    }

#define SIMD_DEF_TYPE_TRAITS_NO_Q_TYPE(type, int_type_, uint_type_, abs_type_, w_type_, sum_type_) \
    template<> struct V_TypeTraits<type> \
    { \
        typedef type value_type; \
        typedef int_type_ int_type; \
        typedef abs_type_ abs_type; \
        typedef uint_type_ uint_type; \
        typedef w_type_ w_type; \
        typedef sum_type_ sum_type; \
    \
        static inline int_type reinterpret_int(type x) \
        { \
            union { type l; int_type i; } v; \
            v.l = x; \
            return v.i; \
        } \
    \
        static inline type reinterpret_from_int(int_type x) \
        { \
            union { type l; int_type i; } v; \
            v.i = x; \
            return v.l; \
        } \
    }

SIMD_DEF_TYPE_TRAITS(uint8_t, int8_t, uint8_t, uint8_t, uint16_t, uint32_t, uint32_t);
SIMD_DEF_TYPE_TRAITS(int8_t, int8_t, uint8_t, uint8_t, int16_t, int32_t, int32_t);
SIMD_DEF_TYPE_TRAITS(uint16_t, int16_t, uint16_t, uint16_t, uint32_t, uint64_t, uint32_t);
SIMD_DEF_TYPE_TRAITS(int16_t, int16_t, uint16_t, uint16_t, int32_t, int64_t, int32_t);
SIMD_DEF_TYPE_TRAITS_NO_Q_TYPE(uint32_t, int32_t, uint32_t, uint32_t, uint64_t, uint32_t);
SIMD_DEF_TYPE_TRAITS_NO_Q_TYPE(int32_t, int32_t, uint32_t, uint32_t, int64_t, int32_t);
SIMD_DEF_TYPE_TRAITS_NO_Q_TYPE(float, int32_t, uint32_t, float, double, float);
SIMD_DEF_TYPE_TRAITS_NO_Q_TYPE(uint64_t, int64_t, uint64_t, uint64_t, void, uint64_t);
SIMD_DEF_TYPE_TRAITS_NO_Q_TYPE(int64_t, int64_t, uint64_t, uint64_t, void, int64_t);
SIMD_DEF_TYPE_TRAITS_NO_Q_TYPE(double, int64_t, uint64_t, double, void, double);

template<typename _Tp, int32_t n> struct v_reg
{
    typedef _Tp lane_type;
    enum { nlanes = n };

    explicit v_reg(const _Tp* ptr)
    {
        for( int32_t i = 0; i < n; i++ )
            s[i] = ptr[i];
    }

    v_reg(_Tp s0, _Tp s1)
    {
        s[0] = s0; s[1] = s1;
    }

    v_reg(_Tp s0, _Tp s1, _Tp s2, _Tp s3)
    {
        s[0] = s0; s[1] = s1; s[2] = s2; s[3] = s3;
    }

    v_reg(_Tp s0, _Tp s1, _Tp s2, _Tp s3,
          _Tp s4, _Tp s5, _Tp s6, _Tp s7)
    {
        s[0] = s0; s[1] = s1; s[2] = s2; s[3] = s3;
        s[4] = s4; s[5] = s5; s[6] = s6; s[7] = s7;
    }

    v_reg(_Tp s0, _Tp s1, _Tp s2, _Tp s3,
          _Tp s4, _Tp s5, _Tp s6, _Tp s7,
          _Tp s8, _Tp s9, _Tp s10, _Tp s11,
          _Tp s12, _Tp s13, _Tp s14, _Tp s15)
    {
        s[0] = s0; s[1] = s1; s[2] = s2; s[3] = s3;
        s[4] = s4; s[5] = s5; s[6] = s6; s[7] = s7;
        s[8] = s8; s[9] = s9; s[10] = s10; s[11] = s11;
        s[12] = s12; s[13] = s13; s[14] = s14; s[15] = s15;
    }

    v_reg() {}

    v_reg(const v_reg<_Tp, n> & r)
    {
        for (int32_t i = 0; i < n; i++)
            s[i] = r.s[i];
    }

    _Tp get0() const
    { return s[0]; }

    _Tp get(const int32_t i) const
    { return s[i]; }

    v_reg<_Tp, n> high() const
    {
        v_reg<_Tp, n> c;
        int32_t i;
        for (i = 0; i < n/2; i++)
        {
            c.s[i] = s[i+(n/2)];
            c.s[i+(n/2)] = 0;
        }
        return c;
    }

    static v_reg<_Tp, n> zero()
    {
        v_reg<_Tp, n> c;
        for (int32_t i = 0; i < n; i++)
            c.s[i] = (_Tp)0;
        return c;
    }

    static v_reg<_Tp, n> all(_Tp s)
    {
        v_reg<_Tp, n> c;
        for (int32_t i = 0; i < n; i++)
            c.s[i] = s;
        return c;
    }

    template<typename _Tp2, int32_t n2> v_reg<_Tp2, n2> reinterpret_as() const
    {
        size_t bytes = std::min(sizeof(_Tp2)*n2, sizeof(_Tp)*n);
        v_reg<_Tp2, n2> c;
        std::memcpy(&c.s[0], &s[0], bytes);
        return c;
    }

    v_reg& operator=(const v_reg<_Tp, n> & r)
    {
        for (int32_t i = 0; i < n; i++)
            s[i] = r.s[i];
        return *this;
    }

    _Tp s[n];
};

typedef v_reg<uint8_t, 16> v_uint8x16;
typedef v_reg<int8_t, 16> v_int8x16;
typedef v_reg<uint16_t, 8> v_uint16x8;
typedef v_reg<int16_t, 8> v_int16x8;
typedef v_reg<uint32_t, 4> v_uint32x4;
typedef v_reg<int32_t, 4> v_int32x4;
typedef v_reg<float, 4> v_float32x4;
typedef v_reg<double, 2> v_float64x2;
typedef v_reg<uint64_t, 2> v_uint64x2;
typedef v_reg<int64_t, 2> v_int64x2;

enum {
    simd128_width = 16,
    simdmax_width = simd128_width
};

template<typename _Tp, int32_t n> inline v_reg<_Tp, n> operator+(const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b);
template<typename _Tp, int32_t n> inline v_reg<_Tp, n>& operator+=(v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b);
template<typename _Tp, int32_t n> inline v_reg<_Tp, n> operator-(const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b);
template<typename _Tp, int32_t n> inline v_reg<_Tp, n>& operator-=(v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b);
template<typename _Tp, int32_t n> inline v_reg<_Tp, n> operator*(const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b);
template<typename _Tp, int32_t n> inline v_reg<_Tp, n>& operator*=(v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b);
template<typename _Tp, int32_t n> inline v_reg<_Tp, n> operator/(const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b);
template<typename _Tp, int32_t n> inline v_reg<_Tp, n>& operator/=(v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b);

template<typename _Tp, int32_t n> inline v_reg<_Tp, n> operator&(const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b);
template<typename _Tp, int32_t n> inline v_reg<_Tp, n>& operator&=(v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b);
template<typename _Tp, int32_t n> inline v_reg<_Tp, n> operator|(const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b);
template<typename _Tp, int32_t n> inline v_reg<_Tp, n>& operator|=(v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b);
template<typename _Tp, int32_t n> inline v_reg<_Tp, n> operator^(const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b);
template<typename _Tp, int32_t n> inline v_reg<_Tp, n>& operator^=(v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b);
template<typename _Tp, int32_t n> inline v_reg<_Tp, n> operator~(const v_reg<_Tp, n>& a);


#define SIMD_EXPAND_WITH_INTEGER_TYPES(macro_name, ...) \
    macro_name(uint8_t,  __VA_ARGS__)                   \
    macro_name(int8_t,   __VA_ARGS__)                   \
    macro_name(uint16_t, __VA_ARGS__)                   \
    macro_name(int16_t,  __VA_ARGS__)                   \
    macro_name(uint32_t, __VA_ARGS__)                   \
    macro_name(int32_t,  __VA_ARGS__)                   \
    macro_name(uint64_t, __VA_ARGS__)                   \
    macro_name(int64_t,  __VA_ARGS__)                   \

#define SIMD_EXPAND_WITH_FP_TYPES(macro_name, ...)      \
    macro_name(float,  __VA_ARGS__)                     \
    macro_name(double, __VA_ARGS__)                     \

#define SIMD_EXPAND_WITH_ALL_TYPES(macro_name, ...)         \
    SIMD_EXPAND_WITH_INTEGER_TYPES(macro_name, __VA_ARGS__) \
    SIMD_EXPAND_WITH_FP_TYPES(macro_name, __VA_ARGS__)      \

#define SIMD_BIN_OP_(_Tp, bin_op)                           \
template<int32_t n> inline                                  \
v_reg<_Tp, n> operator bin_op (const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b) \
{                                                           \
    v_reg<_Tp, n> c;                                        \
    for (int32_t i = 0; i < n; i++)                         \
        c.s[i] = MathUtils::SaturateCast<_Tp>(a.s[i] bin_op b.s[i]); \
    return c;                                               \
}                                                           \
template<int32_t n> inline                                  \
v_reg<_Tp, n>& operator bin_op##= (v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b) \
{                                                           \
    for (int32_t i = 0; i < n; i++)                         \
        a.s[i] = MathUtils::SaturateCast<_Tp>(a.s[i] bin_op b.s[i]); \
    return a;                                               \
}

#define SIMD_BIN_OP(bin_op) SIMD_EXPAND_WITH_ALL_TYPES(SIMD_BIN_OP_, bin_op)

SIMD_BIN_OP(+)
SIMD_BIN_OP(-)
SIMD_BIN_OP(*)
SIMD_EXPAND_WITH_FP_TYPES(SIMD_BIN_OP_, /)

#define SIMD_BIT_OP_(_Tp, bit_op) \
template<int32_t n> inline \
v_reg<_Tp, n> operator bit_op (const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b) \
{ \
    v_reg<_Tp, n> c; \
    typedef typename V_TypeTraits<_Tp>::int_type itype; \
    for( int32_t i = 0; i < n; i++ ) \
        c.s[i] = V_TypeTraits<_Tp>::reinterpret_from_int((itype)(V_TypeTraits<_Tp>::reinterpret_int(a.s[i]) bit_op \
                                                        V_TypeTraits<_Tp>::reinterpret_int(b.s[i]))); \
    return c; \
} \
template<int32_t n> inline \
v_reg<_Tp, n>& operator bit_op##= (v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b) \
{ \
    typedef typename V_TypeTraits<_Tp>::int_type itype; \
    for( int32_t i = 0; i < n; i++ ) \
        a.s[i] = V_TypeTraits<_Tp>::reinterpret_from_int((itype)(V_TypeTraits<_Tp>::reinterpret_int(a.s[i]) bit_op \
                                                        V_TypeTraits<_Tp>::reinterpret_int(b.s[i]))); \
    return a; \
}

#define SIMD_BIT_OP(bit_op) \
    SIMD_EXPAND_WITH_INTEGER_TYPES(SIMD_BIT_OP_, bit_op) \
    SIMD_EXPAND_WITH_FP_TYPES(SIMD_BIT_OP_, bit_op)

SIMD_BIT_OP(&)
SIMD_BIT_OP(|)
SIMD_BIT_OP(^)

#define SIMD_BITWISE_NOT_(_Tp, dummy) \
template<int32_t n> inline \
v_reg<_Tp, n> operator ~ (const v_reg<_Tp, n>& a) \
{ \
    v_reg<_Tp, n> c; \
    for( int32_t i = 0; i < n; i++ ) \
        c.s[i] = V_TypeTraits<_Tp>::reinterpret_from_int(~V_TypeTraits<_Tp>::reinterpret_int(a.s[i])); \
    return c; \
} \

SIMD_EXPAND_WITH_INTEGER_TYPES(SIMD_BITWISE_NOT_, ~)


#define SIMD_MATH_FUNC(func, cfunc, _Tp2) \
template<typename _Tp, int32_t n> inline v_reg<_Tp2, n> func(const v_reg<_Tp, n>& a) \
{ \
    v_reg<_Tp2, n> c; \
    for( int32_t i = 0; i < n; i++ ) \
        c.s[i] = cfunc(a.s[i]); \
    return c; \
}

SIMD_MATH_FUNC(v_sqrt, std::sqrt, _Tp)
SIMD_MATH_FUNC(v_sin,  std::sin,  _Tp)
SIMD_MATH_FUNC(v_cos,  std::cos,  _Tp)
SIMD_MATH_FUNC(v_exp,  std::exp,  _Tp)
SIMD_MATH_FUNC(v_log,  std::log,  _Tp)

SIMD_MATH_FUNC(v_abs, (typename V_TypeTraits<_Tp>::abs_type)std::abs, typename V_TypeTraits<_Tp>::abs_type)

#define SIMD_MINMAX_FUNC(func, cfunc) \
template<typename _Tp, int32_t n> inline v_reg<_Tp, n> func(const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b) \
{ \
    v_reg<_Tp, n> c; \
    for( int32_t i = 0; i < n; i++ ) \
        c.s[i] = cfunc(a.s[i], b.s[i]); \
    return c; \
}

#define SIMD_REDUCE_MINMAX_FUNC(func, cfunc) \
template<typename _Tp, int32_t n> inline _Tp func(const v_reg<_Tp, n>& a) \
{ \
    _Tp c = a.s[0]; \
    for( int32_t i = 1; i < n; i++ ) \
        c = cfunc(c, a.s[i]); \
    return c; \
}

SIMD_MINMAX_FUNC(v_min, std::min)
SIMD_MINMAX_FUNC(v_max, std::max)
SIMD_REDUCE_MINMAX_FUNC(v_reduce_min, std::min)
SIMD_REDUCE_MINMAX_FUNC(v_reduce_max, std::max)

static const uint8_t popCountTable[] =
{
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8,
};

template<typename _Tp, int32_t n>
inline v_reg<typename V_TypeTraits<_Tp>::abs_type, n> v_popcount(const v_reg<_Tp, n>& a)
{
    v_reg<typename V_TypeTraits<_Tp>::abs_type, n> b = v_reg<typename V_TypeTraits<_Tp>::abs_type, n>::zero();
    for (int32_t i = 0; i < n*(int32_t)sizeof(_Tp); i++)
        b.s[i/sizeof(_Tp)] += popCountTable[v_reinterpret_as_u8(a).s[i]];
    return b;
}

template<typename _Tp, int32_t n>
inline void v_minmax(const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b,
                     v_reg<_Tp, n>& minval, v_reg<_Tp, n>& maxval)
{
    for( int32_t i = 0; i < n; i++ )
    {
        minval.s[i] = std::min(a.s[i], b.s[i]);
        maxval.s[i] = std::max(a.s[i], b.s[i]);
    }
}

#define SIMD_CMP_OP(cmp_op) \
template<typename _Tp, int32_t n> \
inline v_reg<_Tp, n> operator cmp_op(const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b) \
{ \
    typedef typename V_TypeTraits<_Tp>::int_type itype; \
    v_reg<_Tp, n> c; \
    for( int32_t i = 0; i < n; i++ ) \
        c.s[i] = V_TypeTraits<_Tp>::reinterpret_from_int((itype)-(int32_t)(a.s[i] cmp_op b.s[i])); \
    return c; \
}

SIMD_CMP_OP(<)
SIMD_CMP_OP(>)
SIMD_CMP_OP(<=)
SIMD_CMP_OP(>=)
SIMD_CMP_OP(==)
SIMD_CMP_OP(!=)

template<int32_t n>
inline v_reg<float, n> v_not_nan(const v_reg<float, n>& a)
{
    typedef typename V_TypeTraits<float>::int_type itype;
    v_reg<float, n> c;
    for (int32_t i = 0; i < n; i++)
        c.s[i] = V_TypeTraits<float>::reinterpret_from_int((itype)-(int32_t)(a.s[i] == a.s[i]));
    return c;
}
template<int32_t n>
inline v_reg<double, n> v_not_nan(const v_reg<double, n>& a)
{
    typedef typename V_TypeTraits<double>::int_type itype;
    v_reg<double, n> c;
    for (int32_t i = 0; i < n; i++)
        c.s[i] = V_TypeTraits<double>::reinterpret_from_int((itype)-(int32_t)(a.s[i] == a.s[i]));
    return c;
}

#define SIMD_ARITHM_OP(func, bin_op, cast_op, _Tp2) \
template<typename _Tp, int32_t n> \
inline v_reg<_Tp2, n> func(const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b) \
{ \
    typedef _Tp2 rtype; \
    v_reg<rtype, n> c; \
    for( int32_t i = 0; i < n; i++ ) \
        c.s[i] = cast_op(a.s[i] bin_op b.s[i]); \
    return c; \
}

SIMD_ARITHM_OP(v_add_wrap, +, (_Tp), _Tp)
SIMD_ARITHM_OP(v_sub_wrap, -, (_Tp), _Tp)
SIMD_ARITHM_OP(v_mul_wrap, *, (_Tp), _Tp)


template<typename T> inline T _absdiff(T a, T b)
{
    return a > b ? a - b : b - a;
}

template<typename _Tp, int32_t n>
inline v_reg<typename V_TypeTraits<_Tp>::abs_type, n> v_absdiff(const v_reg<_Tp, n>& a, const v_reg<_Tp, n> & b)
{
    typedef typename V_TypeTraits<_Tp>::abs_type rtype;
    v_reg<rtype, n> c;
    const rtype mask = (rtype)(std::numeric_limits<_Tp>::is_signed ? (1 << (sizeof(rtype)*8 - 1)) : 0);
    for( int32_t i = 0; i < n; i++ )
    {
        rtype ua = a.s[i] ^ mask;
        rtype ub = b.s[i] ^ mask;
        c.s[i] = _absdiff(ua, ub);
    }
    return c;
}

template<int32_t n> inline v_reg<float, n> v_absdiff(const v_reg<float, n>& a, const v_reg<float, n>& b)
{
    v_reg<float, n> c;
    for( int32_t i = 0; i < c.nlanes; i++ )
        c.s[i] = _absdiff(a.s[i], b.s[i]);
    return c;
}

template<int32_t n> inline v_reg<double, n> v_absdiff(const v_reg<double, n>& a, const v_reg<double, n>& b)
{
    v_reg<double, n> c;
    for( int32_t i = 0; i < c.nlanes; i++ )
        c.s[i] = _absdiff(a.s[i], b.s[i]);
    return c;
}

template<typename _Tp, int32_t n>
inline v_reg<_Tp, n> v_absdiffs(const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b)
{
    v_reg<_Tp, n> c;
    for( int32_t i = 0; i < n; i++)
        c.s[i] = MathUtils::SaturateCast<_Tp>(std::abs(a.s[i] - b.s[i]));
    return c;
}

template<typename _Tp, int32_t n>
inline v_reg<_Tp, n> v_invsqrt(const v_reg<_Tp, n>& a)
{
    v_reg<_Tp, n> c;
    for( int32_t i = 0; i < n; i++ )
        c.s[i] = 1.f/std::sqrt(a.s[i]);
    return c;
}

template<typename _Tp, int32_t n>
inline v_reg<_Tp, n> v_magnitude(const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b)
{
    v_reg<_Tp, n> c;
    for( int32_t i = 0; i < n; i++ )
        c.s[i] = std::sqrt(a.s[i]*a.s[i] + b.s[i]*b.s[i]);
    return c;
}

template<typename _Tp, int32_t n>
inline v_reg<_Tp, n> v_sqr_magnitude(const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b)
{
    v_reg<_Tp, n> c;
    for( int32_t i = 0; i < n; i++ )
        c.s[i] = a.s[i]*a.s[i] + b.s[i]*b.s[i];
    return c;
}

template<typename _Tp, int32_t n>
inline v_reg<_Tp, n> v_fma(const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b,
                           const v_reg<_Tp, n>& c)
{
    v_reg<_Tp, n> d;
    for( int32_t i = 0; i < n; i++ )
        d.s[i] = a.s[i]*b.s[i] + c.s[i];
    return d;
}

template<typename _Tp, int32_t n>
inline v_reg<_Tp, n> v_muladd(const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b,
                              const v_reg<_Tp, n>& c)
{
    return v_fma(a, b, c);
}

template<typename _Tp, int32_t n> inline v_reg<typename V_TypeTraits<_Tp>::w_type, n/2>
v_dotprod(const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b)
{
    typedef typename V_TypeTraits<_Tp>::w_type w_type;
    v_reg<w_type, n/2> c;
    for( int32_t i = 0; i < (n/2); i++ )
        c.s[i] = (w_type)a.s[i*2]*b.s[i*2] + (w_type)a.s[i*2+1]*b.s[i*2+1];
    return c;
}

template<typename _Tp, int32_t n> inline v_reg<typename V_TypeTraits<_Tp>::w_type, n/2>
v_dotprod(const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b,
          const v_reg<typename V_TypeTraits<_Tp>::w_type, n / 2>& c)
{
    typedef typename V_TypeTraits<_Tp>::w_type w_type;
    v_reg<w_type, n/2> s;
    for( int32_t i = 0; i < (n/2); i++ )
        s.s[i] = (w_type)a.s[i*2]*b.s[i*2] + (w_type)a.s[i*2+1]*b.s[i*2+1] + c.s[i];
    return s;
}

template<typename _Tp, int32_t n> inline v_reg<typename V_TypeTraits<_Tp>::w_type, n/2>
v_dotprod_fast(const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b)
{ return v_dotprod(a, b); }

template<typename _Tp, int32_t n> inline v_reg<typename V_TypeTraits<_Tp>::w_type, n/2>
v_dotprod_fast(const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b,
               const v_reg<typename V_TypeTraits<_Tp>::w_type, n / 2>& c)
{ return v_dotprod(a, b, c); }

template<typename _Tp, int32_t n> inline v_reg<typename V_TypeTraits<_Tp>::q_type, n/4>
v_dotprod_expand(const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b)
{
    typedef typename V_TypeTraits<_Tp>::q_type q_type;
    v_reg<q_type, n/4> s;
    for( int32_t i = 0; i < (n/4); i++ )
        s.s[i] = (q_type)a.s[i*4    ]*b.s[i*4    ] + (q_type)a.s[i*4 + 1]*b.s[i*4 + 1] +
                 (q_type)a.s[i*4 + 2]*b.s[i*4 + 2] + (q_type)a.s[i*4 + 3]*b.s[i*4 + 3];
    return s;
}

template<typename _Tp, int32_t n> inline v_reg<typename V_TypeTraits<_Tp>::q_type, n/4>
v_dotprod_expand(const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b,
                 const v_reg<typename V_TypeTraits<_Tp>::q_type, n / 4>& c)
{
    typedef typename V_TypeTraits<_Tp>::q_type q_type;
    v_reg<q_type, n/4> s;
    for( int32_t i = 0; i < (n/4); i++ )
        s.s[i] = (q_type)a.s[i*4    ]*b.s[i*4    ] + (q_type)a.s[i*4 + 1]*b.s[i*4 + 1] +
                 (q_type)a.s[i*4 + 2]*b.s[i*4 + 2] + (q_type)a.s[i*4 + 3]*b.s[i*4 + 3] + c.s[i];
    return s;
}

template<typename _Tp, int32_t n> inline v_reg<typename V_TypeTraits<_Tp>::q_type, n/4>
v_dotprod_expand_fast(const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b)
{ return v_dotprod_expand(a, b); }

template<typename _Tp, int32_t n> inline v_reg<typename V_TypeTraits<_Tp>::q_type, n/4>
v_dotprod_expand_fast(const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b,
                      const v_reg<typename V_TypeTraits<_Tp>::q_type, n / 4>& c)
{ return v_dotprod_expand(a, b, c); }

template<typename _Tp, int32_t n> inline void v_mul_expand(const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b,
                                                       v_reg<typename V_TypeTraits<_Tp>::w_type, n/2>& c,
                                                       v_reg<typename V_TypeTraits<_Tp>::w_type, n/2>& d)
{
    typedef typename V_TypeTraits<_Tp>::w_type w_type;
    for( int32_t i = 0; i < (n/2); i++ )
    {
        c.s[i] = (w_type)a.s[i]*b.s[i];
        d.s[i] = (w_type)a.s[i+(n/2)]*b.s[i+(n/2)];
    }
}

template<typename _Tp, int32_t n> inline v_reg<_Tp, n> v_mul_hi(const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b)
{
    typedef typename V_TypeTraits<_Tp>::w_type w_type;
    v_reg<_Tp, n> c;
    for (int32_t i = 0; i < n; i++)
        c.s[i] = (_Tp)(((w_type)a.s[i] * b.s[i]) >> sizeof(_Tp)*8);
    return c;
}

template<typename _Tp, int32_t n> inline void v_hsum(const v_reg<_Tp, n>& a,
                                                 v_reg<typename V_TypeTraits<_Tp>::w_type, n/2>& c)
{
    typedef typename V_TypeTraits<_Tp>::w_type w_type;
    for( int32_t i = 0; i < (n/2); i++ )
    {
        c.s[i] = (w_type)a.s[i*2] + a.s[i*2+1];
    }
}

#define SIMD_SHIFT_OP(shift_op) \
template<typename _Tp, int32_t n> inline v_reg<_Tp, n> operator shift_op(const v_reg<_Tp, n>& a, int32_t imm) \
{ \
    v_reg<_Tp, n> c; \
    for( int32_t i = 0; i < n; i++ ) \
        c.s[i] = (_Tp)(a.s[i] shift_op imm); \
    return c; \
}

SIMD_SHIFT_OP(<< )
SIMD_SHIFT_OP(>> )

#define SIMD_ROTATE_SHIFT_OP(suffix,opA,opB) \
template<int32_t imm, typename _Tp, int32_t n> inline v_reg<_Tp, n> v_rotate_##suffix(const v_reg<_Tp, n>& a) \
{ \
    v_reg<_Tp, n> b; \
    for (int32_t i = 0; i < n; i++) \
    { \
        int32_t sIndex = i opA imm; \
        if (0 <= sIndex && sIndex < n) \
        { \
            b.s[i] = a.s[sIndex]; \
        } \
        else \
        { \
            b.s[i] = 0; \
        } \
    } \
    return b; \
} \
template<int32_t imm, typename _Tp, int32_t n> inline v_reg<_Tp, n> v_rotate_##suffix(const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b) \
{ \
    v_reg<_Tp, n> c; \
    for (int32_t i = 0; i < n; i++) \
    { \
        int32_t aIndex = i opA imm; \
        int32_t bIndex = i opA imm opB n; \
        if (0 <= bIndex && bIndex < n) \
        { \
            c.s[i] = b.s[bIndex]; \
        } \
        else if (0 <= aIndex && aIndex < n) \
        { \
            c.s[i] = a.s[aIndex]; \
        } \
        else \
        { \
            c.s[i] = 0; \
        } \
    } \
    return c; \
}

SIMD_ROTATE_SHIFT_OP(left,  -, +)
SIMD_ROTATE_SHIFT_OP(right, +, -)

template<typename _Tp, int32_t n> inline typename V_TypeTraits<_Tp>::sum_type v_reduce_sum(const v_reg<_Tp, n>& a)
{
    typename V_TypeTraits<_Tp>::sum_type c = a.s[0];
    for( int32_t i = 1; i < n; i++ )
        c += a.s[i];
    return c;
}

template<int32_t n> inline v_reg<float, n> v_reduce_sum4(const v_reg<float, n>& a, const v_reg<float, n>& b,
    const v_reg<float, n>& c, const v_reg<float, n>& d)
{
    v_reg<float, n> r;
    for(int32_t i = 0; i < (n/4); i++)
    {
        r.s[i*4 + 0] = a.s[i*4 + 0] + a.s[i*4 + 1] + a.s[i*4 + 2] + a.s[i*4 + 3];
        r.s[i*4 + 1] = b.s[i*4 + 0] + b.s[i*4 + 1] + b.s[i*4 + 2] + b.s[i*4 + 3];
        r.s[i*4 + 2] = c.s[i*4 + 0] + c.s[i*4 + 1] + c.s[i*4 + 2] + c.s[i*4 + 3];
        r.s[i*4 + 3] = d.s[i*4 + 0] + d.s[i*4 + 1] + d.s[i*4 + 2] + d.s[i*4 + 3];
    }
    return r;
}

template<typename _Tp, int32_t n> inline typename V_TypeTraits< typename V_TypeTraits<_Tp>::abs_type >::sum_type v_reduce_sad(const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b)
{
    typename V_TypeTraits< typename V_TypeTraits<_Tp>::abs_type >::sum_type c = _absdiff(a.s[0], b.s[0]);
    for (int32_t i = 1; i < n; i++)
        c += _absdiff(a.s[i], b.s[i]);
    return c;
}

template<typename _Tp, int32_t n> inline int32_t v_signmask(const v_reg<_Tp, n>& a)
{
    int32_t mask = 0;
    for( int32_t i = 0; i < n; i++ )
        mask |= (V_TypeTraits<_Tp>::reinterpret_int(a.s[i]) < 0) << i;
    return mask;
}

template <typename _Tp, int32_t n> inline int32_t v_scan_forward(const v_reg<_Tp, n>& a)
{
    for (int32_t i = 0; i < n; i++)
        if(V_TypeTraits<_Tp>::reinterpret_int(a.s[i]) < 0)
            return i;
    return 0;
}

template<typename _Tp, int32_t n> inline bool v_check_all(const v_reg<_Tp, n>& a)
{
    for( int32_t i = 0; i < n; i++ )
        if( V_TypeTraits<_Tp>::reinterpret_int(a.s[i]) >= 0 )
            return false;
    return true;
}

template<typename _Tp, int32_t n> inline bool v_check_any(const v_reg<_Tp, n>& a)
{
    for( int32_t i = 0; i < n; i++ )
        if( V_TypeTraits<_Tp>::reinterpret_int(a.s[i]) < 0 )
            return true;
    return false;
}

template<typename _Tp, int32_t n> inline v_reg<_Tp, n> v_select(
        const v_reg<_Tp, n>& mask, const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b)
{
    typedef V_TypeTraits<_Tp> Traits;
    typedef typename Traits::int_type int_type;
    v_reg<_Tp, n> c;
    for( int32_t i = 0; i < n; i++ )
    {
        int_type m = Traits::reinterpret_int(mask.s[i]);
        CV_DbgAssert(m == 0 || m == (~(int_type)0));  // restrict mask values: 0 or 0xff/0xffff/etc
        c.s[i] = m ? a.s[i] : b.s[i];
    }
    return c;
}

template<typename _Tp, int32_t n> inline void v_expand(const v_reg<_Tp, n>& a,
                            v_reg<typename V_TypeTraits<_Tp>::w_type, n/2>& b0,
                            v_reg<typename V_TypeTraits<_Tp>::w_type, n/2>& b1)
{
    for( int32_t i = 0; i < (n/2); i++ )
    {
        b0.s[i] = a.s[i];
        b1.s[i] = a.s[i+(n/2)];
    }
}

template<typename _Tp, int32_t n>
inline v_reg<typename V_TypeTraits<_Tp>::w_type, n/2>
v_expand_low(const v_reg<_Tp, n>& a)
{
    v_reg<typename V_TypeTraits<_Tp>::w_type, n/2> b;
    for( int32_t i = 0; i < (n/2); i++ )
        b.s[i] = a.s[i];
    return b;
}

template<typename _Tp, int32_t n>
inline v_reg<typename V_TypeTraits<_Tp>::w_type, n/2>
v_expand_high(const v_reg<_Tp, n>& a)
{
    v_reg<typename V_TypeTraits<_Tp>::w_type, n/2> b;
    for( int32_t i = 0; i < (n/2); i++ )
        b.s[i] = a.s[i+(n/2)];
    return b;
}

template<typename _Tp, int32_t n> inline v_reg<typename V_TypeTraits<_Tp>::int_type, n>
    v_reinterpret_as_int(const v_reg<_Tp, n>& a)
{
    v_reg<typename V_TypeTraits<_Tp>::int_type, n> c;
    for( int32_t i = 0; i < n; i++ )
        c.s[i] = V_TypeTraits<_Tp>::reinterpret_int(a.s[i]);
    return c;
}

template<typename _Tp, int32_t n> inline v_reg<typename V_TypeTraits<_Tp>::uint_type, n>
    v_reinterpret_as_uint(const v_reg<_Tp, n>& a)
{
    v_reg<typename V_TypeTraits<_Tp>::uint_type, n> c;
    for( int32_t i = 0; i < n; i++ )
        c.s[i] = V_TypeTraits<_Tp>::reinterpret_uint(a.s[i]);
    return c;
}

template<typename _Tp, int32_t n> inline void v_zip(
        const v_reg<_Tp, n>& a0, const v_reg<_Tp, n>& a1, v_reg<_Tp, n>& b0, v_reg<_Tp, n>& b1)
{
    int32_t i;
    for( i = 0; i < n/2; i++ )
    {
        b0.s[i*2] = a0.s[i];
        b0.s[i*2+1] = a1.s[i];
    }
    for( ; i < n; i++ )
    {
        b1.s[i*2-n] = a0.s[i];
        b1.s[i*2-n+1] = a1.s[i];
    }
}

template<typename _Tp>
inline v_reg<_Tp, simd128_width / sizeof(_Tp)> v_load(const _Tp* ptr)
{
    return v_reg<_Tp, simd128_width / sizeof(_Tp)>(ptr);
}

template<typename _Tp>
inline v_reg<_Tp, simd128_width / sizeof(_Tp)> v_load_aligned(const _Tp* ptr)
{
    return v_reg<_Tp, simd128_width / sizeof(_Tp)>(ptr);
}

template<typename _Tp>
inline v_reg<_Tp, simd128_width / sizeof(_Tp)> v_load_low(const _Tp* ptr)
{
    v_reg<_Tp, simd128_width / sizeof(_Tp)> c;
    for (int32_t i = 0; i < c.nlanes/2; i++)
        c.s[i] = ptr[i];
    return c;
}

template<typename _Tp>
inline v_reg<_Tp, simd128_width / sizeof(_Tp)> v_load_halves(const _Tp* loptr, const _Tp* hiptr)
{
    v_reg<_Tp, simd128_width / sizeof(_Tp)> c;
    for (int32_t i = 0; i < c.nlanes/2; i++)
    {
        c.s[i] = loptr[i];
        c.s[i+c.nlanes/2] = hiptr[i];
    }
    return c;
}

template<typename _Tp>
inline v_reg<typename V_TypeTraits<_Tp>::w_type, simd128_width / sizeof(typename V_TypeTraits<_Tp>::w_type)>
v_load_expand(const _Tp* ptr)
{
    typedef typename V_TypeTraits<_Tp>::w_type w_type;
    v_reg<w_type, simd128_width / sizeof(w_type)> c;
    for (int32_t i = 0; i < c.nlanes; i++)
        c.s[i] = ptr[i];
    return c;
}

template<typename _Tp>
inline v_reg<typename V_TypeTraits<_Tp>::q_type, simd128_width / sizeof(typename V_TypeTraits<_Tp>::q_type)>
v_load_expand_q(const _Tp* ptr)
{
    typedef typename V_TypeTraits<_Tp>::q_type q_type;
    v_reg<q_type, simd128_width / sizeof(q_type)> c;
    for (int32_t i = 0; i < c.nlanes; i++)
        c.s[i] = ptr[i];
    return c;
}

template<typename _Tp, int32_t n> inline void v_load_deinterleave(
        const _Tp* ptr, v_reg<_Tp, n>& a, v_reg<_Tp, n>& b)
{
    int32_t i, i2;
    for (i = i2 = 0; i < n; i++, i2 += 2)
    {
        a.s[i] = ptr[i2];
        b.s[i] = ptr[i2+1];
    }
}

template<typename _Tp, int32_t n> inline void v_load_deinterleave(
        const _Tp* ptr, v_reg<_Tp, n>& a, v_reg<_Tp, n>& b, v_reg<_Tp, n>& c)
{
    int32_t i, i3;
    for (i = i3 = 0; i < n; i++, i3 += 3)
    {
        a.s[i] = ptr[i3];
        b.s[i] = ptr[i3+1];
        c.s[i] = ptr[i3+2];
    }
}

template<typename _Tp, int32_t n>
inline void v_load_deinterleave(const _Tp* ptr, v_reg<_Tp, n>& a,
                                v_reg<_Tp, n>& b, v_reg<_Tp, n>& c,
                                v_reg<_Tp, n>& d)
{
    int32_t i, i4;
    for (i = i4 = 0; i < n; i++, i4 += 4)
    {
        a.s[i] = ptr[i4];
        b.s[i] = ptr[i4+1];
        c.s[i] = ptr[i4+2];
        d.s[i] = ptr[i4+3];
    }
}

template<typename _Tp, int32_t n>
inline void v_store_interleave(_Tp* ptr, const v_reg<_Tp, n>& a,
                               const v_reg<_Tp, n>& b,
                               StoreMode mode)
{
    int32_t i, i2;
    for (i = i2 = 0; i < n; i++, i2 += 2)
    {
        ptr[i2] = a.s[i];
        ptr[i2+1] = b.s[i];
    }
}

template<typename _Tp, int32_t n>
inline void v_store_interleave( _Tp* ptr, const v_reg<_Tp, n>& a,
                                const v_reg<_Tp, n>& b, const v_reg<_Tp, n>& c,
                                StoreMode mode)
{
    int32_t i, i3;
    for (i = i3 = 0; i < n; i++, i3 += 3)
    {
        ptr[i3] = a.s[i];
        ptr[i3+1] = b.s[i];
        ptr[i3+2] = c.s[i];
    }
}

template<typename _Tp, int32_t n> inline void v_store_interleave(
        _Tp* ptr, const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b, const v_reg<_Tp, n>& c,
        const v_reg<_Tp, n>& d, StoreMode mode)
{
    int32_t i, i4;
    for (i = i4 = 0; i < n; i++, i4 += 4)
    {
        ptr[i4] = a.s[i];
        ptr[i4+1] = b.s[i];
        ptr[i4+2] = c.s[i];
        ptr[i4+3] = d.s[i];
    }
}

template<typename _Tp, int32_t n>
inline void v_store(_Tp* ptr, const v_reg<_Tp, n>& a)
{
    for (int32_t i = 0; i < n; i++)
        ptr[i] = a.s[i];
}

template<typename _Tp, int32_t n>
inline void v_store(_Tp* ptr, const v_reg<_Tp, n>& a, StoreMode mode)
{
    v_store(ptr, a);
}

template<typename _Tp, int32_t n>
inline void v_store_low(_Tp* ptr, const v_reg<_Tp, n>& a)
{
    for (int32_t i = 0; i < (n/2); i++)
        ptr[i] = a.s[i];
}

template<typename _Tp, int32_t n>
inline void v_store_high(_Tp* ptr, const v_reg<_Tp, n>& a)
{
    for (int32_t i = 0; i < (n/2); i++)
        ptr[i] = a.s[i+(n/2)];
}

template<typename _Tp, int32_t n>
inline void v_store_aligned(_Tp* ptr, const v_reg<_Tp, n>& a)
{
    v_store(ptr, a);
}

template<typename _Tp, int32_t n>
inline void v_store_aligned_nocache(_Tp* ptr, const v_reg<_Tp, n>& a)
{
    v_store(ptr, a);
}

template<typename _Tp, int32_t n>
inline void v_store_aligned(_Tp* ptr, const v_reg<_Tp, n>& a, StoreMode /*mode*/)
{
    v_store(ptr, a);
}

template<typename _Tp, int32_t n>
inline v_reg<_Tp, n> v_combine_low(const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b)
{
    v_reg<_Tp, n> c;
    for (int32_t i = 0; i < (n/2); i++)
    {
        c.s[i] = a.s[i];
        c.s[i+(n/2)] = b.s[i];
    }
    return c;
}

template<typename _Tp, int32_t n>
inline v_reg<_Tp, n> v_combine_high(const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b)
{
    v_reg<_Tp, n> c;
    for (int32_t i = 0; i < (n/2); i++)
    {
        c.s[i] = a.s[i+(n/2)];
        c.s[i+(n/2)] = b.s[i+(n/2)];
    }
    return c;
}

template<typename _Tp, int32_t n>
inline void v_recombine(const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b,
                        v_reg<_Tp, n>& low, v_reg<_Tp, n>& high)
{
    for (int32_t i = 0; i < (n/2); i++)
    {
        low.s[i] = a.s[i];
        low.s[i+(n/2)] = b.s[i];
        high.s[i] = a.s[i+(n/2)];
        high.s[i+(n/2)] = b.s[i+(n/2)];
    }
}

template<typename _Tp, int32_t n>
inline v_reg<_Tp, n> v_reverse(const v_reg<_Tp, n>& a)
{
    v_reg<_Tp, n> c;
    for (int32_t i = 0; i < n; i++)
        c.s[i] = a.s[n-i-1];
    return c;
}

template<int32_t s, typename _Tp, int32_t n>
inline v_reg<_Tp, n> v_extract(const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b)
{
    v_reg<_Tp, n> r;
    const int32_t shift = n - s;
    int32_t i = 0;
    for (; i < shift; ++i)
        r.s[i] = a.s[i+s];
    for (; i < n; ++i)
        r.s[i] = b.s[i-shift];
    return r;
}

template<int32_t s, typename _Tp, int32_t n>
inline _Tp v_extract_n(const v_reg<_Tp, n>& v)
{
    return v.s[s];
}

template<int32_t i, typename _Tp, int32_t n>
inline v_reg<_Tp, n> v_broadcast_element(const v_reg<_Tp, n>& a)
{
    return v_reg<_Tp, n>::all(a.s[i]);
}

template<int32_t n> inline v_reg<int32_t, n> v_round(const v_reg<float, n>& a)
{
    v_reg<int32_t, n> c;
    for (int32_t i = 0; i < n; i++)
        c.s[i] = std::round(a.s[i]);
    return c;
}

template<int32_t n> inline v_reg<int32_t, n*2> v_round(const v_reg<double, n>& a, const v_reg<double, n>& b)
{
    v_reg<int32_t, n*2> c;
    for (int32_t i = 0; i < n; i++)
    {
        c.s[i] = std::round(a.s[i]);
        c.s[i+n] = std::round(b.s[i]);
    }
    return c;
}

template<int32_t n> inline v_reg<int32_t, n> v_floor(const v_reg<float, n>& a)
{
    v_reg<int32_t, n> c;
    for (int32_t i = 0; i < n; i++)
        c.s[i] = std::floor(a.s[i]);
    return c;
}

template<int32_t n> inline v_reg<int32_t, n> v_ceil(const v_reg<float, n>& a)
{
    v_reg<int32_t, n> c;
    for (int32_t i = 0; i < n; i++)
        c.s[i] = std::ceil(a.s[i]);
    return c;
}

template<int32_t n> inline v_reg<int32_t, n> v_trunc(const v_reg<float, n>& a)
{
    v_reg<int32_t, n> c;
    for (int32_t i = 0; i < n; i++)
        c.s[i] = (int32_t)(a.s[i]);
    return c;
}

template<int32_t n> inline v_reg<int32_t, n*2> v_round(const v_reg<double, n>& a)
{
    v_reg<int32_t, n*2> c;
    for (int32_t i = 0; i < n; i++)
    {
        c.s[i] = std::round(a.s[i]);
        c.s[i+n] = 0;
    }
    return c;
}

template<int32_t n> inline v_reg<int32_t, n*2> v_floor(const v_reg<double, n>& a)
{
    v_reg<int32_t, n*2> c;
    for (int32_t i = 0; i < n; i++)
    {
        c.s[i] = std::floor(a.s[i]);
        c.s[i+n] = 0;
    }
    return c;
}

template<int32_t n> inline v_reg<int32_t, n*2> v_ceil(const v_reg<double, n>& a)
{
    v_reg<int32_t, n*2> c;
    for (int32_t i = 0; i < n; i++)
    {
        c.s[i] = std::ceil(a.s[i]);
        c.s[i+n] = 0;
    }
    return c;
}

template<int32_t n> inline v_reg<int32_t, n*2> v_trunc(const v_reg<double, n>& a)
{
    v_reg<int32_t, n*2> c;
    for (int32_t i = 0; i < n; i++)
    {
        c.s[i] = (int32_t)(a.s[i]);
        c.s[i+n] = 0;
    }
    return c;
}

template<int32_t n> inline v_reg<float, n> v_cvt_f32(const v_reg<int32_t, n>& a)
{
    v_reg<float, n> c;
    for (int32_t i = 0; i < n; i++)
        c.s[i] = (float)a.s[i];
    return c;
}

template<int32_t n> inline v_reg<float, n*2> v_cvt_f32(const v_reg<double, n>& a)
{
    v_reg<float, n*2> c;
    for (int32_t i = 0; i < n; i++)
    {
        c.s[i] = (float)a.s[i];
        c.s[i+n] = 0;
    }
    return c;
}

template<int32_t n> inline v_reg<float, n*2> v_cvt_f32(const v_reg<double, n>& a, const v_reg<double, n>& b)
{
    v_reg<float, n*2> c;
    for (int32_t i = 0; i < n; i++)
    {
        c.s[i] = (float)a.s[i];
        c.s[i+n] = (float)b.s[i];
    }
    return c;
}

template<int32_t n> inline v_reg<double, n/2> v_cvt_f64(const v_reg<int32_t, n>& a)
{
    v_reg<double, (n/2)> c;
    for (int32_t i = 0; i < (n/2); i++)
        c.s[i] = (double)a.s[i];
    return c;
}

template<int32_t n> inline v_reg<double, (n/2)> v_cvt_f64_high(const v_reg<int32_t, n>& a)
{
    v_reg<double, (n/2)> c;
    for (int32_t i = 0; i < (n/2); i++)
        c.s[i] = (double)a.s[i + (n/2)];
    return c;
}

template<int32_t n> inline v_reg<double, (n/2)> v_cvt_f64(const v_reg<float, n>& a)
{
    v_reg<double, (n/2)> c;
    for (int32_t i = 0; i < (n/2); i++)
        c.s[i] = (double)a.s[i];
    return c;
}

template<int32_t n> inline v_reg<double, (n/2)> v_cvt_f64_high(const v_reg<float, n>& a)
{
    v_reg<double, (n/2)> c;
    for (int32_t i = 0; i < (n/2); i++)
        c.s[i] = (double)a.s[i + (n/2)];
    return c;
}

template<int32_t n> inline v_reg<double, n> v_cvt_f64(const v_reg<int64_t, n>& a)
{
    v_reg<double, n> c;
    for (int32_t i = 0; i < n; i++)
        c.s[i] = (double)a.s[i];
    return c;
}

template<typename _Tp> inline v_reg<_Tp, simd128_width / sizeof(_Tp)> v_lut(const _Tp* tab, const int32_t* idx)
{
    v_reg<_Tp, simd128_width / sizeof(_Tp)> c;
    for (int32_t i = 0; i < c.nlanes; i++)
        c.s[i] = tab[idx[i]];
    return c;
}

template<typename _Tp> inline v_reg<_Tp, simd128_width / sizeof(_Tp)> v_lut_pairs(const _Tp* tab, const int32_t* idx)
{
    v_reg<_Tp, simd128_width / sizeof(_Tp)> c;
    for (int32_t i = 0; i < c.nlanes; i++)
        c.s[i] = tab[idx[i / 2] + i % 2];
    return c;
}

template<typename _Tp> inline v_reg<_Tp, simd128_width / sizeof(_Tp)> v_lut_quads(const _Tp* tab, const int32_t* idx)
{
    v_reg<_Tp, simd128_width / sizeof(_Tp)> c;
    for (int32_t i = 0; i < c.nlanes; i++)
        c.s[i] = tab[idx[i / 4] + i % 4];
    return c;
}

template<int32_t n> inline v_reg<int32_t, n> v_lut(const int32_t* tab, const v_reg<int32_t, n>& idx)
{
    v_reg<int32_t, n> c;
    for (int32_t i = 0; i < n; i++)
        c.s[i] = tab[idx.s[i]];
    return c;
}

template<int32_t n> inline v_reg<uint32_t, n> v_lut(const uint32_t* tab, const v_reg<int32_t, n>& idx)
{
    v_reg<int32_t, n> c;
    for (int32_t i = 0; i < n; i++)
        c.s[i] = tab[idx.s[i]];
    return c;
}

template<int32_t n> inline v_reg<float, n> v_lut(const float* tab, const v_reg<int32_t, n>& idx)
{
    v_reg<float, n> c;
    for (int32_t i = 0; i < n; i++)
        c.s[i] = tab[idx.s[i]];
    return c;
}

template<int32_t n> inline v_reg<double, n/2> v_lut(const double* tab, const v_reg<int32_t, n>& idx)
{
    v_reg<double, n/2> c;
    for (int32_t i = 0; i < n/2; i++)
        c.s[i] = tab[idx.s[i]];
    return c;
}

template<int32_t n> inline void v_lut_deinterleave(
        const float* tab, const v_reg<int32_t, n>& idx, v_reg<float, n>& x, v_reg<float, n>& y)
{
    for (int32_t i = 0; i < n; i++)
    {
        int32_t j = idx.s[i];
        x.s[i] = tab[j];
        y.s[i] = tab[j+1];
    }
}

template<int32_t n> inline void v_lut_deinterleave(
        const double* tab, const v_reg<int32_t, n*2>& idx, v_reg<double, n>& x, v_reg<double, n>& y)
{
    for (int32_t i = 0; i < n; i++)
    {
        int32_t j = idx.s[i];
        x.s[i] = tab[j];
        y.s[i] = tab[j+1];
    }
}

template<typename _Tp, int32_t n> inline v_reg<_Tp, n> v_interleave_pairs(const v_reg<_Tp, n>& vec)
{
    v_reg<_Tp, n> c;
    for (int32_t i = 0; i < n/4; i++)
    {
        c.s[4*i  ] = vec.s[4*i  ];
        c.s[4*i+1] = vec.s[4*i+2];
        c.s[4*i+2] = vec.s[4*i+1];
        c.s[4*i+3] = vec.s[4*i+3];
    }
    return c;
}

template<typename _Tp, int32_t n> inline v_reg<_Tp, n> v_interleave_quads(const v_reg<_Tp, n>& vec)
{
    v_reg<_Tp, n> c;
    for (int32_t i = 0; i < n/8; i++)
    {
        c.s[8*i  ] = vec.s[8*i  ];
        c.s[8*i+1] = vec.s[8*i+4];
        c.s[8*i+2] = vec.s[8*i+1];
        c.s[8*i+3] = vec.s[8*i+5];
        c.s[8*i+4] = vec.s[8*i+2];
        c.s[8*i+5] = vec.s[8*i+6];
        c.s[8*i+6] = vec.s[8*i+3];
        c.s[8*i+7] = vec.s[8*i+7];
    }
    return c;
}

template<typename _Tp, int32_t n>
inline void v_interleave(const v_reg<_Tp, n>& a0, const v_reg<_Tp, n>& a1, v_reg<_Tp, n>& b0, v_reg<_Tp, n>& b1)
{
    int32_t i, i2;
    const int32_t m = n/2;
    for (i = i2 = 0; i < m; i++, i2 += 2)
    {
        b0.s[i2  ] = a0.s[i];
        b0.s[i2+1] = a1.s[i];
        b1.s[i2  ] = a0.s[m+i];
        b1.s[i2+1] = a1.s[m+i];
    }
}

template<typename _Tp, int32_t n> inline v_reg<_Tp, n> v_pack_triplets(const v_reg<_Tp, n>& vec)
{
    v_reg<_Tp, n> c;
    for (int32_t i = 0; i < n/4; i++)
    {
        c.s[3*i  ] = vec.s[4*i  ];
        c.s[3*i+1] = vec.s[4*i+1];
        c.s[3*i+2] = vec.s[4*i+2];
    }
    return c;
}

template<typename _Tp, int32_t n>
inline void v_transpose4x4( v_reg<_Tp, n>& a0, const v_reg<_Tp, n>& a1,
                            const v_reg<_Tp, n>& a2, const v_reg<_Tp, n>& a3,
                            v_reg<_Tp, n>& b0, v_reg<_Tp, n>& b1,
                            v_reg<_Tp, n>& b2, v_reg<_Tp, n>& b3 )
{
    for (int32_t i = 0; i < n / 4; i++)
    {
        b0.s[0 + i*4] = a0.s[0 + i*4]; b0.s[1 + i*4] = a1.s[0 + i*4];
        b0.s[2 + i*4] = a2.s[0 + i*4]; b0.s[3 + i*4] = a3.s[0 + i*4];
        b1.s[0 + i*4] = a0.s[1 + i*4]; b1.s[1 + i*4] = a1.s[1 + i*4];
        b1.s[2 + i*4] = a2.s[1 + i*4]; b1.s[3 + i*4] = a3.s[1 + i*4];
        b2.s[0 + i*4] = a0.s[2 + i*4]; b2.s[1 + i*4] = a1.s[2 + i*4];
        b2.s[2 + i*4] = a2.s[2 + i*4]; b2.s[3 + i*4] = a3.s[2 + i*4];
        b3.s[0 + i*4] = a0.s[3 + i*4]; b3.s[1 + i*4] = a1.s[3 + i*4];
        b3.s[2 + i*4] = a2.s[3 + i*4]; b3.s[3 + i*4] = a3.s[3 + i*4];
    }
}

#define SIMD_INIT_ZERO(_Tpvec, prefix, suffix) \
inline _Tpvec prefix##_setzero_##suffix() { return _Tpvec::zero(); }

SIMD_INIT_ZERO(v_uint8x16,  v, u8)
SIMD_INIT_ZERO(v_int8x16,   v, s8)
SIMD_INIT_ZERO(v_uint16x8,  v, u16)
SIMD_INIT_ZERO(v_int16x8,   v, s16)
SIMD_INIT_ZERO(v_uint32x4,  v, u32)
SIMD_INIT_ZERO(v_int32x4,   v, s32)
SIMD_INIT_ZERO(v_float32x4, v, f32)
SIMD_INIT_ZERO(v_float64x2, v, f64)
SIMD_INIT_ZERO(v_uint64x2,  v, u64)
SIMD_INIT_ZERO(v_int64x2,   v, s64)

#define SIMD_INIT_VAL(_Tpvec, _Tp, prefix, suffix) \
inline _Tpvec prefix##_setall_##suffix(_Tp val) { return _Tpvec::all(val); }

SIMD_INIT_VAL(v_uint8x16,  uint8_t,  v, u8)
SIMD_INIT_VAL(v_int8x16,   int8_t,   v, s8)
SIMD_INIT_VAL(v_uint16x8,  uint16_t, v, u16)
SIMD_INIT_VAL(v_int16x8,   int16_t,  v, s16)
SIMD_INIT_VAL(v_uint32x4,  uint32_t, v, u32)
SIMD_INIT_VAL(v_int32x4,   int32_t,  v, s32)
SIMD_INIT_VAL(v_float32x4, float,    v, f32)
SIMD_INIT_VAL(v_float64x2, double,   v, f64)
SIMD_INIT_VAL(v_uint64x2,  uint64_t, v, u64)
SIMD_INIT_VAL(v_int64x2,   int64_t,  v, s64)

#define SIMD_REINTERPRET(_Tp, suffix) \
template<typename _Tp0, int32_t n0> inline v_reg<_Tp, n0*sizeof(_Tp0)/sizeof(_Tp)> \
    v_reinterpret_as_##suffix(const v_reg<_Tp0, n0>& a) \
{ return a.template reinterpret_as<_Tp, n0*sizeof(_Tp0)/sizeof(_Tp)>(); }

SIMD_REINTERPRET(uint8_t, u8)
SIMD_REINTERPRET(int8_t, s8)
SIMD_REINTERPRET(uint16_t, u16)
SIMD_REINTERPRET(int16_t, s16)
SIMD_REINTERPRET(uint32_t, u32)
SIMD_REINTERPRET(int32_t, s32)
SIMD_REINTERPRET(float, f32)
SIMD_REINTERPRET(double, f64)
SIMD_REINTERPRET(uint64_t, u64)
SIMD_REINTERPRET(int64_t, s64)

#define SIMD_SHIFTL(_Tp) \
template<int32_t shift, int32_t n> inline v_reg<_Tp, n> v_shl(const v_reg<_Tp, n>& a) \
{ return a << shift; }

SIMD_SHIFTL(uint16_t)
SIMD_SHIFTL(int16_t)
SIMD_SHIFTL(uint32_t)
SIMD_SHIFTL(int32_t)
SIMD_SHIFTL(uint64_t)
SIMD_SHIFTL(int64_t)

#define SIMD_SHIFTR(_Tp) \
template<int32_t shift, int32_t n> inline v_reg<_Tp, n> v_shr(const v_reg<_Tp, n>& a) \
{ return a >> shift; }

SIMD_SHIFTR(uint16_t)
SIMD_SHIFTR(int16_t)
SIMD_SHIFTR(uint32_t)
SIMD_SHIFTR(int32_t)
SIMD_SHIFTR(uint64_t)
SIMD_SHIFTR(int64_t)

#define SIMD_RSHIFTR(_Tp) \
template<int32_t shift, int32_t n> inline v_reg<_Tp, n> v_rshr(const v_reg<_Tp, n>& a) \
{ \
    v_reg<_Tp, n> c; \
    for (int32_t i = 0; i < n; i++) \
        c.s[i] = (_Tp)((a.s[i] + ((_Tp)1 << (shift - 1))) >> shift); \
    return c; \
}

SIMD_RSHIFTR(uint16_t)
SIMD_RSHIFTR(int16_t)
SIMD_RSHIFTR(uint32_t)
SIMD_RSHIFTR(int32_t)
SIMD_RSHIFTR(uint64_t)
SIMD_RSHIFTR(int64_t)

#define SIMD_PACK(_Tp, _Tpn, pack_suffix, cast) \
template<int32_t n> inline v_reg<_Tpn, 2*n> v_##pack_suffix(const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b) \
{ \
    v_reg<_Tpn, 2*n> c; \
    for (int32_t i = 0; i < n; i++) \
    { \
        c.s[i] = cast<_Tpn>(a.s[i]); \
        c.s[i+n] = cast<_Tpn>(b.s[i]); \
    } \
    return c; \
}

SIMD_PACK(uint16_t, uint8_t,  pack,   MathUtils::SaturateCast)
SIMD_PACK(int16_t,  int8_t,   pack,   MathUtils::SaturateCast)
SIMD_PACK(uint32_t, uint16_t, pack,   MathUtils::SaturateCast)
SIMD_PACK(int32_t,  int16_t,  pack,   MathUtils::SaturateCast)
SIMD_PACK(uint64_t, uint32_t, pack,   static_cast)
SIMD_PACK(int64_t,  int32_t,  pack,   static_cast)
SIMD_PACK(int16_t,  uint8_t,  pack_u, MathUtils::SaturateCast)
SIMD_PACK(int32_t,  uint16_t, pack_u, MathUtils::SaturateCast)

#define SIMD_RSHR_PACK(_Tp, _Tpn, pack_suffix, cast) \
template<int32_t shift, int32_t n> inline v_reg<_Tpn, 2*n> v_rshr_##pack_suffix(const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b) \
{ \
    v_reg<_Tpn, 2*n> c; \
    for (int32_t i = 0; i < n; i++) \
    { \
        c.s[i] = cast<_Tpn>((a.s[i] + ((_Tp)1 << (shift - 1))) >> shift); \
        c.s[i+n] = cast<_Tpn>((b.s[i] + ((_Tp)1 << (shift - 1))) >> shift); \
    } \
    return c; \
}

SIMD_RSHR_PACK(uint16_t, uint8_t,  pack,   MathUtils::SaturateCast)
SIMD_RSHR_PACK(int16_t,  int8_t,   pack,   MathUtils::SaturateCast)
SIMD_RSHR_PACK(uint32_t, uint16_t, pack,   MathUtils::SaturateCast)
SIMD_RSHR_PACK(int32_t,  int16_t,  pack,   MathUtils::SaturateCast)
SIMD_RSHR_PACK(uint64_t, uint32_t, pack,   static_cast)
SIMD_RSHR_PACK(int64_t,  int32_t,  pack,   static_cast)
SIMD_RSHR_PACK(int16_t,  uint8_t,  pack_u, MathUtils::SaturateCast)
SIMD_RSHR_PACK(int32_t,  uint16_t, pack_u, MathUtils::SaturateCast)

#define SIMD_PACK_STORE(_Tp, _Tpn, pack_suffix, cast) \
template<int32_t n> inline void v_##pack_suffix##_store(_Tpn* ptr, const v_reg<_Tp, n>& a) \
{ \
    for (int32_t i = 0; i < n; i++) \
        ptr[i] = cast<_Tpn>(a.s[i]); \
}

SIMD_PACK_STORE(uint16_t, uint8_t,  pack,   MathUtils::SaturateCast)
SIMD_PACK_STORE(int16_t,  int8_t,   pack,   MathUtils::SaturateCast)
SIMD_PACK_STORE(uint32_t, uint16_t, pack,   MathUtils::SaturateCast)
SIMD_PACK_STORE(int32_t,  int16_t,  pack,   MathUtils::SaturateCast)
SIMD_PACK_STORE(uint64_t, uint32_t, pack,   static_cast)
SIMD_PACK_STORE(int64_t,  int32_t,  pack,   static_cast)
SIMD_PACK_STORE(int16_t,  uint8_t,  pack_u, MathUtils::SaturateCast)
SIMD_PACK_STORE(int32_t,  uint16_t, pack_u, MathUtils::SaturateCast)

#define SIMD_RSHR_PACK_STORE(_Tp, _Tpn, pack_suffix, cast) \
template<int32_t shift, int32_t n> inline void v_rshr_##pack_suffix##_store(_Tpn* ptr, const v_reg<_Tp, n>& a) \
{ \
    for (int32_t i = 0; i < n; i++) \
        ptr[i] = cast<_Tpn>((a.s[i] + ((_Tp)1 << (shift - 1))) >> shift); \
}

SIMD_RSHR_PACK_STORE(uint16_t, uint8_t,  pack,   MathUtils::SaturateCast)
SIMD_RSHR_PACK_STORE(int16_t,  int8_t,   pack,   MathUtils::SaturateCast)
SIMD_RSHR_PACK_STORE(uint32_t, uint16_t, pack,   MathUtils::SaturateCast)
SIMD_RSHR_PACK_STORE(int32_t,  int16_t,  pack,   MathUtils::SaturateCast)
SIMD_RSHR_PACK_STORE(uint64_t, uint32_t, pack,   static_cast)
SIMD_RSHR_PACK_STORE(int64_t,  int32_t,  pack,   static_cast)
SIMD_RSHR_PACK_STORE(int16_t,  uint8_t,  pack_u, MathUtils::SaturateCast)
SIMD_RSHR_PACK_STORE(int32_t,  uint16_t, pack_u, MathUtils::SaturateCast)

template<typename _Tpm, typename _Tp, int32_t n>
inline void _pack_b(_Tpm* mptr, const v_reg<_Tp, n>& a, const v_reg<_Tp, n>& b)
{
    for (int32_t i = 0; i < n; ++i)
    {
        mptr[i] = (_Tpm)a.s[i];
        mptr[i + n] = (_Tpm)b.s[i];
    }
}

template<int32_t n> inline v_reg<uint8_t, 2*n> v_pack_b(const v_reg<uint16_t, n>& a, const v_reg<uint16_t, n>& b)
{
    v_reg<uint8_t, 2*n> mask;
    _pack_b(mask.s, a, b);
    return mask;
}

template<int32_t n> inline v_reg<uint8_t, 4*n> v_pack_b(
        const v_reg<uint32_t, n>& a, const v_reg<uint32_t, n>& b, const v_reg<uint32_t, n>& c, const v_reg<uint32_t, n>& d)
{
    v_reg<uint8_t, 4*n> mask;
    _pack_b(mask.s, a, b);
    _pack_b(mask.s + 2*n, c, d);
    return mask;
}

template<int32_t n> inline v_reg<uint8_t, 8*n> v_pack_b(
        const v_reg<uint64_t, n>& a, const v_reg<uint64_t, n>& b, const v_reg<uint64_t, n>& c, const v_reg<uint64_t, n>& d,
        const v_reg<uint64_t, n>& e, const v_reg<uint64_t, n>& f, const v_reg<uint64_t, n>& g, const v_reg<uint64_t, n>& h)
{
    v_reg<uint8_t, 8*n> mask;
    _pack_b(mask.s, a, b);
    _pack_b(mask.s + 2*n, c, d);
    _pack_b(mask.s + 4*n, e, f);
    _pack_b(mask.s + 6*n, g, h);
    return mask;
}

template<int32_t n>
inline v_reg<float, n> v_matmul(const v_reg<float, n>& v,
                                const v_reg<float, n>& a, const v_reg<float, n>& b,
                                const v_reg<float, n>& c, const v_reg<float, n>& d)
{
    v_reg<float, n> res;
    for (int32_t i = 0; i < n / 4; i++)
    {
        res.s[0 + i*4] = v.s[0 + i*4] * a.s[0 + i*4] + v.s[1 + i*4] * b.s[0 + i*4] + v.s[2 + i*4] * c.s[0 + i*4] + v.s[3 + i*4] * d.s[0 + i*4];
        res.s[1 + i*4] = v.s[0 + i*4] * a.s[1 + i*4] + v.s[1 + i*4] * b.s[1 + i*4] + v.s[2 + i*4] * c.s[1 + i*4] + v.s[3 + i*4] * d.s[1 + i*4];
        res.s[2 + i*4] = v.s[0 + i*4] * a.s[2 + i*4] + v.s[1 + i*4] * b.s[2 + i*4] + v.s[2 + i*4] * c.s[2 + i*4] + v.s[3 + i*4] * d.s[2 + i*4];
        res.s[3 + i*4] = v.s[0 + i*4] * a.s[3 + i*4] + v.s[1 + i*4] * b.s[3 + i*4] + v.s[2 + i*4] * c.s[3 + i*4] + v.s[3 + i*4] * d.s[3 + i*4];
    }
    return res;
}

template<int32_t n>
inline v_reg<float, n> v_matmuladd(const v_reg<float, n>& v,
                                   const v_reg<float, n>& a, const v_reg<float, n>& b,
                                   const v_reg<float, n>& c, const v_reg<float, n>& d)
{
    v_reg<float, n> res;
    for (int32_t i = 0; i < n / 4; i++)
    {
        res.s[0 + i * 4] = v.s[0 + i * 4] * a.s[0 + i * 4] + v.s[1 + i * 4] * b.s[0 + i * 4] + v.s[2 + i * 4] * c.s[0 + i * 4] + d.s[0 + i * 4];
        res.s[1 + i * 4] = v.s[0 + i * 4] * a.s[1 + i * 4] + v.s[1 + i * 4] * b.s[1 + i * 4] + v.s[2 + i * 4] * c.s[1 + i * 4] + d.s[1 + i * 4];
        res.s[2 + i * 4] = v.s[0 + i * 4] * a.s[2 + i * 4] + v.s[1 + i * 4] * b.s[2 + i * 4] + v.s[2 + i * 4] * c.s[2 + i * 4] + d.s[2 + i * 4];
        res.s[3 + i * 4] = v.s[0 + i * 4] * a.s[3 + i * 4] + v.s[1 + i * 4] * b.s[3 + i * 4] + v.s[2 + i * 4] * c.s[3 + i * 4] + d.s[3 + i * 4];
    }
    return res;
}

template<int32_t n> inline v_reg<double, n/2> v_dotprod_expand(const v_reg<int32_t, n>& a, const v_reg<int32_t, n>& b)
{
    return v_fma(v_cvt_f64(a), v_cvt_f64(b), v_cvt_f64_high(a) * v_cvt_f64_high(b));
}

template<int32_t n> inline v_reg<double, n/2> v_dotprod_expand(
        const v_reg<int32_t, n>& a, const v_reg<int32_t, n>& b, const v_reg<double, n/2>& c)
{
    return v_fma(v_cvt_f64(a), v_cvt_f64(b), v_fma(v_cvt_f64_high(a), v_cvt_f64_high(b), c));
}

template<int32_t n> inline v_reg<double, n/2> v_dotprod_expand_fast(const v_reg<int32_t, n>& a, const v_reg<int32_t, n>& b)
{
    return v_dotprod_expand(a, b);
}

template<int32_t n> inline v_reg<double, n/2> v_dotprod_expand_fast(
        const v_reg<int32_t, n>& a, const v_reg<int32_t, n>& b, const v_reg<double, n/2>& c)
{
    return v_dotprod_expand(a, b, c);
}

////// FP16 support ///////

inline v_reg<float, simd128_width / sizeof(float)>
v_load_expand(const float16_t* ptr)
{
    v_reg<float, simd128_width / sizeof(float)> v;
    for (int32_t i = 0; i < v.nlanes; i++)
    {
        v.s[i] = ptr[i];
    }
    return v;
}

template<int32_t n> inline void
v_pack_store(float16_t* ptr, const v_reg<float, n>& v)
{
    for (int32_t i = 0; i < v.nlanes; i++)
    {
        ptr[i] = float16_t(v.s[i]);
    }
}

inline void v_cleanup() {}

#undef SIMD_DEF_TYPE_TRAITS
#undef SIMD_DEF_TYPE_TRAITS_NO_Q_TYPE
#undef SIMD_EXPAND_WITH_INTEGER_TYPES
#undef SIMD_EXPAND_WITH_FP_TYPES
#undef SIMD_EXPAND_WITH_ALL_TYPES
#undef SIMD_BIN_OP_
#undef SIMD_BIN_OP
#undef SIMD_BIT_OP_
#undef SIMD_BIT_OP
#undef SIMD_BITWISE_NOT_
#undef SIMD_MATH_FUNC
#undef SIMD_REDUCE_MINMAX_FUNC
#undef SIMD_CMP_OP
#undef SIMD_ARITHM_OP
#undef SIMD_SHIFT_OP
#undef SIMD_ROTATE_SHIFT_OP
#undef SIMD_INIT_ZERO
#undef SIMD_INIT_VAL
#undef SIMD_REINTERPRET
#undef SIMD_SHIFTL
#undef SIMD_SHIFTR
#undef SIMD_RSHIFTR
#undef SIMD_PACK
#undef SIMD_RSHR_PACK
#undef SIMD_PACK_STORE
#undef SIMD_RSHR_PACK_STORE

typedef v_uint8x16  v_uint8;
typedef v_int8x16   v_int8;
typedef v_uint16x8  v_uint16;
typedef v_int16x8   v_int16;
typedef v_uint32x4  v_uint32;
typedef v_int32x4   v_int32;
typedef v_uint64x2  v_uint64;
typedef v_int64x2   v_int64;
typedef v_float32x4 v_float32;
typedef v_float64x2 v_float64;

SIMD_SCOPE_END
} // namespace SimdOpt
