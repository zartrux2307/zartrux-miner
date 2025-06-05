import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from scipy.integrate import solve_ivp
from pathlib import Path
from typing import Dict, Optional, Tuple
from utils.config_manager import ConfigManager
from utils.nonce_loader import NonceLoader
from utils.data_preprocessing import DataPreprocessor
import logging
import joblib

class LorenzEntropyAnalyzer:
    """Clase avanzada para análisis de comportamiento caótico en sistemas de minería."""
    
    def __init__(self, config: Optional[ConfigManager] = None):
        self.config = config or ConfigManager()
        self.loader = NonceLoader(self.config)
        self.logger = logging.getLogger(self.__class__.__name__)
        self._initialize_parameters()
        self._prepare_directories()
        
    def _initialize_parameters(self):
        """Carga parámetros desde la configuración del proyecto"""
        params = self.config.get('lorenz_params', {})
        self.sigma = params.get('sigma', 10.0)
        self.beta = params.get('beta', 8.0/3.0)
        self.rho = params.get('rho', 28.0)
        self.default_steps = self.config.get('simulation_steps', 10000)
        
    def _prepare_directories(self):
        """Crea estructura de directorios necesaria"""
        Path(self.config['reports_path']).mkdir(exist_ok=True)
        Path(self.config['simulations_path']).mkdir(exist_ok=True)

    def lorenz_system(self, t: float, state: np.ndarray) -> np.ndarray:
        """Sistema de ecuaciones diferenciales del modelo de Lorenz con validación."""
        try:
            x, y, z = state
            return np.array([
                self.sigma * (y - x),
                x * (self.rho - z) - y,
                x * y - self.beta * z
            ])
        except Exception as e:
            self.logger.error(f"Error en sistema de ecuaciones: {str(e)}")
            raise

    def simulate_chaotic_system(self, initial_state: list, t_span: Tuple[float, float], 
                               num_steps: Optional[int] = None) -> pd.DataFrame:
        """Ejecuta simulación con manejo de errores y optimización de memoria."""
        num_steps = num_steps or self.default_steps
        try:
            t_eval = np.linspace(t_span[0], t_span[1], num_steps)
            sol = solve_ivp(
                self.lorenz_system,
                t_span,
                initial_state,
                t_eval=t_eval,
                method='LSODA'
            )
            
            df = pd.DataFrame({
                'time': sol.t,
                'x': sol.y[0],
                'y': sol.y[1],
                'z': sol.y[2]
            })
            
            self._save_simulation_data(df)
            return df
            
        except Exception as e:
            self.logger.error(f"Error en simulación: {str(e)}")
            raise

    def _save_simulation_data(self, df: pd.DataFrame):
        """Guarda resultados de simulación en formato optimizado"""
        path = Path(self.config['simulations_path']) / "lorenz_trajectory.feather"
        df = DataPreprocessor().reduce_memory_usage(df)
        df.to_feather(path)
        self.logger.info(f"Datos de simulación guardados en {path}")

    def analyze_nonce_entropy(self, window_size: int = 1000):
        """Analiza entropía de nonces usando patrones caóticos"""
        nonce_data = self.loader.load_hash_data()
        entropy_profile = []
        
        for i in range(0, len(nonce_data), window_size):
            window = nonce_data.iloc[i:i+window_size]
            entropy = self._calculate_window_entropy(window)
            entropy_profile.append(entropy)
            
        self._generate_entropy_report(entropy_profile)

    def _calculate_window_entropy(self, window: pd.DataFrame) -> Dict:
        """Calcula métricas de entropía para una ventana de datos"""
        try:
            x = window['hash_score'].values
            y = window['nonce'].astype(float).values
            state = [x.mean(), y.std(), np.corrcoef(x, y)[0,1]]
            
            simulation = self.simulate_chaotic_system(
                state, 
                t_span=(0, 10),
                num_steps=1000
            )
            
            return {
                'lyapunov_exponent': self._estimate_lyapunov(simulation),
                'entropy_variation': simulation['x'].std(),
                'window_size': len(window)
            }
            
        except Exception as e:
            self.logger.warning(f"Error en cálculo de ventana: {str(e)}")
            return {}

    def _estimate_lyapunov(self, df: pd.DataFrame) -> float:
        """Estima el exponente de Lyapunov para cuantificar caoticidad"""
        try:
            delta_x = np.diff(df['x'])
            return np.mean(np.log(np.abs(delta_x[delta_x != 0])))
        except:
            return 0.0

    def visualize_attractor(self, df: pd.DataFrame, save: bool = True):
        """Genera visualización 3D profesional del atractor"""
        plt.figure(figsize=(16, 12))
        ax = plt.axes(projection='3d')
        
        # Configuración estética
        ax.plot(df['x'], df['y'], df['z'], 
               lw=0.5, 
               color=plt.cm.viridis(np.linspace(0,1,len(df))))
        
        ax.set_xlabel("Eje X\n(Intensidad de Hash)", fontsize=10)
        ax.set_ylabel("Eje Y\n(Variación de Nonces)", fontsize=10)
        ax.set_zlabel("Eje Z\n(Entropía Temporal)", fontsize=10)
        ax.set_title("Atractor de Lorenz en Datos de Minería", pad=20)
        
        if save:
            path = Path(self.config['reports_path']) / "lorenz_attractor.png"
            plt.savefig(path, dpi=300, bbox_inches='tight')
            plt.close()
            self.logger.info(f"Visualización guardada en {path}")
        else:
            plt.show()

    def _generate_entropy_report(self, results: list):
        """Genera reporte completo de análisis de entropía"""
        report_path = Path(self.config['reports_path']) / "chaos_analysis"
        report_path.mkdir(exist_ok=True)
        
        # Guardar datos
        pd.DataFrame(results).to_csv(report_path / "entropy_profile.csv", index=False)
        
        # Guardar visualización
        combined_df = pd.concat([pd.DataFrame(r) for r in results])
        self.visualize_attractor(combined_df)
        
        # Guardar metadatos
        metadata = {
            'parameters': {
                'sigma': self.sigma,
                'beta': self.beta,
                'rho': self.rho
            },
            'data_source': self.config.get('data_path'),
            'timestamp': pd.Timestamp.now().isoformat()
        }
        joblib.dump(metadata, report_path / "simulation_metadata.joblib")

if __name__ == "__main__":
    # Ejemplo de uso integrado
    config = ConfigManager(config_path="config/miner_config.json")
    analyzer = LorenzEntropyAnalyzer(config)
    
    # Análisis completo
    try:
        # Simulación de referencia
        reference_sim = analyzer.simulate_chaotic_system(
            initial_state=[1.0, 1.0, 1.0],
            t_span=(0, 50)
        )
        analyzer.visualize_attractor(reference_sim)
        
        # Análisis de datos reales
        analyzer.analyze_nonce_entropy()
        
    except Exception as e:
        analyzer.logger.error(f"Error en análisis principal: {str(e)}")