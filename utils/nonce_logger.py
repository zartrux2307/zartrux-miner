import logging
import os
import sys
import json
from logging.handlers import RotatingFileHandler, SysLogHandler
from typing import Dict, Optional


class NonceLogger:
    _instance = None

    def __new__(cls, name="NonceLogger", config_file="logging_config.json"):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
            cls._instance._initialized = False
        return cls._instance

    def __init__(self, name="NonceLogger", config_file="logging_config.json"):
        if self._initialized:
            return

        self._initialized = True
        self.config = self._load_config(config_file)
        self.logger = self._setup_logger(name)

    def _load_config(self, config_file: str) -> Dict:
        """Carga la configuración desde un archivo JSON o usa valores predeterminados."""
        default_config = {
            "log_dir": "logs/app_logs",
            "max_bytes": 5 * 1024 * 1024,
            "backup_count": 3,
            "console_level": "INFO",
            "file_level": "DEBUG",
            "syslog_enabled": False,
            "log_format": "%(asctime)s - %(name)s - %(levelname)s - %(message)s",
        }

        try:
            with open(config_file, "r", encoding="utf-8") as f:
                user_config = json.load(f)
                if not isinstance(user_config, dict):
                    raise ValueError("El archivo de configuración debe contener un diccionario JSON válido.")
                return {**default_config, **user_config}
        except (FileNotFoundError, json.JSONDecodeError, ValueError) as e:
            print(f"⚠️ Advertencia: Usando configuración predeterminada debido a: {str(e)}")
            return default_config

    def _setup_logger(self, name: str) -> logging.Logger:
        """Configura el logger con múltiples manejadores (archivo, consola y Syslog opcional)."""
        logger = logging.getLogger(name)
        logger.setLevel(logging.DEBUG)

        # Limpiar handlers existentes para evitar duplicados
        if logger.hasHandlers():
            logger.handlers.clear()

        formatter = logging.Formatter(self.config.get("log_format", "%(asctime)s - %(name)s - %(levelname)s - %(message)s"))

        # Configurar archivo de logs
        log_dir = Path(self.config.get("log_dir", "logs/app_logs"))
        log_dir.mkdir(parents=True, exist_ok=True)

        file_handler = RotatingFileHandler(
            log_dir / f"{name}.log",
            maxBytes=self.config.get("max_bytes", 5 * 1024 * 1024),
            backupCount=self.config.get("backup_count", 3),
        )
        file_handler.setLevel(getattr(logging, self.config.get("file_level", "DEBUG").upper(), logging.DEBUG))
        file_handler.setFormatter(formatter)

        # Configurar consola
        console_handler = logging.StreamHandler(sys.stdout)
        console_handler.setLevel(getattr(logging, self.config.get("console_level", "INFO").upper(), logging.INFO))
        console_handler.setFormatter(formatter)

        # Agregar manejadores al logger
        logger.addHandler(file_handler)
        logger.addHandler(console_handler)

        # Configuración opcional de Syslog
        if self.config.get("syslog_enabled", False):
            syslog_handler = SysLogHandler()
            syslog_handler.setFormatter(formatter)
            logger.addHandler(syslog_handler)

        return logger

    def log(self, level: str, msg: str, extra: Optional[Dict] = None) -> None:
        """Método genérico de log con soporte para niveles dinámicos."""
        level_map = {
            "DEBUG": logging.DEBUG,
            "INFO": logging.INFO,
            "WARNING": logging.WARNING,
            "ERROR": logging.ERROR,
            "CRITICAL": logging.CRITICAL,
        }
        log_level = level_map.get(level.upper(), logging.INFO)

        self.logger.log(log_level, msg, extra=extra if extra else {})

    def debug(self, msg: str, extra: Optional[Dict] = None) -> None:
        """Registra un mensaje de depuración."""
        self.logger.debug(msg, extra=extra if extra else {})

    def info(self, msg: str, extra: Optional[Dict] = None) -> None:
        """Registra un mensaje informativo."""
        self.logger.info(msg, extra=extra if extra else {})

    def warning(self, msg: str, extra: Optional[Dict] = None) -> None:
        """Registra una advertencia."""
        self.logger.warning(msg, extra=extra if extra else {})

    def error(self, msg: str, extra: Optional[Dict] = None) -> None:
        """Registra un mensaje de error."""
        self.logger.error(msg, extra=extra if extra else {})

    def critical(self, msg: str, extra: Optional[Dict] = None) -> None:
        """Registra un mensaje crítico."""
        self.logger.critical(msg, extra=extra if extra else {})

    def log_nonce(self, nonce: str, score: float, algorithm: str = "SHA-256") -> None:
        """Registra operaciones relacionadas con nonces en el log."""
        self.logger.info(
            "Nonce procesado",
            extra={
                "nonce": nonce,
                "score": score,
                "algorithm": algorithm,
                "type": "nonce_operation",
            },
        )
