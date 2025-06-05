import json
import os
import logging
import hashlib
from pathlib import Path
from filelock import FileLock, Timeout
from typing import List, Optional
from datetime import datetime
from logging.handlers import RotatingFileHandler
from evaluation.nonce_quality_filter import NonceQualityFilter
from utils.nonce_validator import HexNonceValidator

# Configuración avanzada
class Config:
    NONCES_DIR = Path("ia-modules/bridge/generated/")
    NONCES_PATTERN = "nonces_*.json"
    INJECTION_LOG = Path("logs/inyectados.log")
    MAX_NONCE_LENGTH = 64
    LOCK_TIMEOUT = 10  # segundos
    LOG_MAX_SIZE = 10 * 1024 * 1024  # 10 MB
    LOG_BACKUP_COUNT = 5

# Configuración de logging estructurado
logging.basicConfig(
    level=logging.INFO,
    format='{"time": "%(asctime)s", "module": "%(name)s", "level": "%(levelname)s", "message": "%(message)s"}',
    handlers=[
        RotatingFileHandler(
            Config.INJECTION_LOG,
            maxBytes=Config.LOG_MAX_SIZE,
            backupCount=Config.LOG_BACKUP_COUNT,
            encoding='utf-8'
        ),
        logging.StreamHandler()
    ]
)
logger = logging.getLogger("NonceInjector")

class NonceInjectionError(Exception):
    """Excepción base para errores de inyección de nonces"""
    pass

class NonceLoader:
    @staticmethod
    def find_latest_nonces_file() -> Optional[Path]:
        try:
            nonce_files = sorted(
                Config.NONCES_DIR.glob(Config.NONCES_PATTERN),
                key=os.path.getmtime,
                reverse=True
            )
            return nonce_files[0] if nonce_files else None
        except Exception as e:
            logger.error(f"Error buscando archivos de nonces: {str(e)}")
            return None

    @staticmethod
    def validate_nonce_structure(nonces: List[str]) -> bool:
        validator = HexNonceValidator(
            min_length=8,
            max_length=Config.MAX_NONCE_LENGTH
        )
        return all(validator.is_valid(nonce) for nonce in nonces)

class NonceInjector:
    def __init__(self):
        self.lock = FileLock(Config.INJECTION_LOG.with_suffix(".lock"))
        self.nonce_file = None

    def _atomic_log_injection(self, nonces: List[str]) -> None:
        """Registro atómico de nonces inyectados"""
        try:
            with self.lock.acquire(timeout=Config.LOCK_TIMEOUT):
                with open(Config.INJECTION_LOG, "a", encoding="utf-8") as log:
                    timestamp = datetime.utcnow().isoformat()
                    for nonce in nonces:
                        log_entry = {
                            "timestamp": timestamp,
                            "nonce": nonce,
                            "nonce_sha256": hashlib.sha256(nonce.encode()).hexdigest(),
                            "source_file": self.nonce_file.name if self.nonce_file else "unknown"
                        }
                        log.write(json.dumps(log_entry) + "\n")
        except Timeout:
            logger.error("Timeout al adquirir bloqueo para registro")
            raise NonceInjectionError("No se pudo adquirir bloqueo de archivo")

    def _process_nonces(self, nonces: List[str]) -> None:
        """Flujo completo de procesamiento de nonces"""
        # Filtrado de calidad
        filtered = NonceQualityFilter.evaluar_nonces(nonces)
        
        # Validación final
        if not NonceLoader.validate_nonce_structure(filtered):
            logger.error("Nonces filtrados no superaron validación final")
            raise NonceInjectionError("Validación post-filtrado fallida")
        
        # Registro seguro
        self._atomic_log_injection(filtered)
        
        logger.info(f"Inyección exitosa: {len(filtered)}/{len(nonces)} nonces")

    def inject(self) -> None:
        """Flujo principal de inyección de nonces"""
        try:
            self.nonce_file = NonceLoader.find_latest_nonces_file()
            if not self.nonce_file:
                logger.warning("No se encontraron archivos de nonces válidos")
                return

            # Carga segura con manejo de bloqueo
            with open(self.nonce_file, "r", encoding="utf-8") as f:
                nonces = json.load(f)
            
            if not isinstance(nonces, list):
                raise ValueError("Formato de archivo inválido: se esperaba lista")
            
            self._process_nonces(nonces)
            
            # Archivar nonces procesados
            archive_path = self.nonce_file.with_name(f"processed_{self.nonce_file.name}")
            self.nonce_file.rename(archive_path)
            
        except json.JSONDecodeError:
            logger.error("Error decodificando archivo JSON")
            raise NonceInjectionError("Formato JSON inválido")
        except PermissionError as pe:
            logger.error(f"Error de permisos: {str(pe)}")
            raise NonceInjectionError("Problema de permisos de archivo")
        except Exception as e:
            logger.error(f"Error inesperado durante inyección: {str(e)}")
            raise NonceInjectionError("Error general de inyección")

def main():
    """Punto de entrada principal"""
    try:
        injector = NonceInjector()
        injector.inject()
    except NonceInjectionError as nie:
        logger.error(f"Fallo crítico en inyección: {str(nie)}")
        return 1
    except Exception as e:
        logger.critical(f"Error no manejado: {str(e)}", exc_info=True)
        return 2
    return 0

if __name__ == "__main__":
    exit(main())