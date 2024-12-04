#include <memory>
#include <iostream>

#include "mesh.h"

namespace gl {
Mesh::Mesh(const std::string path)
{
    // Init param
    eye = vec3(0.0, 0.0, 5.0); 
    up = vec3(0.0, 1.0, 0.0);
    center = vec3(0.0, 0.0, 0.0);
    amount = 5;
    tx = ty = 0.0;  

	object_path = path;
	
    // Initialize shaders
    initialize_shaders();

    // Initialize global mesh
    generate_buffers();

    // read the mesh file (obj and mesh file)
    parse_file_and_bind();   
}

Mesh::Mesh(const ImGui::ImMat& image, const ImGui::ImMat& map, const float depth_scale)
{
    // Init param
    eye = vec3(0.0, 0.0, 5.0); 
    up = vec3(0.0, 1.0, 0.0);
    center = vec3(0.0, 0.0, 0.0);
    amount = 5;
    tx = ty = 0.0;

    // Initialize shaders
    initialize_shaders();

    // Initialize global mesh
    generate_buffers();

    // parser with RGBD data
    parse_image_and_bind(image, map, depth_scale);
}

void Mesh::initialize_shaders()
{
    char GlslVersionString[32] = "# version 330 core\n";

    const GLchar* vertex_shader = 
    "layout (location = 0) in vec3 position;\n"
    "layout (location = 1) in vec3 normal;\n"
    "layout (location = 2) in vec3 color;\n"
    "uniform mat4 modelview;\n"
    "uniform mat4 projection;\n"
    "out vec3 mynormal;\n"
    "out vec4 myvertex;\n"
    "out vec4 mycolor;\n"
    "void main(){\n"
    "    gl_Position = projection * modelview * vec4(position, 1.0f);\n"
    "    mynormal = normal;\n"
    "    myvertex = vec4(position, 1.0f);\n"
    "    mycolor = vec4(color, 1.0f);\n"
    "}\n";

    const GLchar* fragment_shader = 
    "in vec3 mynormal;\n"
    "in vec4 myvertex;\n"
    "in vec4 mycolor;\n"
    "const int num_lights = 5;\n"
    "out vec4 fragColor;\n"
    "uniform mat4 modelview;\n"
    "uniform vec4 light_posn[num_lights];\n"
    "uniform vec4 light_col[num_lights];\n"
    "uniform vec4 ambient;\n"
    "uniform vec4 diffuse;\n"
    "uniform vec4 specular;\n"
    "uniform float shininess;\n"
    "vec4 compute_lighting(vec3 direction, vec4 lightcolor, vec3 normal, vec3 halfvec, vec4 mydiffuse, vec4 myspecular, float myshininess){\n"
    "    float n_dot_l = dot(normal, direction);\n"
    "    vec4 lambert = mydiffuse * lightcolor * max (n_dot_l, 0.0);\n"
    "    float n_dot_h = dot(normal, halfvec);\n"
    "    vec4 phong = myspecular * lightcolor * pow (max(n_dot_h, 0.0), myshininess);\n"
    "    vec4 lighting = lambert + phong;\n"
    "    return lighting;\n"
    "}\n"
    "void main (void){\n"
    "    vec4 finalcolor = mycolor;\n"
    "    finalcolor += ambient;\n"
    "    const vec3 eyepos = vec3(0,0,0);\n"
    "    vec3 mypos = myvertex.xyz / myvertex.w;\n"
    "    vec3 eyedirn = normalize(eyepos - mypos);\n"
    "    vec3 normal = normalize(mynormal);\n"
    "    for (int i = 0; i < num_lights; i++){\n"
    "        vec4 light_position = inverse(modelview) * light_posn[i];\n"
    "        vec3 position = light_position.xyz / light_position.w;\n"
    "        vec3 direction = normalize(position - mypos);\n"
    "        vec3 half_i = normalize(direction + eyedirn);\n"
    "        vec4 light_color = light_col[i];\n"
    "        vec4 col = compute_lighting(direction, light_color, normal, half_i, diffuse, specular, shininess);\n"
    "        finalcolor += col;\n"
    "    }\n"
    "    fragColor = finalcolor;\n"
    "}\n";

    auto shader_errors = [](const GLint shader){
        GLint length; 
        GLchar * log; 
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length); 
        log = new GLchar[length+1];
        glGetShaderInfoLog(shader, length, &length, log); 
        std::cout << "Compile Error, Log Below\n" << log << "\n"; 
        delete [] log; 
    };
    auto program_errors = [](const GLint program){
        GLint length; 
        GLchar * log; 
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length); 
        log = new GLchar[length+1];
        glGetProgramInfoLog(program, length, &length, log); 
        std::cout << "Compile Error, Log Below\n" << log << "\n"; 
        delete [] log; 
    };

    GLint compiled;
    const GLchar* vertex_shader_with_version[2] = { GlslVersionString, vertex_shader };
    vertexshader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexshader, 2, vertex_shader_with_version, nullptr);
    glCompileShader(vertexshader); 
    glGetShaderiv (vertexshader, GL_COMPILE_STATUS, &compiled);
    if (!compiled){ 
        shader_errors(vertexshader); 
        throw 3; 
    }

    const GLchar* fragment_shader_with_version[2] = { GlslVersionString, fragment_shader };
    fragmentshader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentshader, 2, fragment_shader_with_version, nullptr);
    glCompileShader(fragmentshader); 
    glGetShaderiv (fragmentshader, GL_COMPILE_STATUS, &compiled);
    if (!compiled){ 
        shader_errors(fragmentshader); 
        throw 3; 
    }

    GLint linked;
    shaderprogram = glCreateProgram(); 
    glAttachShader(shaderprogram, vertexshader); 
    glAttachShader(shaderprogram, fragmentshader); 
    glLinkProgram(shaderprogram); 
    glGetProgramiv(shaderprogram, GL_LINK_STATUS, &linked); 
    if (!linked)
    {
        program_errors(shaderprogram); 
        throw 4; 
    }
    glUseProgram(shaderprogram); 

    // Get uniform locations 
    lightpos = glGetUniformLocation(shaderprogram,"light_posn") ;       
    lightcol = glGetUniformLocation(shaderprogram,"light_col") ;       
    ambientcol = glGetUniformLocation(shaderprogram,"ambient") ;       
    diffusecol =  glGetUniformLocation(shaderprogram,"diffuse") ;       
    specularcol = glGetUniformLocation(shaderprogram,"specular") ;       
    emissioncol = glGetUniformLocation(shaderprogram,"emission") ;       
    shininesscol = glGetUniformLocation(shaderprogram,"shininess") ;    
    projectionPos = glGetUniformLocation(shaderprogram, "projection");
    modelviewPos = glGetUniformLocation(shaderprogram, "modelview");
}

void Mesh::generate_buffers()
{
	glGenVertexArrays(1, &vertex_array);
	glGenBuffers(1, &vertex_buffer);
	glGenBuffers(1, &normal_buffer);
	glGenBuffers(1, &index_buffer);
    glGenBuffers(1, &color_buffer);
}

void Mesh::destroy_buffers()
{
	glDeleteVertexArrays(1, &vertex_array);
	glDeleteBuffers(1, &vertex_buffer);
	glDeleteBuffers(1, &normal_buffer);
	glDeleteBuffers(1, &index_buffer);
    glDeleteBuffers(1, &color_buffer);
}

void Mesh::set_view_size(float width, float height)
{
	view_width = width;
	view_height = height;
	glEnable(GL_DEPTH_TEST);
    projection = glm::perspective(glm::radians(fovy), width / height, zNear, zFar); 
    glUniformMatrix4fv(projectionPos, 1, GL_FALSE, &projection[0][0]);
}

void Mesh::update()
{
	projection = glm::perspective(glm::radians(fovy), view_width / view_height, zNear, zFar); 
    glUniformMatrix4fv(projectionPos, 1, GL_FALSE, &projection[0][0]);
}

void Mesh::set_angle(float angleX, float angleY)
{
	eye.x = cosf(angleY) * cosf(angleX) * amount;
    eye.y = sinf(angleX) * amount;
    eye.z = sinf(angleY) * cosf(angleX) * amount;
	update();
}

void Mesh::parse_file_and_bind()
{
	FILE* fp;
	// fp >> 
	float minY = INFINITY, minZ = INFINITY;
	float maxY = -INFINITY, maxZ = -INFINITY;
	// judge the extension
	std::string extension = object_path.substr(object_path.find_last_of(".") + 1);
	fp = fopen(object_path.c_str(), "rb");
	// Error loading file
	if (fp == NULL) {
		std::cerr << "Error loading file: " << object_path << "; probably does not exist" << std::endl;
		exit(-1);
	}
	// open the file with obj extension
	if (extension == "obj")
	{
		float x, y, z;
        float r, g, b;
		int fx, fy, fz, fw;
        int uvx, uvy, uvz, uvw;
        int nx, ny, nz, nw;
		int c1, c2;
		
		//
		while (!feof(fp)) { // This loop continues until the end of the file (EOF) is reached.
			c1 = fgetc(fp); // get the first character of the line
			while (!(c1 == 'v' || c1 == 'f')) {
				c1 = fgetc(fp);
				if (feof(fp))
					break;
			}
			// c2 == 'v' or c2 == 'f'
			c2 = fgetc(fp);
			if ((c1 == 'v') && (c2 == ' ')) {
				// scan the first 3 floating number and scan it to x,y and z.
                // scan the last 3 floating number and scan it to r,g and b.
				int ret = fscanf(fp, "%f %f %f %f %f %f", &x, &y, &z, &r, &g, &b);
				objectVertices.push_back(vec3(x, y, z));
                if (ret > 3) 
                    objectColors.push_back(vec3(r, g, b));
                else 
                    objectColors.push_back(vec3(0.5, 0.5, 0.5));
				if (y < minY) minY = y;
				if (z < minZ) minZ = z;
				if (y > maxY) maxY = y;
				if (z > maxZ) maxZ = z;
			}
			else if ((c1 == 'v') && (c2 == 'n')) {
				// the normal information 
				fscanf(fp, "%f %f %f", &x, &y, &z);
				objectNormals.push_back(glm::normalize(vec3(x, y, z)));
			}
			else if (c1 == 'f') {
				// the face information, depends on the format of the obj file.
				int ret = fscanf(fp, "%d/%d/%d %d/%d/%d %d/%d/%d %d/%d/%d", &fx, &uvx, &nx, &fy, &uvy, &ny, &fz, &uvz, &nz, &fw, &uvw, &nw);
                if (ret == 3)
                {
                    objectIndices.push_back(fx - 1);
                    objectIndices.push_back(fy - 1);
                    objectIndices.push_back(fz - 1);
                }
                else if (ret == 4)
                {
                    objectIndices.push_back(fx - 1);
                    objectIndices.push_back(fy - 1);
                    objectIndices.push_back(fz - 1);
                    objectIndices.push_back(fx - 1);
                    objectIndices.push_back(fz - 1);
                    objectIndices.push_back(fw - 1);
                }
                else if (ret == 6)
                {
                    objectIndices.push_back(fx - 1);
                    objectIndices.push_back(fy - 1);
                    objectIndices.push_back(fz - 1);
                    // TODO::Dicky check input
                    uvIndices.push_back(uvx - 1);
                    uvIndices.push_back(uvy - 1);
                    uvIndices.push_back(uvz - 1);
                }
                else if (ret == 8)
                {
                    objectIndices.push_back(fx - 1);
                    objectIndices.push_back(fy - 1);
                    objectIndices.push_back(fz - 1);
                    objectIndices.push_back(fx - 1);
                    objectIndices.push_back(fz - 1);
                    objectIndices.push_back(fw - 1);
                    // TODO::Dicky check input
                    uvIndices.push_back(uvx - 1);
                    uvIndices.push_back(uvy - 1);
                    uvIndices.push_back(uvz - 1);
                    uvIndices.push_back(uvx - 1);
                    uvIndices.push_back(uvz - 1);
                    uvIndices.push_back(uvw - 1);
                }
                else if (ret == 9)
                {
                    objectIndices.push_back(fx - 1);
                    objectIndices.push_back(fy - 1);
                    objectIndices.push_back(fz - 1);
                    uvIndices.push_back(uvx - 1);
                    uvIndices.push_back(uvy - 1);
                    uvIndices.push_back(uvz - 1);
                    normalIndices.push_back(nx - 1);
                    normalIndices.push_back(ny - 1);
                    normalIndices.push_back(nz - 1);
                }
                else if (ret == 12)
                {
                    objectIndices.push_back(fx - 1);
                    objectIndices.push_back(fy - 1);
                    objectIndices.push_back(fz - 1);
                    objectIndices.push_back(fx - 1);
                    objectIndices.push_back(fz - 1);
                    objectIndices.push_back(fw - 1);
                    uvIndices.push_back(uvx - 1);
                    uvIndices.push_back(uvy - 1);
                    uvIndices.push_back(uvz - 1);
                    uvIndices.push_back(uvx - 1);
                    uvIndices.push_back(uvz - 1);
                    uvIndices.push_back(uvw - 1);
                    normalIndices.push_back(nx - 1);
                    normalIndices.push_back(ny - 1);
                    normalIndices.push_back(nz - 1);
                    normalIndices.push_back(nx - 1);
                    normalIndices.push_back(nz - 1);
                    normalIndices.push_back(nw - 1);
                }
			}
		}
	}
	// mesh extension
	else if (extension == "mesh" || extension == "gmsh")
	{
		int numVertices, numTriangles;
		//int vertexId, faceId;
		float x, y, z;
		char keyword[50]; // for section keywords

		// Skip headers until Vertices
		while (fscanf(fp, "%s", keyword) && strcmp(keyword, "Vertices") != 0) {}

		fscanf(fp, "%d", &numVertices);
		for (int i = 0; i < numVertices; ++i) {
			fscanf(fp, " %f %f %f", &x, &y, &z);
			objectVertices.push_back(vec3(x, y, z));
            objectColors.push_back(vec3(0.5, 0.5, 0.5));
			float normal_x = 0.0;
			float normal_y = 0.0;
			float normal_z = 1.0;
			// here I assume every vertex has normal (0,0,1), the actual normal where be computed after introduced the openmesh structure
			objectNormals.push_back(glm::normalize(vec3(normal_x, normal_y, normal_z)));
			if (y < minY) minY = y;
			if (z < minZ) minZ = z;
			if (y > maxY) maxY = y;
			if (z > maxZ) maxZ = z;
		}

		// Skip sections until Triangles
		while (fscanf(fp, "%s", keyword) && strcmp(keyword, "Triangles") != 0) {
			continue;
		}

		fscanf(fp, "%d", &numTriangles);
		int v1, v2, v3;
		for (int i = 0; i < numTriangles; ++i) {
			fscanf(fp, "%d %d %d", &v1, &v2, &v3);
			objectIndices.push_back(v1 - 1); // Assuming indices in the mesh file are 1-based
			objectIndices.push_back(v2 - 1);
			objectIndices.push_back(v3 - 1);
		}
	}
	else {
		std::cerr << "Unsupported file format: " << extension << std::endl;
		exit(-1);
	}
	fclose(fp); // close the document
	// adjust the vertex position 
	float avgY = (minY + maxY) / 2.0f - 0.02f;
	float avgZ = (minZ + maxZ) / 2.0f;
	for (unsigned int i = 0; i < objectVertices.size(); ++i) {
		vec3 shiftedVertex = (objectVertices[i] - vec3(0.0f, avgY, avgZ)) * vec3(1.58f, 1.58f, 1.58f);
		objectVertices[i] = shiftedVertex;
	}

	glBindVertexArray(vertex_array);

	// Bind vertices to layout location 0
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer );
	glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * objectVertices.size(), &objectVertices[0], GL_STATIC_DRAW);
	glEnableVertexAttribArray(0); // This allows usage of layout location 0 in the vertex shader
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), 0);

	// Bind normals to layout location 1
	glBindBuffer(GL_ARRAY_BUFFER, normal_buffer );
	glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * objectNormals.size(), &objectNormals[0], GL_STATIC_DRAW);
	glEnableVertexAttribArray(1); // This allows usage of layout location 1 in the vertex shader
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), 0);

    // Bind vertices to layout location 2
	glBindBuffer(GL_ARRAY_BUFFER, color_buffer );
	glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * objectColors.size(), &objectColors[0], GL_STATIC_DRAW);
	glEnableVertexAttribArray(2); // This allows usage of layout location 2 in the vertex shader
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), 0);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer );
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * objectIndices.size(), &objectIndices[0], GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void Mesh::parse_image_and_bind(const ImGui::ImMat& image, const ImGui::ImMat& map, const float depth_scale)
{
    float minY = INFINITY, minZ = INFINITY;
	float maxY = -INFINITY, maxZ = -INFINITY;
    if (image.empty() || map.empty())
        return;
    float w_scale = (float)image.w / (float)map.w;
    float h_scale = (float)image.h / (float)map.h;
    std::vector<ImPoint3D> faces;
    for (int y = 0; y < map.h - 2; y++)
    {
        for (int x = 1; x < map.w - 2; x++)
        {
            ImPoint3D face1 = ImPoint3D((y + 1) * map.w + x, (y + 2) * map.w + x, (y + 1) * map.w + x + 1);
            faces.push_back(face1);
            ImPoint3D face2 = ImPoint3D((y + 1) * map.w + x + 1, (y + 2) * map.w + x, (y + 2) * map.w + x + 1);
            faces.push_back(face2);
        }
    }
    std::vector<std::pair<ImPoint3D, ImPoint3D>> vertices;
    std::vector<ImPoint3D> normals;
    for (int y = 0; y < map.h; y++)
    {
        for (int x = 0; x < map.w; x++)
        {
            float flen = std::min(map.w / 2, map.h / 2);
            flen = sqrt(flen*flen*2);
            float _x = (float)x - map.w / 2;
            float _y = (float)(map.h - y) - map.h / 2;
            float _z = map.at<float>(x,y) * depth_scale;
            ImPixel pixel = image.get_pixel(x * w_scale, y * h_scale);
            std::pair<ImPoint3D, ImPoint3D> vertice;
            vertice.first = ImPoint3D(_x / flen, _y / flen, _z);
            vertice.second = ImPoint3D(pixel.r, pixel.g, pixel.b);
            vertices.push_back(vertice);
            normals.push_back(ImPoint3D(0,0,0));
        }
    }
    for (auto face : faces)
    {
        auto v1 = vertices[(int)face.x].first;
        auto v2 = vertices[(int)face.y].first;
        auto v3 = vertices[(int)face.z].first;
        auto v11 = v2 - v1, v12 = v3 - v1;
        auto v21 = v3 - v2, v22 = v1 - v2;
        auto v31 = v1 - v3, v32 = v2 - v3;
        auto vn1 = v11.cross(v12);
        auto vn2 = v21.cross(v22);
        auto vn3 = v31.cross(v32);
        normals[(int)face.x] += vn1;
        normals[(int)face.y] += vn2;
        normals[(int)face.z] += vn3;
        normals[(int)face.x] = normals[(int)face.x].normalize();
        normals[(int)face.y] = normals[(int)face.y].normalize();
        normals[(int)face.z] = normals[(int)face.z].normalize();
    }
    for (auto vertice : vertices)
    {
        objectVertices.push_back(vec3(vertice.first.x, vertice.first.y, vertice.first.z));
        if (vertice.first.y < minY) minY = vertice.first.y;
		if (vertice.first.z < minZ) minZ = vertice.first.z;
		if (vertice.first.y > maxY) maxY = vertice.first.y;
		if (vertice.first.z > maxZ) maxZ = vertice.first.z;
        objectColors.push_back(vec3(vertice.second.x, vertice.second.y, vertice.second.z));
    }
    for (auto normal : normals)
    {
        objectNormals.push_back(vec3(normal.x, normal.y, normal.z));
    }
    for (auto face : faces)
    {
        objectIndices.push_back((int)face.x);
        objectIndices.push_back((int)face.y);
        objectIndices.push_back((int)face.z);
    }

    // adjust the vertex position 
	float avgY = (minY + maxY) / 2.0f - 0.02f;
	float avgZ = (minZ + maxZ) / 2.0f;
	for (unsigned int i = 0; i < objectVertices.size(); ++i) {
		vec3 shiftedVertex = (objectVertices[i] - vec3(0.0f, avgY, avgZ)) * vec3(1.58f, 1.58f, 1.58f);
		objectVertices[i] = shiftedVertex;
	}

	glBindVertexArray(vertex_array);

	// Bind vertices to layout location 0
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer );
	glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * objectVertices.size(), &objectVertices[0], GL_STATIC_DRAW);
	glEnableVertexAttribArray(0); // This allows usage of layout location 0 in the vertex shader
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), 0);

	// Bind normals to layout location 1
	glBindBuffer(GL_ARRAY_BUFFER, normal_buffer );
	glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * objectNormals.size(), &objectNormals[0], GL_STATIC_DRAW);
	glEnableVertexAttribArray(1); // This allows usage of layout location 1 in the vertex shader
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), 0);

    // Bind vertices to layout location 2
	glBindBuffer(GL_ARRAY_BUFFER, color_buffer );
	glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * objectColors.size(), &objectColors[0], GL_STATIC_DRAW);
	glEnableVertexAttribArray(2); // This allows usage of layout location 2 in the vertex shader
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), 0);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer );
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * objectIndices.size(), &objectIndices[0], GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void Mesh::display(float& ambient_slider, float& diffuse_slider, float& specular_slider, float& shininess_slider, bool custom_color, float& light_position, float& light_color, float scale, float x_arg, float y_arg, float z_arg)
{
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);    
    modelview = glm::lookAt(eye,center,up); 
    glUniformMatrix4fv(modelviewPos, 1, GL_FALSE, &modelview[0][0]);

    glUniform4fv(lightpos, MAX_LIGHTS, &light_position);
    glUniform4fv(lightcol, MAX_LIGHTS, &light_color);  

    // Transformations for objects, involving translation and scaling 
    mat4 transf(1.0);
    transf = glm::translate(transf, glm::vec3(tx, ty, 0.0f));
    transf = glm::scale(transf, glm::vec3(scale, scale, scale)); 
    transf = glm::rotate(transf, x_arg, glm::vec3(1.0, 0.0, 0.0));
    transf = glm::rotate(transf, y_arg, glm::vec3(0.0, 1.0, 0.0));
    transf = glm::rotate(transf, z_arg, glm::vec3(0.0, 0.0, 1.0));
    modelview = modelview * transf;

    if (!custom_color){
        *(&ambient_slider + 1) = ambient_slider;
        *(&ambient_slider + 2) = ambient_slider; 

        *(&diffuse_slider + 1) = diffuse_slider;
        *(&diffuse_slider + 2) = diffuse_slider; 

        *(&specular_slider + 1) = specular_slider;
        *(&specular_slider + 2) = specular_slider; 
    }

    glUniform4fv(ambientcol, 1, &ambient_slider);
    glUniform4fv(diffusecol, 1, &diffuse_slider);
    glUniform4fv(specularcol, 1, &specular_slider);
    glUniform1f(shininesscol, shininess_slider);

    const GLfloat white[4] = {1, 1, 1, 1};
    const GLfloat black[4] = {0, 0, 0, 0};

    glUniformMatrix4fv(modelviewPos, 1, GL_FALSE, &(modelview * glm::scale(mat4(1.0f), vec3(2, 2, 2)))[0][0]);
    bind();
    if (render_mode == 0){
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDrawElements(GL_TRIANGLES, objectIndices.size(), GL_UNSIGNED_INT, 0);
    }
    if (render_mode == 1){
        glLineWidth(1); 
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDrawElements(GL_TRIANGLES, objectIndices.size(), GL_UNSIGNED_INT, 0);
    } 
    if (render_mode == 2){
        glPointSize(2.5); 
        glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
        glDrawElements(GL_TRIANGLES, objectIndices.size(), GL_UNSIGNED_INT, 0);
    } 
    if (render_mode == 3){
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDrawElements(GL_TRIANGLES, objectIndices.size(), GL_UNSIGNED_INT, 0);
        glUniform4fv(diffusecol, 1, black);
        glUniform4fv(specularcol, 1, white);

        glPointSize(2.5); 
        glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
        glDrawElements(GL_TRIANGLES, objectIndices.size(), GL_UNSIGNED_INT, 0);

        glLineWidth(2.5); 
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDrawElements(GL_TRIANGLES, objectIndices.size(), GL_UNSIGNED_INT, 0);
    } 
    glBindVertexArray(0);
}
}
