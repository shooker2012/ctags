/*
*   $Id: lua.c 443 2006-05-30 04:37:13Z darren $
*
*   Copyright (c) 2000-2001, Max Ischenko <mfi@ukr.net>.
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*   This module contains functions for generating tags for Lua language.
*/

/*
*   INCLUDE FILES
*/
#include "general.h"  /* must always come first */

#include <string.h>

#include "options.h"
#include "parse.h"
#include "read.h"
#include "vstring.h"

#include "entry.h"
#include "gnu_regex/regex.h"

/*
*   DATA DEFINITIONS
*/
typedef enum {
	K_FUNCTION,
	K_CLASS
} luaKind;

static kindOption LuaKinds [] = {
	{ TRUE, 'f', "function", "functions" },
	{ TRUE, 'c', "class", "classes" }
};

/*
*   FUNCTION DEFINITIONS
*/

/* for debugging purposes */
static void __unused__ print_string (char *p, char *q)
{
	for ( ; p != q; p++)
		fprintf (errout, "%c", *p);
	fprintf (errout, "\n");
}

/*
 * Helper function.
 * Returns 1 if line looks like a line of Lua code.
 *
 * TODO: Recognize UNIX bang notation.
 * (Lua treat first line as a comment if it starts with #!)
 *
 */
static boolean is_a_code_line (const unsigned char *line)
{
	boolean result;
	const unsigned char *p = line;
	while (isspace ((int) *p))
		p++;
	if (p [0] == '\0')
		result = FALSE;
	else if (p [0] == '-' && p [1] == '-')
		result = FALSE;
	else
		result = TRUE;
	return result;
}

static void extract_name (const char *begin, const char *end, vString *name)
{
	if (begin != NULL  &&  end != NULL  &&  begin < end)
	{
		const char *cp;

		while (isspace ((int) *begin))
			begin++;
		while (isspace ((int) *end))
			end--;
		if (begin < end)
		{
			for (cp = begin ; cp != end; cp++)
				vStringPut (name, (int) *cp);
			vStringTerminate (name);

			makeSimpleTag (name, LuaKinds, K_FUNCTION);
			vStringClear (name);
		}
	}
}

static void extract_lplus_class_name(const char* class_name)
{
	// printf("extract %s\n", class_name);
	tagEntryInfo e;

	initTagEntry(&e, class_name);

	e.lineNumber = getSourceLineNumber();
	e.filePosition = getInputFilePosition();
	e.isFileScope = true;
	e.kindName = LuaKinds[K_CLASS].name;
	e.kind = LuaKinds[K_CLASS].letter;

	makeTagEntry(&e);
}

static void extract_lplus_function_name(const char* class_name, const char* function_name)
{
	// printf( "extract %s %s\n", class_name, function_name );
	tagEntryInfo e;

	initTagEntry(&e, function_name);

	e.lineNumber = getSourceLineNumber();
	e.filePosition = getInputFilePosition();
	//e.isFileScope = true;
	e.kindName = LuaKinds[K_FUNCTION].name;
	e.kind = LuaKinds[K_FUNCTION].letter;

	e.extensionFields.scope[0] = "class";
	e.extensionFields.scope[1] = class_name;

	makeTagEntry(&e);
}

static void findLuaTags (void)
{
	vString *name = vStringNew ();
	const unsigned char *line;

	const char* class_pattern = "Lplus\\.(Class|Extend)\\(.*\"(\\w+)\".*\\)";
	const char* function_pattern = "def\\.(final|method|virtual|override|static)\\([^\\)]*\\).(\\w+)\\s*=\\s*function\\s*\\(";
	int status = 0;

	regex_t re_class;
	regex_t re_function;

	if (regcomp(&re_class, class_pattern, REG_EXTENDED) != 0 
		|| regcomp(&re_function, function_pattern, REG_EXTENDED) != 0) {
		return;
	}

	vString* lplusClassName = NULL;
	while ((line = fileReadLine ()) != NULL)
	{
		const char *p, *q;

		if (! is_a_code_line (line))
			continue;
		
		// Lplus class
		regmatch_t pmatch[3];
		status = regexec(&re_class, (const char*)line, 3, pmatch, 0);
		if (status == 0)
		{
			lplusClassName = vStringNew();
			vStringNCopyS(lplusClassName, line + pmatch[2].rm_so, pmatch[2].rm_eo - pmatch[2].rm_so);
			extract_lplus_class_name(vStringValue(lplusClassName));
			// printf( "Catch class: %s\n", vStringValue(lplusClassName) );
			continue;
		}


		// Lplus function
		
		status = regexec(&re_function, (const char*)line, 3, pmatch, 0);
		if (status == 0 && lplusClassName != NULL)
		{
			vString* tmp = vStringNew();
			vStringNCopyS(tmp, line + pmatch[2].rm_so, pmatch[2].rm_eo - pmatch[2].rm_so);
			extract_lplus_function_name(vStringValue(lplusClassName), vStringValue(tmp));
			vStringDelete(tmp);
			continue;
		}

		//printf( "Catch!!!!!\n" );
		
		p = (const char*) strstr ((const char*) line, "function");
		if (p == NULL)
			continue;

		q = strchr ((const char*) line, '=');
		
		if (q == NULL) {
			p = p + 9;  /* skip the `function' word */
			q = strchr ((const char*) p, '(');
			extract_name (p, q, name);
		} else {
			p = (const char*) &line[0];
			extract_name (p, q, name);
		}
	}
	vStringDelete (name);
	vStringDelete(lplusClassName);

	regfree(&re_function);
}

extern parserDefinition* LuaParser (void)
{
	static const char* const extensions [] = { "lua", NULL };
	parserDefinition* def = parserNew ("Lua");
	def->kinds      = LuaKinds;
	def->kindCount  = KIND_COUNT (LuaKinds);
	def->extensions = extensions;
	def->parser     = findLuaTags;
	return def;
}

/* vi:set tabstop=4 shiftwidth=4: */
