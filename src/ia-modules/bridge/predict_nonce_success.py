import json
import logging
import joblib
import numpy as np
import hashlib
from datetime import datetime
from pathlib import Path
from typing import List, Dict, Optional, Tuple
from logging.handlers import RotatingFileHandler
from dataclasses import dataclass
from functools import lru_cache

# Módulos internos
from utils.nonce_loader import NonceLoader
from evaluation.nonce_quality_filter import NonceQualityFilter
from evaluation.nonce_stats import NonceStats
from utils.hex_validator import HexNonceValidator

# Configuración
@dataclass
class Config:
    NONCES_PATH: Path = Path("ia-modules/bridge/nonces_ready.json")
    MODEL_PATH: Path = Path("models/ethical_nonce_model.joblib")
    SUCCESS_LOG: Path = Path("logs/nonces_exitosos.log")
    LOG_MAX_SIZE: int = 10 * 1024 * 1024  # 10 MB
    LOG_BACKUP_COUNT: int = 5
    PREDICTION_THRESHOLD: float = 0.75
    NONCE_MIN_LENGTH: int = 8
    NONCE_MAX_LENGTH: int = 64

# Configuración de logging estructurado
logging.basicConfig(
    level=logging.INFO,
    format='{"time": "%(asctime)s", "service": "%(name)s", "level": "%(levelname)s", "message": "%(message)s"}',
    handlers=[
        RotatingFileHandler(
            Config.SUCCESS_LOG,
            maxBytes=Config.LOG_MAX_SIZE,
            backupCount=Config.LOG_BACKUP_COUNT,
            encoding='utf-8'
        ),
        logging.StreamHandler()
    ]
)
logger = logging.getLogger("NoncePredictor")

class NoncePredictionError(Exception):
    """Excepción base para errores en la predicción de nonces"""
    pass

class ModelLoader:
    @staticmethod
    @lru_cache(maxsize=1)
    def load_model(model_path: Path):
        """Carga el modelo IA con cache y verificación de integridad"""
        try:
            if not model_path.exists():
                raise FileNotFoundError(f"Model file not found: {model_path}")
            
            model = joblib.load(model_path)
            
            if not hasattr(model, "predict_proba"):
                raise ValueError("El modelo no implementa predict_proba")
            
            return model
        except Exception as e:
            logger.error(f"Error loading model: {str(e)}")
            raise NoncePredictionError("Critical model error") from e

class NoncePreprocessor:
    def __init__(self):
        self.validator = HexNonceValidator(
            min_length=Config.NONCE_MIN_LENGTH,
            max_length=Config.NONCE_MAX_LENGTH
        )
    
    def extract_features(self, nonce: str) -> Optional[np.ndarray]:
        """Extrae características avanzadas del nonce"""
        if not self.validator.is_valid(nonce):
            return None
        
        try:
            hex_part = nonce.lower().strip()
            byte_values = [int(hex_part[i:i+2], 16) for i in range(0, len(hex_part), 2)]
            
            return np.array([
                len(nonce),  # Longitud
                NonceStats.calculate_entropy(nonce),  # Entropía
                np.mean(byte_values),  # Valor medio de bytes
                np.var(byte_values),   # Varianza de bytes
                NonceStats.hex_purity(nonce),  # Porcentaje de caracteres hex válidos
                self._similarity_score(nonce)  # Similaridad con nonces históricos
            ])
        except Exception as e:
            logger.error(f"Error procesando nonce {nonce[:8]}...: {str(e)}")
            return None
    
    def _similarity_score(self, nonce: str) -> float:
        """Calcula similaridad con nonces exitosos históricos"""
        # Implementación de LSH (Locality-Sensitive Hashing)
        return 0.0  # Placeholder para implementación real

class NoncePredictor:
    def __init__(self):
        self.model = ModelLoader.load_model(Config.MODEL_PATH)
        self.preprocessor = NoncePreprocessor()
    
    def predict_batch(self, nonces: List[str]) -> List[Tuple[str, float]]:
        """Predice probabilidades para un lote de nonces"""
        results = []
        
        for nonce in nonces:
            try:
                features = self.preprocessor.extract_features(nonce)
                if features is None:
                    continue
                
                proba = self.model.predict_proba(features.reshape(1, -1))[0][1]
                results.append((nonce, proba))
            except Exception as e:
                logger.error(f"Error prediciendo nonce {nonce[:8]}...: {str(e)}")
        
        return results

class SuccessLogger:
    @staticmethod
    def log(nonces: List[str], metadata: Dict[str, str]):
        """Registro estructurado con hashes de seguridad"""
        try:
            log_entry = {
                "timestamp": datetime.utcnow().isoformat(),
                "nonces_count": len(nonces),
                "nonces_hashes": [hashlib.sha256(n.encode()).hexdigest() for n in nonces],
                "metadata": metadata
            }
            
            with open(Config.SUCCESS_LOG, "a", encoding="utf-8") as f:
                f.write(json.dumps(log_entry) + "\n")
                
        except Exception as e:
            logger.error(f"Error registrando nonces: {str(e)}")

def main():
    """Flujo principal de ejecución"""
    try:
        # Cargar y validar nonces
        raw_nonces = NonceLoader.load()
        if not raw_nonces:
            logger.warning("No se encontraron nonces para procesar")
            return
        
        # Filtrado inicial
        filtered_nonces = NonceQualityFilter.validate_batch(raw_nonces)
        
        # Predicción IA
        predictor = NoncePredictor()
        predictions = predictor.predict_batch(filtered_nonces)
        
        # Filtrado final
        best_nonces = [
            n for n, p in predictions 
            if p >= Config.PREDICTION_THRESHOLD
        ]
        
        # Registro y monitoreo
        if best_nonces:
            metadata = {
                "model_version": predictor.model.version if hasattr(predictor.model, "version") else "unknown",
                "threshold": Config.PREDICTION_THRESHOLD,
                "success_rate": f"{len(best_nonces)/len(raw_nonces):.2%}"
            }
            SuccessLogger.log(best_nonces, metadata)
            logger.info(f"Nonces exitosos procesados: {len(best_nonces)}/{len(raw_nonces)}")
        else:
            logger.info("No se encontraron nonces que superen el umbral de calidad")
            
    except NoncePredictionError as npe:
        logger.error(f"Error crítico en predicción: {str(npe)}")
    except Exception as e:
        logger.critical(f"Error no controlado: {str(e)}", exc_info=True)
        raise

if __name__ == "__main__":
    main()