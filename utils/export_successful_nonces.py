import os
import pandas as pd
import hashlib
import json
from datetime import datetime
from typing import List, Optional
from pathlib import Path
from dataclasses import dataclass
from nonce_logger import NonceLogger


@dataclass
class NonceData:
    nonce: str
    score: float
    timestamp: float
    checksum: str


class NonceExporter:
    def __init__(self, config_path: str = "export_config.json"):
        self.config = self._load_config(config_path)
        self.logger = NonceLogger("NonceExporter", self.config.get("logging_config"))

        # Configuración de rutas
        self.nonces_file = Path(self.config.get("nonces_path", "logs/nonces_exitosos.txt"))
        self.csv_output = Path(self.config.get("output_dir", "data")) / "nonce_training_data.csv"
        self.backup_dir = Path(self.config.get("backup_dir", "data/backups"))

        # Asegura que los directorios requeridos existan
        self._ensure_dirs()

    def _load_config(self, config_path: str) -> dict:
        """Carga la configuración desde un archivo JSON, manejando errores de forma segura."""
        default_config = {
            "nonces_path": "logs/nonces_exitosos.txt",
            "output_dir": "data",
            "backup_dir": "data/backups",
            "chunk_size": 1000,
            "validation": {
                "min_score": 0.0,
                "max_score": 1.0,
                "nonce_length": 16,
            },
        }

        try:
            with open(config_path, "r", encoding="utf-8") as f:
                user_config = json.load(f)
                return {**default_config, **user_config}
        except (FileNotFoundError, json.JSONDecodeError) as e:
            self.logger.warning(f"Usando configuración predeterminada debido a: {str(e)}")
            return default_config

    def _ensure_dirs(self) -> None:
        """Crea los directorios requeridos si no existen."""
        for dir_path in [self.backup_dir, self.csv_output.parent]:
            dir_path.mkdir(parents=True, exist_ok=True)

    def _validate_nonce(self, nonce: str, score: float) -> bool:
        """Valida que el nonce y el puntaje cumplan los requisitos."""
        val_cfg = self.config["validation"]
        return (
            isinstance(nonce, str) and len(nonce) == val_cfg["nonce_length"]
            and isinstance(score, (int, float))
            and val_cfg["min_score"] <= score <= val_cfg["max_score"]
        )

    def _process_line(self, line: str, line_num: int) -> Optional[NonceData]:
        """Procesa una línea individual del archivo de nonces, con validación y manejo de errores."""
        try:
            parts = line.strip().split()
            if len(parts) < 2:
                self.logger.warning(f"Línea inválida {line_num}: {line.strip()}")
                return None

            nonce, score = parts[0], float(parts[1])
            if self._validate_nonce(nonce, score):
                checksum = hashlib.sha256(line.encode()).hexdigest()
                return NonceData(nonce=nonce, score=score, timestamp=datetime.now().timestamp(), checksum=checksum)

        except ValueError as e:
            self.logger.warning(f"Línea inválida {line_num}: {str(e)}")
        
        return None

    def export(self) -> bool:
        """Método principal para exportar los datos de nonces, con manejo estructurado de errores."""
        if not self.nonces_file.exists():
            self.logger.error(f"Archivo de nonces no encontrado: {self.nonces_file}")
            return False

        nonces: List[NonceData] = []
        try:
            with self.nonces_file.open("r", encoding="utf-8") as f:
                total_lines = sum(1 for _ in f)
                f.seek(0)  # Reiniciar lectura después del conteo

                for i, line in enumerate(f, 1):
                    if nonce_data := self._process_line(line, i):
                        nonces.append(nonce_data)

                    if i % self.config["chunk_size"] == 0 or i == total_lines:
                        self.logger.logExportProgress(i, total_lines)

            if not nonces:
                self.logger.warning("No se encontraron nonces válidos.")
                return False

            # Crear respaldo y guardar a CSV
            self._create_backup()
            self._save_to_csv(nonces)
            return True

        except Exception as e:
            self.logger.critical(f"Error durante la exportación: {str(e)}", exc_info=True)
            return False

    def _create_backup(self) -> Optional[str]:
        """Crea un respaldo del archivo CSV existente."""
        if self.csv_output.exists():
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            backup_file = self.backup_dir / f"nonce_data_{timestamp}.csv"
            self.csv_output.rename(backup_file)
            self.logger.logFileOperation("backup", str(backup_file), True)
            return str(backup_file)
        return None

    def _save_to_csv(self, nonces: List[NonceData]) -> None:
        """Guarda los datos de nonces en un archivo CSV, usando optimizaciones."""
        df = pd.DataFrame([nd.__dict__ for nd in nonces])
        df["export_version"] = "1.2"

        df.to_csv(self.csv_output, index=False, encoding="utf-8")
        self.logger.info(
            f"Exportados {len(nonces)} nonces a {self.csv_output}",
            extra={"file_size": os.path.getsize(self.csv_output)},
        )


if __name__ == "__main__":
    exporter = NonceExporter()
    success = exporter.export()
    exit(0 if success else 1)
