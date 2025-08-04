# DocumentDB PISA Control Tools

DocumentDB-integrated PISA control tools for automated index management, monitoring, and optimization.

## Overview

This package provides Python CLI tools that integrate PISA's text search capabilities with DocumentDB's document storage and query processing. It includes:

- **documentdb-pisactl**: Main CLI for index management and querying
- **documentdb-pisa-monitor**: Monitoring and alerting system
- **documentdb-pisa-scheduler**: Automated scheduling for maintenance tasks

## Installation

```bash
cd tools/documentdb_pisactl
pip install -e .
```

## Configuration

Create a configuration file at `~/.documentdb_pisactl.yaml`:

```yaml
database:
  host: localhost
  port: 5432
  database: documentdb
  user: postgres
  password: ""

pisa:
  default_compression: block_simdbp
  default_algorithm: auto
  default_top_k: 10

monitoring:
  enabled: true
  check_interval: 60
  alert_thresholds:
    index_size_mb: 1000
    pending_operations: 100

scheduling:
  enabled: true
  auto_reorder_threshold: 10.0
  max_concurrent_tasks: 2
```

## Usage

### Index Management

```bash
# Enable PISA integration for a collection
documentdb-pisactl index enable mydb mycollection

# Create a PISA index
documentdb-pisactl index create mydb mycollection --compression block_simdbp

# Create a text index with options
documentdb-pisactl index create mydb mycollection --text-index --index-options '{"analyzer": "english"}'

# Check index status
documentdb-pisactl index status

# Rebuild an index
documentdb-pisactl index rebuild mydb mycollection

# Drop an index
documentdb-pisactl index drop mydb mycollection
```

### Querying

```bash
# Execute a text query
documentdb-pisactl query text mydb mycollection "search terms" --limit 20

# Execute an advanced query with algorithm selection
documentdb-pisactl query advanced mydb mycollection term1 term2 --algorithm wand --top-k 10

# Output results as JSON
documentdb-pisactl query text mydb mycollection "search terms" --format json
```

### Document Reordering

```bash
# Schedule document reordering
documentdb-pisactl reorder schedule mydb mycollection --priority 2

# Check reordering status
documentdb-pisactl reorder status

# Get detailed statistics
documentdb-pisactl reorder stats mydb mycollection

# Cancel reordering
documentdb-pisactl reorder cancel mydb mycollection
```

### Data Export

```bash
# Export collection to PISA format
documentdb-pisactl export collection mydb mycollection /path/to/output
```

### Monitoring

```bash
# Run health check once
documentdb-pisa-monitor --once

# Run interactive dashboard
documentdb-pisa-monitor --dashboard

# Run continuous monitoring
documentdb-pisa-monitor
```

### Scheduling

```bash
# Run scheduler as daemon
documentdb-pisa-scheduler --daemon

# Run specific task immediately
documentdb-pisa-scheduler --run-now reordering
documentdb-pisa-scheduler --run-now cleanup
documentdb-pisa-scheduler --run-now optimize
```

## Features

### Index Management
- Enable/disable PISA integration per collection
- Create, rebuild, and drop PISA indexes
- Support for multiple compression algorithms
- Text index creation with analyzer options
- Index status monitoring and health checks

### Advanced Querying
- Multiple query algorithms (WAND, Block-Max-WAND, MaxScore)
- Automatic algorithm selection
- Text search with scoring
- Hybrid queries combining text search and filtering
- Flexible output formats (table, JSON)

### Document Reordering
- Automated scheduling based on collection analysis
- Priority-based task management
- Background worker integration
- Compression improvement tracking
- Detailed statistics and monitoring

### Monitoring and Alerting
- Real-time health monitoring
- Configurable alert thresholds
- Interactive dashboard
- Performance metrics tracking
- Issue detection and reporting

### Automated Scheduling
- Cron-based task scheduling
- Automatic reordering based on thresholds
- Index optimization scheduling
- Task cleanup and maintenance
- Daemon mode support

## Architecture

The tools integrate with DocumentDB through:

1. **PostgreSQL Extension API**: Direct calls to DocumentDB PISA functions
2. **Database Connection Management**: Efficient connection pooling and error handling
3. **Configuration Management**: YAML-based configuration with defaults
4. **Rich CLI Interface**: User-friendly command-line interface with progress indicators
5. **Monitoring System**: Health checks and alerting for proactive maintenance

## Algorithm Support

- **WAND**: Weak AND algorithm for efficient top-k retrieval
- **Block-Max-WAND**: Advanced WAND with block-level upper bounds
- **MaxScore**: MaxScore pruning with essential/non-essential terms
- **Ranked AND**: Traditional ranked AND processing
- **Auto**: Automatic algorithm selection based on query characteristics

## Compression Types

- **block_simdbp**: SIMD-optimized block compression (default)
- **varint**: Variable-byte encoding
- **maskedvbyte**: Masked VByte compression
- **qmx**: QMX compression

## Dependencies

- psycopg2-binary: PostgreSQL database connectivity
- pydantic: Data validation and settings management
- ruamel.yaml: YAML configuration parsing
- click: Command-line interface framework
- rich: Rich text and beautiful formatting
- schedule: Task scheduling library

## Development

```bash
# Install in development mode
pip install -e .

# Run tests
python -m pytest tests/

# Format code
black src/

# Type checking
mypy src/
```

## License

Apache License 2.0 - see LICENSE file for details.
