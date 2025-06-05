import numpy as np
import scipy.stats as stats
from typing import Union, Dict, List
import logging
import warnings

# Configuración de logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

class NonceStats:
    """
    Clase para análisis estadístico avanzado de nonces en minería blockchain.
    
    Características principales:
    - Cálculo optimizado de métricas estadísticas
    - Detección de patrones y anomalías
    - Integración con pipelines de IA
    - Manejo de grandes volúmenes de datos
    
    Métodos:
    - coeficiente_variacion: Medida de dispersión relativa
    - percentiles: Distribución de valores con intervalos de confianza
    - test_normalidad: Pruebas de normalidad mejoradas
    - kurtosis_skewness: Análisis de forma de distribución
    - analisis_completo: Reporte unificado de métricas clave
    """

    @staticmethod
    def _validar_entrada(values: Union[List[float], np.ndarray]) -> np.ndarray:
        """Valida y preprocesa la entrada para los cálculos estadísticos."""
        if not isinstance(values, np.ndarray):
            values = np.array(values)
            
        if values.size == 0:
            logger.error("Array de entrada vacío")
            raise ValueError("El array de entrada no puede estar vacío")
            
        if np.isnan(values).any():
            logger.warning("Datos contienen NaN, aplicando limpieza")
            values = values[~np.isnan(values)]
            
        return values.astype(np.float64)

    @staticmethod
    def coeficiente_variacion(values: Union[List[float], np.ndarray], robusto: bool = False) -> float:
        """
        Calcula el coeficiente de variación con opción robusta (usando mediana y MAD).
        
        Args:
            values: Array de valores numéricos
            robusto: Si es True, usa mediana y MAD en lugar de media y desviación estándar
            
        Returns:
            float: Coeficiente de variación en porcentaje
            
        Ejemplo:
        >>> NonceStats.coeficiente_variacion([1, 2, 3, 4, 5])
        47.14045207910317
        """
        try:
            values = NonceStats._validar_entrada(values)
            
            if robusto:
                central = np.median(values)
                dispersion = stats.median_abs_deviation(values)
            else:
                central = np.mean(values)
                dispersion = np.std(values)
                
            with np.errstate(invalid='ignore'):
                cv = (dispersion / central) * 100 if central != 0 else 0.0
                
            return round(cv, 4)
            
        except Exception as e:
            logger.error(f"Error calculando CV: {str(e)}")
            return np.nan

    @staticmethod
    def percentiles(values: Union[List[float], np.ndarray], 
                   percentiles: List[float] = [25, 50, 75],
                   intervalo_confianza: float = 0.95) -> Dict[str, Dict[str, float]]:
        """
        Calcula percentiles con intervalos de confianza usando bootstrapping.
        
        Args:
            values: Array de valores numéricos
            percentiles: Lista de percentiles a calcular
            intervalo_confianza: Nivel de confianza (0-1)
            
        Returns:
            dict: Diccionario con percentiles y sus intervalos de confianza
            
        Ejemplo:
        >>> NonceStats.percentiles(np.random.normal(0, 1, 1000))
        """
        try:
            values = NonceStats._validar_entrada(values)
            resultados = {}
            
            # Configuración de bootstrapping
            n_bootstraps = 1000 if len(values) < 1e4 else 100
            boots = np.random.choice(values, (n_bootstraps, len(values)), replace=True)
            
            for p in percentiles:
                estimaciones = np.percentile(boots, p, axis=1)
                lower = np.percentile(estimaciones, (1 - intervalo_confianza)/2 * 100)
                upper = np.percentile(estimaciones, (1 + intervalo_confianza)/2 * 100)
                
                resultados[f'P{p}'] = {
                    'valor': np.percentile(values, p),
                    'intervalo_confianza': (round(lower, 4), round(upper, 4))
                }
                
            return resultados
            
        except Exception as e:
            logger.error(f"Error calculando percentiles: {str(e)}")
            return {}

    @staticmethod
    def test_normalidad(values: Union[List[float], np.ndarray]) -> Dict[str, float]:
        """
        Realiza múltiples pruebas de normalidad y devuelve los p-values.
        
        Incluye:
        - Shapiro-Wilk (mejor para muestras pequeñas)
        - Kolmogorov-Smirnov (adaptado a parámetros muestrales)
        - Anderson-Darling (sensibilidad en colas)
        
        Args:
            values: Array de valores numéricos
            
        Returns:
            dict: Resultados de las pruebas de normalidad
        """
        try:
            values = NonceStats._validar_entrada(values)
            n = len(values)
            
            with warnings.catch_warnings():
                warnings.simplefilter("ignore")
                
                # Shapiro-Wilk
                shapiro = stats.shapiro(values)
                
                # Kolmogorov-Smirnov adaptado
                ks = stats.kstest(values, 'norm', args=(np.mean(values), np.std(values)))
                
                # Anderson-Darling
                anderson = stats.anderson(values, dist='norm')
                anderson_stat = anderson.statistic
                anderson_criticos = anderson.critical_values[2]  # 5% significance level
                
            return {
                'shapiro_pvalue': round(shapiro.pvalue, 4),
                'ks_pvalue': round(ks.pvalue, 4),
                'anderson_stat': round(anderson_stat, 4),
                'anderson_critico': anderson_criticos,
                'es_normal': anderson_stat < anderson_criticos
            }
            
        except Exception as e:
            logger.error(f"Error en test de normalidad: {str(e)}")
            return {}

    @staticmethod
    def kurtosis_skewness(values: Union[List[float], np.ndarray]) -> Dict[str, float]:
        """
        Calcula medidas de forma de distribución con corrección de Fisher-Pearson.
        
        Returns:
            dict: Curtosis, sesgo y sus errores estándar
            
        Ejemplo:
        >>> NonceStats.kurtosis_skewness([1, 2, 3, 4, 5])
        """
        try:
            values = NonceStats._validar_entrada(values)
            n = len(values)
            
            # Cálculos con corrección de sesgo
            skewness = stats.skew(values, bias=False)
            kurtosis = stats.kurtosis(values, bias=False)
            
            # Errores estándar
            se_skewness = np.sqrt((6 * n * (n - 1)) / ((n - 2) * (n + 1) * (n + 3)))
            se_kurtosis = np.sqrt((24 * n * (n - 1)**2) / ((n - 3) * (n - 2) * (n + 3) * (n + 5)))
            
            return {
                'curtosis': round(kurtosis, 4),
                'sesgo': round(skewness, 4),
                'se_curtosis': round(se_kurtosis, 4),
                'se_sesgo': round(se_skewness, 4)
            }
            
        except Exception as e:
            logger.error(f"Error calculando forma de distribución: {str(e)}")
            return {}

    @staticmethod
    def analisis_completo(values: Union[List[float], np.ndarray]) -> Dict:
        """
        Genera un reporte completo de análisis estadístico.
        
        Args:
            values: Array de valores numéricos
            
        Returns:
            dict: Diccionario consolidado con todas las métricas
        """
        return {
            'dispersion': {
                'cv': NonceStats.coeficiente_variacion(values),
                'cv_robusto': NonceStats.coeficiente_variacion(values, robusto=True)
            },
            'distribucion': NonceStats.percentiles(values),
            'normalidad': NonceStats.test_normalidad(values),
            'forma': NonceStats.kurtosis_skewness(values),
            'resumen': {
                'n': len(values),
                'media': np.mean(values),
                'mediana': np.median(values),
                'moda': stats.mode(values)[0][0]
            }
        }

# Ejemplo de uso
if __name__ == "__main__":
    datos = np.random.normal(loc=100, scale=15, size=10_000)
    
    analisis = NonceStats.analisis_completo(datos)
    
    print("Análisis Estadístico Completo de Nonces:")
    print(f"- Coeficiente de Variación: {analisis['dispersion']['cv']}%")
    print(f"- Mediana (P50): {analisis['distribucion']['P50']['valor']}")
    print(f"- Normalidad (Shapiro-Wilk p): {analisis['normalidad']['shapiro_pvalue']}")
    print(f"- Curtosis: {analisis['forma']['curtosis']} ± {analisis['forma']['se_curtosis']}")