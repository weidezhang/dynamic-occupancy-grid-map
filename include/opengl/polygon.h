#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>

class Vertex
{
public:
	Vertex(const glm::vec2& pos)
	{
		this->pos = pos;
	}

	Vertex(const glm::vec2& pos, const glm::vec2& tex_coord)
	{
		this->pos = pos;
		this->tex_coord = tex_coord;
	}

	glm::vec2 pos;
	glm::vec2 tex_coord;
};

class Polygon
{
public:
	Polygon(Vertex* vertices, int num_vertices);
	~Polygon();

	void draw();

private:
	GLuint vao;
	GLuint vbo;

	unsigned int vertices_count;
};
