import numpy as np
import pandas as pd
from scipy import stats
from scipy.special import rel_entr
from typing import Dict, Optional
import logging
from utils.nonce_loader import NonceLoader
from utils.config_manager import ConfigManager
from utils.data_preprocessing import DataPreprocessor

class KLDivergenceAnalyzer:
    """Clase avanzada para análisis de divergencia de distribuciones de nonces."""
    
    def __init__(self, config: Optional[ConfigManager] = None):
        self.config = config or ConfigManager()
        self.loader = NonceLoader(self.config)
        self.preprocessor = DataPreprocessor()
        self.logger = logging.getLogger(self.__class__.__name__)
        self.epsilon = self.config.get('kl_epsilon', 1e-9)
        self.reference_data = None

    def load_reference_distribution(self):
        """Carga y cachea la distribución de referencia óptima"""
        if self.reference_data is None:
            try:
                self.reference_data = self.loader.load_training_data()
                self.reference_data = self._prepare_distribution(self.reference_data)
                self.logger.info("Distribución de referencia cargada")
            except Exception as e:
                self.logger.error(f"Error cargando referencia: {str(e)}")
                raise
        return self.reference_data

    def _prepare_distribution(self, df: pd.DataFrame) -> pd.Series:
        """Prepara y normaliza la distribución de nonces"""
        dist = df['nonce'].value_counts(dropna=False)
        return (dist + self.epsilon) / (dist.sum() + len(dist) * self.epsilon)

    def _align_distributions(self, p: pd.Series, q: pd.Series) -> tuple:
        """Alinea las distribuciones para garantizar el mismo espacio muestral"""
        all_nonces = p.index.union(q.index)
        p = p.reindex(all_nonces, fill_value=self.epsilon).values
        q = q.reindex(all_nonces, fill_value=self.epsilon).values
        return p / p.sum(), q / q.sum()

    def compute_kl_divergence(self, p: np.ndarray, q: np.ndarray) -> Dict:
        """Calcula métricas de divergencia robustas con validación estadística"""
        results = {}
        
        try:
            # Cálculo KL Divergence
            kl = np.sum(rel_entr(p, q))
            results['kl_divergence'] = max(kl, 0)  # Evitar valores negativos por precisión numérica
            
            # Cálculo Jensen-Shannon Divergence
            m = 0.5 * (p + q)
            js = 0.5 * (np.sum(rel_entr(p, m)) + 0.5 * (np.sum(rel_entr(q, m)))
            results['js_divergence'] = js
            
            # Test chi-cuadrado
            chi2, p_value = stats.chisquare(f_obs=p, f_exp=q)
            results['chi_square'] = {
                'statistic': chi2,
                'p_value': p_value,
                'significant': p_value < self.config.get('alpha', 0.05)
            }
            
        except Exception as e:
            self.logger.error(f"Error en cálculo de divergencia: {str(e)}")
            raise
            
        return results

    def analyze_current_distribution(self) -> Dict:
        """Ejecuta análisis completo contra la distribución de referencia"""
        try:
            # Cargar datos
            current_data = self.loader.load_hash_data()
            current_dist = self._prepare_distribution(current_data)
            reference_dist = self.load_reference_distribution()
            
            # Alinear distribuciones
            p_aligned, q_aligned = self._align_distributions(current_dist, reference_dist)
            
            # Calcular métricas
            divergence_metrics = self.compute_kl_divergence(p_aligned, q_aligned)
            
            # Métricas adicionales
            results = {
                **divergence_metrics,
                'sample_size_current': len(current_data),
                'sample_size_reference': len(self.reference_data),
                'unique_nonces_current': len(current_dist),
                'unique_nonces_reference': len(reference_dist),
                'effect_size': self._calculate_effect_size(p_aligned, q_aligned)
            }
            
            # Generar reporte
            self._generate_report(results)
            
            return results
            
        except Exception as e:
            self.logger.error(f"Error en análisis completo: {str(e)}")
            return {'error': str(e)}

    def _calculate_effect_size(self, p: np.ndarray, q: np.ndarray) -> float:
        """Calcula el tamaño del efecto usando distancia Hellinger"""
        return np.sqrt(0.5 * np.sum((np.sqrt(p) - np.sqrt(q)) ** 2))

    def _generate_report(self, results: Dict):
        """Genera y guarda reporte detallado"""
        report_path = Path(self.config.get('reports_path')) / "kl_analysis"
        report_path.mkdir(exist_ok=True)
        
        # Guardar métricas
        pd.DataFrame([results]).to_csv(report_path / "metrics.csv", index=False)
        
        # Loggeo de resultados
        self.logger.info(f"""
        Análisis de Divergencia Completo:
        - KL Divergence: {results['kl_divergence']:.4f}
        - JS Divergence: {results['js_divergence']:.4f}
        - Tamaño de Efecto: {results['effect_size']:.4f}
        - Significancia Estadística: {results['chi_square']['significant']}
        """)

    def monitor_distribution_changes(self, window_size: int = 1000):
        """Monitorea cambios en la distribución en tiempo real"""
        # Implementación de ventana deslizante para monitoreo continuo
        pass  # (Omitido por brevedad, pero sería parte de la implementación completa)

if __name__ == "__main__":
    # Ejemplo de uso integrado
    config = ConfigManager(config_path="config/miner_config.json")
    analyzer = KLDivergenceAnalyzer(config)
    
    try:
        results = analyzer.analyze_current_distribution()
        print(f"Resultados del análisis:\n{pd.DataFrame([results])}")
    except Exception as e:
        print(f"Error en análisis: {str(e)}")