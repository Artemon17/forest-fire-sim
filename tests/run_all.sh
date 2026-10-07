#!/bin/bash
# Прогон всех карт со всеми конфигами.
# Использование: ./tests/run_all.sh

set -u

BIN=./build/fire
MAPS=(data/map_forest_01.map data/map_river_01.map data/map_big_01.map \
      data/map_dense_forest.map data/map_open_field.map \
      data/map_lake.map data/map_corridor.map)
CONFIGS=(data/conditions.conf data/conditions_strong_wind.conf \
         data/conditions_dry.conf data/conditions_rainy.conf \
         data/conditions_strong_teams.conf)

for map in "${MAPS[@]}"; do
    for cfg in "${CONFIGS[@]}"; do
        printf "%-32s  %-32s  " "$(basename "$map")" "$(basename "$cfg")"
        $BIN --conditions "$cfg" --map "$map" --station-scan --seed 42 \
             --quiet 2>/dev/null | grep "Лучшая станция" || echo "—"
    done
done
