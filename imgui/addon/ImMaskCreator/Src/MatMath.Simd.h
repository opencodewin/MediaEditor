namespace SimdOpt
{
SIMD_SCOPE_BEGIN(INTRIN_MODE)

template <typename T> struct VMin
{
    typedef T vtype;
    vtype operator()(const vtype& a, const vtype& b) const { return v_min(a,b); }
};
template <typename T> struct VMax
{
    typedef T vtype;
    vtype operator()(const vtype& a, const vtype& b) const { return v_max(a, b); }
};

template<class VecUpdate> struct RowVecOpS2D1
{
    typedef typename VecUpdate::vtype vtype;
    typedef typename vtype::lane_type stype;

    int operator()(const uint8_t* _src1, const uint8_t* _src2, uint8_t* _dst, int width) const
    {
        const stype* src1 = (const stype*)_src1;
        const stype* src2 = (const stype*)_src2;
        stype* dst = (stype*)_dst;
        int i;
        VecUpdate updateOp;

        for (i = 0; i <= width - 4*vtype::nlanes; i += 4*vtype::nlanes)
        {
            const stype* sptr = src1 + i;
            vtype s0 = vx_load(sptr);
            vtype s1 = vx_load(sptr + vtype::nlanes);
            vtype s2 = vx_load(sptr + 2*vtype::nlanes);
            vtype s3 = vx_load(sptr + 3*vtype::nlanes);
            sptr = src2 + i;
            s0 = updateOp(s0, vx_load(sptr));
            s1 = updateOp(s1, vx_load(sptr + vtype::nlanes));
            s2 = updateOp(s2, vx_load(sptr + 2*vtype::nlanes));
            s3 = updateOp(s3, vx_load(sptr + 3*vtype::nlanes));
            v_store(dst + i, s0);
            v_store(dst + i + vtype::nlanes, s1);
            v_store(dst + i + 2*vtype::nlanes, s2);
            v_store(dst + i + 3*vtype::nlanes, s3);
        }
        if (i <= width - 2*vtype::nlanes)
        {
            const stype* sptr = src1 + i;
            vtype s0 = vx_load(sptr);
            vtype s1 = vx_load(sptr + vtype::nlanes);
            sptr = src2 + i;
            s0 = updateOp(s0, vx_load(sptr));
            s1 = updateOp(s1, vx_load(sptr + vtype::nlanes));
            v_store(dst + i, s0);
            v_store(dst + i + vtype::nlanes, s1);
            i += 2*vtype::nlanes;
        }
        if (i <= width - vtype::nlanes)
        {
            vtype s0 = vx_load(src1 + i);
            s0 = updateOp(s0, vx_load(src2 + i));
            v_store(dst + i, s0);
            i += vtype::nlanes;
        }
        if (i <= width - vtype::nlanes/2)
        {
            vtype s0 = vx_load_low(src1 + i);
            s0 = updateOp(s0, vx_load_low(src2 + i));
            v_store_low(dst + i, s0);
            i += vtype::nlanes/2;
        }
        return i;
    }
};

template<class Op, class VecOp> struct MatMax : public MatUtils::MatOp2
{
    typedef typename Op::rtype T;

    void operator()(const ImGui::ImMat& _src1, const ImGui::ImMat& _src2, ImGui::ImMat& _dst) override
    {
        int width = _src1.w;
        const int height = _src1.h;
        const int cn = _src1.c;
        const int src1LineSize = _src1.w*_src1.c*_src1.elemsize;
        const int src2LineSize = _src2.w*_src2.c*_src2.elemsize;

        if (_dst.w != _src1.w || _dst.h != _src1.h || _dst.c != _src1.c || _dst.type != _src1.type)
        {
            _dst.release();
            _dst.create_type(_src1.w, _src1.h, _src1.c, _src1.type);
        }
        const int dstLineSize = _dst.w*_dst.c*_dst.elemsize;

        Op op;
        const uint8_t* src1 = (const uint8_t*)_src1.data;
        const uint8_t* src2 = (const uint8_t*)_src2.data;
        uint8_t* dst = (uint8_t*)_dst.data;
        width *= cn;
        int i, j;
        for (j = 0; j < height; j++)
        {
            i = vecOp(src1, src2, dst, width);

            T* D = (T*)dst;
            for (; i <= width - 4; i += 4)
            {
                const T* sptr = (const T*)src1 + i;
                T s0 = sptr[0], s1 = sptr[1], s2 = sptr[2], s3 = sptr[3];
                sptr = (const T*)src2 + i;
                s0 = op(s0, sptr[0]); s1 = op(s1, sptr[1]);
                s2 = op(s2, sptr[2]); s3 = op(s3, sptr[3]);
                D[i  ] = s0; D[i+1] = s1;
                D[i+2] = s2; D[i+3] = s3;
            }
            for (; i < width; i++)
            {
                T s0 = *((const T*)src1 + i);
                s0 = op(s0, *((const T*)src2 + i));
                D[i] = s0;
            }

            src1 += src1LineSize;
            src2 += src2LineSize;
            dst += dstLineSize;
        }
    }

    VecOp vecOp;
};

template<typename T1, typename T2> struct VExpandSizeX2
{
    typedef T1 vtype1;
    typedef T2 vtype2;
    void expand(const vtype1& a, vtype2& b0, vtype2& b1) { v_expand(a, b0, b1); }
    void expand2(vtype2& b0, vtype2& b1) {}
    void expand3(vtype2& b0, vtype2& b1) {}
};

template<typename T1, typename M1, typename T2> struct VExpandSizeX4
{
    typedef T1 vtype1;
    typedef T2 vtype2;
    typedef M1 mtype1;
    void expand(const vtype1& a, vtype2& b0, vtype2& b1) {
        M1 m10;
        v_expand(a, m10, m11);
        v_expand(m10, b0, b1);
    }
    void expand2(vtype2& b0, vtype2& b1) {
        v_expand(m11, b0, b1);
    }
    void expand3(vtype2& b0, vtype2& b1) {}
    M1 m11;
};

template<typename T1, typename M1, typename M2, typename T2> struct VExpandSizeX8
{
    typedef T1 vtype1;
    typedef T2 vtype2;
    typedef M1 mtype1;
    typedef M2 mtype2;
    void expand(const vtype1& a, vtype2& b0, vtype2& b1) {
        M1 m10;
        v_expand(a, m10, m11);
        M2 m20;
        v_expand(m10, m20, m21);
        v_expand(m20, b0, b1);
    }
    void expand2(vtype2& b0, vtype2& b1) {
        M2 m20;
        v_expand(m11, m20, m21);
        v_expand(m20, b0, b1);
    }
    void expand3(vtype2& b0, vtype2& b1) {
        v_expand(m21, b0, b1);
    }
    M1 m11;
    M2 m21;
};

template <> struct VExpandSizeX2<v_float32, v_float64>
{
    typedef v_float32 vtype1;
    typedef v_float64 vtype2;
    void expand(const vtype1& a, vtype2& b0, vtype2& b1) { b0 = v_cvt_f64(a); b1 = v_cvt_f64_high(a); }
    void expand2(vtype2& b0, vtype2& b1) {}
    void expand3(vtype2& b0, vtype2& b1) {}
};

template<typename T> struct VCvtF32
{
    typedef T vtype1;
    typedef v_float32 vtype2;
    void operator()(const vtype1& a, vtype2& b) { b = v_cvt_f32(a); }
};

template<typename T> struct VCvtF64
{
    typedef T vtype1;
    typedef v_float64 vtype2;
    void operator()(const vtype1& a, vtype2& b) { b = v_cvt_f64(a); }
};

template<typename T1, typename T2> struct VRound
{
    typedef T1 vtype1;
    typedef T2 vtype2;
    void operator()(const vtype1& a, vtype2& b) { b = v_round(a); }
};

template<typename T1, typename T2> struct VTrunc
{
    typedef T1 vtype1;
    typedef T2 vtype2;

    vtype2 operator()(const vtype1& a) { return v_trunc(a); }
};

template<typename T1, typename T2> struct VFloor
{
    typedef T1 vtype1;
    typedef T2 vtype2;

    vtype2 operator()(const vtype1& a) { return v_floor(a); }
};

template<typename T1, typename T2> struct VCeil
{
    typedef T1 vtype1;
    typedef T2 vtype2;

    vtype2 operator()(const vtype1& a) { return v_ceil(a); }
};

template<typename T> struct VColDepExpandF32
{
    typedef T vtype1;
    typedef v_float32 vtype2;
    void operator()(const vtype1& a, vtype2& b) { b = v_cvt_f32(a); }
    void expand(const vtype1& a, vtype2& b0, vtype2& b1) {}
    void expand2(vtype1& b0, vtype2& b1) {}
    void expand3(vtype1& b0, vtype2& b1) {}
};

template<> struct VColDepExpandF32<v_uint8>
{
    typedef v_uint8 vtype1;
    typedef v_float32 vtype2;

    VColDepExpandF32() {
        vscale = vx_setall_f32(1.f/255.f);
    }
    void expand(const vtype1& a, vtype2& b0, vtype2& b1) {
        v_uint16 m10;
        v_uint32 m20, m21;
        v_expand(a, m10, m11);
        v_expand(m10, m20, m21);
        b0 = v_cvt_f32(v_reinterpret_as_s32(m20)) * vscale;
        b1 = v_cvt_f32(v_reinterpret_as_s32(m21)) * vscale;
    }
    void expand2(vtype2& b0, vtype2& b1) {
        v_uint32 m20, m21;
        v_expand(m11, m20, m21);
        b0 = v_cvt_f32(v_reinterpret_as_s32(m20)) * vscale;
        b1 = v_cvt_f32(v_reinterpret_as_s32(m21)) * vscale;
    }
    void expand3(vtype2& b0, vtype2& b1) {}

    v_uint16 m11;
    v_float32 vscale;
};

template<> struct VColDepExpandF32<v_uint16>
{
    typedef v_uint16 vtype1;
    typedef v_float32 vtype2;

    VColDepExpandF32() {
        vscale = vx_setall_f32(1.f/65535.f);
    }
    void expand(const vtype1& a, vtype2& b0, vtype2& b1) {
        v_uint32 m10, m11;
        v_expand(a, m10, m11);
        b0 = v_cvt_f32(v_reinterpret_as_s32(m10)) * vscale;
        b1 = v_cvt_f32(v_reinterpret_as_s32(m11)) * vscale;
    }
    void expand2(vtype2& b0, vtype2& b1) {}
    void expand3(vtype2& b0, vtype2& b1) {}

    v_float32 vscale;
};

template<typename T> struct VColDepExpandU16
{
    typedef T vtype1;
    typedef v_uint16 vtype2;
    void expand(const vtype1& a, vtype2& b0, vtype2& b1) {}
    void expand2(vtype1& b0, vtype2& b1) {}
    void expand3(vtype1& b0, vtype2& b1) {}
};

template<> struct VColDepExpandU16<v_uint8>
{
    typedef v_uint8 vtype1;
    typedef v_uint16 vtype2;

    void expand(const vtype1& a, vtype2& b0, vtype2& b1) {
        v_expand(a, b0, b1);
        b0 = b0 << 8;
        b1 = b1 << 8;
    }
    void expand2(vtype2& b0, vtype2& b1) {}
    void expand3(vtype2& b0, vtype2& b1) {}
};

template<class VecConvert> struct RowVecConvert
{
    typedef typename VecConvert::vtype1 VecSrcType;
    typedef typename VecConvert::vtype2 VecDstType;
    typedef typename VecSrcType::lane_type SrcType;
    typedef typename VecDstType::lane_type DstType;

    int operator()(const uint8_t* _src, uint8_t* _dst, int width) const
    {
        const SrcType* sptr = (const SrcType*)_src;
        DstType* dptr = (DstType*)_dst;
        VecConvert vecOp;
        VecSrcType s0;
        VecDstType d0;
        int i;
        for (i = 0; i <= width-4*VecSrcType::nlanes; i += 4*VecSrcType::nlanes)
        {

            s0 = vx_load(sptr); sptr += VecSrcType::nlanes;
            vecOp(s0, d0);
            v_store(dptr, d0); dptr += VecDstType::nlanes;

            s0 = vx_load(sptr); sptr += VecSrcType::nlanes;
            vecOp(s0, d0);
            v_store(dptr, d0); dptr += VecDstType::nlanes;

            s0 = vx_load(sptr); sptr += VecSrcType::nlanes;
            vecOp(s0, d0);
            v_store(dptr, d0); dptr += VecDstType::nlanes;

            s0 = vx_load(sptr); sptr += VecSrcType::nlanes;
            vecOp(s0, d0);
            v_store(dptr, d0); dptr += VecDstType::nlanes;
        }
        if (i <= width-2*VecSrcType::nlanes)
        {
            s0 = vx_load(sptr); sptr += VecSrcType::nlanes;
            vecOp(s0, d0);
            v_store(dptr, d0); dptr += VecDstType::nlanes;

            s0 = vx_load(sptr); sptr += VecSrcType::nlanes;
            vecOp(s0, d0);
            v_store(dptr, d0); dptr += VecDstType::nlanes;

            i += 2*VecSrcType::nlanes;
        }
        if (i <= width-VecSrcType::nlanes)
        {
            s0 = vx_load(sptr); sptr += VecSrcType::nlanes;
            vecOp(s0, d0);
            v_store(dptr, d0); dptr += VecDstType::nlanes;

            i += VecSrcType::nlanes;
        }
        if (i <= width-VecSrcType::nlanes/2)
        {
            s0 = vx_load_low(sptr);
            vecOp(s0, d0);
            v_store_low(dptr, d0);

            i += VecSrcType::nlanes/2;
        }
        return i;
    }
};

template<class VecExpand> struct RowVecExpand
{
    typedef typename VecExpand::vtype1 VecSrcType;
    typedef typename VecExpand::vtype2 VecDstType;
    typedef typename VecSrcType::lane_type SrcType;
    typedef typename VecDstType::lane_type DstType;

    int operator()(const uint8_t* _src, uint8_t* _dst, int width) const
    {
        const SrcType* sptr = (const SrcType*)_src;
        DstType* dptr = (DstType*)_dst;
        VecExpand vecOp;
        VecSrcType s0;
        VecDstType d0, d1;
        int i;
        if (VecSrcType::nlanes == VecDstType::nlanes*2)
        {
            for (i = 0; i <= width-4*VecSrcType::nlanes; i += 4*VecSrcType::nlanes)
            {
                s0 = vx_load(sptr); sptr += VecSrcType::nlanes;
                vecOp.expand(s0, d0, d1);
                v_store(dptr, d0); dptr += VecDstType::nlanes;
                v_store(dptr, d1); dptr += VecDstType::nlanes;

                s0 = vx_load(sptr); sptr += VecSrcType::nlanes;
                vecOp.expand(s0, d0, d1);
                v_store(dptr, d0); dptr += VecDstType::nlanes;
                v_store(dptr, d1); dptr += VecDstType::nlanes;

                s0 = vx_load(sptr); sptr += VecSrcType::nlanes;
                vecOp.expand(s0, d0, d1);
                v_store(dptr, d0); dptr += VecDstType::nlanes;
                v_store(dptr, d1); dptr += VecDstType::nlanes;

                s0 = vx_load(sptr); sptr += VecSrcType::nlanes;
                vecOp.expand(s0, d0, d1);
                v_store(dptr, d0); dptr += VecDstType::nlanes;
                v_store(dptr, d1); dptr += VecDstType::nlanes;
            }
            if (i <= width-2*VecSrcType::nlanes)
            {
                s0 = vx_load(sptr); sptr += VecSrcType::nlanes;
                vecOp.expand(s0, d0, d1);
                v_store(dptr, d0); dptr += VecDstType::nlanes;
                v_store(dptr, d1); dptr += VecDstType::nlanes;

                s0 = vx_load(sptr); sptr += VecSrcType::nlanes;
                vecOp.expand(s0, d0, d1);
                v_store(dptr, d0); dptr += VecDstType::nlanes;
                v_store(dptr, d1); dptr += VecDstType::nlanes;

                i += 2*VecSrcType::nlanes;
            }
            if (i <= width-VecSrcType::nlanes)
            {
                s0 = vx_load(sptr); sptr += VecSrcType::nlanes;
                vecOp.expand(s0, d0, d1);
                v_store(dptr, d0); dptr += VecDstType::nlanes;
                v_store(dptr, d1); dptr += VecDstType::nlanes;

                i += VecSrcType::nlanes;
            }
            if (i <= width-VecSrcType::nlanes/2)
            {
                s0 = vx_load_low(sptr);
                vecOp.expand(s0, d0, d1);
                v_store(dptr, d0);

                i += VecSrcType::nlanes/2;
            }
        }
        else if (VecSrcType::nlanes == VecDstType::nlanes*4)
        {
            for (i = 0; i <= width-2*VecSrcType::nlanes; i += 2*VecSrcType::nlanes)
            {
                s0 = vx_load(sptr); sptr += VecSrcType::nlanes;
                vecOp.expand(s0, d0, d1);
                v_store(dptr, d0); dptr += VecDstType::nlanes;
                v_store(dptr, d1); dptr += VecDstType::nlanes;

                vecOp.expand2(d0, d1);
                v_store(dptr, d0); dptr += VecDstType::nlanes;
                v_store(dptr, d1); dptr += VecDstType::nlanes;

                s0 = vx_load(sptr); sptr += VecSrcType::nlanes;
                vecOp.expand(s0, d0, d1);
                v_store(dptr, d0); dptr += VecDstType::nlanes;
                v_store(dptr, d1); dptr += VecDstType::nlanes;

                vecOp.expand2(d0, d1);
                v_store(dptr, d0); dptr += VecDstType::nlanes;
                v_store(dptr, d1); dptr += VecDstType::nlanes;
            }
            if (i <= width-VecSrcType::nlanes)
            {
                s0 = vx_load(sptr); sptr += VecSrcType::nlanes;
                vecOp.expand(s0, d0, d1);
                v_store(dptr, d0); dptr += VecDstType::nlanes;
                v_store(dptr, d1); dptr += VecDstType::nlanes;

                vecOp.expand2(d0, d1);
                v_store(dptr, d0); dptr += VecDstType::nlanes;
                v_store(dptr, d1); dptr += VecDstType::nlanes;

                i += 2*VecSrcType::nlanes;
            }
            if (i <= width-VecSrcType::nlanes/2)
            {
                s0 = vx_load_low(sptr);
                vecOp.expand(s0, d0, d1);
                v_store(dptr, d0); dptr += VecDstType::nlanes;
                v_store(dptr, d1);

                i += VecSrcType::nlanes/2;
            }
        }
        else if (VecSrcType::nlanes == VecDstType::nlanes*8)
        {
            for (i = 0; i <= width-VecSrcType::nlanes; i += VecSrcType::nlanes)
            {
                s0 = vx_load(sptr); sptr += VecSrcType::nlanes;
                vecOp.expand(s0, d0, d1);
                v_store(dptr, d0); dptr += VecDstType::nlanes;
                v_store(dptr, d1); dptr += VecDstType::nlanes;

                vecOp.expand3(d0, d1);
                v_store(dptr, d0); dptr += VecDstType::nlanes;
                v_store(dptr, d1); dptr += VecDstType::nlanes;

                vecOp.expand2(d0, d1);
                v_store(dptr, d0); dptr += VecDstType::nlanes;
                v_store(dptr, d1); dptr += VecDstType::nlanes;

                vecOp.expand3(d0, d1);
                v_store(dptr, d0); dptr += VecDstType::nlanes;
                v_store(dptr, d1); dptr += VecDstType::nlanes;
            }
            if (i <= width-VecSrcType::nlanes/2)
            {
                s0 = vx_load_low(sptr);
                vecOp.expand(s0, d0, d1);
                v_store(dptr, d0); dptr += VecDstType::nlanes;
                v_store(dptr, d1);

                vecOp.expand3(d0, d1);
                v_store(dptr, d0); dptr += VecDstType::nlanes;
                v_store(dptr, d1); dptr += VecDstType::nlanes;

                i += VecSrcType::nlanes/2;
            }
        }
        else
        {
            throw std::runtime_error("INVALID code branch.");
        }
        return i;
    }
};

template<typename T1, typename T2> struct VPackSizeX2
{
    typedef T1 vtype1;
    typedef T2 vtype2;
    void pack(const vtype1& a0, const vtype1& a1, vtype2& b) { b = v_pack(a0, a1); }
    void pack2(const vtype1& a0, const vtype1& a1, const vtype1& a2, const vtype1& a3, vtype2& b) {}
};

template<> struct VPackSizeX2<v_float64, v_float32>
{
    typedef v_float64 vtype1;
    typedef v_float32 vtype2;
    void pack(const vtype1& a0, const vtype1& a1, vtype2& b) { b = v_cvt_f32(a0, a1); }
    void pack2(const vtype1& a0, const vtype1& a1, const vtype1& a2, const vtype1& a3, vtype2& b) {}
};

template<typename T1, typename T2> struct VPacku
{
    typedef T1 vtype1;
    typedef T2 vtype2;
    void pack(const vtype1& a0, const vtype1& a1, vtype2& b) { b = v_pack_u(a0, a1); }
    void pack2(const vtype1& a0, const vtype1& a1, const vtype1& a2, const vtype1& a3, vtype2& b) {}
};

template<typename T1, typename M1, typename T2> struct VPackSizeX4
{
    typedef T1 vtype1;
    typedef T2 vtype2;
    typedef M1 mtype1;
    void pack(const vtype1& a0, const vtype1& a1, vtype2& b) {}
    void pack2(const vtype1& a0, const vtype1& a1, const vtype1& a2, const vtype1& a3, vtype2& b) {
        M1 m0 = v_pack(a0, a1);
        M1 m1 = v_pack(a2, a3);
        b = v_pack(m0, m1);
    }
};

template<typename T1, typename T2> struct VColDepPack
{
    typedef T1 vtype1;
    typedef T2 vtype2;
    void pack(const vtype1& a0, const vtype1& a1, vtype2& b) { throw std::runtime_error("INVALID code branch."); }
    void pack2(const vtype1& a0, const vtype1& a1, const vtype1& a2, const vtype1& a3, vtype2& b) { throw std::runtime_error("INVALID code branch."); }
};

template<> struct VColDepPack<v_uint16, v_uint8>
{
    typedef v_uint16 vtype1;
    typedef v_uint8 vtype2;

    VColDepPack() {
        halfOne = vx_setall_u16((uint16_t)0x80);
    }
    void pack(const vtype1& a0, const vtype1& a1, vtype2& b) {
        b = v_pack((a0+halfOne)>>8, (a1+halfOne)>>8);
    }
    void pack2(const vtype1& a0, const vtype1& a1, const vtype1& a2, const vtype1& a3, vtype2& b) { throw std::runtime_error("INVALID code branch."); }

    v_uint16 halfOne;
};

template<> struct VColDepPack<v_float32, v_uint8>
{
    typedef v_float32 vtype1;
    typedef v_uint8 vtype2;

    VColDepPack() {
        vscale = vx_setall_f32(255.f);
    }
    void pack(const vtype1& a0, const vtype1& a1, vtype2& b) { throw std::runtime_error("INVALID code branch."); }
    void pack2(const vtype1& a0, const vtype1& a1, const vtype1& a2, const vtype1& a3, vtype2& b) {
        v_int32 m10 = v_round(a0*vscale);
        v_int32 m11 = v_round(a1*vscale);
        v_int16 m20 = v_pack(m10, m11);
        m10 = v_round(a2*vscale);
        m11 = v_round(a3*vscale);
        v_int16 m21 = v_pack(m10, m11);
        b = v_pack_u(m20, m21);
    }

    v_float32 vscale;
};

template<> struct VColDepPack<v_float32, v_uint16>
{
    typedef v_float32 vtype1;
    typedef v_uint16 vtype2;

    VColDepPack() {
        vscale = vx_setall_f32(65535.f);
    }
    void pack(const vtype1& a0, const vtype1& a1, vtype2& b) {
        v_int32 m10 = v_round(a0*vscale);
        v_int32 m11 = v_round(a1*vscale);
        b = v_pack_u(m10, m11);
    }
    void pack2(const vtype1& a0, const vtype1& a1, const vtype1& a2, const vtype1& a3, vtype2& b) { throw std::runtime_error("INVALID code branch."); }

    v_float32 vscale;
};

template<class VecPack> struct RowVecPack
{
    typedef typename VecPack::vtype1 VecSrcType;
    typedef typename VecPack::vtype2 VecDstType;
    typedef typename VecSrcType::lane_type SrcType;
    typedef typename VecDstType::lane_type DstType;

    int operator()(const uint8_t* _src, uint8_t* _dst, int width) const
    {
        const SrcType* sptr = (const SrcType*)_src;
        DstType* dptr = (DstType*)_dst;
        VecPack vecOp;
        VecDstType d0;
        int i;
        if (VecSrcType::nlanes*2 == VecDstType::nlanes)
        {
            VecSrcType s0, s1;
            for (i = 0; i <= width-4*VecSrcType::nlanes; i += 4*VecSrcType::nlanes)
            {
                s0 = vx_load(sptr); sptr += VecSrcType::nlanes;
                s1 = vx_load(sptr); sptr += VecSrcType::nlanes;
                vecOp.pack(s0, s1, d0);
                v_store(dptr, d0); dptr += VecDstType::nlanes;

                s0 = vx_load(sptr); sptr += VecSrcType::nlanes;
                s1 = vx_load(sptr); sptr += VecSrcType::nlanes;
                vecOp.pack(s0, s1, d0);
                v_store(dptr, d0); dptr += VecDstType::nlanes;
            }
            if (i <= width-2*VecSrcType::nlanes)
            {
                s0 = vx_load(sptr); sptr += VecSrcType::nlanes;
                s1 = vx_load(sptr); sptr += VecSrcType::nlanes;
                vecOp.pack(s0, s1, d0);
                v_store(dptr, d0); dptr += VecDstType::nlanes;

                i += 2*VecSrcType::nlanes;
            }
            if (i <= width-VecSrcType::nlanes)
            {
                s0 = vx_load(sptr);
                vecOp.pack(s0, s1, d0);
                v_store_low(dptr, d0);

                i += VecSrcType::nlanes;
            }
        }
        else if (VecSrcType::nlanes*4 == VecDstType::nlanes)
        {
            VecSrcType s0, s1, s2, s3;
            for (i = 0; i <= width-4*VecSrcType::nlanes; i += 4*VecSrcType::nlanes)
            {
                s0 = vx_load(sptr); sptr += VecSrcType::nlanes;
                s1 = vx_load(sptr); sptr += VecSrcType::nlanes;
                s2 = vx_load(sptr); sptr += VecSrcType::nlanes;
                s3 = vx_load(sptr); sptr += VecSrcType::nlanes;
                vecOp.pack2(s0, s1, s2, s3, d0);
                v_store(dptr, d0); dptr += VecDstType::nlanes;
            }
            if (i <= width-2*VecSrcType::nlanes)
            {
                s0 = vx_load(sptr); sptr += VecSrcType::nlanes;
                s1 = vx_load(sptr);
                vecOp.pack2(s0, s1, s2, s3, d0);
                v_store_low(dptr, d0);

                i += 2*VecSrcType::nlanes;
            }
        }
        return i;
    }
};

template<class Op, class VecOp> struct MatConvert : public MatUtils::MatOp1
{
    typedef typename Op::stype1 stype1;
    typedef typename Op::stype2 stype2;

    void operator()(const ImGui::ImMat& _src, ImGui::ImMat& _dst) override
    {
        int width = _src.w;
        const int height = _src.h;
        const int cn = _src.c;
        const int srcLineSize = _src.w*_src.c*_src.elemsize;

        if (_dst.w != _src.w || _dst.h != _src.h || _dst.c != _src.c)
        {
            const auto dstType = _dst.type;
            _dst.release();
            _dst.create_type(_src.w, _src.h, _src.c, dstType);
        }
        const int dstLineSize = _dst.w*_dst.c*_dst.elemsize;

        const uint8_t* src = (const uint8_t*)_src.data;
        uint8_t* dst = (uint8_t*)_dst.data;
        width *= cn;
        Op op;
        int i, j;
        for (j = 0; j < height; j++)
        {
            i = vecOp(src, dst, width);

            stype2* D = (stype2*)dst;
            for (; i <= width-4; i += 4)
            {
                const stype1* sptr = (const stype1*)src+i;
                stype1 s0 = sptr[0], s1 = sptr[1], s2 = sptr[2], s3 = sptr[3];
                D[i  ] = op(s0); D[i+1] = op(s1);
                D[i+2] = op(s2); D[i+3] = op(s3);
            }
            for (; i < width; i++)
            {
                stype1 s0 = *((const stype1*)src+i);
                D[i] = op(s0);
            }

            src += srcLineSize;
            dst += dstLineSize;
        }
    }

    VecOp vecOp;
};

template<class vtype> struct RowVecCopyVal
{
    typedef typename vtype::lane_type stype;

    int operator()(const uint8_t* _src, uint8_t* _dst, int width) const
    {
        const stype* src = (const stype*)_src;
        stype* dst = (stype*)_dst;
        int i;
        for (i = 0; i <= width-4*vtype::nlanes; i += 4*vtype::nlanes)
        {
            const stype* sptr = src+i;
            vtype s0 = vx_load(sptr);
            vtype s1 = vx_load(sptr+  vtype::nlanes);
            vtype s2 = vx_load(sptr+2*vtype::nlanes);
            vtype s3 = vx_load(sptr+3*vtype::nlanes);
            v_store(dst+i, s0);
            v_store(dst+i+  vtype::nlanes, s1);
            v_store(dst+i+2*vtype::nlanes, s2);
            v_store(dst+i+3*vtype::nlanes, s3);
        }
        if (i <= width-2*vtype::nlanes)
        {
            const stype* sptr = src+i;
            vtype s0 = vx_load(sptr);
            vtype s1 = vx_load(sptr+vtype::nlanes);
            v_store(dst+i, s0);
            v_store(dst+i+vtype::nlanes, s1);
            i += 2*vtype::nlanes;
        }
        if (i <= width-vtype::nlanes)
        {
            vtype s0 = vx_load(src+i);
            v_store(dst+i, s0);
            i += vtype::nlanes;
        }
        if (i <= width-vtype::nlanes/2)
        {
            vtype s0 = vx_load_low(src+i);
            v_store_low(dst+i, s0);
            i += vtype::nlanes/2;
        }
        return i;
    }
};

template<class stype, class VecOp> struct MatCopy : public MatUtils::MatOp1
{
    void operator()(const ImGui::ImMat& _src, ImGui::ImMat& _dst) override
    {
        int width = _src.w;
        const int height = _src.h;
        const int cn = _src.c;
        const int srcLineSize = _src.w*_src.c*_src.elemsize;

        if (_dst.w != _src.w || _dst.h != _src.h || _dst.c != _src.c || _dst.type != _src.type)
        {
            _dst.release();
            _dst.create_type(_src.w, _src.h, _src.c, _src.type);
        }
        const int dstLineSize = _dst.w*_dst.c*_dst.elemsize;

        const uint8_t* src = (const uint8_t*)_src.data;
        uint8_t* dst = (uint8_t*)_dst.data;
        width *= cn;
        int i, j;
        for (j = 0; j < height; j++)
        {
            i = vecOp(src, dst, width);

            stype* D = (stype*)dst;
            for (; i <= width-4; i += 4)
            {
                const stype* sptr = (const stype*)src+i;
                stype s0 = sptr[0], s1 = sptr[1], s2 = sptr[2], s3 = sptr[3];
                D[i  ] = s0; D[i+1] = s1;
                D[i+2] = s2; D[i+3] = s3;
            }
            for (; i < width; i++)
            {
                stype s0 = *((const stype*)src+i);
                D[i] = s0;
            }

            src += srcLineSize;
            dst += dstLineSize;
        }
    }

    VecOp vecOp;
};

template<typename GrayVtype, typename RgbaVtype> struct RowVecGrayToRgbaPack
{
    typedef VColDepPack<GrayVtype, RgbaVtype> VecPack;
    typedef GrayVtype VecSrcType;
    typedef RgbaVtype VecDstType;
    typedef typename VecSrcType::lane_type SrcType;
    typedef typename VecDstType::lane_type DstType;

    RowVecGrayToRgbaPack(VecDstType _vecAlpha)
    { vecAlpha = _vecAlpha; }

    int operator()(const uint8_t* _src, uint8_t* _dst, int width) const
    {
        const SrcType* sptr = (const SrcType*)_src;
        DstType* dptr = (DstType*)_dst;
        VecPack vecOp;
        VecDstType d0, d1, d2, d3, d4, d5;
        int i;
        if (VecSrcType::nlanes*2 == VecDstType::nlanes)
        {
            VecSrcType s0, s1;
            for (i = 0; i <= width-4*VecSrcType::nlanes; i += 4*VecSrcType::nlanes)
            {
                s0 = vx_load(sptr); sptr += VecSrcType::nlanes;
                s1 = vx_load(sptr); sptr += VecSrcType::nlanes;
                vecOp.pack(s0, s1, d0);
                v_interleave(d0, d0, d1, d2);
                v_interleave(d0, vecAlpha, d3, d4);
                v_interleave(d1, d3, d0, d5);
                v_store(dptr, d0); dptr += VecDstType::nlanes;
                v_store(dptr, d5); dptr += VecDstType::nlanes;
                v_interleave(d2, d4, d0, d5);
                v_store(dptr, d0); dptr += VecDstType::nlanes;
                v_store(dptr, d5); dptr += VecDstType::nlanes;

                s0 = vx_load(sptr); sptr += VecSrcType::nlanes;
                s1 = vx_load(sptr); sptr += VecSrcType::nlanes;
                vecOp.pack(s0, s1, d0);
                v_interleave(d0, d0, d1, d2);
                v_interleave(d0, vecAlpha, d3, d4);
                v_interleave(d1, d3, d0, d5);
                v_store(dptr, d0); dptr += VecDstType::nlanes;
                v_store(dptr, d5); dptr += VecDstType::nlanes;
                v_interleave(d2, d4, d0, d5);
                v_store(dptr, d0); dptr += VecDstType::nlanes;
                v_store(dptr, d5); dptr += VecDstType::nlanes;
            }
            if (i <= width-2*VecSrcType::nlanes)
            {
                s0 = vx_load(sptr); sptr += VecSrcType::nlanes;
                s1 = vx_load(sptr); sptr += VecSrcType::nlanes;
                vecOp.pack(s0, s1, d0);
                v_interleave(d0, d0, d1, d2);
                v_interleave(d0, vecAlpha, d3, d4);
                v_interleave(d1, d3, d0, d5);
                v_store(dptr, d0); dptr += VecDstType::nlanes;
                v_store(dptr, d5); dptr += VecDstType::nlanes;
                v_interleave(d2, d4, d0, d5);
                v_store(dptr, d0); dptr += VecDstType::nlanes;
                v_store(dptr, d5); dptr += VecDstType::nlanes;

                i += 2*VecSrcType::nlanes;
            }
            if (i <= width-VecSrcType::nlanes)
            {
                s0 = vx_load(sptr);
                vecOp.pack(s0, s1, d0);
                v_interleave(d0, d0, d1, d2);
                v_interleave(d0, vecAlpha, d3, d4);
                v_interleave(d1, d3, d0, d5);
                v_store(dptr, d0); dptr += VecDstType::nlanes;
                v_store(dptr, d5); dptr += VecDstType::nlanes;

                i += VecSrcType::nlanes;
            }
        }
        else if (VecSrcType::nlanes*4 == VecDstType::nlanes)
        {
            VecSrcType s0, s1, s2, s3;
            for (i = 0; i <= width-4*VecSrcType::nlanes; i += 4*VecSrcType::nlanes)
            {
                s0 = vx_load(sptr); sptr += VecSrcType::nlanes;
                s1 = vx_load(sptr); sptr += VecSrcType::nlanes;
                s2 = vx_load(sptr); sptr += VecSrcType::nlanes;
                s3 = vx_load(sptr); sptr += VecSrcType::nlanes;
                vecOp.pack2(s0, s1, s2, s3, d0);
                v_interleave(d0, d0, d1, d2);
                v_interleave(d0, vecAlpha, d3, d4);
                v_interleave(d1, d3, d0, d5);
                v_store(dptr, d0); dptr += VecDstType::nlanes;
                v_store(dptr, d5); dptr += VecDstType::nlanes;
                v_interleave(d2, d4, d0, d5);
                v_store(dptr, d0); dptr += VecDstType::nlanes;
                v_store(dptr, d5); dptr += VecDstType::nlanes;
            }
            if (i <= width-2*VecSrcType::nlanes)
            {
                s0 = vx_load(sptr); sptr += VecSrcType::nlanes;
                s1 = vx_load(sptr);
                vecOp.pack2(s0, s1, s2, s3, d0);
                v_interleave(d0, d0, d1, d2);
                v_interleave(d0, vecAlpha, d3, d4);
                v_interleave(d1, d3, d0, d5);
                v_store(dptr, d0); dptr += VecDstType::nlanes;
                v_store(dptr, d5); dptr += VecDstType::nlanes;

                i += 2*VecSrcType::nlanes;
            }
        }
        return i;
    }

    VecDstType vecAlpha;
};

template<typename GrayVtype> struct RowVecGrayToRgbaCopy
{
    typedef GrayVtype VecSrcType;
    typedef GrayVtype VecDstType;
    typedef typename VecSrcType::lane_type SrcType;
    typedef typename VecDstType::lane_type DstType;

    RowVecGrayToRgbaCopy(VecDstType _vecAlpha)
    { vecAlpha = _vecAlpha; }

    int operator()(const uint8_t* _src, uint8_t* _dst, int width) const
    {
        const SrcType* sptr = (const SrcType*)_src;
        DstType* dptr = (DstType*)_dst;
        VecSrcType s0;
        VecDstType d0, d1, d2, d3, d4, d5;
        int i;
        for (i = 0; i <= width-4*VecSrcType::nlanes; i += 4*VecSrcType::nlanes)
        {
            s0 = vx_load(sptr); sptr += VecSrcType::nlanes;
            v_interleave(s0, s0, d1, d2);
            v_interleave(s0, vecAlpha, d3, d4);
            v_interleave(d1, d3, d0, d5);
            v_store(dptr, d0); dptr += VecDstType::nlanes;
            v_store(dptr, d5); dptr += VecDstType::nlanes;
            v_interleave(d2, d4, d0, d5);
            v_store(dptr, d0); dptr += VecDstType::nlanes;
            v_store(dptr, d5); dptr += VecDstType::nlanes;

            s0 = vx_load(sptr); sptr += VecSrcType::nlanes;
            v_interleave(s0, s0, d1, d2);
            v_interleave(s0, vecAlpha, d3, d4);
            v_interleave(d1, d3, d0, d5);
            v_store(dptr, d0); dptr += VecDstType::nlanes;
            v_store(dptr, d5); dptr += VecDstType::nlanes;
            v_interleave(d2, d4, d0, d5);
            v_store(dptr, d0); dptr += VecDstType::nlanes;
            v_store(dptr, d5); dptr += VecDstType::nlanes;

            s0 = vx_load(sptr); sptr += VecSrcType::nlanes;
            v_interleave(s0, s0, d1, d2);
            v_interleave(s0, vecAlpha, d3, d4);
            v_interleave(d1, d3, d0, d5);
            v_store(dptr, d0); dptr += VecDstType::nlanes;
            v_store(dptr, d5); dptr += VecDstType::nlanes;
            v_interleave(d2, d4, d0, d5);
            v_store(dptr, d0); dptr += VecDstType::nlanes;
            v_store(dptr, d5); dptr += VecDstType::nlanes;

            s0 = vx_load(sptr); sptr += VecSrcType::nlanes;
            v_interleave(s0, s0, d1, d2);
            v_interleave(s0, vecAlpha, d3, d4);
            v_interleave(d1, d3, d0, d5);
            v_store(dptr, d0); dptr += VecDstType::nlanes;
            v_store(dptr, d5); dptr += VecDstType::nlanes;
            v_interleave(d2, d4, d0, d5);
            v_store(dptr, d0); dptr += VecDstType::nlanes;
            v_store(dptr, d5); dptr += VecDstType::nlanes;
        }
        if (i <= width-2*VecSrcType::nlanes)
        {
            s0 = vx_load(sptr); sptr += VecSrcType::nlanes;
            v_interleave(s0, s0, d1, d2);
            v_interleave(s0, vecAlpha, d3, d4);
            v_interleave(d1, d3, d0, d5);
            v_store(dptr, d0); dptr += VecDstType::nlanes;
            v_store(dptr, d5); dptr += VecDstType::nlanes;
            v_interleave(d2, d4, d0, d5);
            v_store(dptr, d0); dptr += VecDstType::nlanes;
            v_store(dptr, d5); dptr += VecDstType::nlanes;

            s0 = vx_load(sptr); sptr += VecSrcType::nlanes;
            v_interleave(s0, s0, d1, d2);
            v_interleave(s0, vecAlpha, d3, d4);
            v_interleave(d1, d3, d0, d5);
            v_store(dptr, d0); dptr += VecDstType::nlanes;
            v_store(dptr, d5); dptr += VecDstType::nlanes;
            v_interleave(d2, d4, d0, d5);
            v_store(dptr, d0); dptr += VecDstType::nlanes;
            v_store(dptr, d5); dptr += VecDstType::nlanes;

            i += 2*VecSrcType::nlanes;
        }
        if (i <= width-VecSrcType::nlanes)
        {
            s0 = vx_load(sptr);
            v_interleave(s0, s0, d1, d2);
            v_interleave(s0, vecAlpha, d3, d4);
            v_interleave(d1, d3, d0, d5);
            v_store(dptr, d0); dptr += VecDstType::nlanes;
            v_store(dptr, d5); dptr += VecDstType::nlanes;
            v_interleave(d2, d4, d0, d5);
            v_store(dptr, d0); dptr += VecDstType::nlanes;
            v_store(dptr, d5);

            i += VecSrcType::nlanes;
        }
        return i;
    }

    VecDstType vecAlpha;
};

template<class Op, class VecOp> struct MatGrayToRgba : public MatUtils::MatOp1
{
    typedef typename Op::stype1 stype1;
    typedef typename Op::stype2 stype2;
    typedef typename VecOp::VecDstType vtype2;

    MatGrayToRgba(const vtype2& _vecAlpha) : vecOp(_vecAlpha) {
        alphaVal = _vecAlpha.get0();
    }

    void operator()(const ImGui::ImMat& _src, ImGui::ImMat& _dst) override
    {
        int width = _src.w;
        const int height = _src.h;
        const int cn = _src.c;
        const int srcLineSize = _src.w*_src.c*_src.elemsize;

        if (_dst.empty() || _dst.w != _src.w || _dst.h != _src.h || _dst.c != 4)
        {
            const auto dstType = _dst.type;
            _dst.release();
            _dst.create_type(_src.w, _src.h, 4, dstType);
        }
        const int dstLineSize = _dst.w*_dst.c*_dst.elemsize;

        const uint8_t* src = (const uint8_t*)_src.data;
        uint8_t* dst = (uint8_t*)_dst.data;
        width *= cn;
        Op op;
        int i, j;
        for (j = 0; j < height; j++)
        {
            i = vecOp(src, dst, width);

            stype2* dptr = (stype2*)dst+i*4;
            for (; i <= width-4; i += 4)
            {
                const stype1* sptr = (const stype1*)src+i;
                stype1 s0 = sptr[0], s1 = sptr[1], s2 = sptr[2], s3 = sptr[3];
                *dptr++ = op(s0); *dptr++ = op(s0); *dptr++ = op(s0); *dptr++ = alphaVal;
                *dptr++ = op(s1); *dptr++ = op(s1); *dptr++ = op(s1); *dptr++ = alphaVal;
                *dptr++ = op(s2); *dptr++ = op(s2); *dptr++ = op(s2); *dptr++ = alphaVal;
                *dptr++ = op(s3); *dptr++ = op(s3); *dptr++ = op(s3); *dptr++ = alphaVal;
            }
            for (; i < width; i++)
            {
                stype1 s0 = *((const stype1*)src+i);
                *dptr++ = op(s0); *dptr++ = op(s0); *dptr++ = op(s0); *dptr++ = alphaVal;
            }

            src += srcLineSize;
            dst += dstLineSize;
        }
    }

    VecOp vecOp;
    stype2 alphaVal;
};

SIMD_SCOPE_END
}