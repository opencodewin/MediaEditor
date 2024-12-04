#include "Lut3D_vulkan.h"
#include "Lut3D_shader.h"
#include "ImVulkanShader.h"
#include <algorithm>

#define MAX_LEVEL 256
#define MAX_LINE_SIZE 512
#define NEXT_LINE(loop_cond) do {                           \
    if (!fgets(line, sizeof(line), f)) {                    \
        fprintf(stderr, "Unexpected EOF\n");                \
        return -1;                                          \
    }                                                       \
} while (loop_cond)

static inline float clipf_c(float a, float amin, float amax)
{
    if      (a < amin) return amin;
    else if (a > amax) return amax;
    else               return a;
}

static inline int _isspace(int c) 
{
    return (c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' ||
            c == '\v');
}

static inline int skip_line(const char *p)
{
    while (*p && _isspace(*p))
        p++;
    return !*p || *p == '#';
}

static inline int size_mult(size_t a, size_t b, size_t *r)
{
    size_t t = a * b;
    /* Hack inspired from glibc: don't try the division if nelem and elsize
     * are both less than sqrt(SIZE_MAX). */
    if ((a | b) >= ((size_t)1 << (sizeof(size_t) * 4)) && a && t / a != b)
        return -1;
    *r = t;
    return 0;
}

static inline void * malloc_array(size_t nmemb, size_t size)
{
    size_t result;
    if (size_mult(nmemb, size, &result) < 0)
        return NULL;
    return malloc(result);
}

static void sanitize_name(char* name)
{
    for (std::size_t i = 0; i < strlen(name); i++)
    {
        if (!isalnum(name[i]))
        {
            name[i] = '_';
        }
    }
}

static std::string path_to_varname(const char* path, bool sanitize = true)
{
    const char* lastslash = strrchr(path, '/');
    const char* name = lastslash == NULL ? path : lastslash + 1;

    std::string varname = name;
    if (sanitize)
        sanitize_name((char*)varname.c_str());

    std::transform(varname.begin(), varname.end(), varname.begin(), ::toupper);
    return varname;
}

static std::string path_to_name(const char* path, int up_low = 0)
{
    const char* lastslash = strrchr(path, '/');
    const char* name = lastslash == NULL ? path : lastslash + 1;

    std::string varname = name;
    std::string prefix = varname.substr(0, varname.rfind("."));

    if (up_low == 1)
        std::transform(prefix.begin(), prefix.end(), prefix.begin(), ::toupper);
    else if (up_low == 2)
        std::transform(prefix.begin(), prefix.end(), prefix.begin(), ::tolower);
    return prefix;
}

namespace ImGui 
{
LUT3D_vulkan::LUT3D_vulkan(void * table, int size, float r_scale, float g_scale, float b_scale, float a_scale, int interpolation, int gpu)
{
    if (!table || size <= 0)
    {
        return;
    }
    if (init(interpolation, gpu) != 0)
    {
        return;
    }

    lutsize = size;
    lut = table;
    scale.r = r_scale;
    scale.g = g_scale;
    scale.b = b_scale;
    scale.a = a_scale;

    ImMat lut_cpu;
    lut_cpu.create_type(lutsize, lutsize * 4, lutsize, (void *)lut, IM_DT_FLOAT32);
    VkTransfer tran(vkdev);
    tran.record_upload(lut_cpu, lut_gpu, opt, false);
    tran.submit_and_wait();
    from_file = false;
}

LUT3D_vulkan::LUT3D_vulkan(std::string lut_path, int interpolation, int gpu)
{
    int ret = 0;
    ret = parse_cube(lut_path);
    if (ret != 0 || lutsize == 0)
    {
        if (lut)
        {
            free(lut);
            lut = nullptr;
        }
        return;
    }
    if (init(interpolation, gpu) != 0)
    {
        return;
    }

    ImMat lut_cpu;
    lut_cpu.create_type(lutsize, lutsize * 4, lutsize, (void *)lut, IM_DT_FLOAT32);
    VkTransfer tran(vkdev);
    tran.record_upload(lut_cpu, lut_gpu, opt, false);
    tran.submit_and_wait();
    from_file = true;
}

LUT3D_vulkan::~LUT3D_vulkan()
{
    if (vkdev)
    {
        if (cmd) { delete cmd; cmd = nullptr; }
        if (opt.blob_vkallocator) { vkdev->reclaim_blob_allocator(opt.blob_vkallocator); opt.blob_vkallocator = nullptr; }
        if (opt.staging_vkallocator) { vkdev->reclaim_staging_allocator(opt.staging_vkallocator); opt.staging_vkallocator = nullptr; }
    }
    if (lut && from_file) { free(lut); lut = nullptr; }
}

int LUT3D_vulkan::init(int interpolation, int gpu)
{
    vkdev = get_gpu_device(gpu);
    if (vkdev == NULL) return -1;
    opt.blob_vkallocator = vkdev->acquire_blob_allocator();
    opt.staging_vkallocator = vkdev->acquire_staging_allocator();
#ifdef VULKAN_SHADER_FP16
    opt.use_fp16_arithmetic = true;
    opt.use_fp16_storage = true;
#endif
    cmd = new VkCompute(vkdev, "LUT3D");
    std::vector<vk_specialization_type> specializations(0);
    std::vector<uint32_t> spirv_data;

    if (compile_spirv_module(LUT3D_data, opt, spirv_data) == 0)
    {
        pipeline_lut3d = new Pipeline(vkdev);
        pipeline_lut3d->create(spirv_data.data(), spirv_data.size() * 4, specializations);
    }

    cmd->reset();
    interpolation_mode = interpolation;
    return 0;
}

int LUT3D_vulkan::allocate_3dlut(int size)
{
    int i;
    if (size < 2 || size > MAX_LEVEL) 
    {
        fprintf(stderr, "Too large or invalid 3D LUT size\n");
        return -1;
    }

    if (lut && from_file)
    {
        free(lut);
        lut = nullptr;
    }
    lut = (rgbvec *)malloc_array(size * size * size, sizeof(float) * 4);
    if (!lut)
        return -1;

    lutsize = size;
    return 0;
}

int LUT3D_vulkan::parse_cube(std::string lut_file)
{
    FILE *f = fopen(lut_file.c_str(), "r");
    if (f == NULL) return -1;
    char line[MAX_LINE_SIZE];
    float min[3] = {0.0, 0.0, 0.0};
    float max[3] = {1.0, 1.0, 1.0};

    while (fgets(line, sizeof(line), f)) 
    {
        if (!strncmp(line, "LUT_3D_SIZE", 11)) 
        {
            int ret, i, j, k;
            const int size = strtol(line + 12, NULL, 0);
            const int size2 = size * size;

            ret = allocate_3dlut(size);
            if (ret < 0)
            {
                fclose(f);
                return ret;
            }

            for (k = 0; k < size; k++) 
            {
                for (j = 0; j < size; j++) 
                {
                    for (i = 0; i < size; i++) 
                    {
                        rgbvec *table_lut = (rgbvec *)lut;
                        rgbvec *vec = &table_lut[i * size2 + j * size + k];
                        do 
                        {
try_again:
                            NEXT_LINE(0);
                            if (!strncmp(line, "DOMAIN_", 7)) 
                            {
                                float *vals = NULL;
                                if      (!strncmp(line + 7, "MIN ", 4)) vals = min;
                                else if (!strncmp(line + 7, "MAX ", 4)) vals = max;
                                if (!vals)
                                {
                                    fclose(f);
                                    return -1;
                                }
                                sscanf(line + 11, "%f %f %f", vals, vals + 1, vals + 2);
                                //fprintf(stderr, "min: %f %f %f | max: %f %f %f\n", min[0], min[1], min[2], max[0], max[1], max[2]);
                                goto try_again;
                            } 
                            else if (!strncmp(line, "TITLE", 5)) 
                            {
                                goto try_again;
                            }
                        } while (skip_line(line));
                        if (sscanf(line, "%f %f %f", &vec->r, &vec->g, &vec->b) != 3)
                        {
                            fclose(f);
                            return -1;
                        }
                    }
                }
            }
            break;
        }
    }
    scale.r = clipf_c(1. / (max[0] - min[0]), 0.f, 1.f);
    scale.g = clipf_c(1. / (max[1] - min[1]), 0.f, 1.f);
    scale.b = clipf_c(1. / (max[2] - min[2]), 0.f, 1.f);
    scale.a = 1.f;
    fclose(f);
    // For dump lut file to header files
    //write_header_file("/Users/dicky/Desktop/SDR709_HDR2020_HLG.h");
    return 0;
}

void LUT3D_vulkan::write_header_file(std::string filename)
{
    if (!lut) return;
    rgbvec* _lut = (rgbvec *)lut;
    FILE * fp = fopen(filename.c_str(), "w");
    if (!fp) return;
    std::string include_guard_var = path_to_varname(filename.c_str());
    std::string guard_var_up = path_to_name(filename.c_str(), 1);
    std::string guard_var_low = path_to_name(filename.c_str(), 2);
    fprintf(fp, "#ifndef __LUT_INCLUDE_GUARD_%s__\n", include_guard_var.c_str());
    fprintf(fp, "#define __LUT_INCLUDE_GUARD_%s__\n", include_guard_var.c_str());
    fprintf(fp, "\n");
    fprintf(fp, "#define %s_SIZE %d\n", guard_var_up.c_str(), lutsize);
    fprintf(fp, "\n");
    fprintf(fp, "#define %s_R_SCALE %f\n", guard_var_up.c_str(), scale.r);
    fprintf(fp, "#define %s_G_SCALE %f\n", guard_var_up.c_str(), scale.g);
    fprintf(fp, "#define %s_B_SCALE %f\n", guard_var_up.c_str(), scale.b);
    fprintf(fp, "#define %s_A_SCALE %f\n", guard_var_up.c_str(), scale.a);
    fprintf(fp, "\n");
    int _lutsize = lutsize * lutsize * lutsize;
    fprintf(fp, "DECLARE_ALIGNED(32, const static rgbvec, %s_lut)[%d] = {\n", guard_var_low.c_str(), _lutsize);
    for (int x = 0; x < _lutsize; x++)
    {
        if (x % 4 == 0)
            fprintf(fp, "\n\t");
        if (x < _lutsize - 1)
            fprintf(fp, "{%f, %f, %f, %f}, ", _lut[x].r, _lut[x].g, _lut[x].b, 0.f);
        else
            fprintf(fp, "{%f, %f, %f, %f}", _lut[x].r, _lut[x].g, _lut[x].b, 0.f);
    }

    fprintf(fp, "\n};\n");
    fprintf(fp, "\n");
    fprintf(fp, "#endif /* __LUT_INCLUDE_GUARD_%s__ */\n", include_guard_var.c_str());
    fclose(fp);
}

void LUT3D_vulkan::upload_param(const VkMat& src, VkMat& dst)
{
    std::vector<VkMat> bindings(9);
    if      (dst.type == IM_DT_INT8)     bindings[0] = dst;
    else if (dst.type == IM_DT_INT16 || dst.type == IM_DT_INT16_BE)    bindings[1] = dst;
    else if (dst.type == IM_DT_FLOAT16)  bindings[2] = dst;
    else if (dst.type == IM_DT_FLOAT32)  bindings[3] = dst;

    if      (src.type == IM_DT_INT8)     bindings[4] = src;
    else if (src.type == IM_DT_INT16 || src.type == IM_DT_INT16_BE)    bindings[5] = src;
    else if (src.type == IM_DT_FLOAT16)  bindings[6] = src;
    else if (src.type == IM_DT_FLOAT32)  bindings[7] = src;
    bindings[8] = lut_gpu;
    std::vector<vk_constant_type> constants(12);
    constants[0].i = src.w;
    constants[1].i = src.h;
    constants[2].i = src.c;
    constants[3].i = src.color_format;
    constants[4].i = src.type;
    constants[5].i = dst.w;
    constants[6].i = dst.h;
    constants[7].i = dst.c;
    constants[8].i = dst.color_format;
    constants[9].i = dst.type;
    constants[10].i = interpolation_mode;
    constants[11].i = lut_gpu.w;
    cmd->record_pipeline(pipeline_lut3d, bindings, constants, dst);
}

double LUT3D_vulkan::filter(const ImMat& src, ImMat& dst)
{
    double ret = 0.0;
    if (!vkdev || !pipeline_lut3d || lut_gpu.empty() || !cmd)
    {
        return ret;
    }

    VkMat dst_gpu;
    dst_gpu.create_type(src.w, src.h, 4, dst.type, opt.blob_vkallocator);

    VkMat src_gpu;
    if (src.device == IM_DD_VULKAN)
    {
        src_gpu = src;
    }
    else if (src.device == IM_DD_CPU)
    {
        cmd->record_clone(src, src_gpu, opt);
    }

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_start();
#endif

    upload_param(src_gpu, dst_gpu);

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_end();
#endif

    // download
    if (dst.device == IM_DD_CPU)
        cmd->record_clone(dst_gpu, dst, opt);
    else if (dst.device == IM_DD_VULKAN)
        dst = dst_gpu;
    cmd->submit_and_wait();
#ifdef VULKAN_SHADER_BENCHMARK
    ret = cmd->benchmark();
#endif
    cmd->reset();
    return ret;
}
} //namespace ImGui 