"""
DocumentDB-integrated PISA command-line interface.

Based on PISA's pisactl but integrated with DocumentDB APIs.
"""

import click
import json
import sys
import logging
from typing import Dict, List, Optional
from rich.console import Console
from rich.table import Table
from rich.progress import Progress, SpinnerColumn, TextColumn
from rich.panel import Panel

from .database import DocumentDBConnection
from .config import load_config, save_config, ConfigManager

console = Console()
logger = logging.getLogger(__name__)

ALGORITHM_MAP = {
    "wand": 1,
    "block_max_wand": 2,
    "maxscore": 3,
    "ranked_and": 4,
    "auto": 5
}

COMPRESSION_TYPES = {
    "block_simdbp": 1,
    "varint": 2,
    "maskedvbyte": 3,
    "qmx": 4
}


@click.group()
@click.option("--config", "-c", default="~/.documentdb_pisactl.yaml", 
              help="Configuration file path")
@click.option("--host", default="localhost", help="DocumentDB host")
@click.option("--port", default=5432, help="DocumentDB port")
@click.option("--database", default="documentdb", help="Database name")
@click.option("--user", default="postgres", help="Database user")
@click.option("--password", default="", help="Database password")
@click.option("--verbose", "-v", is_flag=True, help="Enable verbose logging")
@click.pass_context
def cli(ctx, config, host, port, database, user, password, verbose):
    """DocumentDB-integrated PISA control tools."""
    if verbose:
        logging.basicConfig(level=logging.DEBUG)
    else:
        logging.basicConfig(level=logging.INFO)
    
    ctx.ensure_object(dict)
    ctx.obj["config"] = ConfigManager(config)
    ctx.obj["db"] = DocumentDBConnection(
        host=host, port=port, database=database, 
        user=user, password=password
    )


@cli.group()
@click.pass_context
def index(ctx):
    """Index management commands."""
    pass


@index.command("enable")
@click.argument("database_name")
@click.argument("collection_name")
@click.option("--compression", "-c", default="block_simdbp", 
              type=click.Choice(list(COMPRESSION_TYPES.keys())),
              help="Compression type")
@click.pass_context
def enable_index(ctx, database_name, collection_name, compression):
    """Enable PISA integration for a collection."""
    db = ctx.obj["db"]
    compression_type = COMPRESSION_TYPES[compression]
    
    with Progress(SpinnerColumn(), TextColumn("[progress.description]{task.description}")) as progress:
        task = progress.add_task("Enabling PISA integration...", total=None)
        
        try:
            success = db.enable_pisa_integration(database_name, collection_name, compression_type)
            if success:
                console.print(f"✅ PISA integration enabled for {database_name}.{collection_name}")
            else:
                console.print(f"❌ Failed to enable PISA integration for {database_name}.{collection_name}")
                sys.exit(1)
        except Exception as e:
            console.print(f"❌ Error: {e}")
            sys.exit(1)


@index.command("disable")
@click.argument("database_name")
@click.argument("collection_name")
@click.pass_context
def disable_index(ctx, database_name, collection_name):
    """Disable PISA integration for a collection."""
    db = ctx.obj["db"]
    
    try:
        success = db.disable_pisa_integration(database_name, collection_name)
        if success:
            console.print(f"✅ PISA integration disabled for {database_name}.{collection_name}")
        else:
            console.print(f"❌ Failed to disable PISA integration for {database_name}.{collection_name}")
            sys.exit(1)
    except Exception as e:
        console.print(f"❌ Error: {e}")
        sys.exit(1)


@index.command("create")
@click.argument("database_name")
@click.argument("collection_name")
@click.option("--compression", "-c", default="block_simdbp",
              type=click.Choice(list(COMPRESSION_TYPES.keys())),
              help="Compression type")
@click.option("--text-index", is_flag=True, help="Create text index with enhanced options")
@click.option("--index-options", default="{}", help="Index options as JSON")
@click.pass_context
def create_index(ctx, database_name, collection_name, compression, text_index, index_options):
    """Create a PISA index for a collection."""
    db = ctx.obj["db"]
    compression_type = COMPRESSION_TYPES[compression]
    
    with Progress(SpinnerColumn(), TextColumn("[progress.description]{task.description}")) as progress:
        task = progress.add_task("Creating PISA index...", total=None)
        
        try:
            if text_index:
                options = json.loads(index_options)
                success = db.create_pisa_text_index(database_name, collection_name, 
                                                  options, compression_type)
            else:
                success = db.create_pisa_index(database_name, collection_name, compression_type)
            
            if success:
                console.print(f"✅ PISA index created for {database_name}.{collection_name}")
            else:
                console.print(f"❌ Failed to create PISA index for {database_name}.{collection_name}")
                sys.exit(1)
        except Exception as e:
            console.print(f"❌ Error: {e}")
            sys.exit(1)


@index.command("drop")
@click.argument("database_name")
@click.argument("collection_name")
@click.pass_context
def drop_index(ctx, database_name, collection_name):
    """Drop a PISA index for a collection."""
    db = ctx.obj["db"]
    
    try:
        success = db.drop_pisa_index(database_name, collection_name)
        if success:
            console.print(f"✅ PISA index dropped for {database_name}.{collection_name}")
        else:
            console.print(f"❌ Failed to drop PISA index for {database_name}.{collection_name}")
            sys.exit(1)
    except Exception as e:
        console.print(f"❌ Error: {e}")
        sys.exit(1)


@index.command("rebuild")
@click.argument("database_name")
@click.argument("collection_name")
@click.pass_context
def rebuild_index(ctx, database_name, collection_name):
    """Rebuild a PISA index for a collection."""
    db = ctx.obj["db"]
    
    with Progress(SpinnerColumn(), TextColumn("[progress.description]{task.description}")) as progress:
        task = progress.add_task("Rebuilding PISA index...", total=None)
        
        try:
            success = db.rebuild_pisa_index(database_name, collection_name)
            if success:
                console.print(f"✅ PISA index rebuilt for {database_name}.{collection_name}")
            else:
                console.print(f"❌ Failed to rebuild PISA index for {database_name}.{collection_name}")
                sys.exit(1)
        except Exception as e:
            console.print(f"❌ Error: {e}")
            sys.exit(1)


@index.command("status")
@click.option("--database", help="Filter by database name")
@click.option("--collection", help="Filter by collection name")
@click.pass_context
def index_status(ctx, database, collection):
    """Show PISA index status."""
    db = ctx.obj["db"]
    
    try:
        statuses = db.get_pisa_index_status(database, collection)
        
        if not statuses:
            console.print("No PISA indexes found.")
            return
        
        table = Table(title="PISA Index Status")
        table.add_column("Database", style="cyan")
        table.add_column("Collection", style="magenta")
        table.add_column("Enabled", style="green")
        table.add_column("Last Sync", style="yellow")
        table.add_column("Pending Ops", style="red")
        table.add_column("Compression", style="blue")
        table.add_column("Size (MB)", style="white")
        
        for status in statuses:
            size_mb = f"{status['index_size_bytes'] / 1024 / 1024:.2f}" if status['index_size_bytes'] else "N/A"
            table.add_row(
                status['database_name'],
                status['collection_name'],
                "✅" if status['index_enabled'] else "❌",
                str(status['last_sync_time']) if status['last_sync_time'] else "Never",
                str(status['pending_operations']),
                status['compression_type'],
                size_mb
            )
        
        console.print(table)
    except Exception as e:
        console.print(f"❌ Error: {e}")
        sys.exit(1)


@cli.group()
@click.pass_context
def query(ctx):
    """Query execution commands."""
    pass


@query.command("text")
@click.argument("database_name")
@click.argument("collection_name")
@click.argument("query_text")
@click.option("--limit", "-l", default=10, help="Number of results to return")
@click.option("--format", "-f", default="table", type=click.Choice(["table", "json"]),
              help="Output format")
@click.pass_context
def text_query(ctx, database_name, collection_name, query_text, limit, format):
    """Execute a PISA text query."""
    db = ctx.obj["db"]
    
    try:
        results = db.execute_pisa_text_query(database_name, collection_name, query_text, limit)
        
        if format == "json":
            console.print(json.dumps(results, indent=2, default=str))
        else:
            if not results:
                console.print("No results found.")
                return
            
            table = Table(title=f"Text Query Results: '{query_text}'")
            table.add_column("Document ID", style="cyan")
            table.add_column("Score", style="green")
            table.add_column("Collection ID", style="yellow")
            
            for result in results:
                table.add_row(
                    result['document_id'],
                    f"{result['score']:.4f}",
                    str(result['collection_id'])
                )
            
            console.print(table)
    except Exception as e:
        console.print(f"❌ Error: {e}")
        sys.exit(1)


@query.command("advanced")
@click.argument("database_name")
@click.argument("collection_name")
@click.argument("query_terms", nargs=-1)
@click.option("--algorithm", "-a", default="auto", 
              type=click.Choice(list(ALGORITHM_MAP.keys())),
              help="Query algorithm")
@click.option("--top-k", "-k", default=10, help="Number of top results")
@click.option("--format", "-f", default="table", type=click.Choice(["table", "json"]),
              help="Output format")
@click.pass_context
def advanced_query(ctx, database_name, collection_name, query_terms, algorithm, top_k, format):
    """Execute an advanced PISA query with algorithm selection."""
    db = ctx.obj["db"]
    algorithm_id = ALGORITHM_MAP[algorithm]
    
    try:
        results = db.execute_advanced_pisa_query(
            database_name, collection_name, list(query_terms), algorithm_id, top_k
        )
        
        if format == "json":
            console.print(json.dumps(results, indent=2, default=str))
        else:
            if not results:
                console.print("No results found.")
                return
            
            table = Table(title=f"Advanced Query Results ({algorithm.upper()})")
            table.add_column("Document ID", style="cyan")
            table.add_column("Score", style="green")
            table.add_column("Collection ID", style="yellow")
            
            for result in results:
                table.add_row(
                    result['document_id'],
                    f"{result['score']:.4f}",
                    str(result['collection_id'])
                )
            
            console.print(table)
    except Exception as e:
        console.print(f"❌ Error: {e}")
        sys.exit(1)


@cli.group()
@click.pass_context
def reorder(ctx):
    """Document reordering commands."""
    pass


@reorder.command("schedule")
@click.argument("database_name")
@click.argument("collection_name")
@click.option("--priority", "-p", default=1, help="Reordering priority")
@click.pass_context
def schedule_reordering(ctx, database_name, collection_name, priority):
    """Schedule document reordering for a collection."""
    db = ctx.obj["db"]
    
    try:
        success = db.schedule_document_reordering(database_name, collection_name, priority)
        if success:
            console.print(f"✅ Document reordering scheduled for {database_name}.{collection_name}")
        else:
            console.print(f"❌ Failed to schedule reordering for {database_name}.{collection_name}")
            sys.exit(1)
    except Exception as e:
        console.print(f"❌ Error: {e}")
        sys.exit(1)


@reorder.command("cancel")
@click.argument("database_name")
@click.argument("collection_name")
@click.pass_context
def cancel_reordering(ctx, database_name, collection_name):
    """Cancel document reordering for a collection."""
    db = ctx.obj["db"]
    
    try:
        success = db.cancel_document_reordering(database_name, collection_name)
        if success:
            console.print(f"✅ Document reordering cancelled for {database_name}.{collection_name}")
        else:
            console.print(f"❌ Failed to cancel reordering for {database_name}.{collection_name}")
            sys.exit(1)
    except Exception as e:
        console.print(f"❌ Error: {e}")
        sys.exit(1)


@reorder.command("status")
@click.pass_context
def reordering_status(ctx):
    """Show document reordering task status."""
    db = ctx.obj["db"]
    
    try:
        tasks = db.get_all_reordering_tasks()
        
        if not tasks:
            console.print("No reordering tasks found.")
            return
        
        table = Table(title="Document Reordering Tasks")
        table.add_column("Database", style="cyan")
        table.add_column("Collection", style="magenta")
        table.add_column("Status", style="green")
        table.add_column("Priority", style="yellow")
        table.add_column("Scheduled", style="blue")
        table.add_column("Improvement", style="white")
        
        for task in tasks:
            if task['is_running']:
                status = "🔄 Running"
            elif task['is_completed']:
                status = "✅ Completed"
            else:
                status = "⏳ Pending"
            
            improvement = f"{task['compression_improvement']:.2f}%" if task['compression_improvement'] else "N/A"
            
            table.add_row(
                task['database_name'],
                task['collection_name'],
                status,
                str(task['priority']),
                str(task['scheduled_time']),
                improvement
            )
        
        console.print(table)
    except Exception as e:
        console.print(f"❌ Error: {e}")
        sys.exit(1)


@reorder.command("stats")
@click.argument("database_name")
@click.argument("collection_name")
@click.pass_context
def reordering_stats(ctx, database_name, collection_name):
    """Show detailed reordering statistics for a collection."""
    db = ctx.obj["db"]
    
    try:
        stats = db.get_reordering_stats(database_name, collection_name)
        
        if not stats:
            console.print(f"No reordering statistics found for {database_name}.{collection_name}")
            return
        
        panel_content = f"""
[bold cyan]Collection:[/bold cyan] {database_name}.{collection_name}
[bold green]Total Documents:[/bold green] {stats.get('total_documents', 'N/A'):,}
[bold green]Reordered Documents:[/bold green] {stats.get('reordered_documents', 'N/A'):,}
[bold yellow]Compression Ratio Before:[/bold yellow] {stats.get('compression_ratio_before', 'N/A'):.3f}
[bold yellow]Compression Ratio After:[/bold yellow] {stats.get('compression_ratio_after', 'N/A'):.3f}
[bold magenta]Improvement:[/bold magenta] {stats.get('improvement_percentage', 'N/A'):.2f}%
[bold blue]Last Reordering:[/bold blue] {stats.get('last_reordering_time', 'Never')}
[bold white]Reordering Iterations:[/bold white] {stats.get('reordering_iterations', 0)}
        """
        
        console.print(Panel(panel_content, title="Reordering Statistics", expand=False))
    except Exception as e:
        console.print(f"❌ Error: {e}")
        sys.exit(1)


@cli.group()
@click.pass_context
def export(ctx):
    """Data export commands."""
    pass


@export.command("collection")
@click.argument("database_name")
@click.argument("collection_name")
@click.argument("output_path")
@click.pass_context
def export_collection(ctx, database_name, collection_name, output_path):
    """Export collection to PISA format."""
    db = ctx.obj["db"]
    
    with Progress(SpinnerColumn(), TextColumn("[progress.description]{task.description}")) as progress:
        task = progress.add_task("Exporting collection...", total=None)
        
        try:
            success = db.export_collection_to_pisa_format(database_name, collection_name, output_path)
            if success:
                console.print(f"✅ Collection exported to {output_path}")
            else:
                console.print(f"❌ Failed to export collection")
                sys.exit(1)
        except Exception as e:
            console.print(f"❌ Error: {e}")
            sys.exit(1)


def main():
    """Main entry point for documentdb-pisactl."""
    cli()


if __name__ == "__main__":
    main()
