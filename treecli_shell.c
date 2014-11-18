#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

#include "lineedit.h"
#include "treecli_parser.h"
#include "treecli_shell.h"



int32_t treecli_shell_init(struct treecli_shell *sh, const struct treecli_node *top) {
	assert(sh != NULL);

	sh->print_handler = NULL;
	sh->print_handler_ctx = NULL;
	sh->autocomplete = 0;

	/* initialize embedded command parser */
	treecli_parser_init(&(sh->parser), top);
	treecli_parser_set_print_handler(&(sh->parser), treecli_shell_print_handler, (void *)sh);
	treecli_parser_set_match_handler(&(sh->parser), treecli_shell_match_handler, (void *)sh);
	treecli_parser_set_best_match_handler(&(sh->parser), treecli_shell_best_match_handler, (void *)sh);
	sh->parser.allow_exec = 1;

	/* initialize line editing library */
	lineedit_init(&(sh->line), TREECLI_SHELL_LINE_LEN);
	lineedit_set_print_handler(&(sh->line), treecli_shell_print_handler, (void *)sh);
	lineedit_set_prompt_callback(&(sh->line), treecli_shell_prompt_callback, (void *)sh);

	return TREECLI_SHELL_INIT_OK;
}


int32_t treecli_shell_free(struct treecli_shell *sh) {
	assert(sh != NULL);
	treecli_parser_free(&(sh->parser));
	lineedit_free(&(sh->line));

	return TREECLI_SHELL_FREE_OK;
}


int32_t treecli_shell_set_print_handler(struct treecli_shell *sh, int32_t (*print_handler)(const char *line, void *ctx), void *ctx) {
	assert(sh != NULL);
	assert(print_handler != NULL);

	sh->print_handler = print_handler;
	sh->print_handler_ctx = ctx;

	lineedit_refresh(&(sh->line));

	return TREECLI_SHELL_SET_PRINT_HANDLER_OK;
}


int32_t treecli_shell_prompt_callback(struct lineedit *le, void *ctx) {
	assert(le != NULL);
	assert(ctx != NULL);

	uint32_t len = 0;

	struct treecli_shell *sh = (struct treecli_shell *)ctx;

	if (sh->print_handler) {
		sh->print_handler("cli ", sh->print_handler_ctx);
		len += 4;

		uint32_t ret = treecli_parser_pos_print(&(sh->parser));
		if (ret > 0) {
			len += ret;
		}

		sh->print_handler(" > ", sh->print_handler_ctx);
		len += 3;
	}

	return len;
}


int32_t treecli_shell_print_handler(const char *line, void *ctx) {
	assert(line != NULL);
	assert(ctx != NULL);

	struct treecli_shell *sh = (struct treecli_shell *)ctx;

	if (sh->print_handler) {
		sh->print_handler(line, sh->print_handler_ctx);
	}

	return 0;
}


int32_t treecli_shell_match_handler(const char *token, void *ctx) {
	assert(token != NULL);
	assert(ctx != NULL);

	struct treecli_shell *sh = (struct treecli_shell *)ctx;

	printf("%s ", token);

	/* find out of we are right at the end of incomplete token. If yes and
	 * autocompletion is enabled, autocomplete it */

	return 0;
}


int32_t treecli_shell_best_match_handler(const char *token, uint32_t token_len, uint32_t match_pos, uint32_t match_len, void *ctx) {
	assert(token != NULL);
	assert(ctx != NULL);

	struct treecli_shell *sh = (struct treecli_shell *)ctx;

	//~ printf("best match token=%s token_len=%d match_start=%d match_len=%d\n", token, token_len, match_pos, match_len);

	if ((match_pos + match_len) == sh->autocomplete_at && sh->autocomplete) {

		if (match_len < token_len) {
			//~ printf("autocomplete %s len=%d\n", token + match_len, token_len - match_len);
			for (uint32_t i = 0; i < (token_len - match_len); i++) {
				lineedit_insert_char(&(sh->line), token[match_len + i]);
			}

			if (strlen(token) == token_len) {
				lineedit_insert_char(&(sh->line), ' ');
			}

		}
	}

	return 0;
}


int32_t treecli_shell_print_parser_result(struct treecli_shell *sh, int32_t res) {
	assert(sh != NULL);
	if (sh->print_handler == NULL) {
		return TREECLI_SHELL_KEYPRESS_FAILED;
	}

	if (res == TREECLI_PARSER_PARSE_LINE_OK) {
		/* Everything went good, do not print anything, just move on. */
		return TREECLI_SHELL_PRINT_PARSER_RESULT_OK;
	} else {
		/* Error occured, print error position.
		 * TODO: shift depending on prompt length*/
		lineedit_escape_print(&(sh->line), ESC_COLOR, 31);
		lineedit_escape_print(&(sh->line), ESC_BOLD, 0);
		for (uint32_t i = 0; i < (sh->parser.error_pos + sh->line.prompt_len); i++) {
			sh->print_handler("-", sh->print_handler_ctx);
		}
		sh->print_handler("^\n", sh->print_handler_ctx);

		if (res == TREECLI_PARSER_PARSE_LINE_FAILED) {
			sh->print_handler("error: command parsing failed\n", sh->print_handler_ctx);
		}
		if (res == TREECLI_PARSER_PARSE_LINE_MULTIPLE_MATCHES) {
			sh->print_handler("error: multiple matches\n", sh->print_handler_ctx);
		}
		if (res == TREECLI_PARSER_PARSE_LINE_NO_MATCHES) {
			sh->print_handler("error: no match\n", sh->print_handler_ctx);
		}
		if (res == TREECLI_PARSER_PARSE_LINE_CANNOT_MOVE) {
			sh->print_handler("error: cannot change working position\n", sh->print_handler_ctx);
		}
		lineedit_escape_print(&(sh->line), ESC_DEFAULT, 0);
		return TREECLI_SHELL_PRINT_PARSER_RESULT_OK;

	}

	return TREECLI_SHELL_PRINT_PARSER_RESULT_FAILED;
}


int32_t treecli_shell_keypress(struct treecli_shell *sh, int c) {
	assert(sh != NULL);
	if (sh->print_handler == NULL) {
		return TREECLI_SHELL_KEYPRESS_FAILED;
	}

	int32_t ret = lineedit_keypress(&(sh->line), c);

	if (ret == LINEEDIT_ENTER) {
		/* Always move to another line before parsing. */
		sh->print_handler("\r\n", sh->print_handler_ctx);

		/* Line editing is finished (ENTER pressed), get line from line
		 * edit library and try to parse it */
		char *cmd;
		lineedit_get_line(&(sh->line), &cmd);
		sh->parser.allow_exec = 1;
		int32_t parser_ret = treecli_parser_parse_line(&(sh->parser), cmd);

		/* Print parsing results and prepare lineedit for new command. */
		treecli_shell_print_parser_result(sh, parser_ret);
		lineedit_clear(&(sh->line));
		lineedit_refresh(&(sh->line));

		return TREECLI_SHELL_KEYPRESS_OK;
	}

	if (ret == LINEEDIT_TAB) {
		/* always move to next line after <tab> press */
		sh->print_handler("\r\n", sh->print_handler_ctx);

		/* we are autocompleting only at cursor position - get it */
		uint32_t cursor;
		if (lineedit_get_cursor(&(sh->line), &cursor) != LINEEDIT_GET_CURSOR_OK) {
			/* ignore keypress on error */
			return TREECLI_SHELL_KEYPRESS_OK;
		}

		char *cmd;
		lineedit_get_line(&(sh->line), &cmd);

		/* setup the parser */
		sh->parser.allow_exec = 0;
		sh->parser.print_matches = 0;
		sh->parser.allow_suggestions = 0;
		sh->parser.allow_best_match = 0;

		/* try to parse whole command with execution disabled to find out
		 * position of umtiple matches */
		int32_t parser_ret = treecli_parser_parse_line(&(sh->parser), cmd);

		if (parser_ret == TREECLI_PARSER_PARSE_LINE_MULTIPLE_MATCHES) {
			if (cursor == (sh->parser.error_pos + sh->parser.error_len)) {
				/* setup parser again and run it */

				sh->parser.print_matches = 1;
				sh->parser.allow_best_match = 0;
				sh->parser.allow_suggestions = 0;
				sh->autocomplete = 0;
				sh->autocomplete_at = cursor;
				parser_ret = treecli_parser_parse_line(&(sh->parser), cmd);

				sh->print_handler("\r\n", sh->print_handler_ctx);

				sh->parser.print_matches = 1;
				sh->parser.allow_best_match = 1;
				sh->parser.allow_suggestions = 0;
				sh->autocomplete = 1;
				sh->autocomplete_at = cursor;
				parser_ret = treecli_parser_parse_line(&(sh->parser), cmd);

			} else {
				treecli_shell_print_parser_result(sh, parser_ret);
			}
		} else if (parser_ret == TREECLI_PARSER_PARSE_LINE_OK) {
			/* Check if we are on the end of the line. If yes, autocomplete
			 * last token and suggest some more tokens */
			//~ if (cursor == strlen(cmd)) {

				sh->parser.print_matches = 0;
				sh->parser.allow_best_match = 0;
				sh->parser.allow_suggestions = 1;
				sh->autocomplete = 0;
				sh->autocomplete_at = cursor;
				parser_ret = treecli_parser_parse_line(&(sh->parser), cmd);

				sh->print_handler("\r\n", sh->print_handler_ctx);

				sh->parser.print_matches = 0;
				sh->parser.allow_best_match = 1;
				sh->parser.allow_suggestions = 0;
				sh->autocomplete = 1;
				sh->autocomplete_at = cursor;
				parser_ret = treecli_parser_parse_line(&(sh->parser), cmd);

			//~ }
		} else {
			treecli_shell_print_parser_result(sh, parser_ret);
		}

		lineedit_refresh(&(sh->line));

		return TREECLI_SHELL_KEYPRESS_OK;
	}

	return TREECLI_SHELL_KEYPRESS_OK;
}




