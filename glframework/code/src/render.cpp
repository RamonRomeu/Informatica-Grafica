#include <GL\glew.h>
#include <glm\gtc\type_ptr.hpp>
#include <glm\gtc\matrix_transform.hpp>
#include <cstdio>
#include <cassert>
#include <time.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>

#include <imgui\imgui.h>
#include <imgui\imgui_impl_sdl_gl3.h>

#include "GL_framework.h"

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
	const float zNear = 1.f;
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

//////////////////////////////////////////////////////////////////////////
namespace Point {
	GLuint pointVao;
	GLuint pointVbo[20];
	GLuint pointShaders[3];
	GLuint pointProgram;

	glm::vec3 points[20];
	glm::vec3 directions[20];

	int numVerts = 20;

	const char* point_vertShader =
		"#version 330\n\
in vec3 in_Position;\n\
void main() {\n\
	gl_Position = vec4(in_Position, 1);\n\
}";

	const char* point_geomShader =
		"#version 330\n\
layout(points) in;\n\
layout(triangle_strip, max_vertices = 72) out;\n\
out vec4 vert_g_normal;\n\
uniform mat4 mv_Mat;\n\
uniform mat4 mvp;\n\
vec4 vertex[24];\n\
\n\
void createSquare(int index) {\n\
	vec4 face_normal = mv_Mat * vec4(normalize(cross(vertex[index + 1].xyz-vertex[index].xyz, vertex[index + 2].xyz-vertex[index].xyz)), 0);\n\
	for (int i = 0; i < 4; i++) {\n\
		gl_Position = mvp * vertex[index + i];\n\
		vert_g_normal = face_normal;\n\
		EmitVertex();\n\
	}\n\
	EndPrimitive();\n\
}; \n\
\n\
void createHexagon(int v1, int v2, int v3, int v4, int v5, int v6) {\n\
	vec4 face_normal = mv_Mat * vec4(normalize(cross(vertex[v2].xyz-vertex[v1].xyz, vertex[v3].xyz-vertex[v1].xyz)), 0);\n\
\n\
	gl_Position = mvp * vertex[v1];\n\
	vert_g_normal = face_normal;\n\
	EmitVertex();\n\
\n\
	gl_Position = mvp * vertex[v2];\n\
	vert_g_normal = face_normal;\n\
	EmitVertex();\n\
\n\
	gl_Position = mvp * vertex[v3];\n\
	vert_g_normal = face_normal;\n\
	EmitVertex();\n\
\n\
	gl_Position = mvp * vertex[v4];\n\
	vert_g_normal = face_normal;\n\
	EmitVertex();\n\
\n\
	gl_Position = mvp * vertex[v5];\n\
	vert_g_normal = face_normal;\n\
	EmitVertex();\n\
\n\
	gl_Position = mvp * vertex[v6];\n\
	vert_g_normal = face_normal;\n\
	EmitVertex();\n\
\n\
	EndPrimitive();\n\
}\n\
\n\
void main() {\n\
	float a = 1.f;\n\
	float h = 2 * sqrt(2) / 2 * a;\n\
	float cubeDiagonal = sqrt(2 * pow(a, 2));\n\
\n\
	vertex[0] = gl_in[0].gl_Position + vec4(0, h, cubeDiagonal/2, 0);\n\
	vertex[1] = gl_in[0].gl_Position + vec4(cubeDiagonal/2, h, 0, 0);\n\
	vertex[2] = gl_in[0].gl_Position + vec4(-cubeDiagonal/2, h, 0, 0);\n\
	vertex[3] = gl_in[0].gl_Position + vec4(0, h, -cubeDiagonal/2, 0);\n\
\n\
	vertex[4] = gl_in[0].gl_Position + vec4(0, -h, cubeDiagonal/2, 0);\n\
	vertex[5] = gl_in[0].gl_Position + vec4(-cubeDiagonal/2, -h, 0, 0);\n\
	vertex[6] = gl_in[0].gl_Position + vec4(cubeDiagonal/2, -h, 0, 0);\n\
	vertex[7] = gl_in[0].gl_Position + vec4(0, -h, -cubeDiagonal/2, 0);\n\
\n\
	vertex[8] = gl_in[0].gl_Position + vec4(-h, 0, cubeDiagonal/2, 0);\n\
	vertex[9] = gl_in[0].gl_Position + vec4(-h, cubeDiagonal/2, 0, 0);\n\
	vertex[10] = gl_in[0].gl_Position + vec4(-h, -cubeDiagonal/2, 0, 0);\n\
	vertex[11] = gl_in[0].gl_Position + vec4(-h, 0, -cubeDiagonal/2, 0);\n\
\n\
	vertex[12] = gl_in[0].gl_Position + vec4(h, 0, cubeDiagonal/2, 0);\n\
	vertex[13] = gl_in[0].gl_Position + vec4(h, -cubeDiagonal/2, 0, 0);\n\
	vertex[14] = gl_in[0].gl_Position + vec4(h, cubeDiagonal/2, 0, 0);\n\
	vertex[15] = gl_in[0].gl_Position + vec4(h, 0, -cubeDiagonal/2, 0);\n\
\n\
	vertex[16] = gl_in[0].gl_Position + vec4(0, -cubeDiagonal/2, h, 0);\n\
	vertex[17] = gl_in[0].gl_Position + vec4(cubeDiagonal/2, 0, h, 0);\n\
	vertex[18] = gl_in[0].gl_Position + vec4(-cubeDiagonal/2, 0, h, 0);\n\
	vertex[19] = gl_in[0].gl_Position + vec4(0, +cubeDiagonal/2, h, 0);\n\
\n\
	vertex[20] = gl_in[0].gl_Position + vec4(0, -cubeDiagonal/2, -h, 0);\n\
	vertex[21] = gl_in[0].gl_Position + vec4(-cubeDiagonal/2, 0, -h, 0);\n\
	vertex[22] = gl_in[0].gl_Position + vec4(cubeDiagonal/2, 0, -h, 0);\n\
	vertex[23] = gl_in[0].gl_Position + vec4(0, +cubeDiagonal/2, -h, 0);\n\
\n\
	createSquare(0);\n\
	createSquare(4);\n\
	createSquare(8);\n\
	createSquare(12);\n\
	createSquare(16);\n\
	createSquare(20);\n\
\n\
	createHexagon(0, 19, 1, 17, 14, 12);\n\
	createHexagon(0, 2, 19, 9, 18, 8);\n\
	createHexagon(2, 3, 9, 23, 11, 21);\n\
	createHexagon(1, 14, 3, 15, 23, 22);\n\
	createHexagon(17, 16, 12, 4, 13, 6);\n\
	createHexagon(18, 8, 16, 10, 4, 5);\n\
	createHexagon(15, 13, 22, 6, 20, 7);\n\
	createHexagon(11, 21, 10, 20, 5, 7);\n\
}";

	const char* point_fragShader =
		"#version 330\n\
in vec4 vert_g_normal;\n\
out vec4 out_Color;\n\
uniform mat4 mv_Mat;\n\
uniform vec4 color;\n\
void main() {\n\
	out_Color = vec4(color.xyz * dot(vert_g_normal, mv_Mat*vec4(1.0, 0.5, 0.0, 0.0)) + color.xyz * 0.3, 1.0 );\n\
	//out_Color = (mv_Mat * vert_g_normal) * 0.5 + 0.5;\n\
}";

	void setupPoint() {
		for (int i = 0; i < 20; i++) {
			points[i] = glm::vec3(rand() % 1000 / 100.f - 5, rand() % 1000 / 100.f - 5, rand() % 1000 / 100.f - 5);
			directions[i] = glm::normalize(glm::vec3(rand() % 100 / 100.f, rand() % 100 / 100.f, rand() % 100 / 100.f));
		}

		glGenVertexArrays(1, &pointVao);
		glBindVertexArray(pointVao);
		glGenBuffers(1, pointVbo);

		glBindBuffer(GL_ARRAY_BUFFER, pointVbo[0]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(points), points, GL_STATIC_DRAW);
		glVertexAttribPointer((GLuint)0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(0);

		pointShaders[0] = compileShader(point_vertShader, GL_VERTEX_SHADER, "pointVert");
		pointShaders[1] = compileShader(point_geomShader, GL_GEOMETRY_SHADER, "pointGeom");
		pointShaders[2] = compileShader(point_fragShader, GL_FRAGMENT_SHADER, "pointFrag");

		pointProgram = glCreateProgram();
		glAttachShader(pointProgram, pointShaders[0]);
		glAttachShader(pointProgram, pointShaders[1]);
		glAttachShader(pointProgram, pointShaders[2]);
		glBindAttribLocation(pointProgram, 0, "in_Position");
		linkProgram(pointProgram);
	}
	void cleanupPoint() {
		glDeleteVertexArrays(1, &pointVao);

		glDeleteProgram(pointProgram);
		glDeleteShader(pointShaders[0]);
		glDeleteShader(pointShaders[1]);
		glDeleteShader(pointShaders[2]);
	}
	void drawPoint() {//////////////////////////////////////////////////////////////////////////TODO
		glBindVertexArray(pointVao);
		glUseProgram(pointProgram);
		glUniformMatrix4fv(glGetUniformLocation(pointProgram, "mvp"), 1, GL_FALSE, glm::value_ptr(RenderVars::_MVP));
		glUniformMatrix4fv(glGetUniformLocation(pointProgram, "mv_Mat"), 1, GL_FALSE, glm::value_ptr(RenderVars::_modelView));
		glUniform4f(glGetUniformLocation(pointProgram, "color"), 0.1f, 1.f, 1.f, 0.f);
		//glDrawElements(GL_POINTS, numVerts, GL_UNSIGNED_BYTE, 0);
		glDrawArrays(GL_POINTS, 0, numVerts);

		glUseProgram(0);
		glBindVertexArray(0);
	}
	void updatePoints(float dt) {
		static float timer = 0;
		timer += dt;
		
		glBindBuffer(GL_ARRAY_BUFFER, pointVbo[0]);
		float* buff = (float*)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
		for (int i = 0; i < 20; i++) {
			for (int j = 0; j < 3; j++) {
				buff[i * 3 + j] = points[i][j] + glm::sin(timer) * directions[i][j];
			}
		}
		glUnmapBuffer(GL_ARRAY_BUFFER);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
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

	glGenVertexArrays(1, &cubeVao);//
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

/////////////////////////////////////////////////


void GLinit(int width, int height) {
	srand(time(NULL));
	glViewport(0, 0, width, height);
	glClearColor(0.2f, 0.2f, 0.2f, 1.f);
	glClearDepth(1.f);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	//glDisable(GL_CULL_FACE);

	RV::_projection = glm::perspective(RV::FOV, (float)width / (float)height, RV::zNear, RV::zFar);

	// Setup shaders & geometry
	Axis::setupAxis();
	Point::setupPoint();


	/////////////////////////////////////////////////////TODO
	// Do your init code here
	// ...
	// ...
	// ...
	/////////////////////////////////////////////////////////
}

void GLcleanup() {
	Axis::cleanupAxis();
	Point::cleanupPoint();

	/////////////////////////////////////////////////////TODO
	// Do your cleanup code here
	// ...
	// ...
	// ...
	/////////////////////////////////////////////////////////
}

void GLrender(float dt) {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	RV::_modelView = glm::mat4(1.f);
	RV::_modelView = glm::translate(RV::_modelView, glm::vec3(RV::panv[0], RV::panv[1], RV::panv[2]));
	RV::_modelView = glm::rotate(RV::_modelView, RV::rota[1], glm::vec3(1.f, 0.f, 0.f));
	RV::_modelView = glm::rotate(RV::_modelView, RV::rota[0], glm::vec3(0.f, 1.f, 0.f));

	RV::_MVP = RV::_projection * RV::_modelView;

	Axis::drawAxis();

	Point::updatePoints(dt);

	Point::drawPoint();
	/////////////////////////////////////////////////////TODO
	// Do your render code here
	// ...
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