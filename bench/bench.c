#define main game_main
#include "../main.c"
#undef main

#include <stdio.h>
#include <string.h>

#define MAX_REPS 15

typedef struct {
    int game, move, player, level, stones, x, y;
    double t[MAX_REPS];
} MoveRec;

static MoveRec* recs;
static long recCount, recCap;

// Собственный ГСЧ для дебюта, чтобы не трогать rand(), которым пользуются боты
static unsigned rng_state;

static unsigned rng(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static int prev[SIZE][SIZE];

// Боты всегда играют за O (-1). Чтобы бот сходил за X, меняем цвета фишек
static void swap_colors(void) {
    for (int i = 0; i < SIZE; i++)
        for (int j = 0; j < SIZE; j++)
            board[i][j] = -board[i][j];
}

// Случайный дебют: несколько ходов рядом с центром, чтобы партии различались
static void random_opening(int moves) {
    int c = SIZE / 2;
    while (moves > 0 && !gameOver) {
        int x = c - 3 + (int)(rng() % 7);
        int y = c - 3 + (int)(rng() % 7);
        if (board[x][y] != 0) continue;
        place_piece(x, y);
        moves--;
    }
}

static int cmp_double(const void* a, const void* b) {
    double x = *(const double*)a, y = *(const double*)b;
    return (x > y) - (x < y);
}

// Играет все партии. На первом повторе запоминает ходы, на остальных сверяет с ними
static int play_all(int rep, int games, int levelX, int levelO, unsigned seed, int result[3]) {
    double tick_us = 1e6 / (double)glfwGetTimerFrequency();
    long k = 0;

    for (int g = 0; g < games; g++) {
        rng_state = seed * 2654435761u + (unsigned)g * 40503u + 1u;
        srand(seed * 1000u + (unsigned)g);

        init_board();
        int stones = 4;
        random_opening(stones);

        for (int move = 0; !gameOver; move++) {
            int player = turn;
            botLevel = player == 1 ? levelX : levelO;
            memcpy(prev, board, sizeof(board));

            if (player == 1) swap_colors();
            unsigned long long t0 = glfwGetTimerValue();
            bot_mode();
            unsigned long long t1 = glfwGetTimerValue();
            if (player == 1) {
                swap_colors();
                winner = -winner;
            }
            turn = -player;

            // Ищем клетку, куда сходил бот (вне замера)
            int mx = -1, my = -1;
            for (int i = 0; i < SIZE && mx < 0; i++)
                for (int j = 0; j < SIZE; j++)
                    if (board[i][j] != prev[i][j]) { mx = i; my = j; break; }
            if (mx < 0) break;
            stones++;

            if (rep == 0) {
                if (recCount == recCap) {
                    recCap = recCap ? recCap * 2 : 4096;
                    recs = realloc(recs, recCap * sizeof(MoveRec));
                }
                recs[recCount++] = (MoveRec){g, move, player, botLevel, stones, mx, my, {0}};
            } else if (k >= recCount || recs[k].x != mx || recs[k].y != my) {
                fprintf(stderr, "Повтор %d: партия %d разошлась на ходе %d\n", rep, g, move);
                return 0;
            }
            recs[k++].t[rep] = (double)(t1 - t0) * tick_us;
        }

        if (rep == 0) {
            if (winner == 1) result[0]++;
            else if (winner == -1) result[1]++;
            else result[2]++;
        }
    }
    return k == recCount;
}

int main(int argc, char** argv) {
    int games = argc > 1 ? atoi(argv[1]) : 20;
    int levelX = argc > 2 ? atoi(argv[2]) : 3;
    int levelO = argc > 3 ? atoi(argv[3]) : 3;
    unsigned seed = argc > 4 ? (unsigned)atoi(argv[4]) : 1;
    int reps = argc > 5 ? atoi(argv[5]) : 5;
    if (reps < 1) reps = 1;
    if (reps > MAX_REPS) reps = MAX_REPS;

    // Таймер GLFW (QueryPerformanceCounter внутри), окно не создаётся
    if (!glfwInit()) return 1;

    // Ходы делает бенчмарк, place_piece не должен сам вызывать бота
    gameMode = 1;

    int result[3] = {0, 0, 0};   // победы X, победы O, ничьи
    for (int rep = 0; rep < reps; rep++)
        if (!play_all(rep, games, levelX, levelO, seed, result)) return 2;

    unsigned long long checksum = 1469598103934665603ULL;
    double total_us = 0;

    printf("size,game,move,player,level,stones,x,y,time_us\n");
    for (long k = 0; k < recCount; k++) {
        MoveRec* r = &recs[k];
        qsort(r->t, reps, sizeof(double), cmp_double);
        double med = r->t[reps / 2];
        total_us += med;
        checksum = (checksum ^ (unsigned long long)(r->x * SIZE + r->y + 1)) * 1099511628211ULL;
        printf("%d,%d,%d,%d,%d,%d,%d,%d,%.3f\n",
               SIZE, r->game, r->move, r->player, r->level, r->stones, r->x, r->y, med);
    }

    fprintf(stderr,
            "SIZE=%d games=%d X=%d O=%d seed=%u reps=%d | moves=%ld total=%.1f ms avg=%.2f us/move"
            " | X wins=%d O wins=%d draws=%d | checksum=%016llx\n",
            SIZE, games, levelX, levelO, seed, reps, recCount, total_us / 1000.0,
            recCount ? total_us / recCount : 0.0, result[0], result[1], result[2], checksum);

    glfwTerminate();
    return 0;
}
