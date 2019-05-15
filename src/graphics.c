/**
 * graphics.c
 * A SDL graphics abstraction layer.
 *
 * @author Nathan Campos <nathanpc@dreamintech.net>
 */

#include <SDL.h>
#include <SDL_ttf.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "nanocad.h"
#include "osifont.h"
#include "graphics.h"

// Constants
#define ZOOM_INTENSITY 10

// SDL context.
SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
TTF_Font *font = NULL;
const uint8_t *keystates;
bool running = false;
coord_t origin;
int zoom_level = 100;

// Internal functions.
bool is_key_down(const SDL_Scancode key);
void set_origin(const int x, const int y);
void reset_origin();
void zoom(const int percentage);
int draw_text(const char *text, const coord_t pos, const double angle,
			  const uint8_t layer_num);
int draw_line(const coord_t start, const coord_t end, const uint8_t layer_num);
int draw_dimension(const coord_t start, const coord_t end,
				   const coord_t line_start, const coord_t line_end,
				   const uint8_t layer_num);
void graphics_render();
void graphics_eventloop();


/**
 * Initializes the SDL graphics context.
 *
 * @param  width  Window width.
 * @param  height Window height.
 * @return        TRUE if the initialization went according to plan.
 */
bool graphics_init(const int width, const int height) {
	running = false;
	
	// Initialize SDL.
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
		printf("There was an error while trying to initialize SDL: %s\n",
			   SDL_GetError());
		return false;
	}
	
	// Initialize SDL TTF module.
	if (TTF_Init() < 0) {
		printf("There was an error while trying to initialize SDL_ttf: %s\n",
			   TTF_GetError());
		return false;
	}

	// Create a window.
	window = SDL_CreateWindow("nanoCAD", SDL_WINDOWPOS_CENTERED, 
							  SDL_WINDOWPOS_CENTERED, width, height,
							  SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);

	// Create the renderer.
	if (window != 0) {
		renderer = SDL_CreateRenderer(window, -1, 0);
	} else {
		printf("Couldn't create the SDL window.\n");
		return false;
	}
		
	// Create the font object.
	font = TTF_OpenFontRW(SDL_RWFromConstMem(osifont_ttf, osifont_ttf_length),
						  1, 20);
	if (font == NULL) {
		printf("Failed to load the embedded font. SDL_ttf Error: %s\n",
			   TTF_GetError());
		return false;
	}

	// Initialize variables.
	running = true;
	reset_origin();

	return true;
}

/**
 *  Clean the trash.
 */
void graphics_clean() {
	running = false;
	
	// Free our font.
	TTF_CloseFont(font);
	font = NULL;

	// Destroy window.
	SDL_DestroyWindow(window);
	SDL_DestroyRenderer(renderer);
	window = NULL;
	renderer = NULL;
	
	// Quit SDL subsystems.
	TTF_Quit();
	SDL_Quit();
}

/**
 * Render the CAD graphics on screen.
 */
void graphics_render() {
	object_container objects;
	dimension_container dimensions;
	int ret = 0;
	
	// Get the containers from the engine.
	get_object_container(&objects);
	get_dimension_container(&dimensions);

	// Loop through each object and render it.
	ret = 0;
	for (size_t i = 0; i < objects.count; i++) {
		object_t obj = objects.list[i];
		
		// Do something different according to each type of object.
		switch (obj.type) {
		case TYPE_LINE:
			ret = draw_line(obj.coord[0], obj.coord[1], obj.layer_num);
			break;
		default:
			printf("Invalid object type.\n");
			exit(EXIT_FAILURE);
		}

		// Report any errors if there were any.
		if (ret < 0) {
			printf("Error rendering line: %s\n", SDL_GetError());
		}
	}
	
	// Loop through each dimension and render it.
	ret = 0;
	for (size_t i = 0; i < dimensions.count; i++) {
		dimension_t dimen = dimensions.list[i];
		
		// Draw the dimension.
		ret = draw_dimension(dimen.start, dimen.end,
							 dimen.line_start, dimen.line_end, 0);
		
		// Report any errors if there were any.
		if (ret < 0) {
			printf("Error rendering line: %s\n", SDL_GetError());
		}
	}
}

/**
 * Draws a line between two points.
 *
 * @param  start     Starting point for a line.
 * @param  end       Ending point.
 * @param  layer_num Layer number where the line should be rendered.
 * @return           SDL_RenderDrawLine return value.
 */
int draw_line(const coord_t start, const coord_t end, const uint8_t layer_num) {
	// Transpose the coordinates to our own origin.
	int x1 = origin.x + start.x;
	int y1 = origin.y - start.y;
	int x2 = origin.x + end.x;
	int y2 = origin.y - end.y;
	
	// Get the line's layer.
	layer_t *layer = get_layer(layer_num);
	if (layer == NULL) {
		printf("Warning: Invalid layer '%d' to be rendered, falling back to "
			   "layer 0.\n", layer_num);
		layer = get_layer(0);
	}
	
	SDL_SetRenderDrawColor(renderer, layer->color.r, layer->color.g,
						   layer->color.b, layer->color.alpha);
	return SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
}

/**
 * Draws some text on the screen.
 * 
 * @param  text      The text to be rendered on screen.
 * @param  pos       Where to put the text.
 * @param  angle     Which angle should the text be in.
 * @param  layer_num Layer number where the text should be rendered.
 * @return           SDL_RenderCopyEx return value.
 */
int draw_text(const char *text, const coord_t pos, const double angle,
			  const uint8_t layer_num) {
	// Transpose the coordinates to our own origin.
	int x1 = origin.x + pos.x;
	int y1 = origin.y - pos.y;
	
	// Get the line's layer.
	layer_t *layer = get_layer(layer_num);
	if (layer == NULL) {
		printf("Warning: Invalid layer '%d' to be rendered, falling back to "
			   "layer 0.\n", layer_num);
		layer = get_layer(0);
	}
	
	// Create the text's surface.
	SDL_Color color = { layer->color.r, layer->color.g, layer->color.b,
						layer->color.alpha };
	SDL_Surface *surface = TTF_RenderText_Solid(font, text, color);

	// Create a texture for the text.
	SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
	
	// Get the text dimensions and free the surface.
	int text_width = surface->w;
	int text_height = surface->h;
    SDL_FreeSurface(surface);
	
	// Create text area rectangle.
	SDL_Rect rect;
	rect.x = x1;
	rect.y = y1;
	rect.w = text_width;
	rect.h = text_height;

	// TODO: Find a way to free the texture.

	// Copy the texture to the renderer.
	return SDL_RenderCopyEx(renderer, texture, NULL, &rect, angle, NULL,
							SDL_FLIP_NONE);
}

/**
 * Draws a dimension between two points.
 *
 * @param  start      Starting point for the measurement.
 * @param  end        Ending point for the measurement.
 * @param  line_start Starting point for the dimension line.
 * @param  line_end   Ending point for the dimension line.
 * @param  layer_num  Layer number where the line should be rendered.
 * @return            SDL_RenderDrawLine return value.
 */
int draw_dimension(const coord_t start, const coord_t end,
				   const coord_t line_start, const coord_t line_end,
				   const uint8_t layer_num) {
	int ret = 0;

	// Transpose the coordinates to our own origin.
	int x1 = origin.x + line_start.x;
	int y1 = origin.y - line_start.y;
	int x2 = origin.x + line_end.x;
	int y2 = origin.y - line_end.y;
	
	// Define the offsets.
	int pin_offset = 10;
	
	// Get the line's layer.
	layer_t *layer = get_layer(layer_num);
	if (layer == NULL) {
		printf("Warning: Invalid layer '%d' to be rendered, falling back to "
			   "layer 0.\n", layer_num);
		layer = get_layer(0);
	}
	
	// Set the layer color.
	SDL_SetRenderDrawColor(renderer, layer->color.r, layer->color.g,
						   layer->color.b, layer->color.alpha);
	
	// Draw the main dimension line.
	ret = SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
	if (ret < 0) {
		return ret;
	}
	
	// Calculate the perpendicular line angle for the marker pins.
	double perpangle = atan2(y1 - y2, x1 - x2);
	
	// Draw the first leg of the starting pin.
	double cx = x1 + (sin(perpangle) * pin_offset);
	double cy = y1 + (cos(perpangle) * pin_offset);
	ret = SDL_RenderDrawLine(renderer, x1, y1, cx, cy);
	if (ret < 0) {
		return ret;
	}
	
	// Draw the second leg of the starting pin.
	cx = x1 - (sin(perpangle) * pin_offset);
	cy = y1 - (cos(perpangle) * pin_offset);
	ret = SDL_RenderDrawLine(renderer, x1, y1, cx, cy);
	if (ret < 0) {
		return ret;
	}

	// Draw the first leg of the ending pin.
	cx = x2 + (sin(perpangle) * pin_offset);
	cy = y2 + (cos(perpangle) * pin_offset);
	ret = SDL_RenderDrawLine(renderer, x2, y2, cx, cy);
	if (ret < 0) {
		return ret;
	}

	// Draw the second leg of the ending pin.
	cx = x2 - (sin(perpangle) * pin_offset);
	cy = y2 - (cos(perpangle) * pin_offset);
	ret = SDL_RenderDrawLine(renderer, x2, y2, cx, cy);
	if (ret < 0) {
		return ret;
	}
	
	// Render the dimension text.
	double distance = sqrt(pow(end.x - start.x, 2) + pow(end.y - start.y, 2));
	char text[DIMENSION_TEXT_MAX_SIZE];
	snprintf(text, DIMENSION_TEXT_MAX_SIZE, "%.2fmm", distance);
	coord_t text_pos = line_start;
	draw_text(text, text_pos, perpangle * (180.0 / M_PI), layer_num);
	
	return ret;
}

/**
 *  Render loop.
 */
void graphics_eventloop() {
	keystates = SDL_GetKeyboardState(0);
	SDL_Event event;
	int zoom_amount = 0;

	// TODO: Handle touch events.

	while (running && SDL_WaitEvent(&event)) {
		// Set the background color and clear the window.
		SDL_SetRenderDrawColor(renderer, 33, 40, 48, 255);
		SDL_RenderClear(renderer);

		switch (event.type) {
		case SDL_KEYDOWN:
			// Keyboard key pressed.
			if (is_key_down(SDL_SCANCODE_ESCAPE)) {
				// Escape
				SDL_Quit();
				exit(EXIT_SUCCESS);
			}
			break;
		case SDL_MOUSEMOTION:
			// Mouse movement.
			if (event.motion.state & SDL_BUTTON(SDL_BUTTON_LEFT)) {
				// Pan around the view.
				set_origin(origin.x + event.motion.xrel,
						   origin.y + event.motion.yrel);
			}
			break;
		case SDL_MOUSEWHEEL:
			// Mouse wheel turned.
			zoom_amount = zoom_level + (event.wheel.y * ZOOM_INTENSITY);
			zoom(zoom_amount);
#ifdef DEBUG
			printf("Zoom level: %d%%\n", zoom_amount);
#endif
			break;
		case SDL_WINDOWEVENT:
			// Window stuff.
			switch (event.window.event) {
			case SDL_WINDOWEVENT_RESIZED:
#ifdef DEBUG
				SDL_Log("Window %d resized to %dx%d",
						event.window.windowID, event.window.data1,
						event.window.data2);
#endif
				reset_origin();
				break;
			}
			break;
		}

		// Update the graphics on the screen.
		graphics_render();

		// Show the window.
		SDL_RenderPresent(renderer);
	}

	// Clean up the house after the party.
	graphics_clean();
}

/**
 * Sets the current zoom level.
 *
 * @param percentage Percentage of zoom to be applied to the viewport.
 */
void zoom(const int percentage) {
	zoom_level = percentage;
	float scale = (float)zoom_level / 100;
	SDL_RenderSetScale(renderer, scale, scale);
}

/**
 * Sets a new origin point relative to the SDL origin.
 *
 * @param x New x coordinate.
 * @param y New y coordinate.
 */
void set_origin(const int x, const int y) {
	origin.x = x;
	origin.y = y;

#ifdef DEBUG
	printf("New origin set: (%d, %d)\n", (int)origin.x, (int)origin.y);
#endif
}

/**
 * Resets the origin back to a more cartesian place.
 */
void reset_origin() {
	int y = 0;

	// Get window size and set the Y coordinate only.
	SDL_GetWindowSize(window, NULL, &y);
	set_origin(0, y);
}

/**
 *  Check if a key is down.
 *
 *  @param  key SDL_Scancode for the key you want to be tested.
 *  @return     TRUE if the key is down.
 */
bool is_key_down(const SDL_Scancode key) {
	if (keystates != 0) {
		if (keystates[key] == 1) {
			return true;
		} else {
			return false;
		}
	}

	return false;
}
