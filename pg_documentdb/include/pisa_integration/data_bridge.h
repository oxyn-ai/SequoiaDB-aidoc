#pragma once

#ifdef DISABLE_PISA
#include "nodes/pg_list.h"
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

static inline PisaDocument *ConvertBsonDocumentToPisa(const pgbson *bson_doc, int64 collection_id) { return NULL; }
static inline List *ExportCollectionToPisa(const char *database_name, const char *collection_name, PisaExportMode mode) { return NIL; }
static inline bool WritePisaForwardIndex(PisaCollection *collection) { return false; }
static inline bool WritePisaDocumentList(List *documents, const char *output_path) { return false; }
static inline char *ExtractTextContentFromBson(const pgbson *document) { return NULL; }
static inline char *GeneratePisaDocumentId(const pgbson *document, int64 collection_id) { return NULL; }
static inline void FreePisaDocument(PisaDocument *doc) {}
static inline void FreePisaCollection(PisaCollection *collection) {}
#else

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
List *ExportCollectionToPisa(const char *database_name, const char *collection_name, PisaExportMode mode);

bool WritePisaForwardIndex(PisaCollection *collection);
bool WritePisaDocumentList(List *documents, const char *output_path);

char *ExtractTextContentFromBson(const pgbson *document);
char *GeneratePisaDocumentId(const pgbson *document, int64 collection_id);

void FreePisaDocument(PisaDocument *doc);
void FreePisaCollection(PisaCollection *collection);
#endif
