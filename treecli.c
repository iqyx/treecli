#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

#include "treecli.h"


int32_t treecli_print_tree(const struct treecli_node *top, int32_t indent) {
	assert(top != NULL);
	
	const struct treecli_node *n = top;
	while (n != NULL) {
		for (int32_t i = 0; i < indent; i++) {
			printf(" ");
		}
		printf("* %s\n", n->name);
		if (n->subnodes) {
			treecli_print_tree(n->subnodes, indent + 4);
		}
		
		n = n->next;
	}
	
	return TREECLI_PRINT_TREE_OK;
}


/**
 * Search command line and try to get next token. Token is a word describing one
 * subnode, command or value name consisting of alphanumeric characters (lower and
 * uppoer case), underscore, dash, dot and slash.
 * Input line position is being incremented during search and after execution it
 * points to a position where the search can continue (this apply also if function
 * fails).
 * 
 * @param parser TODO: remove
 * @param pos Pointer to initial string position where the search should begin.
 * @param token Pointer which will point to start of the token after successful
 *              execution.
 * @param len Length of the token.
 * 
 * @return TREECLI_TOKEN_GET_OK if valid token was found or
 *         TREECLI_TOKEN_GET_UNEXPECTED if invalid characters were found or
 *         TREECLI_TOKEN_GET_NONE if end of line was reached or
 *         TREECLI_TOKEN_GET_FAILED otherwise.
 */
int32_t treecli_token_get(struct treecli_parser *parser, char **pos, char **token, uint32_t *len) {

	/* eat all whitespaces */
	while (**pos == ' ' || **pos == '\t') {
		(*pos)++;
	}

	/* we have reached end of line */
	if (**pos == 0) {
		return TREECLI_TOKEN_GET_NONE;
	}

	/* mark start of the token and go forward while valid characters are found*/
	*token = *pos;
	while ((**pos >= 'a' && **pos <= 'z') || (**pos >= 'A' && **pos <= 'Z') ||
	       (**pos >= '0' && **pos <= '9') || **pos == '-' || **pos == '_' || **pos == '.' || **pos == '/') {
	
		(*pos)++;
	}
	*len = *pos - *token;

	/* no valid token has been found */
	if (*len == 0) {
		return TREECLI_TOKEN_GET_UNEXPECTED;
	}
	
	return TREECLI_TOKEN_GET_OK;
}



/**
 * Initializes parser context. It is used to parse lines of configuration, execute
 * commands and manipulate configuration variables. Parser operates on a
 * configuration tree defined by its top level node.
 * 
 * @param parser A treecli parser context to initialize.
 * @param top Top node of configuration structure used during parsing.
 * 
 * @return TREECLI_PARSER_INIT_OK on success or
 *         TREECLI_PARSER_INIT_FAILED otherwise.
 */
int32_t treecli_parser_init(struct treecli_parser *parser, const struct treecli_node *top) {
	assert(parser != NULL);
	assert(top != NULL);
	
	parser->top = top;
	
	return TREECLI_PARSER_INIT_OK;
}


/**
 * Frees previously created parser context. Nothing to do here yet, placeholder.
 * 
 * @param parser A parser context to free.
 * 
 * @return TREECLI_PARSER_FREE_OK.
 */
int32_t treecli_parser_free(struct treecli_parser *parser) {
	assert(parser != NULL);
	
	return TREECLI_PARSER_FREE_OK;
}


/**
 * Function parses one line of commands and performs configuration tree traversal
 * to set active node for command execution, sets or reads values and executes
 * commands (if requested).
 * 
 * @param parser A parser context used to do command parsing.
 * @param line String with node names, commands and value set/get specifications.
 * 
 * @return TREECLI_PARSER_PARSE_LINE_OK if the whole line was parsed successfully.
 */
int32_t treecli_parser_parse_line(struct treecli_parser *parser, const char *line) {

	int32_t res;
	char *pos = (char *)line;
	char *token = NULL;
	uint32_t len;

	/* TODO: change to start on current treecli parser position instead of top */
	const struct treecli_node *current = parser->top;

	/* Save the current position as error position in case something goes wrong.
	 * It marks context of failed return value (ie. where the error happened) */
	while ((res = treecli_token_get(parser, &pos, &token, &len)) == TREECLI_TOKEN_GET_OK) {

		struct treecli_matches matches;

		int32_t ret = treecli_parser_get_matches(parser, current, token, len, &matches);
		if (ret == TREECLI_PARSER_GET_MATCHES_FAILED) {
			return TREECLI_PARSER_PARSE_LINE_FAILED;
		}

		if (ret == TREECLI_PARSER_GET_MATCHES_NONE) {
			/* return no matches */
			parser->error_pos = (uint32_t)(pos - line) - len;
			return TREECLI_PARSER_PARSE_LINE_NO_MATCHES;
		}
		if (ret == TREECLI_PARSER_GET_MATCHES_MULTIPLE) {
			/* return multiple matches */
			parser->error_pos = (uint32_t)(pos - line) - len;
			return TREECLI_PARSER_PARSE_LINE_MULTIPLE_MATCHES;
		}

		if (matches.count == 1) {

			if (ret == TREECLI_PARSER_GET_MATCHES_TOP) {
				current = parser->top;
				printf("going to top\n");
			}

			if (ret == TREECLI_PARSER_GET_MATCHES_UP) {
				current = parser->top;
				printf("goint up (unimplemented)\n");
			}

			if (ret == TREECLI_PARSER_GET_MATCHES_SUBNODE) {
				current = matches.subnode;
				printf("going to subnode %s\n", current->name);
			}

			if (ret == TREECLI_PARSER_GET_MATCHES_COMMAND) {
				printf("command %s matched\n", matches.command->name);
				if (parser->allow_exec) {
					if (matches.command->exec != NULL) {
						matches.command->exec(parser, matches.command->exec_context);
					}
				}
			}

			if (ret == TREECLI_PARSER_GET_MATCHES_VALUE) {
				printf("value %s matched\n", matches.value->name);
				/* TODO: get operation */
				/* TODO: get literal */
			}
		}
	}

	/* TODO: update parser tree position */
	return TREECLI_PARSER_PARSE_LINE_OK;
}


int32_t treecli_parser_set_print_handler(struct treecli_parser *parser, int32_t (*print_handler)(char *line, void *ctx), void *ctx) {
	assert(parser != NULL);
	assert(print_handler != NULL);

	parser->print_handler = print_handler;
	parser->print_handler_ctx = ctx;

	return TREECLI_PARSER_SET_PRINT_HANDLER_OK;
	
}


int32_t treecli_parser_get_matches(struct treecli_parser *parser, const struct treecli_node *node, char *token, uint32_t len, struct treecli_matches *matches) {
	assert(parser != NULL);
	assert(token != NULL);
	assert(matches != NULL);

	matches->count = 0;
	int32_t ret = TREECLI_PARSER_GET_MATCHES_NONE;

	if (!strncmp(token, "..", 2) && len == 2) {
		matches->count++;
		ret = TREECLI_PARSER_GET_MATCHES_UP;
	}

	if (!strncmp(token, "/", 1) && len == 1) {
		matches->count++;
		ret = TREECLI_PARSER_GET_MATCHES_TOP;
	}

	const struct treecli_node *n = node->subnodes;
	while (n != NULL) {
		if (len <= strlen(n->name) && !strncmp(token, n->name, len)) {
			matches->count++;
			matches->subnode = n;
			ret = TREECLI_PARSER_GET_MATCHES_SUBNODE;
		}
		n = n->next;
	}

	const struct treecli_value *v = node->values;
	while (v != NULL) {
		if (len <= strlen(v->name) && !strncmp(token, v->name, len)) {
			matches->count++;
			matches->value = v;
			ret = TREECLI_PARSER_GET_MATCHES_VALUE;
		}
		v = v->next;
	}

	const struct treecli_command *c = node->commands;
	while (c != NULL) {
		if (len <= strlen(c->name) && !strncmp(token, c->name, len)) {
			matches->count++;
			matches->command = c;
			ret = TREECLI_PARSER_GET_MATCHES_COMMAND;
		}
		c = c->next;
	}

	if (matches->count == 1) {
		return ret;
	} else if (matches->count > 1) {
		return TREECLI_PARSER_GET_MATCHES_MULTIPLE;
	}

	return TREECLI_PARSER_GET_MATCHES_NONE;
}

