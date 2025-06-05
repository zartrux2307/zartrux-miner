import os
import json
import csv
import logging
from datetime import datetime
from typing import Dict, List, Union, Optional
import pandas as pd
import joblib
from functools import lru_cache
from config_manager import ConfigManager

class NonceLoader:
    """
    Carga eficiente de datos de nonces desde múltiples fuentes con validación y caché
    
    Atributos:
        config (ConfigManager): Gestor de configuración
        logger (logging.Logger): Logger para seguimiento de operaciones
    """
    
    def __init__(self, config: Optional[ConfigManager] = None):
        self.config = config or ConfigManager()
        self.logger = logging.getLogger(__name__)
        self._validate_paths()

    def _validate_paths(self):
        """Verifica la existencia de rutas críticas"""
        required_paths = [
            self.config.get('logs_path'),
            self.config.get('data_path'),
            self.config.get('models_path')
        ]
        for path in required_paths:
            if not os.path.exists(path):
                raise FileNotFoundError(f"Ruta esencial no encontrada: {path}")

    @lru_cache(maxsize=4)
    def load_valid_nonces(self) -> List[int]:
        """
        Carga nonces exitosos desde archivo de texto
        
        Returns:
            List[int]: Lista de nonces válidos
        """
        path = os.path.join(self.config.get('logs_path'), 'nonces_exitosos.txt')
        nonces = []
        
        try:
            with open(path, 'r') as f:
                for line in f:
                    stripped = line.strip()
                    if stripped.isdigit():
                        nonces.append(int(stripped))
                    else:
                        self.logger.warning(f"Nonce inválido detectado: {stripped}")
        except Exception as e:
            self.logger.error(f"Error cargando nonces válidos: {str(e)}")
        
        return nonces

    @lru_cache(maxsize=4)
    def load_hash_data(self) -> pd.DataFrame:
        """
        Carga datos históricos de hashes desde CSV con validación
        
        Returns:
            pd.DataFrame: DataFrame con columnas [timestamp, nonce, hash_score]
        """
        path = os.path.join(self.config.get('logs_path'), 'nonces_hash.csv')
        df = pd.DataFrame()
        
        try:
            df = pd.read_csv(
                path,
                usecols=['timestamp', 'nonce', 'hash_score'],
                parse_dates=['timestamp'],
                dtype={'nonce': 'uint32', 'hash_score': 'float32'},
                on_bad_lines='warn'
            )
            
            # Validación de datos críticos
            if df['nonce'].isnull().any():
                self.logger.warning("Nonces nulos detectados, eliminando...")
                df = df.dropna(subset=['nonce'])
            
        except Exception as e:
            self.logger.error(f"Error cargando datos de hash: {str(e)}")
        
        return df

    def load_injected_nonces(self, days: int = 7) -> List[Dict]:
        """
        Carga nonces inyectados desde log con filtro temporal
        
        Args:
            days (int): Días hacia atrás para filtrar registros
            
        Returns:
            List[Dict]: Lista de entradas con formato {timestamp, nonce, source}
        """
        path = os.path.join(self.config.get('logs_path'), 'inyectados.log')
        cutoff = datetime.now() - timedelta(days=days)
        entries = []
        
        try:
            with open(path, 'r') as f:
                for line in f:
                    try:
                        entry = json.loads(line.strip())
                        entry_time = datetime.fromisoformat(entry['timestamp'])
                        
                        if entry_time >= cutoff:
                            entry['nonce'] = int(entry['nonce'])
                            entries.append(entry)
                    except (json.JSONDecodeError, KeyError) as e:
                        self.logger.warning(f"Entrada corrupta: {line.strip()} - {str(e)}")
        except Exception as e:
            self.logger.error(f"Error cargando nonces inyectados: {str(e)}")
        
        return entries

    @lru_cache(maxsize=2)
    def load_training_data(self) -> pd.DataFrame:
        """
        Carga dataset de entrenamiento con validación de estructura
        
        Returns:
            pd.DataFrame: DataFrame con columnas requeridas para modelos IA
        """
        path = os.path.join(self.config.get('data_path'), 'nonce_training_data.csv')
        required_columns = {
            'nonce': 'uint32',
            'block_timestamp': 'datetime64[s]',
            'difficulty': 'float32',
            'accepted': 'bool'
        }
        
        try:
            df = pd.read_csv(
                path,
                dtype=required_columns,
                parse_dates=['block_timestamp'],
                usecols=required_columns.keys()
            )
            
            # Eliminar filas con datos temporales inválidos
            df = df[df['block_timestamp'].between(
                datetime(2020,1,1), 
                datetime.now()
            )]
            
            return df
        
        except Exception as e:
            self.logger.error(f"Error cargando datos de entrenamiento: {str(e)}")
            return pd.DataFrame()

    def load_models(self) -> Dict[str, any]:
        """
        Carga modelos IA desde disco con verificación de versiones
        
        Returns:
            Dict[str, any]: Diccionario de modelos cargados
        """
        models = {}
        model_paths = {
            'ethical': 'ethical_nonce_model.joblib',
            'classifier': 'hash_classifier_model.joblib'
        }
        
        for name, rel_path in model_paths.items():
            path = os.path.join(self.config.get('models_path'), rel_path)
            try:
                models[name] = joblib.load(path)
                self.logger.info(f"Modelo {name} cargado correctamente")
            except Exception as e:
                self.logger.error(f"Error cargando modelo {name}: {str(e)}")
                models[name] = None
        
        return models

    def load_all(self) -> Dict[str, Union[List, pd.DataFrame]]:
        """
        Carga todos los datos de nonces en un solo llamado
        
        Returns:
            Dict: Diccionario con todos los datasets cargados
        """
        return {
            'valid': self.load_valid_nonces(),
            'hashes': self.load_hash_data(),
            'injected': self.load_injected_nonces(),
            'training': self.load_training_data(),
            'models': self.load_models()
        }

if __name__ == "__main__":
    # Ejemplo de uso
    loader = NonceLoader()
    
    # Cargar todos los datos
    data = loader.load_all()
    
    # Acceso a datos específicos
    print(f"Nonces válidos cargados: {len(data['valid'])}")
    print(f"Registros de hash: {data['hashes'].shape[0]}")
    print(f"Modelos cargados: {list(data['models'].keys())}")