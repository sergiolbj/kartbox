@echo off
title Instalador de Dependencias - KartBox AI
echo ======================================================
echo   INSTALADOR DE DEPENDENCIAS - KARTBOX TRACK INSIGHTS
echo ======================================================
echo.
echo Verificando instalacao do Python...
python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERRO] Python nao encontrado! Por favor, instale o Python em python.org
    pause
    exit
)

echo Atualizando o PIP...
python -m pip install --upgrade pip

echo.
echo Instalando bibliotecas (Pandas, Matplotlib, Numpy, FPDF2, SciPy)...
pip install pandas matplotlib numpy fpdf2 scipy

echo.
echo ======================================================
echo   INSTALACAO CONCLUIDA COM SUCESSO!
echo   Voce ja pode rodar o seu script analise_log.py
echo ======================================================
echo.
pause