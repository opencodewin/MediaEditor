#pragma once
#include <imgui.h>

/** Material with textures */
class Material {
public:
    // Enumerations
    /** Attributes */
        enum Attribute : unsigned int {
        /** Ambient component */
        AMBIENT = 0x0001,
        /** Diffuse component */
        DIFFUSE = 0x0002,
        /** Specular component */
        SPECULAR = 0x0004,
        /** Shininess texture */
        SHININESS = 0x0008,
        /** Roughness value */
        ROUGHNESS = 0x0010,
        /** Metalness value */
        METALNESS = 0x0020,
        /** Transparency component */
        TRANSPARENCY = 0x0040,
        /** Normal texture */
        NORMAL = 0x0080,
        /** Displacement component */
        DISPLACEMENT = 0x0100,
        /** Refractive index value */
        REFRACTIVE_INDEX = 0x0200,
        /** Cube map texture */
        CUBE_MAP = 0x8000,
        /** Cube map right side texture */
        CUBE_MAP_RIGHT = 0x8515,            //GL_TEXTURE_CUBE_MAP_POSITIVE_X,
        /** Cube map left side texture */
        CUBE_MAP_LEFT = 0x8516,             //GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
        /** Cube map top side texture */
        CUBE_MAP_TOP = 0x8517,              //GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
        /** Cube map down side texture */
        CUBE_MAP_BOTTOM = 0x8518,           //GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
        /** Cube map front side texture */
        CUBE_MAP_FRONT = 0x8519,            //GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
        /** Cube map back side texture */
        CUBE_MAP_BACK = 0x851A,             //GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
        /** All textures */
        ALL_TEXTURES = 0x818F
    };
private:
    // Attributes
    /** Material name */
    std::string name;
    /** Colors */
    ImVec3 color[4];
    /** Value */
    float value[6];
    /** Texture enabled status */
    bool texture_enabled[7];
    /** Texture path */
    std::string texture_path[12];
    // Constructors
    /** Disable the default constructor */
    Material() = delete;
    /** Disable the default copy constructor */
    Material(const Material &) = delete;
    /** Disable the assignation operator */
    Material &operator=(const Material &) = delete;

public:
    // Constructor
    /** Material constructor */
    Material(const std::string &name)
        : name(name),
        color{ImVec3(0.0f), ImVec3(1.0f), ImVec3(0.125f), ImVec3(1.0f)},
        value{96.078431f, 0.3f, 0.3f, 0.0f, 0.05f, 1.0f} {}

    // Getters
    /** Get the material name */
    std::string getName() const { return name; }
    /** Get the color of the given attribute */
    ImVec3 getColor(const Material::Attribute &attrib) const;
    /** Get the value of the given attribute */
    float getValue(const Material::Attribute &attrib) const;
    /** Get the texture enabled status */
    bool isTextureEnabled(const Material::Attribute &attrib) const;
    /** Get the texture path of the given attribute */
    std::string getTexturePath(const Material::Attribute &attrib) const;

    // Setters
    /** Set the material name */
    void setName(const std::string &new_name) { name = new_name; }
    /** Set the color of the given attribute */
    void setColor(const Material::Attribute &attrib, const ImVec3 &new_color);
    /** Set the value of the given attribute */
    void setValue(const Material::Attribute &attrib, const float &new_value);
    /** Set the texture enabled status */
    void setTextureEnabled(const Material::Attribute &attrib, const bool &status);
    /** Set the texture path of the given attribute */
    void setTexturePath(const Material::Attribute &attrib, const std::string &path);
    /** Set the cube map texture path */
    void setCubeMapTexturePath(const std::string (&path)[6]);
    // Destructor
    /** Material destructor */
    ~Material() {};
};

class ModelData {
public:
    // Structs
    /** Model object */
    struct Object {
        // Attributes
        /** Number of indices */
        size_t count;
        /** Index offset */
        size_t offset;
        /** Material */
        Material *material;
        // Constructor
        /** Object data constructor */
        Object(const size_t &count = 0, const size_t &offset = 0, Material *const material = nullptr) : count(count), offset(sizeof(size_t) * offset), material(material) {}
    };
    /** Model path */
    std::string model_path;
    /** Material path */
    std::string material_path;
    /** Model open status */
    bool model_open;
    /** Material open status */
    bool material_open;
    /** Origin matrix */
    ImMat4x4 origin_mat;
    /** Minimum position values */
    ImVec3 min;
    /** Maximum position values */
    ImVec3 max;
    /** Vertex array object */
    unsigned int vao;
    /** Vertex buffer object */
    unsigned int vbo;
    /** Element buffer object */
    unsigned int ebo;
    /** Object stock */
    std::vector<ModelData::Object *> object_stock;
    /** Material stock */
    std::vector<Material *> material_stock;
    /** Parsed vertices */
    std::map<std::string, size_t> parsed_vertex;
    /** Indices */
    std::vector<size_t> index_stock;
    /** Vertices */
    std::vector<Vertex> vertex_stock;
    /** Position stock */
    std::vector<ImVec3> position_stock;
    /** Texture coordinates stock */
    std::vector<ImVec2> uv_coord_stock;
    /** Normal stock */
    std::vector<ImVec3> normal_stock;
    /** Barycentric stock */
    std::vector<ImVec3> barycentric_stock;
    /** Number of vertices */
    std::size_t vertices;
    /** Number of elements */
    std::size_t elements;
    /** Number of triangles */
    std::size_t triangles;
    /** Number of textures */
    std::size_t textures;

    // Constructors
    /** Disable the default constructor */
    ModelData() = delete;
    /** Model data constructor */
    ModelData(const std::string &path) :
        model_path(path), model_open(false), material_open(false), origin_mat(1.0f), min(INFINITY),  max(-INFINITY), vao(0), vbo(0), ebo(0), vertices(0U), elements(0U), triangles(0U), textures(0U) { }

    // Destructor
    /** Model data destructor */
    ~ModelData()
    {
        for (auto object : object_stock) { delete object; }
        for (auto material : material_stock) { delete material; }
        // Free memory
        object_stock.clear();
        material_stock.clear();
        parsed_vertex.clear();
        position_stock.clear();
        uv_coord_stock.clear();
        normal_stock.clear();
        barycentric_stock.clear();
    }
};

IMGUI_API ModelData* LoadObj(const std::string path);
