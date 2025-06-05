"""
Módulo de preprocesamiento de datos para modelos de IA minera
Incluye integración con sistemas temporales, análisis estadístico y preparación para entrenamiento
"""

import pandas as pd
import numpy as np
from sklearn.preprocessing import RobustScaler, PowerTransformer
from sklearn.model_selection import TimeSeriesSplit
from sklearn.pipeline import Pipeline
from sklearn.impute import SimpleImputer
from dateutil.parser import parse
import warnings
import json
import hashlib
import gc

# Módulos internos del proyecto
from .nonce_loader import load_nonce_data
from .FeatureEngineer import TemporalFeatures, FrequencyEncoder
from .nonce_stats import calculate_entropy_features
from .config_manager import get_ia_config

warnings.filterwarnings("ignore", category=UserWarning)

class NonceDataPreprocessor:
    def __init__(self, config_path='config/ia_config.json'):
        self.config = get_ia_config(config_path)
        self.feature_pipeline = self._build_feature_pipeline()
        self.data = None
        self.cache_metadata = {}
        
        # Optimización de memoria
        self.dtype_mapping = {
            'nonce': 'uint32',
            'hash_score': 'float32',
            'block_number': 'uint32',
            'difficulty': 'float32',
            'worker_id': 'category'
        }

    def _build_feature_pipeline(self):
        """Construye pipeline de transformación de características"""
        return Pipeline([
            ('temporal_features', TemporalFeatures()),
            ('frequency_encoder', FrequencyEncoder()),
            ('imputer', SimpleImputer(strategy='median')),
            ('scaler', RobustScaler()),
            ('power_transform', PowerTransformer(method='yeo-johnson'))
        ])

    def _load_and_merge_data(self):
        """Carga y fusiona datos de múltiples fuentes"""
        sources = {
            'successful': self.config['data_paths']['successful_nonces'],
            'hashes': self.config['data_paths']['nonce_hashes'],
            'injected': self.config['data_paths']['injected_nonces']
        }
        
        dfs = []
        for data_type, path in sources.items():
            df = load_nonce_data(path, data_type)
            df = self._optimize_data_types(df, data_type)
            dfs.append(df)
            
        merged_df = pd.concat(dfs, axis=0).drop_duplicates(subset=['nonce', 'timestamp'])
        return merged_df.sort_values('timestamp').reset_index(drop=True)

    def _optimize_data_types(self, df, data_type):
        """Optimiza los tipos de datos para eficiencia de memoria"""
        for col, dtype in self.dtype_mapping.items():
            if col in df.columns:
                df[col] = df[col].astype(dtype)
        return df

    def _calculate_derived_features(self, df):
        """Calcula características derivadas y estadísticas"""
        # Características temporales
        df['timestamp'] = pd.to_datetime(df['timestamp'], utc=True)
        df['time_since_last'] = df['timestamp'].diff().dt.total_seconds().fillna(0)
        
        # Estadísticas de ventana temporal
        window_size = self.config['processing_params']['temporal_window']
        df['rolling_hash_mean'] = df['hash_score'].rolling(window_size, min_periods=1).mean()
        df['rolling_nonce_std'] = df['nonce'].rolling(window_size, min_periods=1).std()
        
        # Características de entropía
        entropy_features = calculate_entropy_features(
            df['nonce'].values, 
            window_size=self.config['processing_params']['entropy_window']
        )
        df = pd.concat([df, entropy_features], axis=1)
        
        return df

    def _clean_data(self, df):
        """Limpieza avanzada de datos y manejo de valores atípicos"""
        # Filtrar hashes inválidos
        df = df[df['hash_score'] > 0]
        
        # Manejo de outliers usando IQR
        for col in ['hash_score', 'time_since_last']:
            q1 = df[col].quantile(0.25)
            q3 = df[col].quantile(0.75)
            iqr = q3 - q1
            df = df[(df[col] >= q1 - 1.5*iqr) & (df[col] <= q3 + 1.5*iqr)]
            
        return df.dropna()

    def _encode_categoricals(self, df):
        """Codificación de variables categóricas"""
        if 'worker_id' in df.columns:
            df['worker_id'] = df['worker_id'].cat.codes
        return df

    def _generate_metadata(self, df):
        """Genera metadatos para seguimiento de transformaciones"""
        self.cache_metadata = {
            'data_hash': hashlib.sha256(pd.util.hash_pandas_object(df).hexdigest(),
            'columns': list(df.columns),
            'stats': {
                'mean': df.mean().to_dict(),
                'std': df.std().to_dict(),
                'min': df.min().to_dict(),
                'max': df.max().to_dict()
            }
        }

    def preprocess(self):
        """Ejecuta el pipeline completo de preprocesamiento"""
        print("Cargando y fusionando datos...")
        raw_data = self._load_and_merge_data()
        
        print("Calculando características derivadas...")
        processed_data = self._calculate_derived_features(raw_data)
        
        print("Limpiando datos...")
        cleaned_data = self._clean_data(processed_data)
        
        print("Codificando variables...")
        encoded_data = self._encode_categoricals(cleaned_data)
        
        print("Aplicando transformaciones...")
        final_data = self.feature_pipeline.fit_transform(encoded_data)
        
        print("Generando metadatos...")
        self._generate_metadata(encoded_data)
        
        # Liberar memoria
        del raw_data, processed_data, cleaned_data, encoded_data
        gc.collect()
        
        self.data = final_data
        return self.data

    def split_data(self, test_size=0.2):
        """Divide los datos en conjuntos de entrenamiento y prueba temporal"""
        tscv = TimeSeriesSplit(n_splits=int(1/test_size))
        train_index, test_index = next(tscv.split(self.data))
        return self.data.iloc[train_index], self.data.iloc[test_index]

    def save_processed_data(self, path='data/nonce_training_data.csv'):
        """Guarda los datos procesados y metadatos"""
        # Guardar datos
        self.data.to_csv(path, index=False)
        
        # Guardar metadatos
        meta_path = path.replace('.csv', '_metadata.json')
        with open(meta_path, 'w') as f:
            json.dump(self.cache_metadata, f)
            
        print(f"Datos guardados en {path} | Metadatos en {meta_path}")

    @staticmethod
    def load_processed_data(path='data/nonce_training_data.csv'):
        """Carga datos preprocesados con sus metadatos"""
        data = pd.read_csv(path)
        meta_path = path.replace('.csv', '_metadata.json')
        
        with open(meta_path, 'r') as f:
            metadata = json.load(f)
            
        return data, metadata

if __name__ == "__main__":
    # Ejemplo de uso
    preprocessor = NonceDataPreprocessor()
    processed_data = preprocessor.preprocess()
    train_data, test_data = preprocessor.split_data()
    preprocessor.save_processed_data()
    
    print("\nPreprocesamiento completado exitosamente!")
    print(f"Tamaño del conjunto de entrenamiento: {len(train_data)}")
    print(f"Tamaño del conjunto de prueba: {len(test_data)}")