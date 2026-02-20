#!/usr/bin/env bash
# Скрипт для анализа использования памяти по истории коммитов
# Для каждого коммита выполняет сборку и извлекает статистику RAM/Flash

set -euo pipefail

# Путь к PlatformIO
PIO_BIN="${PIO_BIN:-$HOME/.platformio/penv/bin/pio}"

# Проверка наличия PlatformIO
if ! command -v "$PIO_BIN" &> /dev/null; then
    echo "Ошибка: PlatformIO не найден по пути $PIO_BIN" >&2
    exit 1
fi

# Параметры
COMMIT_COUNT=10          # Количество анализируемых коммитов (0 = все)
BRANCH="main"            # Ветка для анализа
OUTPUT_CSV="commit-stats.csv"
SKIP_BUILT=false         # Пропускать коммиты, уже присутствующие в CSV
CLEAN_EACH=true          # Выполнять pio clean перед каждой сборкой

# Парсинг аргументов командной строки
while [[ $# -gt 0 ]]; do
    case $1 in
        -n|--count)
            COMMIT_COUNT="$2"
            shift 2
            ;;
        -b|--branch)
            BRANCH="$2"
            shift 2
            ;;
        -o|--output)
            OUTPUT_CSV="$2"
            shift 2
            ;;
        --skip-built)
            SKIP_BUILT=true
            shift
            ;;
        --no-clean)
            CLEAN_EACH=false
            shift
            ;;
        *)
            echo "Неизвестный аргумент: $1" >&2
            echo "Использование: $0 [-n COUNT] [-b BRANCH] [-o OUTPUT.csv] [--skip-built] [--no-clean]" >&2
            exit 1
            ;;
    esac
done

echo "=== Анализ использования памяти по коммитам ==="
echo "Ветка: $BRANCH"
echo "Количество коммитов: ${COMMIT_COUNT:-все}"
echo "Выходной файл: $OUTPUT_CSV"
echo ""

# Сохраняем текущую ветку и состояние
CURRENT_BRANCH=$(git branch --show-current)
CURRENT_COMMIT=$(git rev-parse HEAD)
echo "Текущая ветка: $CURRENT_BRANCH, коммит: $(git rev-parse --short HEAD)"

# Создаем заголовок CSV, если файл не существует
if [ ! -f "$OUTPUT_CSV" ]; then
    echo "date,commit_hash,commit_subject,branch,ram_used,ram_total,ram_percent,flash_used,flash_total,flash_percent,build_status,elapsed_sec" > "$OUTPUT_CSV"
fi

# Получаем список коммитов для анализа
echo "Получение списка коммитов..."
if [ "$COMMIT_COUNT" -eq 0 ]; then
    COMMITS=$(git log --oneline --reverse "$BRANCH" | awk '{print $1}')
else
    COMMITS=$(git log --oneline --reverse -n "$COMMIT_COUNT" "$BRANCH" | awk '{print $1}')
fi

COMMIT_ARRAY=($COMMITS)
TOTAL_COMMITS=${#COMMIT_ARRAY[@]}
echo "Найдено коммитов: $TOTAL_COMMITS"
echo ""

# Функция для извлечения статистики из вывода сборки
extract_stats() {
    local output="$1"
    local ram_line=$(echo "$output" | grep -E '^RAM:\s*\[')
    local flash_line=$(echo "$output" | grep -E '^Flash:\s*\[')
    
    if [ -z "$ram_line" ] || [ -z "$flash_line" ]; then
        # Альтернативный поиск
        ram_line=$(echo "$output" | grep -E 'RAM.*%')
        flash_line=$(echo "$output" | grep -E 'Flash.*%')
    fi
    
    local ram_used=""
    local ram_total=""
    local ram_percent=""
    local flash_used=""
    local flash_total=""
    local flash_percent=""
    
    if [ -n "$ram_line" ]; then
        ram_used=$(echo "$ram_line" | sed -E 's/.*used[[:space:]]+([0-9]+)[[:space:]]+bytes.*/\1/')
        ram_total=$(echo "$ram_line" | sed -E 's/.*from[[:space:]]+([0-9]+)[[:space:]]+bytes.*/\1/')
        ram_percent=$(echo "$ram_line" | sed -E 's/.*\[.*\][[:space:]]+([0-9.]+)%.*/\1/')
    fi
    
    if [ -n "$flash_line" ]; then
        flash_used=$(echo "$flash_line" | sed -E 's/.*used[[:space:]]+([0-9]+)[[:space:]]+bytes.*/\1/')
        flash_total=$(echo "$flash_line" | sed -E 's/.*from[[:space:]]+([0-9]+)[[:space:]]+bytes.*/\1/')
        flash_percent=$(echo "$flash_line" | sed -E 's/.*\[.*\][[:space:]]+([0-9.]+)%.*/\1/')
    fi
    
    echo "$ram_used,$ram_total,$ram_percent,$flash_used,$flash_total,$flash_percent"
}

# Проходим по каждому коммиту
INDEX=0
for COMMIT_HASH in "${COMMIT_ARRAY[@]}"; do
    INDEX=$((INDEX + 1))
    COMMIT_SUBJECT=$(git show -s --format="%s" "$COMMIT_HASH")
    COMMIT_DATE=$(git show -s --format="%ci" "$COMMIT_HASH" | cut -d' ' -f1)
    
    echo "--- Коммит $INDEX/$TOTAL_COMMITS: ${COMMIT_HASH:0:8} ---"
    echo "Тема: $COMMIT_SUBJECT"
    echo "Дата: $COMMIT_DATE"
    
    # Пропускаем, если уже есть в CSV и включен skip
    if [ "$SKIP_BUILT" = true ] && grep -q "^[^,]*,$COMMIT_HASH," "$OUTPUT_CSV" 2>/dev/null; then
        echo "Пропуск (уже в CSV)"
        echo ""
        continue
    fi
    
    # Переключаемся на коммит
    echo "Переключение на коммит $COMMIT_HASH..."
    git checkout --quiet "$COMMIT_HASH"
    
    # Проверяем, есть ли platformio.ini (может не быть в старых коммитах)
    if [ ! -f "platformio.ini" ]; then
        echo "Пропуск: platformio.ini отсутствует"
        echo "$COMMIT_DATE,$COMMIT_HASH,\"$COMMIT_SUBJECT\",$BRANCH,,,,,,,missing_platformio,0" >> "$OUTPUT_CSV"
        echo ""
        continue
    fi
    
    # Замер времени
    START_TIME=$(date +%s)
    
    # Очистка ДО сборки (игнорируем ошибки)
    if [ "$CLEAN_EACH" = true ]; then
        echo "Очистка проекта (до сборки)..."
        "$PIO_BIN" clean >/dev/null 2>&1 || echo "Предупреждение: очистка завершилась с ошибкой (игнорируем)"
    fi
    
    # Сборка
    echo "Сборка проекта..."
    BUILD_OUTPUT=$("$PIO_BIN" run 2>&1)
    BUILD_EXIT=$?
    END_TIME=$(date +%s)
    ELAPSED=$((END_TIME - START_TIME))
    
    # Очистка ПОСЛЕ сборки (игнорируем ошибки)
    if [ "$CLEAN_EACH" = true ]; then
        echo "Очистка проекта (после сборки)..."
        "$PIO_BIN" clean >/dev/null 2>&1 || echo "Предупреждение: очистка после сборки завершилась с ошибкой (игнорируем)"
    fi
    
    if [ $BUILD_EXIT -ne 0 ]; then
        echo "Сборка не удалась ($ELAPSED сек)"
        echo "$COMMIT_DATE,$COMMIT_HASH,\"$COMMIT_SUBJECT\",$BRANCH,,,,,,,build_failed,$ELAPSED" >> "$OUTPUT_CSV"
        # Выводим последние строки ошибки
        echo "$BUILD_OUTPUT" | tail -5
    else
        echo "Сборка успешна ($ELAPSED сек)"
        STATS=$(extract_stats "$BUILD_OUTPUT")
        if [ -n "$STATS" ]; then
            IFS=',' read -r ram_used ram_total ram_percent flash_used flash_total flash_percent <<< "$STATS"
            if [[ "$ram_used" =~ ^[0-9]+$ ]]; then
                echo "RAM: $ram_used/$ram_total байт ($ram_percent%)"
                echo "Flash: $flash_used/$flash_total байт ($flash_percent%)"
            fi
            echo "$COMMIT_DATE,$COMMIT_HASH,\"$COMMIT_SUBJECT\",$BRANCH,$ram_used,$ram_total,$ram_percent,$flash_used,$flash_total,$flash_percent,success,$ELAPSED" >> "$OUTPUT_CSV"
        else
            echo "Не удалось извлечь статистику"
            echo "$COMMIT_DATE,$COMMIT_HASH,\"$COMMIT_SUBJECT\",$BRANCH,,,,,,,no_stats,$ELAPSED" >> "$OUTPUT_CSV"
        fi
    fi
    
    echo ""
done

# Возвращаемся в исходное состояние
echo "Возврат к исходному коммиту $CURRENT_COMMIT..."
git checkout --quiet "$CURRENT_BRANCH" 2>/dev/null || git checkout --quiet "$CURRENT_COMMIT"

echo "=== Анализ завершен ==="
echo "Результаты сохранены в $OUTPUT_CSV"
echo ""

# Генерируем простой отчет
if [ -f "$OUTPUT_CSV" ]; then
    echo "Сводка по успешным сборкам:"
    awk -F, 'NR>1 && $11=="success" {print $2" "$5"/"$6" RAM "$7"% "$8"/"$9" Flash "$10"%"}' "$OUTPUT_CSV" | tail -5
fi