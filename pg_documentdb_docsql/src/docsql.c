#include "postgres.h"
#include "fmgr.h"
#include "tcop/utility.h"
#include "commands/event_trigger.h"

PG_MODULE_MAGIC;

static ProcessUtility_hook_type prev_ProcessUtility = NULL;

static void docsql_ProcessUtility(PlannedStmt *pstmt,
                                  const char *queryString,
                                  bool readOnlyTree,
                                  ProcessUtilityContext context,
                                  ParamListInfo params,
                                  QueryEnvironment *queryEnv,
                                  DestReceiver *dest,
                                  QueryCompletion *qc)
{
	if (prev_ProcessUtility)
	{
		prev_ProcessUtility(pstmt, queryString, readOnlyTree, context, params, queryEnv, dest, qc);
		return;
	}
	standard_ProcessUtility(pstmt, queryString, readOnlyTree, context, params, queryEnv, dest, qc);
}

void _PG_init(void)
{
	prev_ProcessUtility = ProcessUtility_hook;
	ProcessUtility_hook = docsql_ProcessUtility;
}

void _PG_fini(void)
{
	ProcessUtility_hook = prev_ProcessUtility;
}

PG_FUNCTION_INFO_V1(documentdb_docsql_version);
Datum documentdb_docsql_version(PG_FUNCTION_ARGS)
{
	text *t = cstring_to_text("1.0");
	PG_RETURN_TEXT_P(t);
}
