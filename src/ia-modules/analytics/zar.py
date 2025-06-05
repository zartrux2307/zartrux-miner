import matplotlib.pyplot as plt
import seaborn as sns
import pandas as pd
from sklearn.model_selection import train_test_split
from sklearn.ensemble import RandomForestRegressor
from sklearn.linear_model import Ridge, Lasso
from sklearn.preprocessing import PolynomialFeatures
from sklearn.metrics import mean_squared_error, r2_score, mean_absolute_error
from utils.nonce_loader import NonceLoader
from utils.data_preprocessor import DataPreprocessor
from utils.config_manager import ConfigManager
from correlation_analysis import CorrelationAnalyzer
from survival_analyzer import SurvivalAnalyzer

# Configuración inicial
config = ConfigManager(config_path="config/miner_config.json")
loader = NonceLoader(config)
preprocessor = DataPreprocessor()

# Cargar datos de entrenamiento
training_data = loader.load_training_data()

# Preprocesar datos
processed_data = preprocessor.process(training_data)

# Variables independientes (features)
X = processed_data[['difficulty', 'hash_score', 'block_timestamp']]

# Variable dependiente (target)
y = processed_data['nonce']

# Dividir datos en conjuntos de entrenamiento y prueba
X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

# Inicializar y entrenar modelos de regresión
rf_regressor = RandomForestRegressor(n_estimators=100, random_state=42)
ridge_regressor = Ridge(alpha=1.0)
lasso_regressor = Lasso(alpha=0.1)

rf_regressor.fit(X_train, y_train)
ridge_regressor.fit(X_train, y_train)
lasso_regressor.fit(X_train, y_train)

# Hacer predicciones
y_pred_rf = rf_regressor.predict(X_test)
y_pred_ridge = ridge_regressor.predict(X_test)
y_pred_lasso = lasso_regressor.predict(X_test)

# Evaluar modelos
mse_rf = mean_squared_error(y_test, y_pred_rf)
r2_rf = r2_score(y_test, y_pred_rf)
mae_rf = mean_absolute_error(y_test, y_pred_rf)

mse_ridge = mean_squared_error(y_test, y_pred_ridge)
r2_ridge = r2_score(y_test, y_pred_ridge)
mae_ridge = mean_absolute_error(y_test, y_pred_ridge)

mse_lasso = mean_squared_error(y_test, y_pred_lasso)
r2_lasso = r2_score(y_test, y_pred_lasso)
mae_lasso = mean_absolute_error(y_test, y_pred_lasso)

# Crear un DataFrame con las métricas de evaluación
metrics = pd.DataFrame({
    'Modelo': ['Random Forest', 'Ridge', 'Lasso'],
    'Mean Squared Error (MSE)': [mse_rf, mse_ridge, mse_lasso],
    'R-squared (R2)': [r2_rf, r2_ridge, r2_lasso],
    'Mean Absolute Error (MAE)': [mae_rf, mae_ridge, mae_lasso]
})

print(metrics)

# Visualizar las predicciones vs los valores reales para todos los modelos
plt.figure(figsize=(14, 7))
plt.scatter(y_test, y_pred_rf, alpha=0.5, label='Random Forest')
plt.scatter(y_test, y_pred_ridge, alpha=0.5, label='Ridge')
plt.scatter(y_test, y_pred_lasso, alpha=0.5, label='Lasso')
plt.xlabel("Valores Reales de Nonces")
plt.ylabel("Predicciones de Nonces")
plt.title("Predicciones vs Valores Reales de Nonces")
plt.plot([y_test.min(), y_test.max()], [y_test.min(), y_test.max()], 'k--', lw=2, label='Línea de Igualdad')
plt.xlim([y_test.min(), y_test.max()])
plt.ylim([min(y_pred_rf.min(), y_pred_ridge.min(), y_pred_lasso.min()), max(y_pred_rf.max(), y_pred_ridge.max(), y_pred_lasso.max())])
plt.grid(True)
plt.legend()
plt.show()

# Visualizar residuos para Random Forest
plt.figure(figsize=(14, 7))
residuals_rf = y_test - y_pred_rf
plt.scatter(y_test, residuals_rf, alpha=0.5)
plt.axhline(y=0, color='r', linestyle='--')
plt.xlabel("Valores Reales de Nonces")
plt.ylabel("Residuos")
plt.title("Residuos vs Valores Reales de Nonces - Random Forest")
plt.grid(True)
plt.show()

# Visualizar residuos para Ridge
plt.figure(figsize=(14, 7))
residuals_ridge = y_test - y_pred_ridge
plt.scatter(y_test, residuals_ridge, alpha=0.5)
plt.axhline(y=0, color='r', linestyle='--')
plt.xlabel("Valores Reales de Nonces")
plt.ylabel("Residuos")
plt.title("Residuos vs Valores Reales de Nonces - Ridge")
plt.grid(True)
plt.show()

# Visualizar residuos para Lasso
plt.figure(figsize=(14, 7))
residuals_lasso = y_test - y_pred_lasso
plt.scatter(y_test, residuals_lasso, alpha=0.5)
plt.axhline(y=0, color='r', linestyle='--')
plt.xlabel("Valores Reales de Nonces")
plt.ylabel("Residuos")
plt.title("Residuos vs Valores Reales de Nonces - Lasso")
plt.grid(True)
plt.show()

# Importancia de características para Random Forest
feature_importances_rf = pd.Series(rf_regressor.feature_importances_, index=X.columns)
feature_importances_rf.sort_values(ascending=False, inplace=True)

# Visualizar la importancia de características para Random Forest
plt.figure(figsize=(10, 6))
sns.barplot(x=feature_importances_rf.values, y=feature_importances_rf.index)
for i, v in enumerate(feature_importances_rf.values):
    plt.text(v + 0.01, i, f"{v:.4f}", va='center')
plt.title("Importancia de Características - Random Forest")
plt.xlabel("Importancia")
plt.ylabel("Características")
plt.show()

# Análisis de Correlaciones
correlation_analyzer = CorrelationAnalyzer(config)
correlation_report = correlation_analyzer.analyze_and_report()
print("Reporte de Correlaciones:", correlation_report)

# Análisis de Supervivencia
survival_analyzer = SurvivalAnalyzer()
survival_results = survival_analyzer.analyze_survival(processed_data, duration_col='block_timestamp', event_col='accepted', covariates=['difficulty', 'hash_score'])
print("Resultados de Supervivencia:", survival_results)