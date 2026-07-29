@echo off
cd /d "%~dp0WebApp"

if not exist ".env" (
    echo Creating .env from .env.example...
    copy .env.example .env
)

if not exist "node_modules" (
    echo Installing dependencies...
    call npm install
)

echo Starting WebApp development server...
call npm run dev
