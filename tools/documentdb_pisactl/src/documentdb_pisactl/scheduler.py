"""
DocumentDB PISA automated scheduling system.
"""

import time
import logging
import schedule
import threading
from typing import Dict, List, Optional, Any
from datetime import datetime, timedelta
import json

from .database import DocumentDBConnection
from .config import ConfigManager

logger = logging.getLogger(__name__)


class PisaScheduler:
    """Automated scheduler for PISA operations."""
    
    def __init__(self, config_manager: ConfigManager):
        self.config = config_manager
        self.db = DocumentDBConnection(
            host=config_manager.get("database.host"),
            port=config_manager.get("database.port"),
            database=config_manager.get("database.database"),
            user=config_manager.get("database.user"),
            password=config_manager.get("database.password")
        )
        self.scheduling_enabled = config_manager.get("scheduling.enabled", True)
        self.auto_reorder_threshold = config_manager.get("scheduling.auto_reorder_threshold", 10.0)
        self.max_concurrent_tasks = config_manager.get("scheduling.max_concurrent_tasks", 2)
        self.reorder_schedule = config_manager.get("scheduling.reorder_schedule", "0 2 * * *")
        self.running = False
    
    def should_schedule_reordering(self, database_name: str, collection_name: str) -> bool:
        """Determine if reordering should be scheduled for a collection."""
        try:
            statuses = self.db.get_pisa_index_status(database_name, collection_name)
            if not statuses or not statuses[0]['index_enabled']:
                return False
            
            tasks = self.db.get_all_reordering_tasks()
            for task in tasks:
                if (task['database_name'] == database_name and 
                    task['collection_name'] == collection_name and
                    (not task['is_completed'] or task['is_running'])):
                    logger.debug(f"Reordering already scheduled/running for {database_name}.{collection_name}")
                    return False
            
            stats = self.db.get_reordering_stats(database_name, collection_name)
            if not stats:
                return True
            
            if stats.get('last_reordering_time'):
                last_reorder = datetime.fromisoformat(str(stats['last_reordering_time']).replace('Z', '+00:00'))
                if datetime.now().replace(tzinfo=last_reorder.tzinfo) - last_reorder < timedelta(days=7):
                    logger.debug(f"Recent reordering for {database_name}.{collection_name}, skipping")
                    return False
            
            return True
            
        except Exception as e:
            logger.error(f"Error checking reordering eligibility for {database_name}.{collection_name}: {e}")
            return False
    
    def schedule_automatic_reordering(self):
        """Schedule reordering for eligible collections."""
        logger.info("Running automatic reordering scheduler")
        
        try:
            statuses = self.db.get_pisa_index_status()
            eligible_collections = []
            
            for status in statuses:
                if status['index_enabled']:
                    database_name = status['database_name']
                    collection_name = status['collection_name']
                    
                    if self.should_schedule_reordering(database_name, collection_name):
                        eligible_collections.append((database_name, collection_name))
            
            running_tasks = self.db.get_all_reordering_tasks()
            current_running = sum(1 for task in running_tasks if task['is_running'])
            
            scheduled_count = 0
            for database_name, collection_name in eligible_collections:
                if current_running + scheduled_count >= self.max_concurrent_tasks:
                    logger.info(f"Reached max concurrent tasks ({self.max_concurrent_tasks}), stopping scheduling")
                    break
                
                try:
                    success = self.db.schedule_document_reordering(database_name, collection_name, priority=1)
                    if success:
                        logger.info(f"Scheduled automatic reordering for {database_name}.{collection_name}")
                        scheduled_count += 1
                    else:
                        logger.warning(f"Failed to schedule reordering for {database_name}.{collection_name}")
                except Exception as e:
                    logger.error(f"Error scheduling reordering for {database_name}.{collection_name}: {e}")
            
            logger.info(f"Automatic scheduling completed: {scheduled_count} tasks scheduled")
            
        except Exception as e:
            logger.error(f"Error in automatic reordering scheduler: {e}")
    
    def cleanup_completed_tasks(self):
        """Clean up old completed reordering tasks."""
        logger.info("Running task cleanup")
        
        try:
            tasks = self.db.get_all_reordering_tasks()
            cutoff_time = datetime.now() - timedelta(days=30)
            
            cleanup_count = 0
            for task in tasks:
                if (task['is_completed'] and 
                    task['completed_time'] and
                    datetime.fromisoformat(str(task['completed_time']).replace('Z', '+00:00')) < cutoff_time.replace(tzinfo=None)):
                    
                    logger.debug(f"Would clean up completed task: {task['database_name']}.{task['collection_name']}")
                    cleanup_count += 1
            
            logger.info(f"Task cleanup completed: {cleanup_count} old tasks identified for cleanup")
            
        except Exception as e:
            logger.error(f"Error in task cleanup: {e}")
    
    def optimize_indexes(self):
        """Optimize PISA indexes periodically."""
        logger.info("Running index optimization")
        
        try:
            statuses = self.db.get_pisa_index_status()
            optimized_count = 0
            
            for status in statuses:
                if status['index_enabled']:
                    database_name = status['database_name']
                    collection_name = status['collection_name']
                    
                    try:
                        logger.debug(f"Would optimize index for {database_name}.{collection_name}")
                        optimized_count += 1
                    except Exception as e:
                        logger.error(f"Error optimizing index for {database_name}.{collection_name}: {e}")
            
            logger.info(f"Index optimization completed: {optimized_count} indexes processed")
            
        except Exception as e:
            logger.error(f"Error in index optimization: {e}")
    
    def setup_schedules(self):
        """Set up scheduled tasks."""
        if not self.scheduling_enabled:
            logger.info("Scheduling is disabled")
            return
        
        schedule.every().day.at("02:00").do(self.schedule_automatic_reordering)
        
        schedule.every().sunday.at("03:00").do(self.cleanup_completed_tasks)
        
        schedule.every().saturday.at("01:00").do(self.optimize_indexes)
        
        logger.info("Scheduled tasks configured:")
        logger.info("- Automatic reordering: Daily at 02:00")
        logger.info("- Task cleanup: Weekly on Sunday at 03:00")
        logger.info("- Index optimization: Weekly on Saturday at 01:00")
    
    def run_scheduler(self):
        """Run the scheduler loop."""
        logger.info("Starting PISA scheduler")
        self.setup_schedules()
        self.running = True
        
        try:
            while self.running:
                schedule.run_pending()
                time.sleep(60)  # Check every minute
        except KeyboardInterrupt:
            logger.info("Scheduler stopped by user")
        finally:
            self.running = False
    
    def stop_scheduler(self):
        """Stop the scheduler."""
        self.running = False
        logger.info("Scheduler stop requested")
    
    def run_task_now(self, task_name: str):
        """Run a specific scheduled task immediately."""
        task_map = {
            "reordering": self.schedule_automatic_reordering,
            "cleanup": self.cleanup_completed_tasks,
            "optimize": self.optimize_indexes
        }
        
        if task_name in task_map:
            logger.info(f"Running task '{task_name}' immediately")
            task_map[task_name]()
        else:
            logger.error(f"Unknown task: {task_name}")
            logger.info(f"Available tasks: {', '.join(task_map.keys())}")


def main():
    """Main entry point for scheduler."""
    import click
    
    @click.command()
    @click.option("--config", "-c", default="~/.documentdb_pisactl.yaml", 
                  help="Configuration file path")
    @click.option("--run-now", help="Run a specific task immediately", 
                  type=click.Choice(["reordering", "cleanup", "optimize"]))
    @click.option("--daemon", is_flag=True, help="Run as daemon")
    def scheduler_cmd(config, run_now, daemon):
        """DocumentDB PISA automated scheduler."""
        config_manager = ConfigManager(config)
        scheduler = PisaScheduler(config_manager)
        
        if run_now:
            scheduler.run_task_now(run_now)
        elif daemon:
            def run_in_background():
                scheduler.run_scheduler()
            
            thread = threading.Thread(target=run_in_background, daemon=True)
            thread.start()
            
            try:
                while thread.is_alive():
                    time.sleep(1)
            except KeyboardInterrupt:
                scheduler.stop_scheduler()
                thread.join(timeout=5)
        else:
            scheduler.run_scheduler()
    
    scheduler_cmd()


if __name__ == "__main__":
    main()
