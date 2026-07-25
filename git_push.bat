@echo off
chcp 65001 >nul
title Git Push Script

echo ========================================
echo          Git 代码提交脚本
echo ========================================
echo.

git status
echo.

set /p commit_msg=请输入提交信息: 
echo.   

echo [1/4] 添加修改的文件...
git add include/ src/
if %errorlevel% neq 0 (
    echo 添加文件失败!
    pause
    exit /b 1
)

echo [2/4] 提交代码...
git commit -m "%commit_msg%"
if %errorlevel% neq 0 (
    echo 提交失败!
    pause
    exit /b 1
)

echo [3/4] 拉取远程代码...
git pull --rebase
if %errorlevel% neq 0 (
    echo 拉取失败，尝试直接推送...
)

echo [4/4] 推送到远程仓库...
git push
if %errorlevel% neq 0 (
    echo 推送失败!
    pause
    exit /b 1
)

echo.
echo ========================================
echo           提交成功!
echo ========================================
echo.
pause