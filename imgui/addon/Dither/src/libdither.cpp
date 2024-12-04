#include <stdlib.h>
#include "libdither.h"
#include "dither_data.h"

const char* libdither_version() {
#ifdef LIB_VERSION
    static char version[] = LIB_VERSION;
#else
    static char version[] = "unknown";
#endif
    return version;
}

ImGui::ImMat dither_test_image()
{
    ImGui::ImMat m;
    int width = 0, height = 0, component = 0;
    if (auto _data = stbi_load_from_memory((stbi_uc const *)david_data, david_size, &width, &height, &component, 1))
    {
        ImGui::ImMat tmp;
        tmp.create_type(width, height, 1, _data, IM_DT_INT8);
        m = tmp.clone();
        stbi_image_free(_data);
    }
    return m;
}