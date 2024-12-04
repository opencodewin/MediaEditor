#define MODULE_API_EXPORTS
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "libdither.h"
#include "dither_dotdiff_data.h"
#include "hash.h"

struct Point {
    int x;
    int y;
};
typedef struct Point Point;

static Point* Point_new(int x, int y) {
    Point* self = (Point*)calloc(1, sizeof(Point));
    self->x = x;
    self->y = y;
    return self;
}

static void Point_delete(Point* self) {
    if(self) {
        free(self);
        self = NULL;
    }
}

struct DotClassMatrix {
    int* buffer;  // buffer for flat matrix array
    int  width;
    int  height;
};

struct DotDiffusionMatrix {
    double* buffer;  // buffer for flat matrix array
    int width;
    int height;
};

static DotClassMatrix* DotClassMatrix_new(int width, int height, const int* matrix) {
    DotClassMatrix* self = (DotClassMatrix*)calloc(1, sizeof(DotClassMatrix));
    self->buffer = (int*)calloc(width * height, sizeof(int));
    memcpy(self->buffer, matrix, width * height * sizeof(int));
    self->width = width;
    self->height = height;
    return self;
}

static void DotClassMatrix_free(DotClassMatrix* self) {
    if(self) {
        free(self->buffer);
        free(self);
        self = NULL;
    }
}

static DotDiffusionMatrix* DotDiffusionMatrix_new(int width, int height, const double* matrix) {
    DotDiffusionMatrix* self = (DotDiffusionMatrix*)calloc(1, sizeof(DotDiffusionMatrix));
    self->buffer = (double*)calloc(width * height, sizeof(double));
    memcpy(self->buffer, matrix, width * height * sizeof(double));
    self->width = width;
    self->height = height;
    return self;
}

static void DotDiffusionMatrix_free(DotDiffusionMatrix* self) {
    if(self) {
        free(self->buffer);
        free(self);
        self = NULL;
    }
}

static DotDiffusionMatrix* get_default_diffusion_matrix() { return DotDiffusionMatrix_new(3, 3, default_diffusion_matrix); }
static DotDiffusionMatrix* get_guoliu8_diffusion_matrix() { return DotDiffusionMatrix_new(3, 3, guoliu8_diffusion_matrix); }
static DotDiffusionMatrix* get_guoliu16_diffusion_matrix() { return DotDiffusionMatrix_new(3, 3, guoliu16_diffusion_matrix); }
static DotClassMatrix* get_mini_knuth_class_matrix() { return DotClassMatrix_new(4, 4, mini_knuth_class_matrix); }
static DotClassMatrix* get_knuth_class_matrix() { return DotClassMatrix_new(8, 8, knuth_class_matrix); }
static DotClassMatrix* get_optimized_knuth_class_matrix() { return DotClassMatrix_new(8, 8, optimized_knuth_class_matrix); }
static DotClassMatrix* get_mese_8x8_class_matrix() { return DotClassMatrix_new(8, 8, mese_8x8_class_matrix); }
static DotClassMatrix* get_mese_16x16_class_matrix() { return DotClassMatrix_new(16, 16, mese_16x16_class_matrix); }
static DotClassMatrix* get_guoliu_8x8_class_matrix() { return DotClassMatrix_new(8, 8, guoliu_8x8_class_matrix); }
static DotClassMatrix* get_guoliu_16x16_class_matrix() { return DotClassMatrix_new(16, 16, guoliu_16x16_class_matrix); }
static DotClassMatrix* get_spiral_class_matrix() { return DotClassMatrix_new(8, 8, spiral_class_matrix); }
static DotClassMatrix* get_spiral_inverted_class_matrix() { return DotClassMatrix_new(8, 8, spiral_inverted_class_matrix); }

static void dot_diffusion_dither(const ImGui::ImMat& img, const DotDiffusionMatrix* dmatrix, const DotClassMatrix* cmatrix, ImGui::ImMat& out) {
    /* Knuth's dot dither algorithm */
    int blocksize = cmatrix->width;
    PHash* lut = PHash_new(blocksize * blocksize);
    for(int y = 0; y < blocksize; y++)
        for (int x = 0; x < blocksize; x++)
            PHash_insert(lut, cmatrix->buffer[y * blocksize + x], Point_new(x, y));

    double* orig_img = (double*)calloc(img.w * img.h, sizeof(double));
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int y = 0; y < img.h; y++)
    {
        for (int x = 0; x < img.w; x++)
        {
            size_t addr = y * img.w + x;
            orig_img[addr] = img.at<uint8_t>(x, y) / 255.f;
        }
    }

    int pixel_no[9];
    double pixel_weight[9];
    int yyend = (int)ceil((double)img.h / (double)blocksize);
    #pragma omp parallel for num_threads(OMP_THREADS)
    for(int yy = 0; yy < yyend; yy++) {
        int ofs_y = yy * blocksize;
        int xxend = (int)ceil((double)img.w / (double)blocksize);
        for(int xx = 0; xx < xxend; xx++) {
            int ofs_x = xx * blocksize;
            for(int current_point_no = 0; current_point_no < blocksize * blocksize; current_point_no++) {
                Point *cm = (Point *)PHash_search(lut, current_point_no);
                if(cm->y + ofs_y >= img.h || cm->x + ofs_x >= img.w)
                    continue;
                int imgx = cm->x + ofs_x;
                int imgy = cm->y + ofs_y;
                double err = orig_img[imgy * img.w + imgx];
                if(err >= 0.5) {
                    out.at<uint8_t>(imgx, imgy) = 0xFF;
                    err -= 1.0;
                }
                size_t j = 0;
                int x = cm->x - 1;
                int y = cm->y - 1;
                int total_err_weight = 0;
                for(int dmy=0; dmy < 3; dmy++) {
                    for(int dmx=0; dmx < 3; dmx++) {
                        int cmy = dmy + y;
                        int cmx = dmx + x;
                        if(-1 < cmx && cmx < blocksize && -1 < cmy && cmy < blocksize) {
                            int point_no = cmatrix->buffer[cmy * blocksize + cmx];
                            if(point_no > current_point_no) {
                                int sub_weight = (int)(dmatrix->buffer[dmy * dmatrix->width + dmx]);
                                total_err_weight += sub_weight;
                                pixel_no[j] = point_no;
                                pixel_weight[j] = (double)sub_weight;
                                j++;
                            }
                        }
                    }
                }
                if(total_err_weight > 0) {
                    err /= (double)total_err_weight;
                    for(size_t i=0; i < j; i++) {
                        int point_no = pixel_no[i];
                        double sub_weight = pixel_weight[i];
                        Point* c = (Point *)PHash_search(lut, point_no);
                        int cx = c->x + ofs_x;
                        int cy = c->y + ofs_y;
                        if(cx < img.w && cy < img.h)
                            orig_img[cy * img.w + cx] += (err * sub_weight);
                    }
                }
            }
        }
    }
    PHash_delete(lut);
}

void dot_diffusion_dither(const ImGui::ImMat& img, const DD_TYPE type, ImGui::ImMat& out)
{
    DotClassMatrix* dcm = nullptr;
    DotDiffusionMatrix* ddm = nullptr;
    switch(type)
    {
        case DD_KNUTH_CLASS :
            ddm = get_default_diffusion_matrix();
            dcm = get_knuth_class_matrix();
            break;
        case DD_MINI_KNUTH_CLASS :
            ddm = get_default_diffusion_matrix();
            dcm = get_mini_knuth_class_matrix();
            break;
        case DD_OPTIMIZED_KNUTH_CLASS : 
            ddm = get_default_diffusion_matrix();
            dcm = get_optimized_knuth_class_matrix();
            break;
        case DD_MESE_8X8_CLASS : 
            ddm = get_default_diffusion_matrix();
            dcm = get_mese_8x8_class_matrix();
            break;
        case DD_MESE_16X16_CLASS : 
            ddm = get_default_diffusion_matrix();
            dcm = get_mese_16x16_class_matrix();
            break;
        case DD_GUOLIU_8X8_CLASS : 
            ddm = get_guoliu8_diffusion_matrix();
            dcm = get_guoliu_8x8_class_matrix();
            break;
        case DD_GUOLIU_16X16_CLASS : 
            ddm = get_guoliu16_diffusion_matrix();
            dcm = get_guoliu_16x16_class_matrix();
            break;
        case DD_SPIRAL_CLASS:
            ddm = get_guoliu8_diffusion_matrix();
            dcm = get_spiral_class_matrix();
            break;
        case DD_SPIRAL_INVERTED_CLASS:
            ddm = get_guoliu8_diffusion_matrix();
            dcm = get_spiral_inverted_class_matrix();
            break;
        default: break;
    }
    dot_diffusion_dither(img, ddm, dcm, out);
    DotClassMatrix_free(dcm);
    DotDiffusionMatrix_free(ddm);
}