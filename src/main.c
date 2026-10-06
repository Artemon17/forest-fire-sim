#include "term.h"
#include <stdio.h>

int main(void) {
    term_install_signals();

    int rows, cols;
    if (term_size(&rows, &cols) == -1) {
        fprintf(stderr, "Не терминал. Размер неизвестен.\n");
        return 1;
    }
    printf("Терминал: %d x %d\n", rows, cols);

    if (term_raw_mode() == -1) {
        fprintf(stderr, "Не удалось перейти в raw-режим\n");
        return 1;
    }

    term_hide_cursor();
    term_clear();

    for (int i = 1; i <= 5 && !term_should_stop(); i++) {
        term_goto(1, 1);
        printf("Такт %d/5   (q — выход)\n", i);
        printf("Нажми q чтобы прервать, p — пауза\n");
        fflush(stdout);

        /* ждём 1 сек, но реагируем на клавиши */
        for (int t = 0; t < 10 && !term_should_stop(); t++) {
            int k = term_poll_key();
            if (k == 'q') { term_request_stop(); break; }
            if (k == 'p') {
                term_goto(3, 1);
                printf("Пауза. Нажми любую клавишу...");
                fflush(stdout);
                while (term_poll_key() == -1 && !term_should_stop())
                    term_sleep_ms(50);
            }
            term_sleep_ms(100);
        }
    }

    term_show_cursor();
    term_reset_color();
    term_clear();
    term_restore_mode();

    if (term_should_stop()) {
        printf("Прервано пользователем.\n");
        return 130;
    }
    printf("Готово.\n");
    return 0;
}