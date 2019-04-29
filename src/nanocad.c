/**
 * nanocad.c
 * A tiny CAD engine with a super simple syntax designed to be easily embeddable.
 *
 * @author Nathan Campos <nathanpc@dreamintech.net>
 */

#include "nanocad.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

// Line parsing stage definitions.
#define PARSING_START      0
#define PARSING_COMMAND    1
#define PARSING_ARGUMENTS  2
#define PARSING_COORDX     3
#define PARSING_COORDY     4
#define PARSING_WIDTH      6
#define PARSING_HEIGHT     7
#define PARSING_NUMBER     8
#define PARSING_UNIT       9
#define PARSING_SET_OBJVAR 10

// Operation type definitions.
#define OPERATION_WIDTH  'w'
#define OPERATION_HEIGHT 'h'

// Variable type definitions.
#define VARIABLE_FIXED  '$'
#define VARIABLE_COORD  '@'
#define VARIABLE_OBJECT '&'

// Stored structures.
object_container objects;
variable_container variables;

// Command type definitions.
#define VALID_OBJECTS_SIZE 3
char valid_objects[VALID_OBJECTS_SIZE][COMMAND_MAX_SIZE] = { "line",
															 "rect",
															 "circle" };

/**
 * Internal functions.
 */
// Various utilities.
void chomp(char *str);
int is_obj_command(const char *command);
long to_base_unit(const char *str);

// Variables.
void set_variable(const char *name, const char *value);

// Parsing.
int parse_line(const char *line, char *command, char **arguments);
void parse_coordinates(coord_t *coord, const char *arg, const coord_t *base);

// Coordinates.
void calc_coordinate(const char oper, const coord_t base, coord_t *coord);

// Objects.
void create_object(const int type, const char argc, char **argv);

/**
 * Initializes the engine.
 */
void nanocad_init() {
	objects.count = 0;
	variables.count = 0;
}

/**
 * Sets a internal variable.
 * 
 * @param name  Variable name.
 * @param value Variable value.
 */
void set_variable(const char *name, const char *value) {
	size_t obj_index;
	variable_t var;
	var.type = *name++;
	var.name = strdup(name);

	// TODO: Check if already exists, if so, override.
	
	// Parse variable value according to type.
	switch (var.type) {
	case VARIABLE_FIXED:
		// Fixed value.
		var.value = malloc(sizeof(double));
		*((double*)var.value) = atof(value);
		break;
	case VARIABLE_COORD:
		// Coordinate.
		var.value = malloc(sizeof(coord_t));
		parse_coordinates((coord_t*)var.value, value, NULL);
		break;
	case VARIABLE_OBJECT:
		// Object.
		var.value = malloc(sizeof(size_t));

		if (sscanf(value, "%zu", &obj_index) == 1) {
			*((size_t*)var.value) = obj_index;
		} else {
			printf("Couldn't parse object index when assigning to variable.\n");
			exit(EXIT_FAILURE);
		}
		break;
	default:
		printf("Invalid variable type '%c' in %s\n", var.type, var.name);
		exit(EXIT_FAILURE);
	}

	// Dynamically add the new variable to the array.
	variables.list = realloc(variables.list,
							 sizeof(variable_t) * (variables.count + 1));
	variables.list[variables.count] = var;
	variables.count++;
}

/**
 * Prints some debug information about a variable.
 * 
 * @param var Variable to be debuged.
 */
void print_variable_info(const variable_t var) {
	printf("Variable Type: %c - ", var.type);

	// Print human-readable type.
	switch (var.type) {
	case VARIABLE_FIXED:
		printf("Fixed Value\n");
		break;
	case VARIABLE_COORD:
		printf("Coordinate\n");
		break;
	case VARIABLE_OBJECT:
		printf("Object\n");
		break;
	default:
		printf("UNKNOWN\n");
	}

	printf("Name: %s - Value: ", var.name);
	
	// Print value according to type.
	switch (var.type) {
	case VARIABLE_FIXED:
		printf("%f\n", *((double*)var.value));
		break;
	case VARIABLE_COORD:
		printf("(%lu, %lu)\n", ((coord_t*)var.value)->x,
			   ((coord_t*)var.value)->y);
		break;
	case VARIABLE_OBJECT:
		print_object_info(objects.list[*((size_t*)var.value)]);
		break;
	default:
		printf("UNKNOWN\n");
	}
}

/**
 * Calculates the coordinates based on a operation.
 *
 * @param oper  Operation type.
 * @param base  Base coordinate.
 * @param coord Operation specific coordinate.
 */
void calc_coordinate(const char oper, const coord_t base, coord_t *coord) {
	switch (oper) {
	case OPERATION_WIDTH:
		// Using coord->x to store the width already.
		coord->x += base.x;
		coord->y = base.y;
		break;
	case OPERATION_HEIGHT:
		// Using coord->y to store the height already.
		coord->x = base.x;
		coord->y += base.y;
		break;
	default:
		printf("Invalid coordinate operation: %c.\n", oper);
		exit(EXIT_FAILURE);
	}
}

/**
 * Parses a coordinate argument into a internal coordinate structure.
 *
 * @param coord Pointer to the output of the function.
 * @param arg   The argument raw string to be parsed.
 * @param base  A base coordinate for "length calculations", can be NULL if
 *              there isn't one.
 */
void parse_coordinates(coord_t *coord, const char *arg, const coord_t *base) {
	uint8_t stage = PARSING_START;
	uint8_t cur_pos = 0;
	char operation = '\0';
	char coord_x[ARGUMENT_MAX_SIZE];
	char coord_y[ARGUMENT_MAX_SIZE];
	coord_x[0] = '0';
	coord_y[0] = '0';
	coord_x[1] = '\0';
	coord_y[1] = '\0';

	// Iterate over the argument string until we hit the NULL terminator.
	while (*arg != '\0') {
		// Get the current character.
		char c = *arg++;

		switch (stage) {
		case PARSING_START:
			if (c == 'x') {
				cur_pos = 0;
				stage = PARSING_COORDX;
			} else if ((c == 'w') || (c == 'h')) {
				// TODO: Make sure we can parse something like w10cm;h25cm.
				cur_pos = 0;
				operation = c;

				if (c == 'w') {
					stage = PARSING_WIDTH;
				} else if (c == 'h') {
					stage = PARSING_HEIGHT;
				}
			} else {
				printf("Unknown first coordinate letter: %c.\n", c);
				exit(EXIT_FAILURE);
			}
			break;
		case PARSING_COORDX:
			if (c == ';') {
				cur_pos = 0;
				stage = PARSING_ARGUMENTS;
			} else {
				coord_x[cur_pos++] = c;
				coord_x[cur_pos] = '\0';
			}
			break;
		case PARSING_ARGUMENTS:
			if (c == 'y') {
				cur_pos = 0;
				stage = PARSING_COORDY;
			} else {
				printf("Unknown next argument start for coordinate: %c.\n", c);
				exit(EXIT_FAILURE);
			}
			break;
		case PARSING_COORDY:
			coord_y[cur_pos++] = c;
			coord_y[cur_pos] = '\0';
			break;
		case PARSING_WIDTH:
			coord_x[cur_pos++] = c;
			coord_x[cur_pos] = '\0';
			break;
		case PARSING_HEIGHT:
			coord_y[cur_pos++] = c;
			coord_y[cur_pos] = '\0';
			break;
		}
	}

	// Convert coordinates.
	coord->x = to_base_unit(coord_x);
	coord->y = to_base_unit(coord_y);

	// Looks like we'll need to calculate some stuff.
	if ((base != NULL) && (operation != '\0')) {
		calc_coordinate(operation, *base, coord);
	}
}

/**
 * Creates a object in the object array.
 *
 * @param type Object type.
 * @param argc Number of arguments passed by the command.
 * @param argv Aguments passed by the command.
 */
void create_object(const int type, const char argc, char **argv) {
	// Create a new object.
	object_t obj;
	obj.type = (uint8_t)type;

	// Allocate the correct amount of memory for each type of object.
	switch (type) {
	case TYPE_LINE:
		obj.coord_count = 2;
		obj.coord = (coord_t *)malloc(sizeof(coord_t) * 2);
		parse_coordinates(&obj.coord[0], argv[0], NULL);
		parse_coordinates(&obj.coord[1], argv[1], &obj.coord[0]);
		break;
	}

	// Dynamically add the new object to the array.
	objects.list = realloc(objects.list,
						   sizeof(object_t) * (objects.count + 1));
	objects.list[objects.count++] = obj;
	
	// Check if we need to store this object into a variable.
	if (argv[argc - 1][0] == '&') {
		// Pass the object index as a string to be compliant with the function.
		char str_idx[VARIABLE_MAX_SIZE];
		snprintf(str_idx, VARIABLE_MAX_SIZE, "%lu", objects.count - 1);
		set_variable(argv[argc - 1], str_idx);
		
#ifdef DEBUG
		print_variable_info(variables.list[variables.count - 1]);
#endif
	}
}

/**
 * Parses a command and executes it.
 *
 * @param  line A command line without the newline character at the end.
 * @return      TRUE if the parsing went fine.
 */
bool parse_command(const char *line) {
	int argc;
	char command[COMMAND_MAX_SIZE];
	char *argv[ARGUMENT_ARRAY_MAX_SIZE];

	// Ignoring empty lines and comments.
	if ((line[0] == '\0') || (line[0] == '#')) {
		return true;
	}

	// Parse the line.
	if ((argc = parse_line(line, command, argv)) >= 0) {
#ifdef DEBUG
		printf("Command: %s - Arg. Count: %d\n", command, argc);
		for (int i = 0; i < argc; i++) {
			printf("Argument %d: %s\n", i, argv[i]);
		}
#endif
		// Check which type of command this is.
		int type = -1;
		if ((type = is_obj_command(command)) > 0) {
			// Command will generate a object.
			create_object(type, argc, argv);
#ifdef DEBUG
			print_object_info(objects.list[objects.count - 1]);
#endif
		} else if (strcmp("set", command) == 0) {
			// Command will set a variable.
			set_variable(argv[0], argv[1]);
#ifdef DEBUG
			print_variable_info(variables.list[variables.count - 1]);
#endif
		} else {
			// Not a known command.
			printf("Unknown command.\n");
			return false;
		}

		return true;
	}

	return false;
}

/**
 * Converts a raw string to a long in the base unit.
 *
 * @param  str Raw string in a coordinate form.
 * @return     Number in the base unit.
 */
long to_base_unit(const char *str) {
	long num = 0;
	char unit[3];
	char strnum[ARGUMENT_MAX_SIZE];
	uint8_t stage = PARSING_NUMBER;
	uint8_t cur_pos = 0;
	unit[0] = '\0';

	// Iterate over the coordinate string until we hit the NULL terminator.
	while (*str != '\0') {
		// Get the current character.
		char c = *str++;

		switch (stage) {
		case PARSING_NUMBER:
			if (((c >= 0x30) && (c <= 0x39)) || 
				((c >= 0x2B) && (c <= 0x2E))) {
				// Character is a number.
				strnum[cur_pos++] = c;
				strnum[cur_pos] = '\0';
			} else if ((c >= 0x61) && (c <= 0x7A)) {
				// Character is a letter.
				cur_pos = 0;
				stage = PARSING_UNIT;
				unit[cur_pos++] = c;
				unit[cur_pos] = '\0';
			} else {
				// Invalid character.
				printf("Invalid character found while trying to parse a "
					   "number: %c.\n", c);
				exit(EXIT_FAILURE);
			}
			break;
		case PARSING_UNIT:
			if ((c >= 0x61) && (c <= 0x7A)) {
				// Character is a letter.
				unit[cur_pos++] = c;
				unit[cur_pos] = '\0';
			} else {
				// Invalid character.
				printf("Invalid character found while trying to parse a "
					   "unit: %c.\n", c);
				exit(EXIT_FAILURE);
			}
			break;
		}
	}

	// Parse number from string and convert it.
	double orig = atof(strnum);
	if (unit[0] == '\0') {
		// Already at the base unit.
		num = (long)orig;
	} else if (!strcmp(unit, "m")) {
		// Meters.
		num = (long)(orig * 1000);
	} else if (!strcmp(unit, "cm")) {
		// Centimeters.
		num = (long)(orig * 10);
	} else if (!strcmp(unit, "mm")) {
		// Millimeters.
		num = (long)orig;
	} else {
		// Invalid unit.
		printf("Invalid unit: %s\n", unit);
		exit(EXIT_FAILURE);
	}

#ifdef DEBUG
	printf("Number: %s - Unit: %s - Double: %f - Final: %ld\n", strnum, unit, orig, num);
#endif

	return num;
}

/**
 * Checks if a command is of object type and returns the correct type for it.
 *
 * @param  command Command string to be checked.
 * @return         Object type if valid, otherwise -1.
 */
int is_obj_command(const char *command) {
	for (uint8_t i = 0; i < VALID_OBJECTS_SIZE; i++) {
		if (strcmp(command, valid_objects[i]) == 0) {
			return i + 1;
		}
	}

	return -1;
}

/**
 * Prints some debug information about a given object.
 *
 * @param object Object that you want to get information from.
 */
void print_object_info(const object_t object) {
	printf("Object Type: %d - %s\n", object.type, valid_objects[object.type - 1]);
	printf("Coordinates (%d total):\n", object.coord_count);

	for (uint8_t i = 0; i < object.coord_count; i++) {
		printf("    %d. (%ld, %ld)\n", i, object.coord[i].x, object.coord[i].y);
	}
}

/**
 * Parses a command line and separates each part.
 *
 * @param  line      The command line without the newline character at the end.
 * @param  command   Pointer to a string that will contain the command after
 *                   parsing.
 * @param  arguments Array of strings that will contain the arguemnts.
 * @return           Number of arguments found for the command or -1 if there
 *                   was an error while parsing.
 */
int parse_line(const char *line, char *command, char **arguments) {
	uint8_t stage = PARSING_COMMAND;
	uint16_t cur_cpos = 0;
	int argc = -1;
	char cur_arg[ARGUMENT_MAX_SIZE];
	
	// Reset the command string.
	command[0] = '\0';

	// Iterate over the line string until we hit the NULL terminator.
	while (*line != '\0') {
		// Get the current character.
		char c = *line++;
		
		// Ignore everything after the start of a comment.
		if (c == '#') {
			break;
		}

		// Treat each stage of parsing differently.
		switch (stage) {
		case PARSING_COMMAND:
			if ((c == ' ') || (c == '\t')) {
				// Space found, so the command part has ended.
				command[cur_cpos] = '\0';
				chomp(command);
				cur_cpos = 0;
				argc = 0;
				stage = PARSING_ARGUMENTS;
			} else {
				if ((cur_cpos + 1) < COMMAND_MAX_SIZE) {
					// Append characters to the command string.
					command[cur_cpos] = c;
					cur_cpos++;
				} else {
					printf("ERROR: Command maximum character limit "
						   "exceeded.\n");
					return -1;
				}
			}
			break;
		case PARSING_ARGUMENTS:
			if (c == ',') {
				// Comma found, so the argument has ended.
				chomp(cur_arg);
				arguments[argc - 1] = strdup(cur_arg);
				cur_cpos = 0;
				cur_arg[0] = '\0';

				if (argc == ARGUMENT_ARRAY_MAX_SIZE) {
					printf("Maximum number of arguments exceeded.\n");
					return -1;
				}
			} else if ((c == ' ') || (c == '\t')) {
				// Ignoring the spaces or tabs.
			} else if (c == '=') {
				// We need to make sure we'll store this into a variable.
				chomp(cur_arg);
				arguments[argc - 1] = strdup(cur_arg);
				cur_cpos = 0;
				stage = PARSING_SET_OBJVAR;
			} else {
				// Increment the argument counter if at start of an argument.
				if (cur_cpos == 0) {
					argc++;
				}

				if ((cur_cpos + 1) < ARGUMENT_MAX_SIZE) {
					cur_arg[cur_cpos++] = c;
					cur_arg[cur_cpos] = '\0';
				} else {
					printf("Maximum argument character size exceeded on "
						   "argument number %d.\n", argc);
					return -1;
				}
			}
			break;
		case PARSING_SET_OBJVAR:
			// Check if we are starting with the right variable type.
			if (cur_cpos == 0) {
				if ((c != ' ') && (c != '\t') && (c != '&')) {
					printf("Unknown first character for a object variable "
						   "'%c'\n", c);
					return -1;
				} else if (c == '&') {
					cur_arg[cur_cpos++] = c;
					cur_arg[cur_cpos] = '\0';
				}
			} else if ((c == ' ') || (c == '\t')) {
				// Reached a space. The variable must have ended.
				break;
			} else {
				// Get variable name.
				cur_arg[cur_cpos++] = c;
				cur_arg[cur_cpos] = '\0';
			}
			break;
		default:
			printf("ERROR: Unknown line parsing state. This shouldn't "
				   "happen.\n");
			return -1;
		}
	}

	// Leave the object variable as a argument to be dealt with later.
	if (stage == PARSING_SET_OBJVAR) {
		argc++;
	}

	// Store the last argument parsed.
	if (argc > 0) {
		chomp(cur_arg);
		arguments[argc - 1] = strdup(cur_arg);
	}

	return argc;
}

/**
 * Parses a nanoCAD formatted file.
 *
 * @param  filename Path to the file to be parsed.
 * @return          TRUE if everything went OK.
 */
bool parse_file(const char *filename) {
	// Open the CAD file for parsing.
	FILE *fp = fopen(filename, "r");
	if (fp == NULL) {
		printf("Couldn't open the CAD file: %s\n", filename);
		return false;
	}

	// Go through each line.
	char *line;
	size_t len = 0;
	ssize_t read;
	unsigned int linenum = 1;
	while ((read = getline(&line, &len, fp)) != -1) {
		// Remove the trailling newline.
		if (line[read - 1] == '\n') {
			line[read - 1] = '\0';
		}

#ifdef DEBUG
		if (linenum > 1) {
			printf("\n\n");
		}

		printf("Line %d: %s\n", linenum, line);
#endif

		// Parse lines.
		if (!parse_command(line)) {
			printf("Failed to parse line %d: %s\n", linenum, line);
			return false;
		}

		linenum++;
	}

	// Some clean-up.
	fclose(fp);
	free(line);

	return true;
}

/**
 * Removes trailling whitespace from a string.
 * Warning: This function modifies the original string.
 * 
 * @param str String to be trimmed.
 */
void chomp(char *str) {
	for (int i = strlen(str) - 1; i >= 0; i--) {
		if (isspace(str[i])) {
			str[i] = '\0';
		}
	}
}

/**
 * Retrieves the internal object container for external use.
 *
 * @param container Pointer that will be pointing to the internal object
 * container.
 */
void get_container(object_container *container) {
	*container = objects;
}

/**
 * Gets a object from the objects array.
 *
 * @param  i Index of the object to be fetched.
 * @return   The requested object.
 */
object_t get_object(const size_t i) {
	return objects.list[i];
}

