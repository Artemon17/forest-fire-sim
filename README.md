# Обнаружение и тушение лесного пожара

Консольное приложение на C, моделирующее обнаружение и тушение
лесного пожара на клеточной карте.

## Сборка

    make

## Запуск

    ./fire --conditions data/conditions.conf --map data/map_forest_01.map --station 4 6
    ./fire --conditions data/conditions.conf --map data/map_forest_01.map --station-scan

## Структура

- `src/`     — исходный код
- `include/` — заголовочные файлы
- `data/`    — конфигурация и карты
- `tests/`   — тестовые скрипты
- `logs/`    — результаты прогонов

## Требования

- Linux, gcc, make
