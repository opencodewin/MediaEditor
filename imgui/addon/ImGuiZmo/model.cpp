#include "model.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <fstream>
#include <sstream>
#include <algorithm>

/** Set the platform-specific directory path separator */
#if defined(_WIN32)
#define DIR_SEP '\\'
#else
#define DIR_SEP '/'
#endif

const std::string SPACE = " \t\n\r\f\v";
static inline void rtrim(std::string &str)
{
    str.erase(str.find_last_not_of(SPACE) + 1);
}

// Model file stuff
// Get the color of the given attribute
ImVec3 Material::getColor(const Material::Attribute &attrib) const
{
    switch (attrib)
    {
    // Return color by attribute
    case Material::AMBIENT:
        return color[0];
    case Material::DIFFUSE:
        return color[1];
    case Material::SPECULAR:
        return color[2];
    case Material::TRANSPARENCY:
        return color[3];
    // Invalid attribute
    default:
        std::cerr << "error: invalid attribute `" << attrib << "'" << std::endl;
        return ImVec3(NAN);
    }
}

// Get the value of the given attribute
float Material::getValue(const Material::Attribute &attrib) const
{
    switch (attrib)
    {
    // Return value by attribute
    case Material::SHININESS:
        return value[0];
    case Material::ROUGHNESS:
        return value[1];
    case Material::METALNESS:
        return value[2];
    case Material::TRANSPARENCY:
        return value[3];
    case Material::DISPLACEMENT:
        return value[4];
    case Material::REFRACTIVE_INDEX:
        return value[5];
    // Invalid attribute
    default:
        std::cerr << "error: invalid attribute `" << attrib << "'" << std::endl;
        return NAN;
    }
}

// Get the texture enabled status
bool Material::isTextureEnabled(const Material::Attribute &attrib) const
{
    switch (attrib)
    {
    // Return texture path by attribute
    case Material::AMBIENT:
        return texture_enabled[0];
    case Material::DIFFUSE:
        return texture_enabled[1];
    case Material::SPECULAR:
        return texture_enabled[2];
    case Material::SHININESS:
        return texture_enabled[3];
    case Material::NORMAL:
        return texture_enabled[4];
    case Material::DISPLACEMENT:
        return texture_enabled[5];
    // Cubemap
    case Material::CUBE_MAP:
    case Material::CUBE_MAP_RIGHT:
    case Material::CUBE_MAP_LEFT:
    case Material::CUBE_MAP_TOP:
    case Material::CUBE_MAP_BOTTOM:
    case Material::CUBE_MAP_FRONT:
    case Material::CUBE_MAP_BACK:
        return texture_enabled[6];
    // Invalid attribute
    default:
        std::cerr << "error: invalid attribute `" << attrib << "'" << std::endl;
        return false;
    }
}

// Get the texture path of the given attribute
std::string Material::getTexturePath(const Material::Attribute &attrib) const
{
    switch (attrib)
    {
    // Return texture path by attribute
    case Material::AMBIENT:
        return texture_path[0];
    case Material::DIFFUSE:
        return texture_path[1];
    case Material::SPECULAR:
        return texture_path[2];
    case Material::SHININESS:
        return texture_path[3];
    case Material::NORMAL:
        return texture_path[4];
    case Material::DISPLACEMENT:
        return texture_path[5];
    // Cube map texture paths
    case Material::CUBE_MAP_RIGHT:
        return texture_path[6];
    case Material::CUBE_MAP_LEFT:
        return texture_path[7];
    case Material::CUBE_MAP_TOP:
        return texture_path[8];
    case Material::CUBE_MAP_BOTTOM:
        return texture_path[9];
    case Material::CUBE_MAP_FRONT:
        return texture_path[10];
    case Material::CUBE_MAP_BACK:
        return texture_path[11];
    // Invalid attribute
    default:
        std::cerr << "error: invalid attribute `" << attrib << "'" << std::endl;
        return "INVALID";
    }
}

// Set the color of the given attribute
void Material::setColor(const Material::Attribute &attrib, const ImVec3 &new_color)
{
    switch (attrib)
    {
    // Set color by attribute
    case Material::AMBIENT:
        color[0] = new_color;
        return;
    case Material::DIFFUSE:
        color[1] = new_color;
        return;
    case Material::SPECULAR:
        color[2] = new_color;
        return;
    case Material::TRANSPARENCY:
        color[3] = new_color;
        return;
    // Invalid attribute
    default:
        std::cerr << "error: invalid attribute `" << attrib << "'" << std::endl;
    }
}

// Set the value of the given attribute
void Material::setValue(const Material::Attribute &attrib, const float &new_value)
{
    switch (attrib)
    {
    // Set value by attribute
    case Material::SHININESS:
        value[0] = new_value;
        return;
    case Material::ROUGHNESS:
        value[1] = new_value;
        return;
    case Material::METALNESS:
        value[2] = new_value;
        return;
    case Material::TRANSPARENCY:
        value[3] = new_value;
        return;
    case Material::DISPLACEMENT:
        value[4] = new_value;
        return;
    case Material::REFRACTIVE_INDEX:
        value[5] = new_value;
        return;
    // Invalid attribute
    default:
        std::cerr << "error: invalid attribute `" << attrib << "'" << std::endl;
    }
}

// Set the texture enabled status
void Material::setTextureEnabled(const Material::Attribute &attrib, const bool &status)
{
    switch (attrib)
    {
    // Return texture path by attribute
    case Material::AMBIENT:
        texture_enabled[0] = status;
        return;
    case Material::DIFFUSE:
        texture_enabled[1] = status;
        return;
    case Material::SPECULAR:
        texture_enabled[2] = status;
        return;
    case Material::SHININESS:
        texture_enabled[3] = status;
        return;
    case Material::NORMAL:
        texture_enabled[4] = status;
        return;
    case Material::DISPLACEMENT:
        texture_enabled[5] = status;
        return;
    // Cubemap
    case Material::CUBE_MAP:
    case Material::CUBE_MAP_RIGHT:
    case Material::CUBE_MAP_LEFT:
    case Material::CUBE_MAP_TOP:
    case Material::CUBE_MAP_BOTTOM:
    case Material::CUBE_MAP_FRONT:
    case Material::CUBE_MAP_BACK:
        texture_enabled[6] = status;
        return;
    // Invalid attribute
    default:
        std::cerr << "error: invalid attribute `" << attrib << "'" << std::endl;
    }
}

// Set the texture path of the given attribute
void Material::setTexturePath(const Material::Attribute &attrib, const std::string &path)
{
    switch (attrib)
    {
    // Set texture path by attribute
    case Material::AMBIENT:
        texture_path[0] = path;
        break;
    case Material::DIFFUSE:
        texture_path[1] = path;
        break;
    case Material::SPECULAR:
        texture_path[2] = path;
        break;
    case Material::SHININESS:
        texture_path[3] = path;
        break;
    case Material::NORMAL:
        texture_path[4] = path;
        break;
    case Material::DISPLACEMENT:
        texture_path[5] = path;
        break;
    // Cube map texture paths
    case Material::CUBE_MAP:
    case Material::CUBE_MAP_RIGHT:
    case Material::CUBE_MAP_LEFT:
    case Material::CUBE_MAP_TOP:
    case Material::CUBE_MAP_BOTTOM:
    case Material::CUBE_MAP_FRONT:
    case Material::CUBE_MAP_BACK:
        // Print warning message
        std::cout << "warning: using the same texture for all cube map sides, use `Material::setCubeMapTexturePath(const std::string (&path)[6])' to set each cube map path separately" << std::endl;

        // Set the cube map texture paths
        for (int i = 6; i < 12; i++)
        {
            texture_path[i] = path;
        }
        break;
    // Invalid attribute
    default:
        std::cerr << "error: invalid attribute `" << attrib << "'" << std::endl;
        return;
    }
    // Reload texture
    // reloadTexture(attrib);
}

// Set the cube map texture path
void Material::setCubeMapTexturePath(const std::string (&path)[6])
{
    // Set the texture paths
    for (int i = 6, j = 1; j < 6; j++)
    {
        texture_path[i] = path[j];
    }

    // Reload texture
    // reloadTexture(Material::CUBE_MAP);
}

// Read material data from file
static bool readMaterial(const std::string &mtl, ModelData *model_data)
{
    // Get the relative directory and set the material file path
    model_data->material_path = mtl;
    const std::string relative = model_data->material_path.substr(0U, model_data->material_path.find_last_of(DIR_SEP) + 1U);

    // Open the material file and check it
    std::ifstream file(model_data->material_path);
    if (!file.is_open())
    {
        std::cerr << "error: could not open the material file `" << model_data->material_path << "'" << std::endl;
        return false;
    }

    // Material description read variables
    Material *material = nullptr;
    bool load_cube_map = false;
    std::string cube_map_path[6];
    std::string token;
    std::string line;
    ImVec3 data;
    float value;

    // Read the file
    while (!file.eof())
    {
        // Read line
        std::getline(file, line);

        // Skip comments
        if (line[0] == '#')
        {
            continue;
        }

        // Right trim line and skip empty line
        rtrim(line);
        if (line.empty())
        {
            continue;
        }

        // Create a string stream and read the first token
        std::istringstream stream(line);
        stream >> token;

        // Convert token to lower case
        std::transform(token.begin(), token.end(), token.begin(), ::tolower);

        // New material
        if (token == "newmtl")
        {
            // Set cube map paths to the previous material
            if (load_cube_map)
            {
                material->setCubeMapTexturePath(cube_map_path);

                // Clear cube map paths
                load_cube_map = false;
                for (int i = 0; i < 6; i++)
                {
                    cube_map_path[i].clear();
                }
            }

            // Get left trimed full name
            stream >> std::ws;
            std::getline(stream, token);

            // Create and store the new material
            model_data->material_stock.emplace_back(new Material(token));
            material = model_data->material_stock.back();
        }

        // Colors

        // Ambient color
        else if (token == "ka")
        {
            stream >> data.x >> data.y >> data.z;
            material->setColor(Material::AMBIENT, data);
        }

        // Diffuse color
        else if (token == "kd")
        {
            stream >> data.x >> data.y >> data.z;
            material->setColor(Material::DIFFUSE, data);
        }

        // Specular color
        else if (token == "ks")
        {
            stream >> data.x >> data.y >> data.z;
            material->setColor(Material::SPECULAR, data);
        }

        // Transparency color
        else if (token == "tf")
        {
            stream >> data.x >> data.y >> data.z;
            material->setColor(Material::TRANSPARENCY, data);
        }

        // Values

        // Shininess
        else if (token == "ns")
        {
            stream >> value;
            material->setValue(Material::SHININESS, value);
        }

        // Disolve
        else if (token == "d")
        {
            stream >> value;
            material->setValue(Material::TRANSPARENCY, 1.0F - value);
        }

        // Transparency
        else if (token == "tr")
        {
            stream >> value;
            material->setValue(Material::TRANSPARENCY, value);
        }

        // Refractive index
        else if (token == "ni")
        {
            stream >> value;
            material->setValue(Material::REFRACTIVE_INDEX, value);
        }

        // Textures

        // Ambient texture
        else if (token == "map_ka")
        {
            stream >> std::ws;
            std::getline(stream, token);
            material->setTexturePath(Material::AMBIENT, relative + token);
            model_data->textures++;
        }

        // Diffuse texture
        else if (token == "map_kd")
        {
            stream >> std::ws;
            std::getline(stream, token);
            material->setTexturePath(Material::DIFFUSE, relative + token);
            model_data->textures++;
        }

        // Specular texture
        else if (token == "map_ks")
        {
            stream >> std::ws;
            std::getline(stream, token);
            material->setTexturePath(Material::SPECULAR, relative + token);
            model_data->textures++;
        }

        // Shininess texture
        else if (token == "map_ns")
        {
            stream >> std::ws;
            std::getline(stream, token);
            material->setTexturePath(Material::SHININESS, relative + token);
            model_data->textures++;
        }

        // Normal texture
        else if ((token == "map_bump") || (token == "bump") || (token == "kn"))
        {
            stream >> std::ws;
            std::getline(stream, token);
            material->setTexturePath(Material::NORMAL, relative + token);
            model_data->textures++;
        }

        // Displacement texture
        else if (token == "disp")
        {
            stream >> std::ws;
            std::getline(stream, token);
            material->setTexturePath(Material::DISPLACEMENT, relative + token);
            model_data->textures++;
        }

        // Cube map texture
        else if (token == "refl")
        {
            // Is a cube map flag
            bool is_cube = false;

            // Skip the "-type" token and get the cube side
            stream >> token;
            stream >> token;

            // Right side
            if (token == "cube_right")
            {
                stream >> std::ws;
                std::getline(stream, token);
                cube_map_path[0] = relative + token;
                is_cube = true;
            }

            // Left side
            else if (token == "cube_left")
            {
                stream >> std::ws;
                std::getline(stream, token);
                cube_map_path[1] = relative + token;
                is_cube = true;
            }

            // Top side
            else if (token == "cube_top")
            {
                stream >> std::ws;
                std::getline(stream, token);
                cube_map_path[2] = relative + token;
                is_cube = true;
            }

            // Bottom side
            else if (token == "cube_bottom")
            {
                stream >> std::ws;
                std::getline(stream, token);
                cube_map_path[3] = relative + token;
                is_cube = true;
            }

            // Front side
            else if (token == "cube_front")
            {
                stream >> std::ws;
                std::getline(stream, token);
                cube_map_path[4] = relative + token;
                is_cube = true;
            }

            // Back side
            else if (token == "cube_back")
            {
                stream >> std::ws;
                std::getline(stream, token);
                cube_map_path[5] = relative + token;
                is_cube = true;
            }

            // Count texture set load cube map true
            if (is_cube)
            {
                model_data->textures++;
                load_cube_map = true;
            }
        }
    }

    // Close file
    file.close();

    // Return true if not error has been found
    model_data->material_open = true;
    return true;
}

ModelData *LoadObj(const std::string path)
{
    ModelData *model_data = nullptr;
    std::ifstream file(path);
    if (!file.is_open())
    {
        std::cerr << "error: could not open the model `" << path << "'" << std::endl;
        return model_data;
    }
    model_data = new ModelData(path);

    // Model read variables
    std::vector<std::string> face;
    std::string token;
    std::string line;
    ImVec3 data;
    size_t count = 0U;

    auto storeVertex = [&](const std::string &vertex_str)
    {
        // Search the vertex
        std::map<std::string, size_t>::iterator result = model_data->parsed_vertex.find(vertex_str);
        // Return the index of already parsed vertex
        if (result != model_data->parsed_vertex.end())
        {
            model_data->index_stock.emplace_back(result->second);
            return result->second;
        }
        // Create a new vertex
        Vertex vertex;
        // Parse vertex indices data
        for (std::size_t i = 0, begin = 0, end = 0; (i < 3) && (end != std::string::npos); i++, begin = end + 1)
        {
            // Find next attribute position
            end = vertex_str.find('/', begin);

            // Parse index and get attribute
            if ((begin < vertex_str.size()) && (begin < end))
            {
                switch (i)
                {
                case 0:
                    vertex.position = model_data->position_stock[std::stoi(vertex_str.substr(begin)) - 1];
                    break;
                case 1:
                    vertex.uv_coord = model_data->uv_coord_stock[std::stoi(vertex_str.substr(begin)) - 1];
                    break;
                case 2:
                    vertex.normal = model_data->normal_stock[std::stoi(vertex_str.substr(begin)) - 1];
                }
            }
        }
        // Add vertex
        size_t index = static_cast<size_t>(model_data->vertex_stock.size());
        model_data->parsed_vertex[vertex_str] = index;
        model_data->index_stock.emplace_back(index);
        model_data->vertex_stock.emplace_back(vertex);

        // Return the index
        return index;
    };

    auto calcTangent = [&](const size_t &ind_0, const size_t &ind_1, const size_t &ind_2)
    {
        // Get vertices
        Vertex &vertex_0 = model_data->vertex_stock.at(ind_0);
        Vertex &vertex_1 = model_data->vertex_stock.at(ind_1);
        Vertex &vertex_2 = model_data->vertex_stock.at(ind_2);

        // Get position triangle edges
        const ImVec3 l0(vertex_1.position - vertex_0.position);
        const ImVec3 l1(vertex_2.position - vertex_0.position);

        // Get texture triangle edges
        const ImVec2 d0(vertex_1.uv_coord - vertex_0.uv_coord);
        const ImVec2 d1(vertex_2.uv_coord - vertex_0.uv_coord);

        // Calculate tangent
        const ImVec3 s0 = l0 * d1.y;
        const ImVec3 s1 = l1 * d1.y;
        const ImVec3 tangent((s0 - s1) / fabs(d0.x * d1.y - d1.x * d0.y));

        // Accumulate tangent
        vertex_0.tangent += tangent;
        vertex_1.tangent += tangent;
        vertex_2.tangent += tangent;
    };
    
    auto TriArea2D = [](float x1,float y1,float x2,float y2,float x3,float y3 )
    {
        return (x1-x2)*(y2-y3)-(x2-x3)*(y1-y2);
    };
    auto calcBarycentric = [&](const ImVec3& a, const ImVec3& b, const ImVec3& c)
    {
        ImVec3 centric = (a + b + c) / 3;
        return centric;
    };

    // Read the file
    while (!file.eof())
    {
        // Read line
        std::getline(file, line);
        // Skip comments
        if (line[0] == '#')
        {
            continue;
        }

        // Right trim line and skip empty line
        rtrim(line);
        if (line.empty())
        {
            continue;
        }

        // Create a string stream and read the first token
        std::istringstream stream(line);
        stream >> token;

        // Load material file
        if (token == "mtllib")
        {
            // Get the relative path to the material file
            stream >> std::ws;
            std::getline(stream, token);

            // Read mtl file
            readMaterial(path.substr(0U, path.find_last_of(DIR_SEP) + 1U) + token, model_data);
        }

        // Use material for the next vertices
        else if ((token == "usemtl") && model_data->material_open)
        {
            // Set count to the previous object
            if (!model_data->object_stock.empty())
            {
                model_data->object_stock.back()->count = static_cast<size_t>(model_data->index_stock.size()) - count;
                count = static_cast<size_t>(model_data->index_stock.size());
            }

            // Read material name
            stream >> std::ws;
            std::getline(stream, token);

            // Search in the stock
            for (Material *const material : model_data->material_stock)
            {
                if (material->getName() == token)
                {
                    model_data->object_stock.emplace_back(new ModelData::Object(0, count, material));
                    break;
                }
            }
        }

        // Store vertex position
        else if (token == "v")
        {
            stream >> data.x >> data.y >> data.z;
            model_data->position_stock.emplace_back(data);

            // Update the position limits
            if (data.x < model_data->min.x)
                model_data->min.x = data.x;
            if (data.y < model_data->min.y)
                model_data->min.y = data.y;
            if (data.z < model_data->min.z)
                model_data->min.z = data.z;
            if (data.x > model_data->max.x)
                model_data->max.x = data.x;
            if (data.y > model_data->max.y)
                model_data->max.y = data.y;
            if (data.z > model_data->max.z)
                model_data->max.z = data.z;
        }

        // Store normal
        else if (token == "vn")
        {
            stream >> data.x >> data.y >> data.z;
            model_data->normal_stock.emplace_back(data);
        }

        // Store texture coordinate
        else if (token == "vt")
        {
            stream >> data.x >> data.y;
            model_data->uv_coord_stock.emplace_back(ImVec2(data.x, data.y));
        }

        // Store face
        else if (token == "f")
        {
            // Read the face vertex data
            while (stream >> token)
            {
                face.emplace_back(token);
            }

            // First vertex
            const std::string &first = *face.begin();

            // Triangulate polygon
            for (std::vector<std::string>::iterator it = face.begin() + 2; it != face.end(); it++)
            {
                // Store the first and previous vertex
                const size_t ind_0 = storeVertex(first);
                const size_t ind_1 = storeVertex(*(it - 1));

                // Store the current vertex
                const size_t ind_2 = storeVertex(*it);

                // Calculate the tangent of the triangle
                calcTangent(ind_0, ind_1, ind_2);
            }

            // Clear faces vector
            face.clear();
        }
    }

    // Close file
    file.close();

    // Set count to the last object
    if (model_data->material_open)
    {
        model_data->object_stock.back()->count = static_cast<size_t>(model_data->index_stock.size()) - count;
    }

    // Create a default material and associate all vertices to it if the material file could not be open
    else
    {
        Material *material = new Material("default");
        model_data->material_stock.emplace_back(material);
        model_data->object_stock.emplace_back(new ModelData::Object(static_cast<size_t>(model_data->index_stock.size()), 0, material));
    }

    // Orthogonalize tangents
    for (Vertex &vertex : model_data->vertex_stock)
    {
        vertex.tangent = (vertex.tangent - vertex.normal * vertex.normal.Dot(vertex.tangent)).Normalize();
    }

    // Setup origin matrix
    ImVec3 dim = model_data->max - model_data->min;
    float min_dim = 1.0F / fmax(fmax(dim.x, dim.y), dim.z);
    ImVec3 vt = (model_data->min + model_data->max) / -2.0f;
    ImMat4x4 mat(1.0f);
    mat.Scale(min_dim, min_dim, min_dim);
    mat.Translation(vt.x, vt.y, vt.z);
    model_data->origin_mat = mat;

    // Save statistics
    model_data->vertices = model_data->position_stock.size();
    model_data->elements = model_data->vertex_stock.size();
    model_data->triangles = model_data->index_stock.size() / 3U;
    // Calculate triangle barycentric
    for (size_t ii = 0; ii < model_data->triangles; ii++ )
    {
        size_t i1 = model_data->index_stock[ii * 3 + 0];
        size_t i2 = model_data->index_stock[ii * 3 + 1];
        size_t i3 = model_data->index_stock[ii * 3 + 2];
        ImVec3 coord1 = model_data->vertex_stock[i1].position;
        ImVec3 coord2 = model_data->vertex_stock[i2].position;
        ImVec3 coord3 = model_data->vertex_stock[i3].position;
        auto cent = calcBarycentric(coord1, coord2, coord3);
        model_data->barycentric_stock.emplace_back(cent);
    }

    // Return true if not error has been found
    model_data->model_open = true;
    return model_data;
}
