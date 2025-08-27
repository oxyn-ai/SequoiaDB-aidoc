#ifdef DISABLE_PISA
#include "nodes/pg_list.h"
#include "io/bson_core.h"

static inline List *ExportCollectionToPisa(const char *database_name, const char *collection_name, int mode) { return NIL; }
#endif

#else

#pragma once

#include "postgres.h"
#include "utils/builtins.h"
#include "io/bson_core.h"

typedef struct PisaDocument
{
    char *doc_id;
    char *content;
    int64 collection_id;
    char *metadata;
} PisaDocument;

typedef struct PisaCollection
{
    char *name;
    char *database_name;
    int64 collection_id;
    List *documents;
    char *index_path;
} PisaCollection;

typedef enum PisaExportMode
{
    PISA_EXPORT_FULL = 0,
    PISA_EXPORT_INCREMENTAL = 1
} PisaExportMode;

PisaDocument *ConvertBsonDocumentToPisa(const pgbson *bson_doc, int64 collection_id);
List *ExportCollectionToPisa(const char *database_name, const char *collection_name, 
                             PisaExportMode mode);

bool WritePisaForwardIndex(PisaCollection *collection);
bool WritePisaDocumentList(List *documents, const char *output_path);

char *ExtractTextContentFromBson(const pgbson *document);
char *GeneratePisaDocumentId(const pgbson *document, int64 collection_id);

void FreePisaDocument(PisaDocument *doc);
void FreePisaCollection(PisaCollection *collection);
#endif
