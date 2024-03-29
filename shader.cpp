#include "shader.h"
#include "material.h"
#include "light.h"

void printShaderCompileResult(GLuint id, GLint success, const char* type) {
	if (!success) {
		printf("Error in %s shader!\n", type);
		int length;
		glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);			// Get length of error message
		char* errorMessage = (char *)malloc(length * sizeof(char));	// Allocate memory for error message
		glGetShaderInfoLog(id, length, &length, errorMessage);		// Get error message
		printf("%s\n", errorMessage);					// Print error message
	}
	else {
		printf("%s shader compiled successfully!\n", type);
	}
}

void changeShader(void) {
	static const char * vs[1];
	static const char * fs[1];
	Mesh *mesh = scene->meshes;

	glUseProgram(0);
	glDeleteProgram(player->shader->program);
	player->shader = player->shader->next;
	readShaderFile(player->shader->shaderFiles[0], vs);
	readShaderFile(player->shader->shaderFiles[1], fs);
	player->shader->program = prepareShaderProgram(vs, fs);
	glUseProgram(player->shader->program);
	while (mesh != NULL) {
		prepareMesh(mesh, player->shader->program);
		mesh = mesh->next;
	}
}

GLuint prepareShaderProgram(const char ** vs_src, const char ** fs_src) {
	GLint success = GL_FALSE;
	GLuint shprg = glCreateProgram();

	GLuint vs = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vs, 1, vs_src, NULL);
	glCompileShader(vs);
	glGetShaderiv(vs, GL_COMPILE_STATUS, &success);

	printShaderCompileResult(vs, success, "vertex");
	/*if (!success) printf("Error in vertex shader!\n");
	else printf("Vertex shader compiled successfully!\n");*/

	GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fs, 1, fs_src, NULL);
	glCompileShader(fs);
	glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
	printShaderCompileResult(fs, success, "fragment");
	/*if (!success) printf("Error in fragment shader!\n");
	else printf("Fragment shader compiled successfully!\n");*/

	glAttachShader(shprg, vs);
	glAttachShader(shprg, fs);
	glLinkProgram(shprg);
	GLint isLinked = GL_FALSE;
	glGetProgramiv(shprg, GL_LINK_STATUS, &isLinked);
	if (!isLinked) printf("Link error in shader program!\n");
	else printf("Shader program linked successfully!\n");

	glDetachShader(shprg, vs);
	glDetachShader(shprg, fs);
	glDeleteShader(vs);
	glDeleteShader(fs);

	return shprg;
}

void prepareMesh(Mesh *mesh, GLuint shader) {
	int sizeVerts = mesh->nv * 3 * sizeof(float);
	int sizeCols = mesh->nv * 3 * sizeof(float);
	int sizeTris = mesh->nt * 3 * sizeof(int);
	int sizeUVs = mesh->nv * 2 * sizeof(float);

	// For storage of state and other buffer objects needed for vertex specification
	glGenVertexArrays(1, &mesh->vao);
	glBindVertexArray(mesh->vao);

	// Allocate VBO and load mesh data (vertices and normals)
	glGenBuffers(1, &mesh->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeVerts + sizeCols + sizeUVs, NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeVerts, (void *)mesh->vertices);
	glBufferSubData(GL_ARRAY_BUFFER, sizeVerts, sizeCols, (void *)mesh->vnorms);
	glBufferSubData(GL_ARRAY_BUFFER, sizeVerts + sizeCols, sizeUVs, (void *)mesh->uvs);


	// Allocate index buffer and load mesh indices
	glGenBuffers(1, &mesh->ibo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeTris, (void *)mesh->triangles, GL_STATIC_DRAW);

	// Define the format of the vertex data
	GLint vPos = glGetAttribLocation(shader, "vPos");
	glEnableVertexAttribArray(vPos);
	glVertexAttribPointer(vPos, 3, GL_FLOAT, GL_FALSE, 0, NULL);

	// Define the format of the vertex data 
	GLint vNorm = glGetAttribLocation(shader, "vNorm");
	glEnableVertexAttribArray(vNorm);
	glVertexAttribPointer(vNorm, 3, GL_FLOAT, GL_FALSE, 0, (void *)(mesh->nv * 3 * sizeof(float)));

	GLint vUVs = glGetAttribLocation(shader, "vUVs");
	glEnableVertexAttribArray(vUVs);
	glVertexAttribPointer(vUVs, 2, GL_FLOAT, GL_FALSE, 0, (void *)(sizeVerts + sizeCols));

	glBindVertexArray(0);

}

void readShaderFile(const char file[], const char *shader[]) {
	int i = 0, bufblock = 128, buflen = bufblock;
	char *buffer = (char*)malloc(buflen);
	int ch;
	FILE *fp;
	fp = fopen(file, "r");
	while ((ch = fgetc(fp)) != EOF) {
		buffer[i++] = ch;
		if (i >= buflen) {
			buflen += bufblock;							// Get another block
			buffer = (char*)realloc(buffer, buflen);	// to save stuff
		}
	}
	fclose(fp);
	buffer[i] = '\0';
	buffer = (char*)realloc(buffer, i + 1);				//Remove unused space, save that memory
	shader[0] = buffer;
}

void renderMesh(Mesh *mesh, Matrix V, Matrix P, Matrix PV) {

	static Matrix W, M;

	// Assignment 1: Apply the transforms from local mesh coordinates to world coordinates here
	// Combine it with the viewing transform that is passed to the shader below
	Vector radRotation = { Deg2Rad(mesh->rotation.x), Deg2Rad(mesh->rotation.y), Deg2Rad(mesh->rotation.z) };

	W = LocalToWorld(mesh->translation, radRotation, mesh->scale);
	M = MatMatMul(PV, W);

	// TODO: REPLACE WITH DIFFERENT MATERIALS PER MESH ?
	/*Material mat = {
		{ 0.1f, 0.1f, 0.1f },	// Ambient
		{ 0.7f, 0.1f, 0.1f },	// Diffuse
		{ 1.0f, 1.0f, 1.0f },	// Specular
		0.8f					// Shininess
	};*/

	Material mat = mesh->material;

	PointLight* light = scene->pointLights;

	Camera cam = *(player->cam);

	// Pass the viewing transform to the shader
	GLint loc_PV = glGetUniformLocation(player->shader->program, "PV");
	glUniformMatrix4fv(loc_PV, 1, GL_FALSE, M.e);
	GLint loc_Mod = glGetUniformLocation(player->shader->program, "model");
	glUniformMatrix4fv(loc_Mod, 1, GL_FALSE, W.e);
	GLint loc_Vie = glGetUniformLocation(player->shader->program, "view");
	glUniformMatrix4fv(loc_Vie, 1, GL_FALSE, V.e);
	GLint loc_Pro = glGetUniformLocation(player->shader->program, "projection");
	glUniformMatrix4fv(loc_Pro, 1, GL_FALSE, P.e);

	GLint loc_VP = glGetUniformLocation(player->shader->program, "viewPos");
	glUniform3f(loc_VP, cam.position.x, cam.position.y, cam.position.z);

	// Material properties passed to shader
	GLint loc_MA = glGetUniformLocation(player->shader->program, "material.ambient");
	glUniform3f(loc_MA, mat.ambient.x, mat.ambient.y, mat.ambient.z);
	GLint loc_MD = glGetUniformLocation(player->shader->program, "material.diffuse");
	glUniform3f(loc_MD, mat.diffuse.x, mat.diffuse.y, mat.diffuse.z);
	GLint loc_MS = glGetUniformLocation(player->shader->program, "material.specular");
	glUniform3f(loc_MS, mat.specular.x, mat.specular.y, mat.specular.z);
	GLint loc_MSH = glGetUniformLocation(player->shader->program, "material.shininess");
	glUniform1f(loc_MSH, mat.shininess);


	char buff[27];
	for (int i = 0; i < 2; i++) {
		// Light properties passed to shader
		sprintf(buff, "lights[%d].pos", i);
		GLint loc_LPOS = glGetUniformLocation(player->shader->program, buff);
		glUniform3f(loc_LPOS, light->pos.x, light->pos.y, light->pos.z);
		sprintf(buff, "lights[%d].diffuse", i);
		GLint loc_LDIF = glGetUniformLocation(player->shader->program, buff);
		glUniform3f(loc_LDIF, light->diffuse.x, light->diffuse.y, light->diffuse.z);
		sprintf(buff, "lights[%d].ambient", i);
		GLint loc_LAMB = glGetUniformLocation(player->shader->program, buff);
		glUniform3f(loc_LAMB, light->ambient.x, light->ambient.y, light->ambient.z);
		sprintf(buff, "lights[%d].specular", i);
		GLint loc_LSPE = glGetUniformLocation(player->shader->program, buff);
		glUniform3f(loc_LSPE, light->specular.x, light->specular.y, light->specular.z);
		sprintf(buff, "lights[%d].attenuation", i);
		GLint loc_LATT = glGetUniformLocation(player->shader->program, buff);
		glUniform1f(loc_LATT, light->attenuation);
		light = light->next;
	}

	GLint loc_TEX = glGetUniformLocation(player->shader->program, "tex");
	glUniform1i(loc_TEX, 0);
	glActiveTexture(GL_TEXTURE0 + 0);
	glBindTexture(GL_TEXTURE_2D, mat.texture);



	// Select current resources 
	glBindVertexArray(mesh->vao);

	// To accomplish wireframe rendering (can be removed to get filled triangles)
	if (player->shaderMode == 1) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}
	else
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	// Draw all triangles
	glDrawElements(GL_TRIANGLES, mesh->nt * 3, GL_UNSIGNED_INT, NULL);
	glBindVertexArray(0);
}