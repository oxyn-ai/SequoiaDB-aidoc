#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(documentdb_docsql_version);
Datum documentdb_docsql_version(PG_FUNCTION_ARGS)
{
	text *t = cstring_to_text("1.0");
	PG_RETURN_TEXT_P(t);
}
