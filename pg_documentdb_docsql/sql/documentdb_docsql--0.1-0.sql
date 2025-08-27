CREATE SCHEMA documentdb_docsql;

#include "udfs/ddl/create_table--0.1-0.sql"
#include "udfs/ddl/drop_table--0.1-0.sql"
#include "udfs/ddl/create_index--0.1-0.sql"
#include "udfs/ddl/drop_index--0.1-0.sql"

#include "udfs/dml/insert--0.1-0.sql"
#include "udfs/dml/select--0.1-0.sql"
#include "udfs/dml/update--0.1-0.sql"
#include "udfs/dml/delete--0.1-0.sql"

#include "udfs/planner/sql_planner_hooks--0.1-0.sql"

GRANT USAGE ON SCHEMA documentdb_docsql TO PUBLIC;
GRANT EXECUTE ON ALL FUNCTIONS IN SCHEMA documentdb_docsql TO PUBLIC;
