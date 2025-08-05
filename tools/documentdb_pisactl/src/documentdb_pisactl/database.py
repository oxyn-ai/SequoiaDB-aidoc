"""
Database connection and DocumentDB API integration.
"""

import psycopg2
import psycopg2.extras
from typing import Dict, List, Optional, Any, Tuple
import json
import logging
from contextlib import contextmanager

logger = logging.getLogger(__name__)


class DocumentDBConnection:
    """Manages connections to DocumentDB and provides API access."""
    
    def __init__(self, host: str = "localhost", port: int = 5432, 
                 database: str = "documentdb", user: str = "postgres", 
                 password: str = "", **kwargs):
        self.connection_params = {
            "host": host,
            "port": port,
            "database": database,
            "user": user,
            "password": password,
            **kwargs
        }
        self._connection = None
    
    @contextmanager
    def get_connection(self):
        """Get a database connection with automatic cleanup."""
        conn = None
        try:
            conn = psycopg2.connect(**self.connection_params)
            conn.autocommit = True
            yield conn
        except Exception as e:
            logger.error(f"Database connection error: {e}")
            raise
        finally:
            if conn:
                conn.close()
    
    def enable_pisa_integration(self, database_name: str, collection_name: str, 
                               compression_type: int = 1) -> bool:
        """Enable PISA integration for a collection."""
        with self.get_connection() as conn:
            with conn.cursor() as cursor:
                cursor.execute(
                    "SELECT documentdb_api.enable_pisa_integration(%s, %s, %s)",
                    (database_name, collection_name, compression_type)
                )
                result = cursor.fetchone()
                return result[0] if result else False
    
    def disable_pisa_integration(self, database_name: str, collection_name: str) -> bool:
        """Disable PISA integration for a collection."""
        with self.get_connection() as conn:
            with conn.cursor() as cursor:
                cursor.execute(
                    "SELECT documentdb_api.disable_pisa_integration(%s, %s)",
                    (database_name, collection_name)
                )
                result = cursor.fetchone()
                return result[0] if result else False
    
    def create_pisa_index(self, database_name: str, collection_name: str, 
                         compression_type: int = 1) -> bool:
        """Create a PISA index for a collection."""
        with self.get_connection() as conn:
            with conn.cursor() as cursor:
                cursor.execute(
                    "SELECT documentdb_api.create_pisa_index(%s, %s, %s)",
                    (database_name, collection_name, compression_type)
                )
                result = cursor.fetchone()
                return result[0] if result else False
    
    def create_pisa_text_index(self, database_name: str, collection_name: str,
                              index_options: Dict = None, compression_type: int = 1) -> bool:
        """Create a PISA text index with options."""
        if index_options is None:
            index_options = {}
        
        with self.get_connection() as conn:
            with conn.cursor() as cursor:
                cursor.execute(
                    "SELECT documentdb_api.create_pisa_text_index(%s, %s, %s, %s)",
                    (database_name, collection_name, json.dumps(index_options), compression_type)
                )
                result = cursor.fetchone()
                return result[0] if result else False
    
    def drop_pisa_index(self, database_name: str, collection_name: str) -> bool:
        """Drop a PISA index for a collection."""
        with self.get_connection() as conn:
            with conn.cursor() as cursor:
                cursor.execute(
                    "SELECT documentdb_api.drop_pisa_index(%s, %s)",
                    (database_name, collection_name)
                )
                result = cursor.fetchone()
                return result[0] if result else False
    
    def rebuild_pisa_index(self, database_name: str, collection_name: str) -> bool:
        """Rebuild a PISA index for a collection."""
        with self.get_connection() as conn:
            with conn.cursor() as cursor:
                cursor.execute(
                    "SELECT documentdb_api.rebuild_pisa_index(%s, %s)",
                    (database_name, collection_name)
                )
                result = cursor.fetchone()
                return result[0] if result else False
    
    def get_pisa_index_status(self, database_name: str = None, 
                             collection_name: str = None) -> List[Dict]:
        """Get PISA index status for collections."""
        with self.get_connection() as conn:
            with conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor) as cursor:
                cursor.execute(
                    "SELECT * FROM documentdb_api.pisa_index_status(%s, %s)",
                    (database_name, collection_name)
                )
                return [dict(row) for row in cursor.fetchall()]
    
    def schedule_document_reordering(self, database_name: str, collection_name: str,
                                   priority: int = 1) -> bool:
        """Schedule document reordering for a collection."""
        with self.get_connection() as conn:
            with conn.cursor() as cursor:
                cursor.execute(
                    "SELECT documentdb_api.schedule_document_reordering(%s, %s, %s)",
                    (database_name, collection_name, priority)
                )
                result = cursor.fetchone()
                return result[0] if result else False
    
    def cancel_document_reordering(self, database_name: str, collection_name: str) -> bool:
        """Cancel document reordering for a collection."""
        with self.get_connection() as conn:
            with conn.cursor() as cursor:
                cursor.execute(
                    "SELECT documentdb_api.cancel_document_reordering(%s, %s)",
                    (database_name, collection_name)
                )
                result = cursor.fetchone()
                return result[0] if result else False
    
    def get_reordering_stats(self, database_name: str, collection_name: str) -> Dict:
        """Get document reordering statistics."""
        with self.get_connection() as conn:
            with conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor) as cursor:
                cursor.execute(
                    "SELECT * FROM documentdb_api.get_reordering_stats(%s, %s)",
                    (database_name, collection_name)
                )
                result = cursor.fetchone()
                return dict(result) if result else {}
    
    def get_all_reordering_tasks(self) -> List[Dict]:
        """Get all document reordering tasks."""
        with self.get_connection() as conn:
            with conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor) as cursor:
                cursor.execute("SELECT * FROM documentdb_api.get_all_reordering_tasks()")
                return [dict(row) for row in cursor.fetchall()]
    
    def execute_pisa_text_query(self, database_name: str, collection_name: str,
                               query_text: str, limit_count: int = 10) -> List[Dict]:
        """Execute a PISA text query."""
        with self.get_connection() as conn:
            with conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor) as cursor:
                cursor.execute(
                    "SELECT * FROM documentdb_api.execute_pisa_text_query(%s, %s, %s, %s)",
                    (database_name, collection_name, query_text, limit_count)
                )
                return [dict(row) for row in cursor.fetchall()]
    
    def execute_advanced_pisa_query(self, database_name: str, collection_name: str,
                                   query_terms: List[str], algorithm: int = 5,
                                   top_k: int = 10) -> List[Dict]:
        """Execute an advanced PISA query with algorithm selection."""
        with self.get_connection() as conn:
            with conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor) as cursor:
                cursor.execute(
                    "SELECT * FROM documentdb_api.execute_advanced_pisa_query(%s, %s, %s, %s, %s)",
                    (database_name, collection_name, json.dumps(query_terms), algorithm, top_k)
                )
                return [dict(row) for row in cursor.fetchall()]
    
    def analyze_query_routing(self, query_json: Dict) -> Dict:
        """Analyze query routing decisions."""
        with self.get_connection() as conn:
            with conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor) as cursor:
                cursor.execute(
                    "SELECT * FROM documentdb_api.analyze_query_routing(%s)",
                    (json.dumps(query_json),)
                )
                result = cursor.fetchone()
                return dict(result) if result else {}
    
    def export_collection_to_pisa_format(self, database_name: str, collection_name: str,
                                        output_path: str) -> bool:
        """Export collection to PISA format."""
        with self.get_connection() as conn:
            with conn.cursor() as cursor:
                cursor.execute(
                    "SELECT documentdb_api.export_collection_to_pisa_format(%s, %s, %s)",
                    (database_name, collection_name, output_path)
                )
                result = cursor.fetchone()
                return result[0] if result else False
    
    def build_complete_pisa_index(self, database_name: str, collection_name: str,
                                 index_path: str, compression_type: int = 1) -> bool:
        """Build a complete PISA index."""
        with self.get_connection() as conn:
            with conn.cursor() as cursor:
                cursor.execute(
                    "SELECT documentdb_api.build_complete_pisa_index(%s, %s, %s, %s)",
                    (database_name, collection_name, index_path, compression_type)
                )
                result = cursor.fetchone()
                return result[0] if result else False
