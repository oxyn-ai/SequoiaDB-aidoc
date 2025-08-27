/*-------------------------------------------------------------------------
 * Copyright (c) Microsoft Corporation.  All rights reserved.
 *
 * src/pg_documentdb_docsql.c
 *
 * Initialization of the shared library for the DocumentDB SQL API.
 *-------------------------------------------------------------------------
 */
#include <postgres.h>
#include <fmgr.h>
#include <miscadmin.h>
#include <utils/guc.h>
#include <utils/guc_tables.h>

PG_MODULE_MAGIC;

void _PG_init(void);
void _PG_fini(void);

bool SkipDocumentDBDocSQLLoad = false;

/*
 * _PG_init gets called when the extension is loaded.
 */
void
_PG_init(void)
{
	if (SkipDocumentDBDocSQLLoad)
	{
		return;
	}

	if (!process_shared_preload_libraries_in_progress)
	{
		ereport(ERROR, (errmsg(
							"pg_documentdb_docsql can only be loaded via shared_preload_libraries"),
						errdetail_log(
							"Add pg_documentdb_docsql to shared_preload_libraries configuration "
							"variable in postgresql.conf. ")));
	}

	/* MarkGUCPrefixReserved("documentdb_docsql"); // Function not available in this PostgreSQL version */

	ereport(LOG, (errmsg("Initialized pg_documentdb_docsql extension")));
}

/*
 * _PG_fini is called before the extension is reloaded.
 */
void
_PG_fini(void)
{
	if (SkipDocumentDBDocSQLLoad)
	{
		return;
	}
}
