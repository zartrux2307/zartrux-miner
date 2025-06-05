import pandas as pd
import numpy as np
import os
import time
import logging
from collections import Counter
from scipy import stats as scipy_stats
from typing import Dict, Optional, Tuple, Union
from dataclasses import dataclass, field
from datetime import datetime
from tqdm.auto import tqdm
import dask.dataframe as dd
from dask.diagnostics import ProgressBar

# Configuración de logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("NonceAnalyzer")

@dataclass
class AnalysisConfig:
    entropy_bin_size: int = 2**20
    outlier_threshold: float = 3.5
    temporal_resolution: str = '6H'
    default_bins: int = 100
    timestamp_unit: str = 's'
    max_anomalies: int = 1000
    chunk_size: int = 100000
    memory_limit: str = '2GB'
    
    def __post_init__(self):
        self.validate()
    
    def validate(self):
        if self.entropy_bin_size <= 0:
            raise ValueError("entropy_bin_size debe ser positivo")
        if not self.timestamp_unit in ['s', 'ms', 'ns']:
            raise ValueError("Unidad de tiempo inválida")
        if self.max_anomalies <= 0:
            raise ValueError("max_anomalies debe ser positivo")

class NoncePatternAnalyzer:
    def __init__(self, data_path: str, config: Optional[AnalysisConfig] = None):
        self.data_path = data_path
        self.config = config or AnalysisConfig()
        self.df: Union[pd.DataFrame, dd.DataFrame] = None
        self.metadata: Dict = field(default_factory=dict)
        self._initialize()

    def _initialize(self):
        """Inicializa parámetros y verifica el archivo"""
        if not os.path.exists(self.data_path):
            raise FileNotFoundError(f"Archivo no encontrado: {self.data_path}")
        
        self.file_type = 'parquet' if self.data_path.endswith('.parquet') else 'csv'
        self.metadata.update({
            'file_size': os.path.getsize(self.data_path),
            'file_type': self.file_type,
            'initialized_at': datetime.now().isoformat()
        })

    def load_data(self) -> None:
        """Carga optimizada de datos con manejo de memoria"""
        start_time = time.time()
        mem_usage = []
        
        try:
            if self.file_type == 'parquet':
                self.df = dd.read_parquet(self.data_path)
            else:
                self.df = dd.read_csv(
                    self.data_path, 
                    dtype={'nonce': 'uint64'},
                    blocksize=self.config.chunk_size
                )

            with ProgressBar():
                self.df = self.df.persist()
                mem_usage.append(self.df.memory_usage(deep=True).compute().sum())

            self._preprocess_data()
            
            self.metadata.update({
                'load_time': round(time.time() - start_time, 2),
                'memory_usage': f"{sum(mem_usage) / 1e6:.2f} MB",
                'rows_loaded': len(self.df),
                'columns': list(self.df.columns)
            })
            
            logger.info(f"Datos cargados: {self.metadata['rows_loaded']} filas")
            
        except Exception as e:
            logger.error(f"Error cargando datos: {str(e)}")
            raise

    def _preprocess_data(self) -> None:
        """Limpieza y transformación de datos"""
        # Filtrar nonces inválidos
        initial_count = len(self.df)
        self.df = self.df.dropna(subset=['nonce'])
        
        # Convertir timestamp
        if 'block_timestamp' in self.df.columns:
            self.df['block_datetime'] = dd.to_datetime(
                self.df['block_timestamp'], 
                unit=self.config.timestamp_unit,
                errors='coerce'
            )
            self.df = self.df.dropna(subset=['block_datetime'])
        
        filtered_count = len(self.df)
        self.metadata.update({
            'filtered_rows': initial_count - filtered_count,
            'preprocess_time': datetime.now().isoformat()
        })

    def _calculate_distribution(self) -> Dict:
        """Calcula distribución de frecuencias optimizada"""
        counts = self.df['nonce'].value_counts().compute()
        return {
            'counts': counts.to_dict(),
            'unique_values': len(counts),
            'entropy': self._calculate_entropy(counts)
        }

    def _calculate_entropy(self, counts: pd.Series) -> float:
        """Calcula entropía de Shannon desde conteos"""
        probs = counts / counts.sum()
        return round(float(-np.sum(probs * np.log2(probs))), 4)

    def temporal_analysis(self) -> Dict:
        """Análisis temporal de la distribución de nonces"""
        if 'block_datetime' not in self.df.columns:
            return {}
        
        return self.df.set_index('block_datetime').resample(
            self.config.temporal_resolution
        )['nonce'].agg(['mean', 'std', 'count']).compute().to_dict()

    def detect_anomalies(self, method: str = 'iqr') -> pd.DataFrame:
        """Detección de anomalías con múltiples métodos"""
        if method == 'iqr':
            q1, q3 = self.df['nonce'].quantile([0.25, 0.75]).compute()
            iqr = q3 - q1
            lower_bound = q1 - (1.5 * iqr)
            upper_bound = q3 + (1.5 * iqr)
            anomalies = self.df[(self.df['nonce'] < lower_bound) | (self.df['nonce'] > upper_bound)]
        elif method == 'zscore':
            mean = self.df['nonce'].mean().compute()
            std = self.df['nonce'].std().compute()
            anomalies = self.df[np.abs(self.df['nonce'] - mean) > (self.config.outlier_threshold * std)]
        else:
            raise ValueError("Método de detección no soportado")
        
        return anomalies.head(self.config.max_anomalies).compute()

    def advanced_analytics(self) -> Dict:
        """Pipeline completo de análisis"""
        results = {}
        
        # Distribución básica
        dist = self._calculate_distribution()
        results.update(dist)
        
        # Análisis temporal
        results['temporal'] = self.temporal_analysis()
        
        # Detección de anomalías
        results['anomalies'] = {
            'iqr': self.detect_anomalies('iqr').to_dict(orient='records'),
            'zscore': self.detect_anomalies('zscore').to_dict(orient='records')
        }
        
        # Métricas estadísticas
        results['stats'] = self.df['nonce'].describe(percentiles=[
            0.05, 0.25, 0.5, 0.75, 0.95
        ]).compute().to_dict()
        
        return results

    def generate_report(self, format: str = 'json') -> Union[Dict, pd.DataFrame]:
        """Genera reporte en múltiples formatos"""
        report = {
            'metadata': self.metadata,
            'analytics': self.advanced_analytics()
        }
        
        if format == 'dataframe':
            return pd.json_normalize(report)
        return report

# Uso ejemplo
if __name__ == "__main__":
    analyzer = NoncePatternAnalyzer(
        data_path="data/nonces.parquet",
        config=AnalysisConfig(
            temporal_resolution='1D',
            max_anomalies=500
        )
    )
    
    analyzer.load_data()
    report = analyzer.generate_report()
    
    print(f"Reporte generado con {report['metadata']['rows_loaded']} filas analizadas")
    print(f"Entropía del sistema: {report['analytics']['entropy']}")