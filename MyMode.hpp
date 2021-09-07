#include "ColorTextureProgram.hpp"

#include "Mode.hpp"
#include "GL.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>


#define RINGS 5
#define BRICKS_PER_ROW 12
#define BRICK_ANGLE (360.0f / BRICKS_PER_ROW)
#define INNER_RADIUS 2.0f
#define RING_WIDTH 0.9f
#define LERP_TIME 0.1f
#define GUI_BALL_RADIUS 0.1f

enum Sides { INNER, OUTER, LEFT, RIGHT };

/*
 * MyMode is a game mode that implements a single-player game of Pong.
 */

struct MyMode : Mode {
	MyMode();
	virtual ~MyMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	int ball_cnt = 3;

	float mouse_angle = 0;
	float sec_angle = 0;

	bool bricks[RINGS][BRICKS_PER_ROW];
	Sides hit_side[RINGS][BRICKS_PER_ROW];
	float hit_lerp[RINGS][BRICKS_PER_ROW];
	
	//glm::vec2 court_radius = glm::vec2(7.0f, 5.0f);
	glm::vec2 court_radius = glm::vec2(9.0f, 7.0f);
	float ball_radius = 0.2f;

	glm::vec2 ball = glm::vec2(0.0f, 1.5f);
	glm::vec2 ball_velocity = glm::vec2(2.0f, 0.0f);

	//----- opengl assets / helpers ------

	//draw functions will work on vectors of vertices, defined as follows:
	struct Vertex {
		Vertex(glm::vec3 const &Position_, glm::u8vec4 const &Color_, glm::vec2 const &TexCoord_) :
			Position(Position_), Color(Color_), TexCoord(TexCoord_) { }
		glm::vec3 Position;
		glm::u8vec4 Color;
		glm::vec2 TexCoord;
	};
	static_assert(sizeof(Vertex) == 4*3 + 1*4 + 4*2, "MyMode::Vertex should be packed");

	//Shader program that draws transformed, vertices tinted with vertex colors:
	ColorTextureProgram color_texture_program;

	//Buffer used to hold vertex data during drawing:
	GLuint vertex_buffer = 0;

	//Vertex Array Object that maps buffer locations to color_texture_program attribute locations:
	GLuint vertex_buffer_for_color_texture_program = 0;

	//Solid white texture:
	GLuint white_tex = 0;

	//matrix that maps from clip coordinates to court-space coordinates:
	glm::mat3x2 clip_to_court = glm::mat3x2(1.0f);
	// computed in draw() as the inverse of OBJECT_TO_CLIP
	// (stored here so that the mouse handling code can use it to position the paddle)

};
