import pytest
import time
import numpy as np
from typing import List, Dict, Any
from unittest.mock import MagicMock, patch
from hypothesis import given, strategies as st, settings, HealthCheck

from ia_modules.bridge.predict_nonce_inference import PredictNonceInference
from evaluation.nonce_quality_filter import NonceQualityFilter

# Fixtures y configuración
@pytest.fixture(scope="module")
def inference_engine():
    """Fixture que provee una instancia inicializada del motor de inferencia"""
    engine = PredictNonceInference()
    yield engine
    # Limpieza después de todas las pruebas
    engine.model = None

@pytest.fixture
def sample_nonces() -> Dict[str, List[str]]:
    """Conjunto de prueba con nonces válidos e inválidos"""
    return {
        "valid": [
            "1a3f8b2e5c7d90f4",  # Formato hexadecimal válido
            "cafebabe12345678",    # Nonce con alta entropía
            "deadbeef87654321"     # Nonce común pero bien formado
        ],
        "invalid": [
            "ghijklmnopqrstuv",    # Caracteres no hexadecimales
            "",                     # Cadena vacía
            "x"*64                 # Longitud excesiva
        ]
    }

# Pruebas principales
class TestNonceInference:
    """Suite de pruebas para el sistema de inferencia de nonces"""
    
    @pytest.mark.parametrize("nonce,expected", [
        ("1a3f8b2e5c7d90f4", (1, 12)),  # 12 características esperadas
        ("cafebabe", (1, 12)),
        ("1234", (1, 12))
    ])
    def test_feature_engineering(self, inference_engine, nonce, expected):
        """Verifica el correcto preprocesamiento de nonces"""
        processed = inference_engine.preprocess_nonce(nonce)
        assert processed.shape == expected, \
            f"Formato de características incorrecto: {processed.shape}"
            
    @given(st.text(min_size=8, max_size=64, alphabet=st.characters(whitelist_categories=('H', 'Nd'))))
    @settings(suppress_health_check=[HealthCheck.too_slow])
    def test_probabiliad_rango_valido(self, inference_engine, nonce):
        """Prueba basada en propiedades con Hypothesis"""
        probability = inference_engine.predict_success(nonce)
        assert 0 <= probability <= 1, \
            f"Probabilidad fuera de rango: {probability}"

    def test_seleccion_nonces_optimos(self, inference_engine, sample_nonces):
        """Verifica el filtrado de nonces con umbral personalizado"""
        all_nonces = sample_nonces["valid"] + sample_nonces["invalid"]
        filtered = inference_engine.select_best_nonces(all_nonces, threshold=0.75)
        
        assert len(filtered) > 0, "No se seleccionaron nonces válidos"
        assert all(n in sample_nonces["valid"] for n in filtered), \
            "Nonces inválidos en resultados filtrados"

    @patch('ia_modules.bridge.predict_nonce_inference.PredictNonceInference.load_model')
    def test_comportamiento_sin_modelo(self, mock_load, inference_engine):
        """Prueba de manejo de errores cuando falta el modelo"""
        mock_load.side_effect = FileNotFoundError
        with pytest.raises(SystemExit):
            PredictNonceInference()

    @pytest.mark.performance
    def test_rendimiento_inferencia(self, inference_engine, benchmark):
        """Prueba de rendimiento con pytest-benchmark"""
        nonces = [f"nonce{i:04x}"*4 for i in range(1000)]  # 1000 nonces de prueba
        
        # Benchmark de la función completa
        result = benchmark(inference_engine.select_best_nonces, nonces)
        
        assert len(result) > 0, "Error en procesamiento por lotes"
        assert benchmark.stats['mean'] < 0.5, "El rendimiento no cumple los requisitos"

    @pytest.mark.stress
    def test_carga_alta(self, inference_engine):
        """Prueba de estrés con gran volumen de datos"""
        start_time = time.time()
        nonces = [f"stress_test_{i:08x}" for i in range(10_000)]
        
        results = [inference_engine.predict_success(nonce) for nonce in nonces]
        
        duration = time.time() - start_time
        assert duration < 2.0, f"Tiempo de inferencia excedido: {duration:.2f}s"
        assert len(results) == 10_000, "No se procesaron todos los nonces"

# Pruebas de integración
class TestIntegrationPipeline:
    """Pruebas de integración con el sistema de filtrado"""
    
    def test_pipeline_completo(self, inference_engine, sample_nonces):
        """Prueba completa del flujo de procesamiento"""
        raw_nonces = sample_nonces["valid"] + sample_nonces["invalid"]
        
        # Paso 1: Filtrado inicial
        filtered = NonceQualityFilter.evaluar_nonces(raw_nonces)
        assert len(filtered) == len(sample_nonces["valid"]), \
            "Filtrado inicial falló"
            
        # Paso 2: Inferencia IA
        scored = inference_engine.select_best_nonces(filtered)
        assert len(scored) > 0, "No se seleccionaron nonces óptimos"
        
        # Paso 3: Validación final
        assert all(n in sample_nonces["valid"] for n in scored), \
            "Nonces inválidos en resultado final"

if __name__ == "__main__":
    pytest.main(["-v", "--benchmark-skip", "-m", "not performance and not stress"])