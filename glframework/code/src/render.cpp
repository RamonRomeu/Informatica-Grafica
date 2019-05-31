#include <GL\glew.h>
#include <glm\gtc\type_ptr.hpp>
#include <glm\gtc\matrix_transform.hpp>
#include <cstdio>
#include <cassert>
#include <string>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

#include <imgui\imgui.h>
#include <imgui\imgui_impl_sdl_gl3.h>

#include "GL_framework.h"

glm::vec3 moonPos{0, 20, 20};
glm::vec3 moonCol{0.53, 0.68, 0.92};
glm::vec3 bulbPos{ 0 };
glm::vec3 bulbCol{ 1, 1, 0 };

///////// fw decl
namespace ImGui {
	void Render();
}
namespace Axis {
void setupAxis();
void cleanupAxis();
void drawAxis();
}
////////////////

namespace RenderVars {
	const float FOV = glm::radians(65.f);
	const float zNear = 0.1f;
	const float zFar = 50.f;

	glm::mat4 _projection;
	glm::mat4 _modelView;
	glm::mat4 _MVP;
	glm::mat4 _inv_modelview;
	glm::vec4 _cameraPoint;

	struct prevMouse {
		float lastx, lasty;
		MouseEvent::Button button = MouseEvent::Button::None;
		bool waspressed = false;
	} prevMouse;

	float panv[3] = { 0.f, -5.f, -15.f };
	float rota[2] = { 0.f, 0.f };
}
namespace RV = RenderVars;

void GLResize(int width, int height) {
	glViewport(0, 0, width, height);
	if(height != 0) RV::_projection = glm::perspective(RV::FOV, (float)width / (float)height, RV::zNear, RV::zFar);
	else RV::_projection = glm::perspective(RV::FOV, 0.f, RV::zNear, RV::zFar);
}

void GLmousecb(MouseEvent ev) {
	if(RV::prevMouse.waspressed && RV::prevMouse.button == ev.button) {
		float diffx = ev.posx - RV::prevMouse.lastx;
		float diffy = ev.posy - RV::prevMouse.lasty;
		switch(ev.button) {
		case MouseEvent::Button::Left: // ROTATE
			RV::rota[0] += diffx * 0.005f;
			RV::rota[1] += diffy * 0.005f;
			break;
		case MouseEvent::Button::Right: // MOVE XY
			RV::panv[0] += diffx * 0.03f;
			RV::panv[1] -= diffy * 0.03f;
			break;
		case MouseEvent::Button::Middle: // MOVE Z
			RV::panv[2] += diffy * 0.05f;
			break;
		default: break;
		}
	} else {
		RV::prevMouse.button = ev.button;
		RV::prevMouse.waspressed = true;
	}
	RV::prevMouse.lastx = ev.posx;
	RV::prevMouse.lasty = ev.posy;
}

//////////////////////////////////////////////////

void readShader(std::string file, std::string &buffer) {
	std::ifstream myFile(file);
	if (myFile.is_open()) {
		buffer.clear();
		std::string aux;
		while (getline(myFile, aux)) {
			buffer.append(aux);
			buffer.append("\n");
			aux.clear();
		}
		myFile.close();
	}
	else { std::cout << "Unable to read " << file << " file" << std::endl; }
}

GLuint compileShader(const char* shaderStr, GLenum shaderType, const char* name="") {
	GLuint shader = glCreateShader(shaderType);
	glShaderSource(shader, 1, &shaderStr, NULL);
	glCompileShader(shader);
	GLint res;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &res);
	if (res == GL_FALSE) {
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &res);
		char *buff = new char[res];
		glGetShaderInfoLog(shader, res, &res, buff);
		fprintf(stderr, "Error Shader %s: %s", name, buff);
		delete[] buff;
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}
void linkProgram(GLuint program) {
	glLinkProgram(program);
	GLint res;
	glGetProgramiv(program, GL_LINK_STATUS, &res);
	if (res == GL_FALSE) {
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &res);
		char *buff = new char[res];
		glGetProgramInfoLog(program, res, &res, buff);
		fprintf(stderr, "Error Link: %s", buff);
		delete[] buff;
	}
}

bool loadOBJ(const char * path,
	std::vector < glm::vec3 > & out_vertices,
	std::vector < glm::vec2 > & out_uvs,
	std::vector < glm::vec3 > & out_normals
) {
	std::vector< unsigned int > vertexIndices, uvIndices, normalIndices;
	std::vector< glm::vec3 > temp_vertices;
	std::vector< glm::vec2 > temp_uvs;
	std::vector< glm::vec3 > temp_normals;

	FILE * file = fopen(path, "r");
	if (file == NULL) {
		printf("Impossible to open the file !\n");
		return false;
	}

	while (1) {

		char lineHeader[128];
		// read the first word of the line
		int res = fscanf(file, "%s", lineHeader);
		if (res == EOF)
			break; // EOF = End Of File. Quit the loop.

		if (strcmp(lineHeader, "v") == 0) {
			glm::vec3 vertex;
			fscanf(file, "%f %f %f\n", &vertex.x, &vertex.y, &vertex.z);
			temp_vertices.push_back(vertex);
		}
		else if (strcmp(lineHeader, "vt") == 0) {
			glm::vec2 uv;
			fscanf(file, "%f %f\n", &uv.x, &uv.y);
			temp_uvs.push_back(uv);
		}
		else if (strcmp(lineHeader, "vn") == 0) {
			glm::vec3 normal;
			fscanf(file, "%f %f %f\n", &normal.x, &normal.y, &normal.z);
			temp_normals.push_back(normal);
		}
		else if (strcmp(lineHeader, "f") == 0) {
			std::string vertex1, vertex2, vertex3;
			unsigned int vertexIndex[3], uvIndex[3], normalIndex[3];
			int matches = fscanf(file, "%d/%d/%d %d/%d/%d %d/%d/%d\n", &vertexIndex[0], &uvIndex[0], &normalIndex[0], &vertexIndex[1], &uvIndex[1], &normalIndex[1], &vertexIndex[2], &uvIndex[2], &normalIndex[2]);
			if (matches != 9) {
				printf("File can't be read by our simple parser : ( Try exporting with other options\n");
				return false;
			}
			vertexIndices.push_back(vertexIndex[0]);
			vertexIndices.push_back(vertexIndex[1]);
			vertexIndices.push_back(vertexIndex[2]);
			uvIndices.push_back(uvIndex[0]);
			uvIndices.push_back(uvIndex[1]);
			uvIndices.push_back(uvIndex[2]);
			normalIndices.push_back(normalIndex[0]);
			normalIndices.push_back(normalIndex[1]);
			normalIndices.push_back(normalIndex[2]);
		}






	}

	// For each vertex of each triangle

	for (unsigned int i = 0; i < vertexIndices.size(); i++) {
		unsigned int vertexIndex = vertexIndices[i];
		glm::vec3 vertex = temp_vertices[vertexIndex - 1];
		out_vertices.push_back(vertex);
		unsigned int uvindex = uvIndices[i];
		glm::vec2 uv = temp_uvs[uvindex - 1];
		out_uvs.push_back(uv);
		unsigned int normalIndex = normalIndices[i];
		glm::vec3 normal = temp_normals[normalIndex - 1];
		out_normals.push_back(normal);
	}



	return true;
}

struct Model {
	std::string fileName;
	GLuint objectVao;
	GLuint objectVbo[2];

	void modelSetup(std::string n) {
		fileName = n;
		std::vector<glm::vec3> verts, norms;
		std::vector<glm::vec2> uvs;
		loadOBJ(fileName.c_str(), verts, uvs, norms);

		glGenVertexArrays(1, &objectVao);
		glBindVertexArray(objectVao);
		glGenBuffers(2, objectVbo);

		glBindBuffer(GL_ARRAY_BUFFER, objectVbo[0]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * verts.size(), verts.data(), GL_STATIC_DRAW);///////////
		glVertexAttribPointer((GLuint)0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(0);

		glBindBuffer(GL_ARRAY_BUFFER, objectVbo[1]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * norms.size(), norms.data(), GL_STATIC_DRAW);////////////////
		glVertexAttribPointer((GLuint)1, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(1);

		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}

	void cleanupModel() {
		glDeleteBuffers(2, objectVbo);
		glDeleteVertexArrays(1, &objectVao);
	}
};

////////////////////////////////////////////////// AXIS
namespace Axis {
GLuint AxisVao;
GLuint AxisVbo[3];
GLuint AxisShader[2];
GLuint AxisProgram;

float AxisVerts[] = {
	0.0, 0.0, 0.0,
	1.0, 0.0, 0.0,
	0.0, 0.0, 0.0,
	0.0, 1.0, 0.0,
	0.0, 0.0, 0.0,
	0.0, 0.0, 1.0
};
float AxisColors[] = {
	1.0, 0.0, 0.0, 1.0,
	1.0, 0.0, 0.0, 1.0,
	0.0, 1.0, 0.0, 1.0,
	0.0, 1.0, 0.0, 1.0,
	0.0, 0.0, 1.0, 1.0,
	0.0, 0.0, 1.0, 1.0
};
GLubyte AxisIdx[] = {
	0, 1,
	2, 3,
	4, 5
};
const char* Axis_vertShader =
"#version 330\n\
in vec3 in_Position;\n\
in vec4 in_Color;\n\
out vec4 vert_color;\n\
uniform mat4 mvpMat;\n\
void main() {\n\
	vert_color = in_Color;\n\
	gl_Position = mvpMat * vec4(in_Position, 1.0);\n\
}";
const char* Axis_fragShader =
"#version 330\n\
in vec4 vert_color;\n\
out vec4 out_Color;\n\
void main() {\n\
	out_Color = vert_color;\n\
}";

void setupAxis() {
	glGenVertexArrays(1, &AxisVao);
	glBindVertexArray(AxisVao);
	glGenBuffers(3, AxisVbo);

	glBindBuffer(GL_ARRAY_BUFFER, AxisVbo[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 24, AxisVerts, GL_STATIC_DRAW);
	glVertexAttribPointer((GLuint)0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, AxisVbo[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 24, AxisColors, GL_STATIC_DRAW);
	glVertexAttribPointer((GLuint)1, 4, GL_FLOAT, false, 0, 0);
	glEnableVertexAttribArray(1);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, AxisVbo[2]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLubyte) * 6, AxisIdx, GL_STATIC_DRAW);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	AxisShader[0] = compileShader(Axis_vertShader, GL_VERTEX_SHADER, "AxisVert");
	AxisShader[1] = compileShader(Axis_fragShader, GL_FRAGMENT_SHADER, "AxisFrag");

	AxisProgram = glCreateProgram();
	glAttachShader(AxisProgram, AxisShader[0]);
	glAttachShader(AxisProgram, AxisShader[1]);
	glBindAttribLocation(AxisProgram, 0, "in_Position");
	glBindAttribLocation(AxisProgram, 1, "in_Color");
	linkProgram(AxisProgram);
}
void cleanupAxis() {
	glDeleteBuffers(3, AxisVbo);
	glDeleteVertexArrays(1, &AxisVao);

	glDeleteProgram(AxisProgram);
	glDeleteShader(AxisShader[0]);
	glDeleteShader(AxisShader[1]);
}
void drawAxis() {
	glBindVertexArray(AxisVao);
	glUseProgram(AxisProgram);
	glUniformMatrix4fv(glGetUniformLocation(AxisProgram, "mvpMat"), 1, GL_FALSE, glm::value_ptr(RV::_MVP));
	glDrawElements(GL_LINES, 6, GL_UNSIGNED_BYTE, 0);

	glUseProgram(0);
	glBindVertexArray(0);
}
}

////////////////////////////////////////////////// CUBE
namespace Cube {
GLuint cubeVao;
GLuint cubeVbo[3];
GLuint cubeShaders[2];
GLuint cubeProgram;
glm::mat4 objMat = glm::mat4(1.f);

extern const float halfW = 0.5f;
int numVerts = 24 + 6; // 4 vertex/face * 6 faces + 6 PRIMITIVE RESTART

					   //   4---------7
					   //  /|        /|
					   // / |       / |
					   //5---------6  |
					   //|  0------|--3
					   //| /       | /
					   //|/        |/
					   //1---------2
glm::vec3 verts[] = {
	glm::vec3(-halfW, -halfW, -halfW),
	glm::vec3(-halfW, -halfW,  halfW),
	glm::vec3(halfW, -halfW,  halfW),
	glm::vec3(halfW, -halfW, -halfW),
	glm::vec3(-halfW,  halfW, -halfW),
	glm::vec3(-halfW,  halfW,  halfW),
	glm::vec3(halfW,  halfW,  halfW),
	glm::vec3(halfW,  halfW, -halfW)
};
glm::vec3 norms[] = {
	glm::vec3(0.f, -1.f,  0.f),
	glm::vec3(0.f,  1.f,  0.f),
	glm::vec3(-1.f,  0.f,  0.f),
	glm::vec3(1.f,  0.f,  0.f),
	glm::vec3(0.f,  0.f, -1.f),
	glm::vec3(0.f,  0.f,  1.f)
};

glm::vec3 cubeVerts[] = {
	verts[1], verts[0], verts[2], verts[3],
	verts[5], verts[6], verts[4], verts[7],
	verts[1], verts[5], verts[0], verts[4],
	verts[2], verts[3], verts[6], verts[7],
	verts[0], verts[4], verts[3], verts[7],
	verts[1], verts[2], verts[5], verts[6]
};
glm::vec3 cubeNorms[] = {
	norms[0], norms[0], norms[0], norms[0],
	norms[1], norms[1], norms[1], norms[1],
	norms[2], norms[2], norms[2], norms[2],
	norms[3], norms[3], norms[3], norms[3],
	norms[4], norms[4], norms[4], norms[4],
	norms[5], norms[5], norms[5], norms[5]
};
GLubyte cubeIdx[] = {
	0, 1, 2, 3, UCHAR_MAX,
	4, 5, 6, 7, UCHAR_MAX,
	8, 9, 10, 11, UCHAR_MAX,
	12, 13, 14, 15, UCHAR_MAX,
	16, 17, 18, 19, UCHAR_MAX,
	20, 21, 22, 23, UCHAR_MAX
};

const char* cube_vertShader =
"#version 330\n\
in vec3 in_Position;\n\
in vec3 in_Normal;\n\
out vec4 vert_Normal;\n\
uniform mat4 objMat;\n\
uniform mat4 mv_Mat;\n\
uniform mat4 mvpMat;\n\
void main() {\n\
	gl_Position = mvpMat * objMat * vec4(in_Position, 1.0);\n\
	vert_Normal = mv_Mat * objMat * vec4(in_Normal, 0.0);\n\
}";
const char* cube_fragShader =
"#version 330\n\
in vec4 vert_Normal;\n\
out vec4 out_Color;\n\
uniform mat4 mv_Mat;\n\
uniform vec4 color;\n\
void main() {\n\
	out_Color = vec4(color.xyz * dot(vert_Normal, mv_Mat*vec4(0.0, 1.0, 0.0, 0.0)) + color.xyz * 0.3, 1.0 );\n\
}";
void setupCube() {
	glGenVertexArrays(1, &cubeVao);
	glBindVertexArray(cubeVao);
	glGenBuffers(3, cubeVbo);

	glBindBuffer(GL_ARRAY_BUFFER, cubeVbo[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVerts), cubeVerts, GL_STATIC_DRAW);
	glVertexAttribPointer((GLuint)0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, cubeVbo[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cubeNorms), cubeNorms, GL_STATIC_DRAW);
	glVertexAttribPointer((GLuint)1, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(1);

	glPrimitiveRestartIndex(UCHAR_MAX);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeVbo[2]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIdx), cubeIdx, GL_STATIC_DRAW);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	cubeShaders[0] = compileShader(cube_vertShader, GL_VERTEX_SHADER, "cubeVert");
	cubeShaders[1] = compileShader(cube_fragShader, GL_FRAGMENT_SHADER, "cubeFrag");

	cubeProgram = glCreateProgram();
	glAttachShader(cubeProgram, cubeShaders[0]);
	glAttachShader(cubeProgram, cubeShaders[1]);
	glBindAttribLocation(cubeProgram, 0, "in_Position");
	glBindAttribLocation(cubeProgram, 1, "in_Normal");
	linkProgram(cubeProgram);
}
void cleanupCube() {
	glDeleteBuffers(3, cubeVbo);
	glDeleteVertexArrays(1, &cubeVao);

	glDeleteProgram(cubeProgram);
	glDeleteShader(cubeShaders[0]);
	glDeleteShader(cubeShaders[1]);
}
void updateCube(const glm::mat4& transform) {
	objMat = transform;
}
void drawCube() {
	glEnable(GL_PRIMITIVE_RESTART);
	glBindVertexArray(cubeVao);
	glUseProgram(cubeProgram);
	glUniformMatrix4fv(glGetUniformLocation(cubeProgram, "objMat"), 1, GL_FALSE, glm::value_ptr(objMat));
	glUniformMatrix4fv(glGetUniformLocation(cubeProgram, "mv_Mat"), 1, GL_FALSE, glm::value_ptr(RenderVars::_modelView));
	glUniformMatrix4fv(glGetUniformLocation(cubeProgram, "mvpMat"), 1, GL_FALSE, glm::value_ptr(RenderVars::_MVP));
	glUniform4f(glGetUniformLocation(cubeProgram, "color"), 0.1f, 1.f, 1.f, 0.f);
	glDrawElements(GL_TRIANGLE_STRIP, numVerts, GL_UNSIGNED_BYTE, 0);

	glUseProgram(0);
	glBindVertexArray(0);
	glDisable(GL_PRIMITIVE_RESTART);
}
}

namespace Object {
	GLuint objectShaders[2];
	GLuint objectProgram;
	glm::mat4 objMat = glm::mat4(1.f);
	float k_amb = 0.f;
	float k_dif = 0.f;
	float k_spe = 0.f;
	float light_pos[3] = { 5.f,10.f,0.f };

	int spec_pow;
	glm::vec3 light_col;

	void setupObject() {
		k_amb = k_dif = .5f;
		k_spe = 1.f;
		spec_pow = 30;
		light_col = { 1.f, 1.f, 1.f };

		std::string object_vertShader;
		readShader("object_vertShader.txt", object_vertShader);
		objectShaders[0] = compileShader(object_vertShader.c_str(), GL_VERTEX_SHADER, "objectVert");

		std::string object_fragShader;
		readShader("object_fragShader.txt", object_fragShader);
		objectShaders[1] = compileShader(object_fragShader.c_str(), GL_FRAGMENT_SHADER, "objectFrag");

		objectProgram = glCreateProgram();
		glAttachShader(objectProgram, objectShaders[0]);
		glAttachShader(objectProgram, objectShaders[1]);
		glBindAttribLocation(objectProgram, 0, "in_Position");
		glBindAttribLocation(objectProgram, 1, "in_Normal");
		linkProgram(objectProgram);
	}
	void cleanupObject() {
		glDeleteProgram(objectProgram);
		glDeleteShader(objectShaders[0]);
		glDeleteShader(objectShaders[1]);
	}
	void updateObject(const glm::mat4& transform) {
		objMat = transform;
	}
	void drawObject(Model m) {
		glBindVertexArray(m.objectVao);
		glUseProgram(objectProgram);

		glUniformMatrix4fv(glGetUniformLocation(objectProgram, "objMat"), 1, GL_FALSE, glm::value_ptr(objMat));
		glUniformMatrix4fv(glGetUniformLocation(objectProgram, "mv_Mat"), 1, GL_FALSE, glm::value_ptr(RenderVars::_modelView));
		glUniformMatrix4fv(glGetUniformLocation(objectProgram, "mvpMat"), 1, GL_FALSE, glm::value_ptr(RenderVars::_MVP));
		glUniform3f(glGetUniformLocation(objectProgram, "color"), 0.4, 0.4, 0.4);

		glUniform1f(glGetUniformLocation(objectProgram, "k_amb"), k_amb);
		glUniform1f(glGetUniformLocation(objectProgram, "k_dif"), k_dif);
		glUniform1f(glGetUniformLocation(objectProgram, "k_spe"), k_spe);
		glUniform1i(glGetUniformLocation(objectProgram, "spec_pow"), spec_pow);
		glUniform3f(glGetUniformLocation(objectProgram, "moon_pos"), moonPos[0], moonPos[1], moonPos[2]);
		glUniform3f(glGetUniformLocation(objectProgram, "moon_col"), moonCol[0], moonCol[1], moonCol[2]);
		glUniform3f(glGetUniformLocation(objectProgram, "bulb_pos"), bulbPos[0], bulbPos[1], bulbPos[2]);
		glUniform3f(glGetUniformLocation(objectProgram, "bulb_col"), bulbCol[0], bulbCol[1], bulbCol[2]);
		glUniform3f(glGetUniformLocation(objectProgram, "ambient_col"), 0.1f, 0.1f, 0.1f);
		glUniform3f(glGetUniformLocation(objectProgram, "camera_pos"), RV::_cameraPoint.x, RV::_cameraPoint.y, RV::_cameraPoint.z);


		glDrawArrays(GL_TRIANGLES, 0, 14000);

		glUseProgram(0);
		glBindVertexArray(0);
	}
}

/////////////////////////////////////////////////
Model wheel, cabin, support, trump, chicken;

void GLinit(int width, int height) {
	glViewport(0, 0, width, height);
	glClearColor(0.2f, 0.2f, 0.2f, 1.f);
	glClearDepth(1.f);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	RV::_projection = glm::perspective(RV::FOV, (float)width / (float)height, RV::zNear, RV::zFar);

	// Setup shaders & geometry
	Axis::setupAxis();


	/////////////////////////////////////////////////////TODO
	// Do your init code here
	// ...
	Cube::setupCube();
	Object::setupObject();
	wheel.modelSetup("Wheel.obj");
	cabin.modelSetup("Cabin.obj");
	support.modelSetup("Support.obj");
	trump.modelSetup("Trump.obj");
	chicken.modelSetup("Chicken.obj");
	// ...
	// ...
	/////////////////////////////////////////////////////////
}

void GLcleanup() {
	Axis::cleanupAxis();

	/////////////////////////////////////////////////////TODO
	// Do your cleanup code here
	// ...
	Object::cleanupObject();
	wheel.cleanupModel();
	cabin.cleanupModel();
	support.cleanupModel();
	trump.cleanupModel();
	chicken.cleanupModel();
	// ...
	// ...
	/////////////////////////////////////////////////////////
}

float timer = 0.f;
float cameraTimer = -2.f;
float bulbTimer = 0.f;
int bulbMul = 90;
float RotationVelocity = 5.f;
bool playSimulation = true;

float xOffset = 0.f;
float zOffset = -0.15f;
float yOffset = 0.f;

void GLrender(float dt) {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	RV::_modelView = glm::mat4(1.f);
	RV::_modelView = glm::translate(RV::_modelView, glm::vec3(RV::panv[0], RV::panv[1], RV::panv[2]));
	RV::_modelView = glm::rotate(RV::_modelView, RV::rota[1], glm::vec3(1.f, 0.f, 0.f));
	RV::_modelView = glm::rotate(RV::_modelView, RV::rota[0], glm::vec3(0.f, 1.f, 0.f));

	RV::_MVP = RV::_projection * RV::_modelView;
	if (playSimulation) {
		timer += dt;
		cameraTimer += dt;
		if (cameraTimer > 2.f)
			cameraTimer = 0.f;
		bulbTimer += dt * bulbMul;
		if (glm::abs(bulbTimer) > 45)
			bulbMul *= -1;
	}

	bulbPos = { glm::cos(glm::radians(timer * RotationVelocity)) * 7.1  + glm::sin(glm::radians(bulbTimer)) * 0.2, glm::sin(glm::radians(timer * RotationVelocity)) * 7.1 - glm::cos(glm::radians(bulbTimer)) * 0.2, 0 - 0.15f };

	glm::vec3 trumpPos(0, glm::sin(glm::radians(timer * RotationVelocity)) * 7.1 - 0.84, glm::cos(glm::radians(timer * RotationVelocity)) * 7.1 - 0.25);
	glm::vec3 chickenPos(0, glm::sin(glm::radians(timer * RotationVelocity)) * 7.1 - 0.41, -(glm::cos(glm::radians(timer * RotationVelocity)) * 7.1 + 0.35));

	glm::mat4 cameraTransform;
	if (cameraTimer < 0.f) {
		cameraTransform = glm::rotate(glm::lookAt(glm::vec3(0, 3.98, 13.24), glm::vec3(0), glm::vec3(0, 1, 0)), glm::radians(30.f), glm::vec3(0, 1, 0));
	}
	else if (cameraTimer < 1.f)
		cameraTransform = glm::lookAt(glm::vec3(trumpPos.z + 0.04, trumpPos.y + 0.65, 0.25), glm::vec3(-chickenPos.z, chickenPos.y + 0.1, chickenPos.x), glm::vec3(0.f, 1.f, 0.f));
	else
		cameraTransform = glm::lookAt(glm::vec3(-chickenPos.z + 0.05, chickenPos.y + 0.18, 0.17), glm::vec3(trumpPos.z, trumpPos.y + 0.43, trumpPos.x), glm::vec3(0.f, 1.f, 0.f));
	
	RV::_modelView = cameraTransform;
	RV::_MVP = RV::_projection * RV::_modelView;

	Axis::drawAxis();
	/////////////////////////////////////////////////////TODO
	// Do your render code here
	// ...

	//Cube::updateCube(translate(glm::scale(glm::mat4(1), glm::vec3(0.5)), bulbPos));
	Cube::updateCube(glm::scale(translate(glm::mat4(1), bulbPos), glm::vec3(0.1)));
	Cube::drawCube();

	Object::drawObject(support);
	
	for (int i = 0; i < 20; i++) {
		glm::vec3 aux(glm::cos(glm::radians(timer * RotationVelocity + 18 * i)) * 7.1, glm::sin(glm::radians(timer * RotationVelocity + 18 * i)) * 7.1, 0);
		Object::updateObject(glm::translate(glm::mat4(1.f), aux));
		Object::drawObject(cabin);
	}

	Object::updateObject(glm::mat4(1.f));
	
	Object::updateObject(glm::translate(glm::rotate(glm::mat4(1.f), glm::radians(90.f), glm::vec3(0, 1, 0)), trumpPos));
	Object::drawObject(trump);

	Object::updateObject(glm::mat4(1.f));
	
	Object::updateObject(glm::translate(glm::rotate(glm::mat4(1.f), glm::radians(-90.f), glm::vec3(0, 1, 0)), chickenPos));
	Object::drawObject(chicken);

	Object::updateObject(glm::mat4(1.f));
	Object::updateObject(glm::rotate(Object::objMat, glm::radians(timer * RotationVelocity), glm::vec3(0, 0, 1)));
	Object::drawObject(wheel);
	Object::updateObject(glm::mat4(1.f));

	
	// ...
	// ...
	/////////////////////////////////////////////////////////

	ImGui::Render();
}

void GUI() {
	bool show = true;
	ImGui::Begin("Physics Parameters", &show, 0);

	{
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::Checkbox("Play Simulation", &playSimulation);
		if (ImGui::Button("Reload Shaders")) {
			Object::cleanupObject();
			Object::setupObject();
		}
		ImGui::DragFloat("Rotation Velocity", &RotationVelocity, 0.1, 0, 100);
		ImGui::DragFloat3("MoonPos", &moonPos.x, 0.05);
		ImGui::DragFloat3("MoonCol", &moonCol.x, 0.05);

		ImGui::DragFloat3("BulbCol", &bulbCol.x, 0.05);

		ImGui::DragFloat("x", &xOffset, 0.01);
		ImGui::DragFloat("z", &zOffset, 0.01);
		ImGui::DragFloat("y", &yOffset, 0.01);
		/////////////////////////////////////////////////////TODO
		// Do your GUI code here....
		// ...
		// ...
		// ...
		/////////////////////////////////////////////////////////
	}
	// .........................

	ImGui::End();

	// Example code -- ImGui test window. Most of the sample code is in ImGui::ShowTestWindow()
	bool show_test_window = false;
	if (show_test_window) {
		ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiSetCond_FirstUseEver);
		ImGui::ShowTestWindow(&show_test_window);
	}
}