#ifdef DISABLE_PISA
#include "nodes/pg_list.h"
#include "io/bson_core.h"

typedef enum PisaCompressionType
{
    PISA_COMPRESSION_DEFAULT = 0
} PisaCompressionType;

typedef struct PisaQueryContext
{
    char *database_name;
    char *collection_name;
    char *text_query;
    pgbson *filter_criteria;
    int limit;
} PisaQueryContext;

static inline bool PisaIntegrationEnabled(void) { return false; }
static inline List *ExecutePisaTextSearch(PisaQueryContext *context) { return NIL; }
static inline bool CreatePisaTextIndex(const char *database_name, const char *collection_name, bytea *indexOptions, PisaCompressionType compression_type) { return false; }
static inline bool UpdatePisaTextIndex(const char *database_name, const char *collection_name, bytea *indexOptions) { return false; }
static inline bool OptimizePisaTextIndex(const char *database_name, const char *collection_name) { return false; }
#endif
#else


#pragma once

#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"
#include "utils/jsonb.h"
#include "catalog/pg_type.h"

#include "io/bson_core.h"
#include "utils/documentdb_errors.h"

typedef struct PisaIndexConfig
{
    char *collection_name;
    char *database_name;
    char *index_path;
    bool enabled;
    int compression_type;
    bool auto_update;
} PisaIndexConfig;

typedef struct PisaQueryContext
{
    char *query_text;
    char *collection_name;
    char *database_name;
    int limit;
    bool use_pisa;
} PisaQueryContext;

typedef enum PisaCompressionType
{
    PISA_COMPRESSION_NONE = 0,
    PISA_COMPRESSION_BLOCK_SIMDBP = 1,
    PISA_COMPRESSION_BLOCK_INTERPOLATIVE = 2,
    PISA_COMPRESSION_BLOCK_QMXINT = 3
} PisaCompressionType;

extern bool pisa_integration_enabled;
extern char *pisa_index_base_path;
extern int pisa_default_compression;

void InitializePisaIntegration(void);
void ShutdownPisaIntegration(void);

bool CreatePisaIndex(const char *database_name, const char *collection_name, 
                     PisaCompressionType compression_type);
bool UpdatePisaIndex(const char *database_name, const char *collection_name,
                     const pgbson *document, bool is_delete);
bool DropPisaIndex(const char *database_name, const char *collection_name);

List *ExecutePisaTextSearch(PisaQueryContext *context);
bool ShouldUsePisaForQuery(const char *query_json);

char *ConvertBsonToPisaFormat(const pgbson *document);
pgbson *ConvertPisaResultToBson(const char *pisa_result);

void RegisterPisaConfigurationParameters(void);
#endif
