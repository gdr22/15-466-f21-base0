#include "MyMode.hpp"

//for the GL_ERRORS() macro:
#include "gl_errors.hpp"

//for glm::value_ptr() :
#include <glm/gtc/type_ptr.hpp>

#include <random>
#include <math.h>
#include <stdio.h>

#define DEG2RAD(X)  ((X) * 3.14159f / 180)
#define RAD2DEG(X)  ((X) * 180 / 3.14159f)

#define HEX_TO_U8VEC4( HX ) (glm::u8vec4( (HX >> 24) & 0xff, (HX >> 16) & 0xff, (HX >> 8) & 0xff, (HX) & 0xff ))

MyMode::MyMode() {

	// Ensure all the bricks are present
	for (int ring = 0; ring < RINGS; ring++) {
		for (int brick = 0; brick < BRICKS_PER_ROW; brick++) {
			//bricks[ring][brick] = (brick) % (ring + 2)!= 0;
			bricks[ring][brick] = true;
		}
	}

	//----- allocate OpenGL resources -----
	{ //vertex buffer:
		glGenBuffers(1, &vertex_buffer);
		//for now, buffer will be un-filled.

		GL_ERRORS(); //PARANOIA: print out any OpenGL errors that may have happened
	}

	{ //vertex array mapping buffer for color_texture_program:
		//ask OpenGL to fill vertex_buffer_for_color_texture_program with the name of an unused vertex array object:
		glGenVertexArrays(1, &vertex_buffer_for_color_texture_program);

		//set vertex_buffer_for_color_texture_program as the current vertex array object:
		glBindVertexArray(vertex_buffer_for_color_texture_program);

		//set vertex_buffer as the source of glVertexAttribPointer() commands:
		glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);

		//set up the vertex array object to describe arrays of MyMode::Vertex:
		glVertexAttribPointer(
			color_texture_program.Position_vec4, //attribute
			3, //size
			GL_FLOAT, //type
			GL_FALSE, //normalized
			sizeof(Vertex), //stride
			(GLbyte *)0 + 0 //offset
		);
		glEnableVertexAttribArray(color_texture_program.Position_vec4);
		//[Note that it is okay to bind a vec3 input to a vec4 attribute -- the w component will be filled with 1.0 automatically]

		glVertexAttribPointer(
			color_texture_program.Color_vec4, //attribute
			4, //size
			GL_UNSIGNED_BYTE, //type
			GL_TRUE, //normalized
			sizeof(Vertex), //stride
			(GLbyte *)0 + 4*3 //offset
		);
		glEnableVertexAttribArray(color_texture_program.Color_vec4);

		glVertexAttribPointer(
			color_texture_program.TexCoord_vec2, //attribute
			2, //size
			GL_FLOAT, //type
			GL_FALSE, //normalized
			sizeof(Vertex), //stride
			(GLbyte *)0 + 4*3 + 4*1 //offset
		);
		glEnableVertexAttribArray(color_texture_program.TexCoord_vec2);

		//done referring to vertex_buffer, so unbind it:
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		//done setting up vertex array object, so unbind it:
		glBindVertexArray(0);

		GL_ERRORS(); //PARANOIA: print out any OpenGL errors that may have happened
	}

	{ //solid white texture:
		//ask OpenGL to fill white_tex with the name of an unused texture object:
		glGenTextures(1, &white_tex);

		//bind that texture object as a GL_TEXTURE_2D-type texture:
		glBindTexture(GL_TEXTURE_2D, white_tex);

		//upload a 1x1 image of solid white to the texture:
		glm::uvec2 size = glm::uvec2(1,1);
		std::vector< glm::u8vec4 > data(size.x*size.y, glm::u8vec4(0xff, 0xff, 0xff, 0xff));
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());

		//set filtering and wrapping parameters:
		//(it's a bit silly to mipmap a 1x1 texture, but I'm doing it because you may want to use this code to load different sizes of texture)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		//since texture uses a mipmap and we haven't uploaded one, instruct opengl to make one for us:
		glGenerateMipmap(GL_TEXTURE_2D);

		//Okay, texture uploaded, can unbind it:
		glBindTexture(GL_TEXTURE_2D, 0);

		GL_ERRORS(); //PARANOIA: print out any OpenGL errors that may have happened
	}
}

MyMode::~MyMode() {

	//----- free OpenGL resources -----
	glDeleteBuffers(1, &vertex_buffer);
	vertex_buffer = 0;

	glDeleteVertexArrays(1, &vertex_buffer_for_color_texture_program);
	vertex_buffer_for_color_texture_program = 0;

	glDeleteTextures(1, &white_tex);
	white_tex = 0;
}

// Computes the intersection of a ray {origin, dir} with a circle centered about
// the scene origin with a given radius
float intersect_ring(glm::vec2 origin, glm::vec2 dir, float radius) {
	float a = (   dir.x *    dir.x) + (   dir.y *    dir.y);
	float b = (   dir.x * origin.x) + (   dir.y * origin.y);
	float c = (origin.x * origin.x) + (origin.y * origin.y) - (radius * radius);
	
	float d = (b * b) - (a * c);

	// If we have no intersection, return -1
	if (d < 0) return -1;

	// Otherwise return the t where the ray intersects the ring
	float t0 = (-b - sqrtf(d)) / a;
	float t1 = (-b + sqrtf(d)) / a;
	
	if (t0 < 0) return t1;
	return (t0 < t1) ? t0 : t1;
}

// Computes the cross product between a and b
float cross(glm::vec2 a, glm::vec2 b) {
	return (a.x * b.y) - (a.y * b.x);
}

// Computes the intersection of a ray {origin, dir} with a line segment direction
// r from the center between radii inner_rad and outer_rad
float intersect_line_segment(glm::vec2 r, float inner_rad, float outer_rad, glm::vec2 origin, glm::vec2 dir) {
	// (I'm sure this formula exists elsewhere, but I worked this one out by hand)
	float t = -cross(origin, r) / cross(dir, r);

	// Check that we intersect within the next frame
	if (t < 0 || t > 1) return -1;
	
	glm::vec2 hit_pos = origin + (dir * t);
	float rad = sqrtf((hit_pos.x * hit_pos.x) + (hit_pos.y * hit_pos.y));
		
	//Check that we hit within the radius range
	if (rad < inner_rad || rad > outer_rad) return -1;

	// Make sure this hit is on the positive side of the line
	float dot = (r.x * hit_pos.x) + (r.y * hit_pos.y);
	if (dot < 0) return -1;

	return t;
}

// Reflects dir vector about the normal vector
glm::vec2 reflect(glm::vec2 dir, glm::vec2 normal) {
	return dir - (normal * 2.0f * (dir.x * normal.x + dir.y * normal.y));
}

bool MyMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	float past_mouse_angle = mouse_angle;

	if (evt.type == SDL_MOUSEMOTION) {
		//convert mouse from window pixels (top-left origin, +y is down) to clip space ([-1,1]x[-1,1], +y is up):
		glm::vec2 clip_mouse = glm::vec2(
			(evt.motion.x + 0.5f) / window_size.x * 2.0f - 1.0f,
			(evt.motion.y + 0.5f) / window_size.y *-2.0f + 1.0f
		);

		mouse_angle = RAD2DEG(atan2f(clip_mouse.y, clip_mouse.x));
		
		// Compute the difference in angle between frames
		float delta_angle = mouse_angle - past_mouse_angle;

		// Normalize this difference to -180 to 180, and add it to our total angle
		if (delta_angle >  180) delta_angle -= 360;
		if (delta_angle < -180) delta_angle += 360;

		sec_angle += delta_angle;
	}

	return false;
}

void MyMode::update(float elapsed) {
	// Update the ring animations
	for (int ring = 0; ring < RINGS; ring++) {
		for (int brick = 0; brick < BRICKS_PER_ROW; brick++) {
			// Decrease time for disappearing bricks
			if (!bricks[ring][brick]) {
				hit_lerp[ring][brick] -= elapsed;

				// Clamp lerps to 0
				if(hit_lerp[ring][brick] < 0)
					hit_lerp[ring][brick] = 0;
			}
		}
	}
	
	
	bool hit = false;
	//Check collisions
	for (int ring = 0; ring < RINGS; ring++) {
		float radius = INNER_RADIUS + ring;
		float angle = sec_angle * INNER_RADIUS / radius;

		float t = intersect_ring(ball, ball_velocity * elapsed, radius - ball_radius);
		bool hit_inner = true;

		// If we don't hit the inside, try the outside
		if (t < 0 || t > 1) {
			t = intersect_ring(ball, ball_velocity * elapsed, (radius + RING_WIDTH) + ball_radius);
			hit_inner = false;
		}

		// If we intersect with a ring
		if (t > 0 && t < 1) {
			glm::vec2 hit_pos = ball + (ball_velocity * elapsed * t);
			float norm = sqrtf(hit_pos.x * hit_pos.x + hit_pos.y * hit_pos.y);
			glm::vec2 hit_norm = hit_pos / -norm;

			// Figure out which brick we just hit
			float hit_angle = RAD2DEG(atan2f(hit_pos.y, hit_pos.x)) - angle;

			while (hit_angle < 0 || hit_angle > 360) {
				if (hit_angle < 0) hit_angle += 360;
				if (hit_angle > 360) hit_angle -= 360;
			}

			int brick = (int)(hit_angle / BRICK_ANGLE);

			// Bounce off the wall if we did hit a brick
			if (bricks[ring][brick]) {
				ball_velocity = reflect(ball_velocity, hit_norm);

				ball = hit_pos + (ball_velocity * elapsed * (1 - t));
				bricks[ring][brick] = false;
				hit_side[ring][brick] = hit_inner ? INNER : OUTER;
				hit_lerp[ring][brick] = LERP_TIME;
				hit = true;
				break;
			}

		}

		// Test the sides of the rings
		for (int i = 0; i < BRICKS_PER_ROW; i++) {
			float brick_angle = DEG2RAD((i * BRICK_ANGLE) + angle);
			glm::vec2 line (cosf(brick_angle), sinf(brick_angle));
			
			t = intersect_line_segment(line, radius - ball_radius, radius + RING_WIDTH + ball_radius, ball, ball_velocity * elapsed);
			
			
			int brick = i;
			bool side = cross(ball, line) < 0;
			// If the ball is CCW to the line, look at the previous brick
			brick -=  side ? 1 : 0;
			brick += (brick < 0) ? BRICKS_PER_ROW : 0;

			if (t > 0 && bricks[ring][brick]) {
				glm::vec2 perp(-line.y, line.x);

				ball_velocity = reflect(ball_velocity, perp);
				bricks[ring][brick] = false;
				hit_side[ring][brick] = hit_inner ? LEFT : RIGHT;
				hit_lerp[ring][brick] = LERP_TIME;

				hit = true;
				break;
			}
		}
	}

	// Test collision with the inner circle
	if (!hit) {
		float t = intersect_ring(ball, ball_velocity * elapsed, 1 + ball_radius);
		
		if (t > 0 && t < 1) {
			glm::vec2 hit_pos = ball + (ball_velocity * elapsed * t);
			float norm = sqrtf(hit_pos.x * hit_pos.x + hit_pos.y * hit_pos.y);
			glm::vec2 hit_norm = hit_pos / -norm;
			ball_velocity = reflect(ball_velocity, hit_norm);

			ball = hit_pos + (ball_velocity * elapsed * (1 - t));
			hit = true;
		}
	}
	
	// Update the ball position unless we've already handled the collision
	if(!hit)
	{
		ball += elapsed * ball_velocity;
	}
	else
	{
		ball_velocity *= powf(2.0f, 1.0f / (BRICKS_PER_ROW * 2));
	}


	//If the ball leaves the walls, count the loss
	if (ball.x < -court_radius.x || ball.x > court_radius.x ||
		  ball.y < -court_radius.y || ball.y > court_radius.y ) {

		float ball_vel = sqrtf((ball_velocity.x * ball_velocity.x) + 
													 (ball_velocity.y * ball_velocity.y));

		ball = glm::vec2(0.0f, 1.5f);
		ball_velocity = glm::vec2(ball_vel, 0.0f);

		ball_cnt--;

		if (ball_cnt <= 0) {
			printf("You lose!");
			Mode::set_current(nullptr);
		}
	}

	// Check if all bricks have been broken
	bool done = true;

	for (int ring = 0; ring < RINGS; ring++) {
		for (int brick = 0; brick < BRICKS_PER_ROW; brick++) {
			if (bricks[ring][brick]) {
				done = false;
			}
		}
	}

	if (done) {
		printf("You win!");
		Mode::set_current(nullptr);
	}
}

void MyMode::draw(glm::uvec2 const &drawable_size) {
	//some nice colors from the course web page:
	const glm::u8vec4 bg_color = HEX_TO_U8VEC4(0x193b59ff);
	const glm::u8vec4 fg_color = HEX_TO_U8VEC4(0xffffffff);

	//other useful drawing constants:
	const float wall_radius = 0.05f;
	const float padding = 0.14f; //padding between outside of walls and edge of window

	//---- compute vertices to draw ----

	//vertices will be accumulated into this list and then uploaded+drawn at the end of this function:
	std::vector< Vertex > vertices;

	//inline helper functions for sector and circle drawing:
	auto draw_sector = [&vertices](glm::vec2 center, glm::vec2 radius, glm::vec2 angles, glm::u8vec4 const& color) {
		// Draw a sector as a series of 1-degree trapezoids
		
		float step = 1;

		for (float angle = angles.x; angle < angles.y + .01f; angle += step) {
			float rad0 = DEG2RAD(angle);
			float rad1 = DEG2RAD(angle + step);
			glm::vec2 inner_point0 = glm::vec2(cosf(rad0), sinf(rad0)) * radius.x + center;
			glm::vec2 outer_point0 = glm::vec2(cosf(rad0), sinf(rad0)) * radius.y + center;
			glm::vec2 inner_point1 = glm::vec2(cosf(rad1), sinf(rad1)) * radius.x + center;
			glm::vec2 outer_point1 = glm::vec2(cosf(rad1), sinf(rad1)) * radius.y + center;
			
			vertices.emplace_back(glm::vec3(inner_point1.x, inner_point1.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
			vertices.emplace_back(glm::vec3(inner_point0.x, inner_point0.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
			vertices.emplace_back(glm::vec3(outer_point0.x, outer_point0.y, 0.0f), color, glm::vec2(0.5f, 0.5f));

			vertices.emplace_back(glm::vec3(outer_point0.x, outer_point0.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
			vertices.emplace_back(glm::vec3(outer_point1.x, outer_point1.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
			vertices.emplace_back(glm::vec3(inner_point1.x, inner_point1.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
		}
	};

	auto draw_circle = [&vertices](glm::vec2 center, float radius, glm::u8vec4 const& color) {
		//draw a circle as a series of triangles

		float step = 5;

		for (float angle = 0; angle < 360; angle += step) {
			float rad0 = DEG2RAD(angle);
			float rad1 = DEG2RAD(angle + step);
			glm::vec2 outer_point0 = glm::vec2(cosf(rad0), sinf(rad0)) * radius + center;
			glm::vec2 outer_point1 = glm::vec2(cosf(rad1), sinf(rad1)) * radius + center;

			vertices.emplace_back(glm::vec3(center.x, center.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
			vertices.emplace_back(glm::vec3(outer_point0.x, outer_point0.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
			vertices.emplace_back(glm::vec3(outer_point1.x, outer_point1.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
		}
	};
	

	//ball:
	draw_circle(ball, ball_radius, fg_color);

	// Draw rings
	glm::vec2 sec_center = glm::vec2(0, 0);
	for (int ring = 0; ring < RINGS; ring++) {
		// Compute the radius and angle offset for this ring
		float radius = INNER_RADIUS + ring;
		float ring_angle = sec_angle * INNER_RADIUS / radius;

		for (int brick = 0; brick < BRICKS_PER_ROW; brick++) {
			// Only draw if ring is present
			if (!bricks[ring][brick] && hit_lerp[ring][brick] <= 0) continue;

			glm::vec2 sec_angles = glm::vec2(BRICK_ANGLE *  brick + 1, 
																			 BRICK_ANGLE * (brick + 1) - 1)
														 + ring_angle;
			glm::vec2 sec_radius = glm::vec2(radius, radius + RING_WIDTH);

			// If the brick is destroyed but still being animated, adjust the drawing
			// parameters
			if (!bricks[ring][brick]) {
				float lerp = 1 - (hit_lerp[ring][brick] / LERP_TIME);

				switch (hit_side[ring][brick]) {
				case INNER:
					sec_radius.x += RING_WIDTH * lerp;
					break;
				case OUTER:
					sec_radius.y -= RING_WIDTH * lerp;
					break;
				case RIGHT:
					sec_angles.x += (sec_angles.y - sec_angles.x) * lerp;
					break;
				case LEFT:
					sec_angles.y -= (sec_angles.y - sec_angles.x) * lerp;
					break;
				default:
					break;

				}
			}

			draw_sector(sec_center, sec_radius, sec_angles, fg_color);
		}
	}

	draw_circle(sec_center, 1, fg_color);

	// Draw ball counter
	for (int i = 0; i < ball_cnt; i++) {
		glm::vec2 pos(-court_radius.x, court_radius.y);

		pos.x += i * GUI_BALL_RADIUS * 3;

		draw_circle(pos, GUI_BALL_RADIUS, fg_color);
	}	

	//------ compute court-to-window transform ------

	//compute area that should be visible:
	glm::vec2 scene_min = glm::vec2(
		-court_radius.x - 2.0f * wall_radius - padding,
		-court_radius.y - 2.0f * wall_radius - padding
	);
	glm::vec2 scene_max = glm::vec2(
		court_radius.x + 2.0f * wall_radius + padding,
		court_radius.y + 2.0f * wall_radius + padding
	);

	//compute window aspect ratio:
	float aspect = drawable_size.x / float(drawable_size.y);
	//we'll scale the x coordinate by 1.0 / aspect to make sure things stay square.

	//compute scale factor for court given that...
	float scale = std::min(
		(2.0f * aspect) / (scene_max.x - scene_min.x), //... x must fit in [-aspect,aspect] ...
		(2.0f) / (scene_max.y - scene_min.y) //... y must fit in [-1,1].
	);

	glm::vec2 center = 0.5f * (scene_max + scene_min);

	//build matrix that scales and translates appropriately:
	glm::mat4 court_to_clip = glm::mat4(
		glm::vec4(scale / aspect, 0.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, scale, 0.0f, 0.0f),
		glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
		glm::vec4(-center.x * (scale / aspect), -center.y * scale, 0.0f, 1.0f)
	);
	//NOTE: glm matrices are specified in *Column-Major* order,
	// so each line above is specifying a *column* of the matrix(!)

	//also build the matrix that takes clip coordinates to court coordinates (used for mouse handling):
	clip_to_court = glm::mat3x2(
		glm::vec2(aspect / scale, 0.0f),
		glm::vec2(0.0f, 1.0f / scale),
		glm::vec2(center.x, center.y)
	);

	//---- actual drawing ----

	//clear the color buffer:
	glClearColor(bg_color.r / 255.0f, bg_color.g / 255.0f, bg_color.b / 255.0f, bg_color.a / 255.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	//use alpha blending:
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	//don't use the depth test:
	glDisable(GL_DEPTH_TEST);

	//upload vertices to vertex_buffer:
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer); //set vertex_buffer as current
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vertices[0]), vertices.data(), GL_STREAM_DRAW); //upload vertices array
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	//set color_texture_program as current program:
	glUseProgram(color_texture_program.program);

	//upload OBJECT_TO_CLIP to the proper uniform location:
	glUniformMatrix4fv(color_texture_program.OBJECT_TO_CLIP_mat4, 1, GL_FALSE, glm::value_ptr(court_to_clip));

	//use the mapping vertex_buffer_for_color_texture_program to fetch vertex data:
	glBindVertexArray(vertex_buffer_for_color_texture_program);

	//bind the solid white texture to location zero so things will be drawn just with their colors:
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, white_tex);

	//run the OpenGL pipeline:
	glDrawArrays(GL_TRIANGLES, 0, GLsizei(vertices.size()));

	//unbind the solid white texture:
	glBindTexture(GL_TEXTURE_2D, 0);

	//reset vertex array to none:
	glBindVertexArray(0);

	//reset current program to none:
	glUseProgram(0);
	

	GL_ERRORS(); //PARANOIA: print errors just in case we did something wrong.

}
