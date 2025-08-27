#include "postgres.h"
#include "fmgr.h"
#include "tcop/utility.h"
#include "commands/event_trigger.h"
#include "nodes/parsenodes.h"
#include "utils/builtins.h"
#include "catalog/namespace.h"

PG_MODULE_MAGIC;

static ProcessUtility_hook_type prev_ProcessUtility = NULL;

static void
docsql_ProcessUtility(PlannedStmt *pstmt,
                      const char *queryString,
                      bool readOnlyTree,
                      ProcessUtilityContext context,
                      ParamListInfo params,
                      QueryEnvironment *queryEnv,
                      DestReceiver *dest,
                      QueryCompletion *qc)
{
	Node *parsetree = pstmt ? pstmt->utilityStmt : NULL;

	if (parsetree && IsA(parsetree, CreateSchemaStmt))
	{
		CreateSchemaStmt *stmt = (CreateSchemaStmt *) parsetree;
		if (stmt->schemaname && strncmp(stmt->schemaname, "docsql_", 7) == 0)
		{
			const char *db = stmt->schemaname + 7;
			StringInfoData buf;
			initStringInfo(&buf);
			appendStringInfo(&buf, "SELECT documentdb_docsql.create_database(%s)",
							 quote_literal_cstr(db));
			Portal portal = NULL;
			List *querytree_list;
			List *plantree_list;

			querytree_list = raw_parser(buf.data, RAW_PARSE_DEFAULT);
			plantree_list = pg_plan_queries(querytree_list, queryString, 0, params);

			ProcessUtilityResult res;
			(void) res;
			if (prev_ProcessUtility)
				prev_ProcessUtility((PlannedStmt *) linitial(plantree_list), buf.data, false,
									PROCESS_UTILITY_TOPLEVEL, params, NULL, dest, qc);
			else
				standard_ProcessUtility((PlannedStmt *) linitial(plantree_list), buf.data, false,
										PROCESS_UTILITY_TOPLEVEL, params, NULL, dest, qc);
		}
	}

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
