"""
Configuration management for DocumentDB PISA tools.
"""

import os
import yaml
from pathlib import Path
from typing import Dict, Any, Optional
import logging

logger = logging.getLogger(__name__)


class ConfigManager:
    """Manages configuration for DocumentDB PISA tools."""
    
    def __init__(self, config_path: str = "~/.documentdb_pisactl.yaml"):
        self.config_path = Path(config_path).expanduser()
        self.config = self.load_config()
    
    def load_config(self) -> Dict[str, Any]:
        """Load configuration from file."""
        if not self.config_path.exists():
            return self.get_default_config()
        
        try:
            with open(self.config_path, 'r') as f:
                config = yaml.safe_load(f) or {}
            return {**self.get_default_config(), **config}
        except Exception as e:
            logger.warning(f"Failed to load config from {self.config_path}: {e}")
            return self.get_default_config()
    
    def save_config(self, config: Dict[str, Any] = None) -> bool:
        """Save configuration to file."""
        if config is None:
            config = self.config
        
        try:
            self.config_path.parent.mkdir(parents=True, exist_ok=True)
            with open(self.config_path, 'w') as f:
                yaml.safe_dump(config, f, default_flow_style=False)
            self.config = config
            return True
        except Exception as e:
            logger.error(f"Failed to save config to {self.config_path}: {e}")
            return False
    
    def get_default_config(self) -> Dict[str, Any]:
        """Get default configuration."""
        return {
            "database": {
                "host": "localhost",
                "port": 5432,
                "database": "documentdb",
                "user": "postgres",
                "password": "",
                "connection_timeout": 30,
                "command_timeout": 300
            },
            "pisa": {
                "default_compression": "block_simdbp",
                "default_algorithm": "auto",
                "default_top_k": 10,
                "index_base_path": "/var/lib/documentdb/pisa_indexes",
                "export_base_path": "/tmp/documentdb_pisa_exports"
            },
            "monitoring": {
                "enabled": True,
                "check_interval": 60,
                "alert_thresholds": {
                    "index_size_mb": 1000,
                    "pending_operations": 100,
                    "query_latency_ms": 1000
                }
            },
            "scheduling": {
                "enabled": True,
                "auto_reorder_threshold": 10.0,
                "max_concurrent_tasks": 2,
                "reorder_schedule": "0 2 * * *"  # Daily at 2 AM
            },
            "logging": {
                "level": "INFO",
                "file": "/var/log/documentdb_pisactl.log",
                "max_size_mb": 100,
                "backup_count": 5
            }
        }
    
    def get(self, key: str, default: Any = None) -> Any:
        """Get configuration value by dot-separated key."""
        keys = key.split('.')
        value = self.config
        
        for k in keys:
            if isinstance(value, dict) and k in value:
                value = value[k]
            else:
                return default
        
        return value
    
    def set(self, key: str, value: Any) -> None:
        """Set configuration value by dot-separated key."""
        keys = key.split('.')
        config = self.config
        
        for k in keys[:-1]:
            if k not in config:
                config[k] = {}
            config = config[k]
        
        config[keys[-1]] = value
    
    def update(self, updates: Dict[str, Any]) -> None:
        """Update configuration with new values."""
        def deep_update(base_dict, update_dict):
            for key, value in update_dict.items():
                if isinstance(value, dict) and key in base_dict and isinstance(base_dict[key], dict):
                    deep_update(base_dict[key], value)
                else:
                    base_dict[key] = value
        
        deep_update(self.config, updates)


def load_config(config_path: str = "~/.documentdb_pisactl.yaml") -> Dict[str, Any]:
    """Load configuration from file."""
    manager = ConfigManager(config_path)
    return manager.config


def save_config(config: Dict[str, Any], config_path: str = "~/.documentdb_pisactl.yaml") -> bool:
    """Save configuration to file."""
    manager = ConfigManager(config_path)
    return manager.save_config(config)
