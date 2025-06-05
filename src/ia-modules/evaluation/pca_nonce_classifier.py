import numpy as np
import pandas as pd
import joblib
import matplotlib.pyplot as plt
import seaborn as sns
from pathlib import Path
from typing import Tuple, Dict, Optional
from sklearn.decomposition import PCA
from sklearn.exceptions import ConvergenceWarning
from utils.nonce_loader import NonceLoader
from utils.data_preprocessing import DataPreprocessor
from utils.config_manager import ConfigManager
import logging
import warnings

class PCANonceClassifier:
    """Clase avanzada para reducción de dimensionalidad y análisis de nonces con integración de proyecto."""

    def __init__(self, config: Optional[ConfigManager] = None):
        """
        Inicializa el clasificador PCA con configuración del proyecto.
        
        Args:
            config (ConfigManager): Instancia de configuración del proyecto
        """
        self.config = config or ConfigManager()
        self.loader = NonceLoader(self.config)
        self.preprocessor = DataPreprocessor()
        self.logger = logging.getLogger(self.__class__.__name__)
        
        # Cargar parámetros desde configuración
        self.num_components = self.config.get('pca_components', 5)
        self.model_path = Path(self.config.get('models_path')) / "pca_nonce_model.joblib"
        
        self.scaler = None
        self.pca = None
        self._initialize_components()

    def _initialize_components(self):
        """Inicializa componentes con verificación de tipos de datos"""
        try:
            self.scaler = StandardScaler()
            self.pca = PCA(n_components=self.num_components, random_state=self.config.get('random_seed', 42))
        except Exception as e:
            self.logger.error(f"Error inicializando componentes: {str(e)}")
            raise

    def load_data(self) -> pd.DataFrame:
        """Carga y prepara datos usando la infraestructura del proyecto"""
        data = self.loader.load_training_data()
        
        required_columns = ['nonce', 'block_timestamp', 'difficulty', 'hash_score']
        if not all(col in data.columns for col in required_columns):
            missing = set(required_columns) - set(data.columns)
            raise ValueError(f"Columnas requeridas faltantes: {missing}")
            
        return self.preprocessor.process(data)

    def fit_transform(self) -> Tuple[np.ndarray, Dict]:
        """
        Ejecuta el pipeline completo de PCA con manejo de errores y validación.
        
        Returns:
            Tuple: (Datos transformados, Métricas de varianza)
        """
        try:
            df = self.load_data()
            features = df.drop(columns=["nonce", "block_timestamp"])
            
            # Preprocesamiento y validación
            self._check_data_quality(features)
            scaled_features = self.scaler.fit_transform(features)
            
            with warnings.catch_warnings():
                warnings.simplefilter("ignore", category=ConvergenceWarning)
                pca_features = self.pca.fit_transform(scaled_features)

            metrics = self._calculate_metrics()
            self._save_artifacts(df)
            
            return pca_features, metrics
            
        except Exception as e:
            self.logger.error(f"Error en fit_transform: {str(e)}")
            raise

    def _check_data_quality(self, data: pd.DataFrame):
        """Realiza validaciones críticas de los datos de entrada"""
        if data.isnull().any().any():
            raise ValueError("Datos contienen valores nulos")
            
        if (data.std() == 0).any():
            problematic = data.columns[data.std() == 0].tolist()
            raise ValueError(f"Características constantes detectadas: {problematic}")

    def _calculate_metrics(self) -> Dict:
        """Calcula métricas detalladas de rendimiento del PCA"""
        variance = self.pca.explained_variance_ratio_
        cumulative = np.cumsum(variance)
        
        return {
            'components': {f"PC{i+1}": var for i, var in enumerate(variance)},
            'cumulative_variance': {f"PC{i+1}": cum for i, cum in enumerate(cumulative)},
            'total_variance': cumulative[-1],
            'n_features': self.pca.n_features_in_,
            'model_version': self.config.get('model_version', '1.0.0')
        }

    def plot_explained_variance(self, save_path: Optional[Path] = None):
        """Genera y guarda visualización profesional de varianza explicada"""
        plt.figure(figsize=(10, 6))
        
        # Gráfico de barras para varianza individual
        ax = sns.barplot(
            x=np.arange(1, self.num_components + 1),
            y=self.pca.explained_variance_ratio_,
            palette="Blues_d"
        )
        
        # Línea para varianza acumulada
        cumulative = np.cumsum(self.pca.explained_variance_ratio_)
        ax2 = ax.twinx()
        ax2.plot(
            np.arange(1, self.num_components + 1),
            cumulative,
            color='red',
            marker='o',
            linestyle='--'
        )
        
        # Configuración estética
        ax.set_title("Varianza Explicada por Componentes Principales", pad=20)
        ax.set_xlabel("Componentes Principales")
        ax.set_ylabel("Varianza Individual", color='blue')
        ax2.set_ylabel("Varianza Acumulada", color='red')
        ax2.grid(False)
        
        plt.tight_layout()
        
        if save_path:
            save_path.parent.mkdir(exist_ok=True)
            plt.savefig(save_path, dpi=300, bbox_inches='tight')
            plt.close()
        else:
            plt.show()

    def save_model(self):
        """Guarda el modelo con metadatos y versionado"""
        artifact = {
            'scaler': self.scaler,
            'pca': self.pca,
            'metadata': {
                'training_date': pd.Timestamp.now().isoformat(),
                'data_version': self.config.get('data_version'),
                'num_samples': self.loader.load_training_data().shape[0],
                'features': self.pca.feature_names_in_.tolist()
            }
        }
        
        joblib.dump(artifact, self.model_path)
        self.logger.info(f"Modelo guardado en {self.model_path}")

    def load_model(self):
        """Carga un modelo existente con verificación de versión"""
        try:
            artifact = joblib.load(self.model_path)
            self.scaler = artifact['scaler']
            self.pca = artifact['pca']
            
            current_data_version = self.config.get('data_version')
            if artifact['metadata']['data_version'] != current_data_version:
                self.logger.warning("Versión de datos diferente a la usada en entrenamiento")
                
            self.logger.info("Modelo cargado exitosamente")
            
        except Exception as e:
            self.logger.error(f"Error cargando modelo: {str(e)}")
            raise

    def _save_artifacts(self, data: pd.DataFrame):
        """Guarda artefactos de diagnóstico y resultados intermedios"""
        report_path = Path(self.config.get('reports_path')) / "pca_analysis"
        report_path.mkdir(exist_ok=True)
        
        # Guardar gráfico
        self.plot_explained_variance(report_path / "variance_plot.png")
        
        # Guardar métricas
        metrics = self._calculate_metrics()
        pd.DataFrame(metrics['components'].items()).to_csv(
            report_path / "variance_metrics.csv",
            index=False
        )

    def example_usage(self):
        """Ejemplo de uso integrado con el proyecto"""
        try:
            # Entrenamiento y transformación
            pca_features, metrics = self.fit_transform()
            
            # Guardar modelo
            self.save_model()
            
            # Generar reporte
            self.plot_explained_variance()
            
            return {
                "status": "success",
                "components": pca_features.shape[1],
                "total_variance": metrics['total_variance']
            }
            
        except Exception as e:
            return {
                "status": "error",
                "message": str(e)
            }

if __name__ == "__main__":
    # Configuración inicial
    config = ConfigManager(config_path="config/miner_config.json")
    
    # Ejemplo de uso
    pca = PCANonceClassifier(config)
    result = pca.example_usage()
    print(f"Resultado del análisis PCA: {result}")