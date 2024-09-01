#include <stdio.h>
#include <imgui.h>
#include <immat.h>

// stbi image custom context
typedef struct {
    int last_pos;
    void *context;
} stbi_mem_context;

static void custom_stbi_write_mem(void *context, void *data, int size)
{
    stbi_mem_context *c = (stbi_mem_context*)context; 
    char *dst = (char *)c->context;
    char *src = (char *)data;
    int cur_pos = c->last_pos;
    for (int i = 0; i < size; i++) {
        dst[cur_pos++] = src[i];
    }
    c->last_pos = cur_pos;
}

static void load_image(std::string path, ImGui::ImMat & mat)
{
    int width = 0, height = 0, component = 0;
    uint8_t * data = nullptr;
    data = stbi_load(path.c_str(), &width, &height, &component, 4);
    if (data)
    {
        mat.release();
        mat.create_type(width, height, 4, data, IM_DT_INT8);
    }
}

static bool binary_to_compressed_c(const char* filename, const char* symbol, void * data, int data_sz)
{
    // Read file
    FILE* f = fopen(filename, "wb");
    if (!f) return false;

    // Compress
    int maxlen = data_sz + 512 + (data_sz >> 2) + sizeof(int); // total guess
    char* compressed = (char*)data;
    int compressed_sz = data_sz;

    //fprintf(f, "// File: '%s' (%d bytes)\n", filename, (int)data_sz);
    //fprintf(f, "// Exported using binary_to_compressed_c.cpp\n");
    const char* static_str = "extern ";
    const char* compressed_str = "";
    {
        fprintf(f, "%sconst unsigned int %s_%ssize = %d;\n", static_str, symbol, compressed_str, (int)compressed_sz);
        fprintf(f, "%sconst unsigned int %s_%sdata[%d/4] =\n{", static_str, symbol, compressed_str, (int)((compressed_sz + 3) / 4)*4);
        int column = 0;
        for (int i = 0; i < compressed_sz; i += 4)
        {
            unsigned int d = *(unsigned int*)(compressed + i);
            if ((column++ % 12) == 0)
                fprintf(f, "\n    0x%08x, ", d);
            else
                fprintf(f, "0x%08x, ", d);
        }
        fprintf(f, "\n};");
    }

    // Cleanup
    fclose(f);
    return true;
}

int main(int argc, char** argv)
{
    if( argc < 4 )
	{
		printf("Usage error: Program need 3 arguments:\n");
		printf("  img2cc <in_file.png> <out_file_base> <symbol_name>\n");
		return -1;
	}
    static int output_quality = argc > 4 ? atoi(argv[4]) : 90;
    if (output_quality <= 0) output_quality = 90;
    static void * data_memory = nullptr;
    stbi_mem_context image_context {0, nullptr};
    ImGui::ImMat input_mat;
    load_image(std::string(argv[1]), input_mat);
    if (input_mat.empty())
    {
        printf("error: input image not opened:%s\n", argv[1]);
        return -1;
    }
    if (!data_memory) { data_memory = malloc(4 * 1024 * 1024); image_context.last_pos = 0; image_context.context = data_memory; }

    image_context.last_pos = 0;
    int ret = stbi_write_jpg_to_func(custom_stbi_write_mem, &image_context, input_mat.w, input_mat.h, input_mat.c, input_mat.data, output_quality);
    if (ret)
    {
        binary_to_compressed_c(argv[2], argv[3], image_context.context, image_context.last_pos);
    }

    if (data_memory) { free(data_memory); data_memory = nullptr; }
    return 0;
}