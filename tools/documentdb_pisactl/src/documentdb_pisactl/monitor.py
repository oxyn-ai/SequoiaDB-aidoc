"""
DocumentDB PISA monitoring and alerting system.
"""

import time
import logging
import asyncio
from typing import Dict, List, Optional, Any
from datetime import datetime, timedelta
import json
from rich.console import Console
from rich.table import Table
from rich.live import Live
from rich.panel import Panel

from .database import DocumentDBConnection
from .config import ConfigManager

console = Console()
logger = logging.getLogger(__name__)


class PisaMonitor:
    """Monitors PISA integration health and performance."""
    
    def __init__(self, config_manager: ConfigManager):
        self.config = config_manager
        self.db = DocumentDBConnection(
            host=config_manager.get("database.host"),
            port=config_manager.get("database.port"),
            database=config_manager.get("database.database"),
            user=config_manager.get("database.user"),
            password=config_manager.get("database.password")
        )
        self.monitoring_enabled = config_manager.get("monitoring.enabled", True)
        self.check_interval = config_manager.get("monitoring.check_interval", 60)
        self.alert_thresholds = config_manager.get("monitoring.alert_thresholds", {})
        self.alerts = []
    
    def check_index_health(self) -> Dict[str, Any]:
        """Check the health of all PISA indexes."""
        try:
            statuses = self.db.get_pisa_index_status()
            health_summary = {
                "total_indexes": len(statuses),
                "enabled_indexes": sum(1 for s in statuses if s['index_enabled']),
                "disabled_indexes": sum(1 for s in statuses if not s['index_enabled']),
                "total_size_mb": sum(s['index_size_bytes'] or 0 for s in statuses) / 1024 / 1024,
                "total_pending_ops": sum(s['pending_operations'] or 0 for s in statuses),
                "indexes_with_issues": []
            }
            
            for status in statuses:
                issues = []
                
                size_mb = (status['index_size_bytes'] or 0) / 1024 / 1024
                if size_mb > self.alert_thresholds.get("index_size_mb", 1000):
                    issues.append(f"Large index size: {size_mb:.2f} MB")
                
                pending = status['pending_operations'] or 0
                if pending > self.alert_thresholds.get("pending_operations", 100):
                    issues.append(f"High pending operations: {pending}")
                
                if status['last_sync_time']:
                    last_sync = datetime.fromisoformat(str(status['last_sync_time']).replace('Z', '+00:00'))
                    if datetime.now().replace(tzinfo=last_sync.tzinfo) - last_sync > timedelta(hours=24):
                        issues.append("Stale index (not synced in 24h)")
                
                if issues:
                    health_summary["indexes_with_issues"].append({
                        "database": status['database_name'],
                        "collection": status['collection_name'],
                        "issues": issues
                    })
            
            return health_summary
        except Exception as e:
            logger.error(f"Failed to check index health: {e}")
            return {"error": str(e)}
    
    def check_reordering_health(self) -> Dict[str, Any]:
        """Check the health of document reordering tasks."""
        try:
            tasks = self.db.get_all_reordering_tasks()
            health_summary = {
                "total_tasks": len(tasks),
                "pending_tasks": sum(1 for t in tasks if not t['is_running'] and not t['is_completed']),
                "running_tasks": sum(1 for t in tasks if t['is_running']),
                "completed_tasks": sum(1 for t in tasks if t['is_completed']),
                "failed_tasks": [],
                "long_running_tasks": []
            }
            
            now = datetime.now()
            for task in tasks:
                if task['is_running'] and task['started_time']:
                    started = datetime.fromisoformat(str(task['started_time']).replace('Z', '+00:00'))
                    if now.replace(tzinfo=started.tzinfo) - started > timedelta(hours=6):
                        health_summary["long_running_tasks"].append({
                            "database": task['database_name'],
                            "collection": task['collection_name'],
                            "duration_hours": (now.replace(tzinfo=started.tzinfo) - started).total_seconds() / 3600
                        })
                
                if (task['is_completed'] and 
                    task['compression_improvement'] is not None and 
                    task['compression_improvement'] <= 0):
                    health_summary["failed_tasks"].append({
                        "database": task['database_name'],
                        "collection": task['collection_name'],
                        "improvement": task['compression_improvement']
                    })
            
            return health_summary
        except Exception as e:
            logger.error(f"Failed to check reordering health: {e}")
            return {"error": str(e)}
    
    def generate_alerts(self, index_health: Dict, reordering_health: Dict) -> List[Dict]:
        """Generate alerts based on health checks."""
        alerts = []
        
        if "indexes_with_issues" in index_health:
            for issue_info in index_health["indexes_with_issues"]:
                alerts.append({
                    "type": "index_issue",
                    "severity": "warning",
                    "message": f"Index issues in {issue_info['database']}.{issue_info['collection']}: {', '.join(issue_info['issues'])}",
                    "timestamp": datetime.now().isoformat()
                })
        
        if index_health.get("total_pending_ops", 0) > self.alert_thresholds.get("pending_operations", 100):
            alerts.append({
                "type": "high_pending_operations",
                "severity": "warning",
                "message": f"High total pending operations: {index_health['total_pending_ops']}",
                "timestamp": datetime.now().isoformat()
            })
        
        if "long_running_tasks" in reordering_health:
            for task_info in reordering_health["long_running_tasks"]:
                alerts.append({
                    "type": "long_running_task",
                    "severity": "warning",
                    "message": f"Long-running reordering task in {task_info['database']}.{task_info['collection']}: {task_info['duration_hours']:.1f} hours",
                    "timestamp": datetime.now().isoformat()
                })
        
        if "failed_tasks" in reordering_health:
            for task_info in reordering_health["failed_tasks"]:
                alerts.append({
                    "type": "failed_reordering",
                    "severity": "error",
                    "message": f"Failed reordering task in {task_info['database']}.{task_info['collection']}: {task_info['improvement']}% improvement",
                    "timestamp": datetime.now().isoformat()
                })
        
        return alerts
    
    def create_dashboard_table(self, index_health: Dict, reordering_health: Dict) -> Table:
        """Create a dashboard table for monitoring."""
        table = Table(title="DocumentDB PISA Health Dashboard")
        table.add_column("Metric", style="cyan")
        table.add_column("Value", style="green")
        table.add_column("Status", style="yellow")
        
        table.add_row("Total Indexes", str(index_health.get("total_indexes", 0)), "✅")
        table.add_row("Enabled Indexes", str(index_health.get("enabled_indexes", 0)), "✅")
        table.add_row("Total Size (MB)", f"{index_health.get('total_size_mb', 0):.2f}", "✅")
        
        pending_ops = index_health.get("total_pending_ops", 0)
        pending_status = "❌" if pending_ops > self.alert_thresholds.get("pending_operations", 100) else "✅"
        table.add_row("Pending Operations", str(pending_ops), pending_status)
        
        issues_count = len(index_health.get("indexes_with_issues", []))
        issues_status = "❌" if issues_count > 0 else "✅"
        table.add_row("Indexes with Issues", str(issues_count), issues_status)
        
        table.add_row("Reordering Tasks", str(reordering_health.get("total_tasks", 0)), "✅")
        table.add_row("Running Tasks", str(reordering_health.get("running_tasks", 0)), "✅")
        table.add_row("Pending Tasks", str(reordering_health.get("pending_tasks", 0)), "✅")
        
        long_running = len(reordering_health.get("long_running_tasks", []))
        long_running_status = "❌" if long_running > 0 else "✅"
        table.add_row("Long-running Tasks", str(long_running), long_running_status)
        
        failed = len(reordering_health.get("failed_tasks", []))
        failed_status = "❌" if failed > 0 else "✅"
        table.add_row("Failed Tasks", str(failed), failed_status)
        
        return table
    
    async def monitor_loop(self):
        """Main monitoring loop."""
        logger.info("Starting PISA monitoring loop")
        
        while self.monitoring_enabled:
            try:
                index_health = self.check_index_health()
                reordering_health = self.check_reordering_health()
                
                new_alerts = self.generate_alerts(index_health, reordering_health)
                self.alerts.extend(new_alerts)
                
                for alert in new_alerts:
                    if alert["severity"] == "error":
                        logger.error(alert["message"])
                    else:
                        logger.warning(alert["message"])
                
                cutoff_time = datetime.now() - timedelta(hours=24)
                self.alerts = [
                    alert for alert in self.alerts 
                    if datetime.fromisoformat(alert["timestamp"]) > cutoff_time
                ]
                
                await asyncio.sleep(self.check_interval)
                
            except Exception as e:
                logger.error(f"Error in monitoring loop: {e}")
                await asyncio.sleep(self.check_interval)
    
    def run_dashboard(self):
        """Run interactive dashboard."""
        def generate_dashboard():
            index_health = self.check_index_health()
            reordering_health = self.check_reordering_health()
            
            dashboard_table = self.create_dashboard_table(index_health, reordering_health)
            
            recent_alerts = self.alerts[-5:] if self.alerts else []
            alerts_content = "\n".join([
                f"[{alert['severity'].upper()}] {alert['message']}"
                for alert in recent_alerts
            ]) or "No recent alerts"
            
            alerts_panel = Panel(alerts_content, title="Recent Alerts", expand=False)
            
            return f"{dashboard_table}\n\n{alerts_panel}"
        
        try:
            with Live(generate_dashboard(), refresh_per_second=1/self.check_interval) as live:
                while True:
                    time.sleep(self.check_interval)
                    live.update(generate_dashboard())
        except KeyboardInterrupt:
            console.print("\n👋 Dashboard stopped")


def main():
    """Main entry point for monitoring."""
    import click
    
    @click.command()
    @click.option("--config", "-c", default="~/.documentdb_pisactl.yaml", 
                  help="Configuration file path")
    @click.option("--dashboard", is_flag=True, help="Run interactive dashboard")
    @click.option("--once", is_flag=True, help="Run health check once and exit")
    def monitor_cmd(config, dashboard, once):
        """DocumentDB PISA monitoring tool."""
        config_manager = ConfigManager(config)
        monitor = PisaMonitor(config_manager)
        
        if once:
            index_health = monitor.check_index_health()
            reordering_health = monitor.check_reordering_health()
            
            console.print(monitor.create_dashboard_table(index_health, reordering_health))
            
            alerts = monitor.generate_alerts(index_health, reordering_health)
            if alerts:
                console.print("\n[bold red]Alerts:[/bold red]")
                for alert in alerts:
                    console.print(f"[{alert['severity'].upper()}] {alert['message']}")
        
        elif dashboard:
            monitor.run_dashboard()
        
        else:
            asyncio.run(monitor.monitor_loop())
    
    monitor_cmd()


if __name__ == "__main__":
    main()
