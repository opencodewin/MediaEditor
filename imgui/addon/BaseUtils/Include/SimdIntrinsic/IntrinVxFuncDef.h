namespace SimdOpt
{
SIMD_SCOPE_BEGIN(INTRIN_MODE)

inline v_uint8 vx_setall_u8(uint8_t v) { return VXPREFIX(_setall_u8)(v); }
inline v_int8 vx_setall_s8(int8_t v) { return VXPREFIX(_setall_s8)(v); }
inline v_uint16 vx_setall_u16(uint16_t v) { return VXPREFIX(_setall_u16)(v); }
inline v_int16 vx_setall_s16(int16_t v) { return VXPREFIX(_setall_s16)(v); }
inline v_int32 vx_setall_s32(int32_t v) { return VXPREFIX(_setall_s32)(v); }
inline v_uint32 vx_setall_u32(uint32_t v) { return VXPREFIX(_setall_u32)(v); }
inline v_float32 vx_setall_f32(float v) { return VXPREFIX(_setall_f32)(v); }
inline v_int64 vx_setall_s64(int64_t v) { return VXPREFIX(_setall_s64)(v); }
inline v_uint64 vx_setall_u64(uint64_t v) { return VXPREFIX(_setall_u64)(v); }
inline v_float64 vx_setall_f64(double v) { return VXPREFIX(_setall_f64)(v); }

inline v_uint8 vx_setzero_u8() { return VXPREFIX(_setzero_u8)(); }
inline v_int8 vx_setzero_s8() { return VXPREFIX(_setzero_s8)(); }
inline v_uint16 vx_setzero_u16() { return VXPREFIX(_setzero_u16)(); }
inline v_int16 vx_setzero_s16() { return VXPREFIX(_setzero_s16)(); }
inline v_int32 vx_setzero_s32() { return VXPREFIX(_setzero_s32)(); }
inline v_uint32 vx_setzero_u32() { return VXPREFIX(_setzero_u32)(); }
inline v_float32 vx_setzero_f32() { return VXPREFIX(_setzero_f32)(); }
inline v_int64 vx_setzero_s64() { return VXPREFIX(_setzero_s64)(); }
inline v_uint64 vx_setzero_u64() { return VXPREFIX(_setzero_u64)(); }
inline v_float64 vx_setzero_f64() { return VXPREFIX(_setzero_f64)(); }

inline v_uint8 vx_load(const uint8_t * ptr) { return VXPREFIX(_load)(ptr); }
inline v_int8 vx_load(const int8_t * ptr) { return VXPREFIX(_load)(ptr); }
inline v_uint16 vx_load(const uint16_t * ptr) { return VXPREFIX(_load)(ptr); }
inline v_int16 vx_load(const int16_t * ptr) { return VXPREFIX(_load)(ptr); }
inline v_int32 vx_load(const int32_t * ptr) { return VXPREFIX(_load)(ptr); }
inline v_uint32 vx_load(const uint32_t * ptr) { return VXPREFIX(_load)(ptr); }
inline v_float32 vx_load(const float * ptr) { return VXPREFIX(_load)(ptr); }
inline v_int64 vx_load(const int64_t * ptr) { return VXPREFIX(_load)(ptr); }
inline v_uint64 vx_load(const uint64_t * ptr) { return VXPREFIX(_load)(ptr); }
inline v_float64 vx_load(const double * ptr) { return VXPREFIX(_load)(ptr); }

inline v_uint8 vx_load_aligned(const uint8_t * ptr) { return VXPREFIX(_load_aligned)(ptr); }
inline v_int8 vx_load_aligned(const int8_t * ptr) { return VXPREFIX(_load_aligned)(ptr); }
inline v_uint16 vx_load_aligned(const uint16_t * ptr) { return VXPREFIX(_load_aligned)(ptr); }
inline v_int16 vx_load_aligned(const int16_t * ptr) { return VXPREFIX(_load_aligned)(ptr); }
inline v_int32 vx_load_aligned(const int32_t * ptr) { return VXPREFIX(_load_aligned)(ptr); }
inline v_uint32 vx_load_aligned(const uint32_t * ptr) { return VXPREFIX(_load_aligned)(ptr); }
inline v_float32 vx_load_aligned(const float * ptr) { return VXPREFIX(_load_aligned)(ptr); }
inline v_int64 vx_load_aligned(const int64_t * ptr) { return VXPREFIX(_load_aligned)(ptr); }
inline v_uint64 vx_load_aligned(const uint64_t * ptr) { return VXPREFIX(_load_aligned)(ptr); }
inline v_float64 vx_load_aligned(const double * ptr) { return VXPREFIX(_load_aligned)(ptr); }

inline v_uint8 vx_load_low(const uint8_t * ptr) { return VXPREFIX(_load_low)(ptr); }
inline v_int8 vx_load_low(const int8_t * ptr) { return VXPREFIX(_load_low)(ptr); }
inline v_uint16 vx_load_low(const uint16_t * ptr) { return VXPREFIX(_load_low)(ptr); }
inline v_int16 vx_load_low(const int16_t * ptr) { return VXPREFIX(_load_low)(ptr); }
inline v_int32 vx_load_low(const int32_t * ptr) { return VXPREFIX(_load_low)(ptr); }
inline v_uint32 vx_load_low(const uint32_t * ptr) { return VXPREFIX(_load_low)(ptr); }
inline v_float32 vx_load_low(const float * ptr) { return VXPREFIX(_load_low)(ptr); }
inline v_int64 vx_load_low(const int64_t * ptr) { return VXPREFIX(_load_low)(ptr); }
inline v_uint64 vx_load_low(const uint64_t * ptr) { return VXPREFIX(_load_low)(ptr); }
inline v_float64 vx_load_low(const double * ptr) { return VXPREFIX(_load_low)(ptr); }

inline v_uint8 vx_load_halves(const uint8_t * ptr0, const uint8_t * ptr1) { return VXPREFIX(_load_halves)(ptr0, ptr1); }
inline v_int8 vx_load_halves(const int8_t * ptr0, const int8_t * ptr1) { return VXPREFIX(_load_halves)(ptr0, ptr1); }
inline v_uint16 vx_load_halves(const uint16_t * ptr0, const uint16_t * ptr1) { return VXPREFIX(_load_halves)(ptr0, ptr1); }
inline v_int16 vx_load_halves(const int16_t * ptr0, const int16_t * ptr1) { return VXPREFIX(_load_halves)(ptr0, ptr1); }
inline v_int32 vx_load_halves(const int32_t * ptr0, const int32_t * ptr1) { return VXPREFIX(_load_halves)(ptr0, ptr1); }
inline v_uint32 vx_load_halves(const uint32_t * ptr0, const uint32_t * ptr1) { return VXPREFIX(_load_halves)(ptr0, ptr1); }
inline v_float32 vx_load_halves(const float * ptr0, const float * ptr1) { return VXPREFIX(_load_halves)(ptr0, ptr1); }
inline v_int64 vx_load_halves(const int64_t * ptr0, const int64_t * ptr1) { return VXPREFIX(_load_halves)(ptr0, ptr1); }
inline v_uint64 vx_load_halves(const uint64_t * ptr0, const uint64_t * ptr1) { return VXPREFIX(_load_halves)(ptr0, ptr1); }
inline v_float64 vx_load_halves(const double * ptr0, const double * ptr1) { return VXPREFIX(_load_halves)(ptr0, ptr1); }

inline v_uint8 vx_lut(const uint8_t * ptr, const int32_t* idx) { return VXPREFIX(_lut)(ptr, idx); }
inline v_int8 vx_lut(const int8_t * ptr, const int32_t* idx) { return VXPREFIX(_lut)(ptr, idx); }
inline v_uint16 vx_lut(const uint16_t * ptr, const int32_t* idx) { return VXPREFIX(_lut)(ptr, idx); }
inline v_int16 vx_lut(const int16_t* ptr, const int32_t* idx) { return VXPREFIX(_lut)(ptr, idx); }
inline v_int32 vx_lut(const int32_t* ptr, const int32_t* idx) { return VXPREFIX(_lut)(ptr, idx); }
inline v_uint32 vx_lut(const uint32_t* ptr, const int32_t* idx) { return VXPREFIX(_lut)(ptr, idx); }
inline v_float32 vx_lut(const float* ptr, const int32_t* idx) { return VXPREFIX(_lut)(ptr, idx); }
inline v_int64 vx_lut(const int64_t * ptr, const int32_t* idx) { return VXPREFIX(_lut)(ptr, idx); }
inline v_uint64 vx_lut(const uint64_t * ptr, const int32_t* idx) { return VXPREFIX(_lut)(ptr, idx); }
inline v_float64 vx_lut(const double* ptr, const int32_t* idx) { return VXPREFIX(_lut)(ptr, idx); }

inline v_uint8 vx_lut_pairs(const uint8_t * ptr, const int32_t* idx) { return VXPREFIX(_lut_pairs)(ptr, idx); }
inline v_int8 vx_lut_pairs(const int8_t * ptr, const int32_t* idx) { return VXPREFIX(_lut_pairs)(ptr, idx); }
inline v_uint16 vx_lut_pairs(const uint16_t * ptr, const int32_t* idx) { return VXPREFIX(_lut_pairs)(ptr, idx); }
inline v_int16 vx_lut_pairs(const int16_t* ptr, const int32_t* idx) { return VXPREFIX(_lut_pairs)(ptr, idx); }
inline v_int32 vx_lut_pairs(const int32_t* ptr, const int32_t* idx) { return VXPREFIX(_lut_pairs)(ptr, idx); }
inline v_uint32 vx_lut_pairs(const uint32_t* ptr, const int32_t* idx) { return VXPREFIX(_lut_pairs)(ptr, idx); }
inline v_float32 vx_lut_pairs(const float* ptr, const int32_t* idx) { return VXPREFIX(_lut_pairs)(ptr, idx); }
inline v_int64 vx_lut_pairs(const int64_t * ptr, const int32_t* idx) { return VXPREFIX(_lut_pairs)(ptr, idx); }
inline v_uint64 vx_lut_pairs(const uint64_t * ptr, const int32_t* idx) { return VXPREFIX(_lut_pairs)(ptr, idx); }
inline v_float64 vx_lut_pairs(const double* ptr, const int32_t* idx) { return VXPREFIX(_lut_pairs)(ptr, idx); }

inline v_uint8 vx_lut_quads(const uint8_t* ptr, const int32_t* idx) { return VXPREFIX(_lut_quads)(ptr, idx); }
inline v_int8 vx_lut_quads(const int8_t* ptr, const int32_t* idx) { return VXPREFIX(_lut_quads)(ptr, idx); }
inline v_uint16 vx_lut_quads(const uint16_t* ptr, const int32_t* idx) { return VXPREFIX(_lut_quads)(ptr, idx); }
inline v_int16 vx_lut_quads(const int16_t* ptr, const int32_t* idx) { return VXPREFIX(_lut_quads)(ptr, idx); }
inline v_int32 vx_lut_quads(const int32_t* ptr, const int32_t* idx) { return VXPREFIX(_lut_quads)(ptr, idx); }
inline v_uint32 vx_lut_quads(const uint32_t* ptr, const int32_t* idx) { return VXPREFIX(_lut_quads)(ptr, idx); }
inline v_float32 vx_lut_quads(const float* ptr, const int32_t* idx) { return VXPREFIX(_lut_quads)(ptr, idx); }

inline v_uint16 vx_load_expand(const uint8_t * ptr) { return VXPREFIX(_load_expand)(ptr); }
inline v_int16 vx_load_expand(const int8_t * ptr) { return VXPREFIX(_load_expand)(ptr); }
inline v_uint32 vx_load_expand(const uint16_t * ptr) { return VXPREFIX(_load_expand)(ptr); }
inline v_int32 vx_load_expand(const int16_t* ptr) { return VXPREFIX(_load_expand)(ptr); }
inline v_int64 vx_load_expand(const int32_t* ptr) { return VXPREFIX(_load_expand)(ptr); }
inline v_uint64 vx_load_expand(const uint32_t* ptr) { return VXPREFIX(_load_expand)(ptr); }
inline v_float32 vx_load_expand(const float16_t * ptr) { return VXPREFIX(_load_expand)(ptr); }

inline v_uint32 vx_load_expand_q(const uint8_t * ptr) { return VXPREFIX(_load_expand_q)(ptr); }
inline v_int32 vx_load_expand_q(const int8_t * ptr) { return VXPREFIX(_load_expand_q)(ptr); }

inline void vx_cleanup() { VXPREFIX(_cleanup)(); }

template<typename _Tp, typename _Tvec> static inline
void vx_store(_Tp* dst, const _Tvec& v) { return v_store(dst, v); }

template<typename _Tp, typename _Tvec> static inline
void vx_store_aligned(_Tp* dst, const _Tvec& v) { return v_store_aligned(dst, v); }

SIMD_SCOPE_END
} // namespace SimdOpt
