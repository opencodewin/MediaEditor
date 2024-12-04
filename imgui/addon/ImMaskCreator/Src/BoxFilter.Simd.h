namespace SimdOpt
{
SIMD_SCOPE_BEGIN(INTRIN_MODE)

template<typename SrcType, typename SumType>
struct RowSum : public MatUtils::RowFilter
{
    RowSum(int _ksize, int _anchor)
    {
        ksize = _ksize;
        anchor = _anchor;
    }

    void operator()(const uint8_t* src, uint8_t* dst, int width, int cn) override
    {
        const SrcType* S = (const SrcType*)src;
        SumType* D = (SumType*)dst;
        int i = 0, k, ksz_cn = ksize*cn;

        width = (width-1)*cn;
        if (ksize == 3)
        {
            for (i = 0; i < width+cn; i++)
            {
                D[i] = (SumType)S[i]+(SumType)S[i+cn]+(SumType)S[i+cn*2];
            }
        }
        else if (ksize == 5)
        {
            for (i = 0; i < width+cn; i++)
            {
                D[i] = (SumType)S[i]+(SumType)S[i+cn]+(SumType)S[i+cn*2]+(SumType)S[i+cn*3]+(SumType)S[i+cn*4];
            }
        }
        else if (cn == 1)
        {
            SumType s = 0;
            for (i = 0; i < ksz_cn; i++)
                s += (SumType)S[i];
            D[0] = s;
            for (i = 0; i < width; i++)
            {
                s += (SumType)S[i+ksz_cn]-(SumType)S[i];
                D[i+1] = s;
            }
        }
        else if (cn == 3)
        {
            SumType s0 = 0, s1 = 0, s2 = 0;
            for (i = 0; i < ksz_cn; i += 3)
            {
                s0 += (SumType)S[i];
                s1 += (SumType)S[i+1];
                s2 += (SumType)S[i+2];
            }
            D[0] = s0;
            D[1] = s1;
            D[2] = s2;
            for (i = 0; i < width; i += 3)
            {
                s0 += (SumType)S[i+ksz_cn  ]-(SumType)S[i  ];
                s1 += (SumType)S[i+ksz_cn+1]-(SumType)S[i+1];
                s2 += (SumType)S[i+ksz_cn+2]-(SumType)S[i+2];
                D[i+3] = s0;
                D[i+4] = s1;
                D[i+5] = s2;
            }
        }
        else if (cn == 4)
        {
            SumType s0 = 0, s1 = 0, s2 = 0, s3 = 0;
            for (i = 0; i < ksz_cn; i += 4)
            {
                s0 += (SumType)S[i  ];
                s1 += (SumType)S[i+1];
                s2 += (SumType)S[i+2];
                s3 += (SumType)S[i+3];
            }
            D[0] = s0;
            D[1] = s1;
            D[2] = s2;
            D[3] = s3;
            for (i = 0; i < width; i += 4)
            {
                s0 += (SumType)S[i+ksz_cn  ]-(SumType)S[i  ];
                s1 += (SumType)S[i+ksz_cn+1]-(SumType)S[i+1];
                s2 += (SumType)S[i+ksz_cn+2]-(SumType)S[i+2];
                s3 += (SumType)S[i+ksz_cn+3]-(SumType)S[i+3];
                D[i+4] = s0;
                D[i+5] = s1;
                D[i+6] = s2;
                D[i+7] = s3;
            }
        }
        else
        {
            for (k = 0; k < cn; k++, S++, D++)
            {
                SumType s = 0;
                for (i = 0; i < ksz_cn; i += cn)
                    s += (SumType)S[i];
                D[0] = s;
                for (i = 0; i < width; i += cn)
                {
                    s += (SumType)S[i+ksz_cn]-(SumType)S[i];
                    D[i+cn] = s;
                }
            }
        }
    }
};

template<typename SumType, typename DstType>
struct ColumnSum : public MatUtils::ColumnFilter
{
    ColumnSum(int _ksize, int _anchor, double _scale)
    {
        ksize = _ksize;
        anchor = _anchor;
        scale = _scale;
        sumCount = 0;
    }

    // void reset() override { sumCount = 0; }

    void operator()(const uint8_t** src, uint8_t* dst, int dststep, int count, int width) override
    {
        int i;
        SumType* SUM;
        bool haveScale = scale != 1;
        double _scale = scale;

        if (width != (int)sum.size())
        {
            sum.resize(width);
            sumCount = 0;
        }

        SUM = &sum[0];
        if (sumCount == 0)
        {
            memset((void*)SUM, 0, width*sizeof(SumType));
            for (; sumCount < ksize-1; sumCount++, src++)
            {
                const SumType* Sp = (const SumType*)src[0];
                for (i = 0; i < width; i++)
                    SUM[i] += Sp[i];
            }
        }
        else
        {
            assert(sumCount == ksize-1);
            src += ksize-1;
        }

        for (; count--; src++)
        {
            const SumType* Sp = (const SumType*)src[0];
            const SumType* Sm = (const SumType*)src[1-ksize];
            DstType* D = (DstType*)dst;
            if (haveScale)
            {
                for (i = 0; i <= width-2; i += 2)
                {
                    SumType s0 = SUM[i]+Sp[i], s1 = SUM[i+1]+Sp[i+1];
                    D[i  ] = MathUtils::SaturateCast<DstType>(s0*_scale);
                    D[i+1] = MathUtils::SaturateCast<DstType>(s1*_scale);
                    s0 -= Sm[i]; s1 -= Sm[i+1];
                    SUM[i] = s0; SUM[i+1] = s1;
                }
                for (; i < width; i++)
                {
                    SumType s0 = SUM[i]+Sp[i];
                    D[i] = MathUtils::SaturateCast<DstType>(s0*_scale);
                    SUM[i] = s0-Sm[i];
                }
            }
            else
            {
                for (i = 0; i <= width-2; i += 2)
                {
                    SumType s0 = SUM[i]+Sp[i], s1 = SUM[i+1]+Sp[i+1];
                    D[i  ] = MathUtils::SaturateCast<DstType>(s0);
                    D[i+1] = MathUtils::SaturateCast<DstType>(s1);
                    s0 -= Sm[i]; s1 -= Sm[i+1];
                    SUM[i] = s0; SUM[i+1] = s1;
                }
                for (; i < width; i++)
                {
                    SumType s0 = SUM[i]+Sp[i];
                    D[i] = MathUtils::SaturateCast<DstType>(s0);
                    SUM[i] = s0-Sm[i];
                }
            }
            dst += dststep;
        }
    }

    double scale;
    int sumCount;
    std::vector<SumType> sum;
};

template<> struct ColumnSum<int32_t, uint8_t> : public MatUtils::ColumnFilter
{
    ColumnSum(int _ksize, int _anchor, double _scale)
    {
        ksize = _ksize;
        anchor = _anchor;
        scale = _scale;
        sumCount = 0;
    }

    // void reset() override { sumCount = 0; }

    void operator()(const uint8_t** src, uint8_t* dst, int dststep, int count, int width) override
    {
        int32_t* SUM;
        bool haveScale = scale != 1;
        double _scale = scale;

        if (width != (int)sum.size())
        {
            sum.resize(width);
            sumCount = 0;
        }

        SUM = &sum[0];
        if (sumCount == 0)
        {
            memset((void*)SUM, 0, width*sizeof(int32_t));
            for (; sumCount < ksize-1; sumCount++, src++)
            {
                const int32_t* Sp = (const int32_t*)src[0];
                int i = 0;
                for (; i <= width-v_int32::nlanes; i += v_int32::nlanes)
                {
                    v_store(SUM+i, vx_load(SUM+i)+vx_load(Sp+i));
                }
                for (; i < width; i++)
                    SUM[i] += Sp[i];
            }
        }
        else
        {
            assert(sumCount == ksize-1);
            src += ksize-1;
        }

        for (; count--; src++)
        {
            const int32_t* Sp = (const int32_t*)src[0];
            const int32_t* Sm = (const int32_t*)src[1-ksize];
            uint8_t* D = (uint8_t*)dst;
            if (haveScale)
            {
                int i = 0;
                v_float32 _v_scale = vx_setall_f32((float)_scale);
                for (; i <= width-v_uint16::nlanes; i += v_uint16::nlanes)
                {
                    v_int32 v_s0 = vx_load(SUM+i)+vx_load(Sp+i);
                    v_int32 v_s01 = vx_load(SUM+i+v_int32::nlanes)+vx_load(Sp+i+v_int32::nlanes);

                    v_uint32 v_s0d = v_reinterpret_as_u32(v_round(v_cvt_f32(v_s0)*_v_scale));
                    v_uint32 v_s01d = v_reinterpret_as_u32(v_round(v_cvt_f32(v_s01)*_v_scale));

                    v_uint16 v_dst = v_pack(v_s0d, v_s01d);
                    v_pack_store(D+i, v_dst);

                    v_store(SUM+i, v_s0-vx_load(Sm+i));
                    v_store(SUM+i+v_int32::nlanes, v_s01-vx_load(Sm+i+v_int32::nlanes));
                }
                for (; i < width; i++)
                {
                    int s0 = SUM[i]+Sp[i];
                    D[i] = MathUtils::SaturateCast<uint8_t>(s0*_scale);
                    SUM[i] = s0 - Sm[i];
                }
            }
            else
            {
                int i = 0;
                for (; i <= width-v_uint16::nlanes; i += v_uint16::nlanes)
                {
                    v_int32 v_s0 = vx_load(SUM+i)+vx_load(Sp+i);
                    v_int32 v_s01 = vx_load(SUM+i+v_int32::nlanes)+vx_load(Sp+i+v_int32::nlanes);

                    v_uint16 v_dst = v_pack(v_reinterpret_as_u32(v_s0), v_reinterpret_as_u32(v_s01));
                    v_pack_store(D+i, v_dst);

                    v_store(SUM+i, v_s0-vx_load(Sm+i));
                    v_store(SUM+i+v_int32::nlanes, v_s01-vx_load(Sm+i+v_int32::nlanes));
                }
                for (; i < width; i++)
                {
                    int s0 = SUM[i]+Sp[i];
                    D[i] = MathUtils::SaturateCast<uint8_t>(s0);
                    SUM[i] = s0-Sm[i];
                }
            }
            dst += dststep;
        }
    }

    double scale;
    int sumCount;
    std::vector<int32_t> sum;
};

template<> struct ColumnSum<uint16_t, uint8_t> : public MatUtils::ColumnFilter
{
    enum { SHIFT = 23 };

    ColumnSum(int _ksize, int _anchor, double _scale)
    {
        ksize = _ksize;
        anchor = _anchor;
        scale = _scale;
        sumCount = 0;
        divDelta = 0;
        divScale = 1;
        if (scale != 1)
        {
            int d = std::round(1./scale);
            double scalef = ((double)(1 << SHIFT))/d;
            divScale = std::floor(scalef);
            scalef -= divScale;
            divDelta = d/2;
            if (scalef < 0.5)
                divDelta++;
            else
                divScale++;
        }
    }

    void operator()(const uint8_t** src, uint8_t* dst, int dststep, int count, int width) override
    {
        const int ds = divScale;
        const int dd = divDelta;
        uint16_t* SUM;
        const bool haveScale = scale != 1;

        if (width != (int)sum.size())
        {
            sum.resize(width);
            sumCount = 0;
        }

        SUM = &sum[0];
        if (sumCount == 0)
        {
            memset((void*)SUM, 0, width*sizeof(SUM[0]));
            for (; sumCount < ksize-1; sumCount++, src++)
            {
                const uint16_t* Sp = (const uint16_t*)src[0];
                int i = 0;
                for (; i <= width-v_uint16::nlanes; i += v_uint16::nlanes)
                {
                    v_store(SUM+i, vx_load(SUM+i)+vx_load(Sp+i));
                }
                for (; i < width; i++)
                    SUM[i] += Sp[i];
            }
        }
        else
        {
            assert(sumCount == ksize-1);
            src += ksize-1;
        }

        for (; count--; src++)
        {
            const uint16_t* Sp = (const uint16_t*)src[0];
            const uint16_t* Sm = (const uint16_t*)src[1-ksize];
            uint8_t* D = (uint8_t*)dst;
            if (haveScale)
            {
                int i = 0;
                v_uint32 _ds4 = vx_setall_u32((unsigned)ds);
                v_uint16 _dd8 = vx_setall_u16((uint16_t)dd);

                for (; i <= width-v_uint8::nlanes; i+=v_uint8::nlanes)
                {
                    v_uint16 _sm0 = vx_load(Sm + i);
                    v_uint16 _sm1 = vx_load(Sm + i + v_uint16::nlanes);

                    v_uint16 _s0 = v_add_wrap(vx_load(SUM + i), vx_load(Sp + i));
                    v_uint16 _s1 = v_add_wrap(vx_load(SUM + i + v_uint16::nlanes), vx_load(Sp + i + v_uint16::nlanes));

                    v_uint32 _s00, _s01, _s10, _s11;

                    v_expand(_s0 + _dd8, _s00, _s01);
                    v_expand(_s1 + _dd8, _s10, _s11);

                    _s00 = v_shr<SHIFT>(_s00*_ds4);
                    _s01 = v_shr<SHIFT>(_s01*_ds4);
                    _s10 = v_shr<SHIFT>(_s10*_ds4);
                    _s11 = v_shr<SHIFT>(_s11*_ds4);

                    v_int16 r0 = v_pack(v_reinterpret_as_s32(_s00), v_reinterpret_as_s32(_s01));
                    v_int16 r1 = v_pack(v_reinterpret_as_s32(_s10), v_reinterpret_as_s32(_s11));

                    _s0 = v_sub_wrap(_s0, _sm0);
                    _s1 = v_sub_wrap(_s1, _sm1);

                    v_store(D + i, v_pack_u(r0, r1));
                    v_store(SUM + i, _s0);
                    v_store(SUM + i + v_uint16::nlanes, _s1);
                }
                for (; i < width; i++)
                {
                    int s0 = SUM[i] + Sp[i];
                    D[i] = (uint8_t)((s0 + dd)*ds >> SHIFT);
                    SUM[i] = (uint16_t)(s0 - Sm[i]);
                }
            }
            else
            {
                int i = 0;
                for( ; i < width; i++ )
                {
                    int s0 = SUM[i] + Sp[i];
                    D[i] = MathUtils::SaturateCast<uint8_t>(s0);
                    SUM[i] = (uint16_t)(s0 - Sm[i]);
                }
            }
            dst += dststep;
        }
    }

    double scale;
    int sumCount;
    int divDelta;
    int divScale;
    std::vector<uint16_t> sum;
};

template<> struct ColumnSum<int32_t, uint16_t> : public MatUtils::ColumnFilter
{
    ColumnSum(int _ksize, int _anchor, double _scale)
    {
        ksize = _ksize;
        anchor = _anchor;
        scale = _scale;
        sumCount = 0;
    }

    void operator()(const uint8_t** src, uint8_t* dst, int dststep, int count, int width) override
    {
        int i;
        int32_t* SUM;
        bool haveScale = scale != 1;
        double _scale = scale;

        if (width != (int)sum.size())
        {
            sum.resize(width);
            sumCount = 0;
        }

        SUM = &sum[0];
        if (sumCount == 0)
        {
            memset((void*)SUM, 0, width*sizeof(int32_t));
            for (; sumCount < ksize-1; sumCount++, src++)
            {
                const int32_t* Sp = (const int32_t*)src[0];
                i = 0;
                for (; i <= width-v_int32::nlanes; i += v_int32::nlanes)
                {
                    v_store(SUM + i, vx_load(SUM + i) + vx_load(Sp + i));
                }
                for (; i < width; i++)
                    SUM[i] += Sp[i];
            }
        }
        else
        {
            assert(sumCount == ksize-1);
            src += ksize-1;
        }

        for (; count--; src++)
        {
            const int32_t* Sp = (const int32_t*)src[0];
            const int32_t* Sm = (const int32_t*)src[1-ksize];
            uint16_t* D = (uint16_t*)dst;
            if (haveScale)
            {
                i = 0;
                v_float32 _v_scale = vx_setall_f32((float)_scale);
                for (; i <= width-v_int16::nlanes; i += v_int16::nlanes)
                {
                    v_int32 v_s0 = vx_load(SUM + i) + vx_load(Sp + i);
                    v_int32 v_s01 = vx_load(SUM + i + v_int32::nlanes) + vx_load(Sp + i + v_int32::nlanes);

                    v_uint32 v_s0d =  v_reinterpret_as_u32(v_round(v_cvt_f32(v_s0) * _v_scale));
                    v_uint32 v_s01d = v_reinterpret_as_u32(v_round(v_cvt_f32(v_s01) * _v_scale));
                    v_store(D + i, v_pack(v_s0d, v_s01d));

                    v_store(SUM + i, v_s0 - vx_load(Sm + i));
                    v_store(SUM + i + v_int32::nlanes, v_s01 - vx_load(Sm + i + v_int32::nlanes));
                }
                for (; i < width; i++)
                {
                    int s0 = SUM[i] + Sp[i];
                    D[i] = MathUtils::SaturateCast<uint16_t>(s0*_scale);
                    SUM[i] = s0 - Sm[i];
                }
            }
            else
            {
                i = 0;
                for (; i <= width-v_int16::nlanes; i += v_int16::nlanes)
                {
                    v_int32 v_s0 = vx_load(SUM + i) + vx_load(Sp + i);
                    v_int32 v_s01 = vx_load(SUM + i + v_int32::nlanes) + vx_load(Sp + i + v_int32::nlanes);

                    v_store(D + i, v_pack(v_reinterpret_as_u32(v_s0), v_reinterpret_as_u32(v_s01)));

                    v_store(SUM + i, v_s0 - vx_load(Sm + i));
                    v_store(SUM + i + v_int32::nlanes, v_s01 - vx_load(Sm + i + v_int32::nlanes));
                }
                for (; i < width; i++)
                {
                    int s0 = SUM[i] + Sp[i];
                    D[i] = MathUtils::SaturateCast<uint16_t>(s0);
                    SUM[i] = s0 - Sm[i];
                }
            }
            dst += dststep;
        }
    }

    double scale;
    int sumCount;
    std::vector<int32_t> sum;
};

template<> struct ColumnSum<int32_t, int32_t> : public MatUtils::ColumnFilter
{
    ColumnSum(int _ksize, int _anchor, double _scale)
    {
        ksize = _ksize;
        anchor = _anchor;
        scale = _scale;
        sumCount = 0;
    }

    void operator()(const uint8_t** src, uint8_t* dst, int dststep, int count, int width) override
    {
        int32_t* SUM;
        bool haveScale = scale != 1;
        double _scale = scale;

        if( width != (int)sum.size() )
        {
            sum.resize(width);
            sumCount = 0;
        }

        SUM = &sum[0];
        if (sumCount == 0)
        {
            memset((void*)SUM, 0, width*sizeof(int32_t));
            for (; sumCount < ksize-1; sumCount++, src++)
            {
                const int32_t* Sp = (const int32_t*)src[0];
                int i = 0;
                for (; i <= width-v_int32::nlanes; i += v_int32::nlanes)
                {
                    v_store(SUM + i, vx_load(SUM + i) + vx_load(Sp + i));
                }
                for (; i < width; i++)
                    SUM[i] += Sp[i];
            }
        }
        else
        {
            assert(sumCount == ksize-1);
            src += ksize-1;
        }

        for (; count--; src++)
        {
            const int32_t* Sp = (const int32_t*)src[0];
            const int32_t* Sm = (const int32_t*)src[1-ksize];
            int32_t* D = (int32_t*)dst;
            if (haveScale)
            {
                int i = 0;
                v_float32 _v_scale = vx_setall_f32((float)_scale);
                for (; i <= width-v_int32::nlanes; i += v_int32::nlanes)
                {
                    v_int32 v_s0 = vx_load(SUM + i) + vx_load(Sp + i);
                    v_int32 v_s0d = v_round(v_cvt_f32(v_s0) * _v_scale);

                    v_store(D + i, v_s0d);
                    v_store(SUM + i, v_s0 - vx_load(Sm + i));
                }
                for (; i < width; i++)
                {
                    int s0 = SUM[i] + Sp[i];
                    D[i] = MathUtils::SaturateCast<int32_t>(s0*_scale);
                    SUM[i] = s0 - Sm[i];
                }
            }
            else
            {
                int i = 0;
                for (; i <= width-v_int32::nlanes; i += v_int32::nlanes)
                {
                    v_int32 v_s0 = vx_load(SUM + i) + vx_load(Sp + i);

                    v_store(D + i, v_s0);
                    v_store(SUM + i, v_s0 - vx_load(Sm + i));
                }
                for (; i < width; i++)
                {
                    int s0 = SUM[i] + Sp[i];
                    D[i] = s0;
                    SUM[i] = s0 - Sm[i];
                }
            }
            dst += dststep;
        }
    }

    double scale;
    int sumCount;
    std::vector<int32_t> sum;
};

template<> struct ColumnSum<double, float> : public MatUtils::ColumnFilter
{
    ColumnSum(int _ksize, int _anchor, double _scale)
    {
        ksize = _ksize;
        anchor = _anchor;
        scale = _scale;
        sumCount = 0;
    }

    void operator()(const uint8_t** src, uint8_t* dst, int dststep, int count, int width) override
    {
        double* SUM;
        bool haveScale = scale != 1;
        double _scale = scale;

        if( width != (int)sum.size() )
        {
            sum.resize(width);
            sumCount = 0;
        }

        SUM = &sum[0];
        if (sumCount == 0)
        {
            memset((void*)SUM, 0, width*sizeof(double));
            for (; sumCount < ksize-1; sumCount++, src++)
            {
                const double* Sp = (const double*)src[0];
                int i = 0;
                for (; i <= width-v_float64::nlanes; i += v_float64::nlanes)
                {
                    v_store(SUM + i, vx_load(SUM + i) + vx_load(Sp + i));
                }
                for (; i < width; i++)
                    SUM[i] += Sp[i];
            }
        }
        else
        {
            assert(sumCount == ksize-1);
            src += ksize-1;
        }

        for (; count--; src++)
        {
            const double* Sp = (const double*)src[0];
            const double* Sm = (const double*)src[1-ksize];
            float* D = (float*)dst;
            if (haveScale)
            {
                int i = 0;
                v_float64 _v_scale = vx_setall_f64(_scale);
                for (; i <= width-v_float32::nlanes; i += v_float32::nlanes)
                {
                    v_float64 v_s0 = vx_load(SUM + i) + vx_load(Sp + i);
                    v_float64 v_s01 = vx_load(SUM + i + v_float64::nlanes) + vx_load(Sp + i + v_float64::nlanes);

                    v_float64 v_s0d = v_s0 * _v_scale;
                    v_float64 v_s01d = v_s01 * _v_scale;
                    v_store(D + i, v_cvt_f32(v_s0d, v_s01d));

                    v_store(SUM + i, v_s0 - vx_load(Sm + i));
                    v_store(SUM + i + v_float64::nlanes, v_s01 - vx_load(Sm + i + v_float64::nlanes));
                }
                for (; i < width; i++)
                {
                    int s0 = SUM[i] + Sp[i];
                    D[i] = MathUtils::SaturateCast<int32_t>(s0*_scale);
                    SUM[i] = s0 - Sm[i];
                }
            }
            else
            {
                int i = 0;
                for (; i <= width-v_float32::nlanes; i += v_float32::nlanes)
                {
                    v_float64 v_s0 = vx_load(SUM + i) + vx_load(Sp + i);
                    v_float64 v_s01 = vx_load(SUM + i + v_float64::nlanes) + vx_load(Sp + i + v_float64::nlanes);

                    v_store(D + i, v_cvt_f32(v_s0, v_s01));

                    v_store(SUM + i, v_s0 - vx_load(Sm + i));
                    v_store(SUM + i + v_float64::nlanes, v_s01 - vx_load(Sm + i + v_float64::nlanes));
                }
                for (; i < width; i++)
                {
                    int s0 = SUM[i] + Sp[i];
                    D[i] = s0;
                    SUM[i] = s0 - Sm[i];
                }
            }
            dst += dststep;
        }
    }

    double scale;
    int sumCount;
    std::vector<double> sum;
};

SIMD_SCOPE_END
}