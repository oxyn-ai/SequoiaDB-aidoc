#ifdef DISABLE_PISA
#include "postgres.h"
#include "nodes/pg_list.h"
#include "io/bson_core.h"
#include "pisa_integration/data_bridge.h"

List *ExportCollectionToPisa(const char *database_name, const char *collection_name, int mode) { return NIL; }
#else
#else


#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/jsonb.h"
#include "catalog/pg_type.h"
#include "access/htup_details.h"

#include "io/bson_core.h"
#include "io/bson_traversal.h"
#include "pisa_integration/data_bridge.h"
#include "metadata/collection.h"

PisaDocument *
ConvertBsonDocumentToPisa(const pgbson *bson_doc, int64 collection_id)
{
    PisaDocument *pisa_doc;
    char *text_content;
    char *doc_id;

    if (bson_doc == NULL)
        return NULL;

    pisa_doc = (PisaDocument *) palloc0(sizeof(PisaDocument));
    
    doc_id = GeneratePisaDocumentId(bson_doc, collection_id);
    text_content = ExtractTextContentFromBson(bson_doc);

    pisa_doc->doc_id = pstrdup(doc_id);
    pisa_doc->content = pstrdup(text_content);
    pisa_doc->collection_id = collection_id;
    pisa_doc->metadata = NULL;

    return pisa_doc;
}

List *
ExportCollectionToPisa(const char *database_name, const char *collection_name, 
                       PisaExportMode mode)
{
    List *documents = NIL;
    List *bson_documents;
    ListCell *cell;
    int64 collection_id;

    elog(LOG, "Exporting collection %s.%s to PISA format (mode: %d)", 
         database_name, collection_name, mode);

    collection_id = GetCollectionId(database_name, collection_name);
    if (collection_id == InvalidOid)
    {
        elog(WARNING, "Collection %s.%s not found", database_name, collection_name);
        return NIL;
    }

    PG_TRY();
    {
        if (mode == PISA_EXPORT_FULL)
        {
            bson_documents = GetAllDocumentsFromCollection(database_name, collection_name);
        }
        else
        {
            bson_documents = GetModifiedDocumentsFromCollection(database_name, collection_name);
        }

        foreach(cell, bson_documents)
        {
            pgbson *bson_doc = (pgbson *) lfirst(cell);
            PisaDocument *pisa_doc = ConvertBsonDocumentToPisa(bson_doc, collection_id);
            
            if (pisa_doc != NULL)
            {
                documents = lappend(documents, pisa_doc);
            }
        }

        elog(LOG, "Successfully exported %d documents from collection %s.%s", 
             list_length(documents), database_name, collection_name);
    }
    PG_CATCH();
    {
        elog(ERROR, "Failed to export collection %s.%s to PISA format", 
             database_name, collection_name);
    }
    PG_END_TRY();

    return documents;
}

bool
WritePisaForwardIndex(PisaCollection *collection)
{
    char forward_index_path[MAXPGPATH];
    FILE *forward_file;
    ListCell *cell;
    int doc_count = 0;

    if (collection == NULL || collection->documents == NIL)
        return false;

    snprintf(forward_index_path, MAXPGPATH, "%s.forward", collection->index_path);

    forward_file = fopen(forward_index_path, "w");
    if (forward_file == NULL)
    {
        elog(ERROR, "Failed to create forward index file: %s", forward_index_path);
        return false;
    }

    PG_TRY();
    {
        foreach(cell, collection->documents)
        {
            PisaDocument *doc = (PisaDocument *) lfirst(cell);
            
            fprintf(forward_file, "%s\t%s\n", doc->doc_id, doc->content);
            doc_count++;
        }

        fclose(forward_file);
        
        elog(LOG, "Successfully wrote %d documents to forward index: %s", 
             doc_count, forward_index_path);
    }
    PG_CATCH();
    {
        if (forward_file)
            fclose(forward_file);
        elog(ERROR, "Failed to write forward index file: %s", forward_index_path);
        return false;
    }
    PG_END_TRY();

    return true;
}

bool
WritePisaDocumentList(List *documents, const char *output_path)
{
    char doc_list_path[MAXPGPATH];
    FILE *doc_file;
    ListCell *cell;
    int doc_count = 0;

    if (documents == NIL)
        return false;

    snprintf(doc_list_path, MAXPGPATH, "%s.docs", output_path);

    doc_file = fopen(doc_list_path, "w");
    if (doc_file == NULL)
    {
        elog(ERROR, "Failed to create document list file: %s", doc_list_path);
        return false;
    }

    PG_TRY();
    {
        foreach(cell, documents)
        {
            PisaDocument *doc = (PisaDocument *) lfirst(cell);
            
            fprintf(doc_file, "%ld\t%s\t%s\n", 
                   doc->collection_id, doc->doc_id, doc->content);
            doc_count++;
        }

        fclose(doc_file);
        
        elog(LOG, "Successfully wrote %d documents to file: %s", 
             doc_count, doc_list_path);
    }
    PG_CATCH();
    {
        if (doc_file)
            fclose(doc_file);
        elog(ERROR, "Failed to write document list file: %s", doc_list_path);
        return false;
    }
    PG_END_TRY();

    return true;
}

char *
ExtractTextContentFromBson(const pgbson *document)
{
    StringInfoData text_content;
    bson_iter_t iter;
    bson_t *bson_doc;

    if (document == NULL)
        return pstrdup("");

    initStringInfo(&text_content);

    bson_doc = pgbson_get_bson(document);
    if (!bson_iter_init(&iter, bson_doc))
    {
        return pstrdup("");
    }

    while (bson_iter_next(&iter))
    {
        const char *key = bson_iter_key(&iter);
        bson_type_t type = bson_iter_type(&iter);

        if (strcmp(key, "_id") == 0)
            continue;

        switch (type)
        {
            case BSON_TYPE_UTF8:
            {
                const char *str_value = bson_iter_utf8(&iter, NULL);
                if (text_content.len > 0)
                    appendStringInfoChar(&text_content, ' ');
                appendStringInfoString(&text_content, str_value);
                break;
            }
            case BSON_TYPE_DOCUMENT:
            case BSON_TYPE_ARRAY:
            {
                bson_iter_t sub_iter;
                if (bson_iter_recurse(&iter, &sub_iter))
                {
                    while (bson_iter_next(&sub_iter))
                    {
                        if (bson_iter_type(&sub_iter) == BSON_TYPE_UTF8)
                        {
                            const char *str_value = bson_iter_utf8(&sub_iter, NULL);
                            if (text_content.len > 0)
                                appendStringInfoChar(&text_content, ' ');
                            appendStringInfoString(&text_content, str_value);
                        }
                    }
                }
                break;
            }
            default:
                break;
        }
    }

    return text_content.data;
}

char *
GeneratePisaDocumentId(const pgbson *document, int64 collection_id)
{
    bson_iter_t iter;
    bson_t *bson_doc;
    char *doc_id;
    StringInfoData id_buffer;

    if (document == NULL)
        return pstrdup("unknown");

    initStringInfo(&id_buffer);

    bson_doc = pgbson_get_bson(document);
    if (bson_iter_init_find(&iter, bson_doc, "_id"))
    {
        bson_type_t type = bson_iter_type(&iter);
        
        switch (type)
        {
            case BSON_TYPE_OID:
            {
                const bson_oid_t *oid = bson_iter_oid(&iter);
                char oid_str[25];
                bson_oid_to_string(oid, oid_str);
                appendStringInfoString(&id_buffer, oid_str);
                break;
            }
            case BSON_TYPE_UTF8:
            {
                const char *str_id = bson_iter_utf8(&iter, NULL);
                appendStringInfoString(&id_buffer, str_id);
                break;
            }
            case BSON_TYPE_INT32:
            {
                int32_t int_id = bson_iter_int32(&iter);
                appendStringInfo(&id_buffer, "%d", int_id);
                break;
            }
            case BSON_TYPE_INT64:
            {
                int64_t long_id = bson_iter_int64(&iter);
                appendStringInfo(&id_buffer, "%ld", long_id);
                break;
            }
            default:
                appendStringInfo(&id_buffer, "col_%ld_unknown", collection_id);
                break;
        }
    }
    else
    {
        appendStringInfo(&id_buffer, "col_%ld_auto_%p", collection_id, document);
    }

    doc_id = pstrdup(id_buffer.data);
    pfree(id_buffer.data);

    return doc_id;
}

void
FreePisaDocument(PisaDocument *doc)
{
    if (doc == NULL)
        return;

    if (doc->doc_id)
        pfree(doc->doc_id);
    if (doc->content)
        pfree(doc->content);
    if (doc->metadata)
        pfree(doc->metadata);
    
    pfree(doc);
}

void
FreePisaCollection(PisaCollection *collection)
{
    ListCell *cell;

    if (collection == NULL)
        return;

    if (collection->name)
        pfree(collection->name);
    if (collection->database_name)
        pfree(collection->database_name);
    if (collection->index_path)
        pfree(collection->index_path);

    foreach(cell, collection->documents)
    {
        PisaDocument *doc = (PisaDocument *) lfirst(cell);
        FreePisaDocument(doc);
    }

    list_free(collection->documents);
    pfree(collection);
}

static List *
GetAllDocumentsFromCollection(const char *database_name, const char *collection_name)
{
    return NIL;
}

static List *
GetModifiedDocumentsFromCollection(const char *database_name, const char *collection_name)
{
    return NIL;
}
#endif
