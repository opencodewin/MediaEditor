#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <immat.h>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>

#define MAX_LIGHTS 5

typedef glm::mat3 mat3;
typedef glm::mat4 mat4; 
typedef glm::vec3 vec3; 
typedef glm::vec4 vec4; 

namespace gl {
class IMGUI_API Mesh{
public:
    Mesh(const std::string path);
    Mesh(const ImGui::ImMat& image, const ImGui::ImMat& map, const float depth_scale = 1.0);
    ~Mesh() { destroy_buffers(); };

    std::vector <vec3> objectVertices;
    std::vector <vec3> objectNormals;
    std::vector <vec3> objectColors;
    std::vector <unsigned int> objectIndices;
    std::vector <unsigned int> uvIndices;
    std::vector <unsigned int> normalIndices;

    int render_mode = 0;
    float tx = 0.f, ty = 0.f; 
    float fovy = 70.0f;
    float zFar = 99.0f;
    float zNear = 0.1f;
    float amount = 5.f;
    vec3 eye = {0.0, 0.0, 5.0};  
    vec3 up = {0.0, 1.0, 0.0};  
    vec3 center = {0.0, 0.0, 0.0};  

private:
    float view_width = 800, view_height = 600;
    GLuint vertex_array, vertex_buffer, normal_buffer, index_buffer, color_buffer;
    std::string object_path; 
    GLuint vertexshader, fragmentshader, shaderprogram;
    mat4 projection, modelview;
    GLuint projectionPos, modelviewPos;
    GLuint lightcol; 
    GLuint lightpos; 
    GLuint numusedcol; 
    GLuint enablelighting; 
    GLuint ambientcol; 
    GLuint diffusecol; 
    GLuint specularcol; 
    GLuint emissioncol; 
    GLuint shininesscol; 

    void initialize_shaders();
    void generate_buffers();
    void destroy_buffers();
    void parse_file_and_bind();
    void parse_image_and_bind(const ImGui::ImMat& image, const ImGui::ImMat& map, const float depth_scale);
    inline void bind(){glBindVertexArray(vertex_array);}
public:
    void set_view_size(float width, float height);
    void set_angle(float angleX, float angleY);
    void update();
    void display(float& ambient_slider, float& diffuse_slider, float& specular_slider, float& shininess_slider, bool custom_color, float& light_position, float& light_color, float scale = 1.f, float x_arg = 0.f, float y_arg = 0.f, float z_arg = 0.f);
};
}
