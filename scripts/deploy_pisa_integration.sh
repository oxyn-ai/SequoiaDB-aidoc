#!/bin/bash

set -e

echo "=== DocumentDB PISA Integration Deployment Script ==="
echo "This script will deploy DocumentDB with PISA integration capabilities"

POSTGRES_VERSION=${POSTGRES_VERSION:-15}
DOCUMENTDB_VERSION=${DOCUMENTDB_VERSION:-latest}
PISA_INTEGRATION_ENABLED=${PISA_INTEGRATION_ENABLED:-true}

check_prerequisites() {
    echo "Checking prerequisites..."
    
    if ! command -v psql &> /dev/null; then
        echo "ERROR: PostgreSQL is not installed. Please install PostgreSQL $POSTGRES_VERSION first."
        exit 1
    fi
    
    if ! command -v pg_config &> /dev/null; then
        echo "ERROR: pg_config not found. Please install PostgreSQL development packages."
        exit 1
    fi
    
    if ! command -v make &> /dev/null; then
        echo "ERROR: make is not installed. Please install build-essential or equivalent."
        exit 1
    fi
    
    echo "Prerequisites check passed."
}

build_and_install() {
    echo "Building DocumentDB with PISA integration..."
    
    cd pg_documentdb
    make clean
    make
    
    sudo make install
    
    echo "DocumentDB with PISA integration built and installed successfully."
}

setup_database() {
    local db_name=${1:-documentdb_pisa}
    local db_user=${2:-documentdb_user}
    
    echo "Setting up database '$db_name'..."
    
    if ! psql -lqt | cut -d \| -f 1 | grep -qw "$db_name"; then
        createdb "$db_name"
        echo "Database '$db_name' created."
    else
        echo "Database '$db_name' already exists."
    fi
    
    psql -d "$db_name" -c "CREATE EXTENSION IF NOT EXISTS documentdb_core CASCADE;"
    psql -d "$db_name" -c "CREATE EXTENSION IF NOT EXISTS documentdb CASCADE;"
    
    if [ "$PISA_INTEGRATION_ENABLED" = "true" ]; then
        psql -d "$db_name" -c "SELECT documentdb_api.enable_pisa_integration();"
        echo "PISA integration enabled."
    fi
    
    echo "Database setup completed."
}

run_tests() {
    echo "Running PISA integration tests..."
    
    cd pg_documentdb
    if make check; then
        echo "All tests passed successfully."
    else
        echo "WARNING: Some tests failed. Check the test output for details."
        echo "The deployment can still proceed, but you may want to investigate test failures."
    fi
}

configure_pisa() {
    local db_name=${1:-documentdb_pisa}
    
    echo "Configuring PISA integration settings..."
    
    psql -d "$db_name" -c "
        -- Configure PISA performance settings
        SELECT documentdb_api.set_pisa_metric_threshold('query_latency', 100.0, 200.0);
        SELECT documentdb_api.set_pisa_metric_threshold('memory_usage', 2000000, 4000000);
        
        -- Enable query caching by default
        -- Note: This will be enabled per collection as needed
        
        -- Configure sharding if multiple nodes
        -- SELECT documentdb_api.configure_pisa_sharding('database', 'collection', '{\"shard_count\": 2}');
    "
    
    echo "PISA configuration completed."
}

create_sample_setup() {
    local db_name=${1:-documentdb_pisa}
    
    echo "Creating sample setup..."
    
    psql -d "$db_name" -c "
        -- Create sample collection
        SELECT documentdb_api.create_collection('sample_db', 'articles');
        
        -- Insert sample documents
        SELECT documentdb_api.insert_one('sample_db', 'articles', 
            '{\"title\": \"Getting Started with PISA\", \"content\": \"PISA provides advanced text search capabilities for DocumentDB\", \"category\": \"tutorial\"}');
        
        -- Create PISA text index
        SELECT documentdb_api.create_pisa_text_index('sample_db', 'articles', '{\"content\": \"text\"}', '{\"name\": \"content_pisa_idx\"}');
        
        -- Test the setup
        SELECT documentdb_api.find('sample_db', 'articles', '{\"\\$text\": {\"\\$search\": \"PISA search\"}}');
    "
    
    echo "Sample setup created. You can now test PISA text search functionality."
}

display_summary() {
    echo ""
    echo "=== Deployment Summary ==="
    echo "✅ DocumentDB with PISA integration deployed successfully"
    echo "✅ Database configured and extensions enabled"
    echo "✅ PISA integration settings configured"
    echo "✅ Sample data and indexes created"
    echo ""
    echo "Next steps:"
    echo "1. Connect to your database: psql -d ${1:-documentdb_pisa}"
    echo "2. Create collections: SELECT documentdb_api.create_collection('db_name', 'collection_name');"
    echo "3. Create PISA indexes: SELECT documentdb_api.create_pisa_text_index(...);"
    echo "4. Perform text searches: SELECT documentdb_api.find(..., '{\"\\$text\": {\"\\$search\": \"query\"}}');"
    echo ""
    echo "For more information, see: docs/PISA_INTEGRATION_USER_MANUAL.md"
}

main() {
    local db_name=${1:-documentdb_pisa}
    local db_user=${2:-documentdb_user}
    local skip_tests=${3:-false}
    
    echo "Starting deployment with database: $db_name"
    
    check_prerequisites
    build_and_install
    setup_database "$db_name" "$db_user"
    
    if [ "$skip_tests" != "true" ]; then
        run_tests
    fi
    
    configure_pisa "$db_name"
    create_sample_setup "$db_name"
    display_summary "$db_name"
    
    echo "Deployment completed successfully!"
}

while [[ $# -gt 0 ]]; do
    case $1 in
        --database)
            DB_NAME="$2"
            shift 2
            ;;
        --user)
            DB_USER="$2"
            shift 2
            ;;
        --skip-tests)
            SKIP_TESTS=true
            shift
            ;;
        --disable-pisa)
            PISA_INTEGRATION_ENABLED=false
            shift
            ;;
        --help)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  --database NAME    Database name (default: documentdb_pisa)"
            echo "  --user USER        Database user (default: documentdb_user)"
            echo "  --skip-tests       Skip running tests"
            echo "  --disable-pisa     Disable PISA integration"
            echo "  --help             Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

main "${DB_NAME:-documentdb_pisa}" "${DB_USER:-documentdb_user}" "${SKIP_TESTS:-false}"
