#include <GLFW/glfw3.h>
#include <stb_easy_font.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

// Размер игрового поля (50x50) и условие победы (5 в ряд)
// SIZE можно переопределить при сборке (-DSIZE=...), это нужно бенчмарку
#ifndef SIZE
#define SIZE 50
#endif
#define WIN 5
// Размер одной клетки в пикселях
#define CELL_SIZE 20
#define WIDTH (SIZE * CELL_SIZE)
#define HEIGHT (SIZE * CELL_SIZE)

// board[x][y]: 0 - пусто, 1 - X (человек), -1 - O (бот/второй игрок)
int board[SIZE][SIZE];

// Структуры, которые обновляются при каждом ходе, чтобы бот не сканировал всё поле.
// Индекс клетки idx = x * SIZE + y - это порядок обхода исходных циклов for i, for j.
int stoneCount = 0;                  // число фишек на поле
int near1[SIZE][SIZE];               // число фишек в квадрате 3x3 с центром в клетке
int near2[SIZE][SIZE];               // число фишек в квадрате 5x5 с центром в клетке
int cand[SIZE * SIZE];               // пустые клетки с near2 > 0, по возрастанию индекса
int candCount = 0;
int stoneList[SIZE * SIZE];          // занятые клетки, по возрастанию индекса
int turn = 1;
int gameMode = 1;
int botLevel = 2;
int gameOver = 0;
int winner = 0;
int menu = 1;
int menuSelected = 1;
int botSelected = 1;
int mousePressed = 0;

typedef struct {
    int x, y, w, h;
} Button;

Button btnPvP = {0, 200, 250, 60};
Button btnPvE = {0, 300, 250, 60};
Button btnEasy = {0, 430, 220, 50};
Button btnMedium = {0, 510, 220, 50};
Button btnHard = {0, 590, 220, 50};

// Рисует рамку кнопки
void draw_rect_outline(int x, int y, int w, int h, float r, float g, float b, float lineWidth) {
    glColor3f(r, g, b);
    glLineWidth(lineWidth);
    glBegin(GL_LINE_LOOP);
    glVertex2i(x, y);
    glVertex2i(x + w, y);
    glVertex2i(x + w, y + h);
    glVertex2i(x, y + h);
    glEnd();
}

// Выводит текст
void print_text(float x, float y, char* text, float r, float g, float b) {
    static char buffer[99999];
    int quads = stb_easy_font_print(x, y, text, NULL, buffer, sizeof(buffer));

    glColor3f(r, g, b);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 16, buffer);
    glDrawArrays(GL_QUADS, 0, quads * 4);
    glDisableClientState(GL_VERTEX_ARRAY);
}

// Рисует закрашенный прямоугольник
void draw_rect(int x, int y, int w, int h, float r, float g, float b) {
    glColor3f(r, g, b);
    glBegin(GL_QUADS);
    glVertex2i(x, y);
    glVertex2i(x + w, y);
    glVertex2i(x + w, y + h);
    glVertex2i(x, y + h);
    glEnd();
}

// Рисует закрашенный круг (нолик)
void draw_circle(float cx, float cy, float radius, float r, float g, float b) {
    glColor3f(r, g, b);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= 60; i++) {
        float a = (float)i / 60.0f * 2.0f * 3.14159f;
        glVertex2f(cx + cos(a) * radius, cy + sin(a) * radius);
    }
    glEnd();
}

// Обнуляет поле и сбрасывает состояние партии
void init_board() {
    // Полная очистка поля и сброс состояния партии
    for (int i = 0; i < SIZE; i++)
        for (int j = 0; j < SIZE; j++) {
            board[i][j] = 0;
            near1[i][j] = 0;
            near2[i][j] = 0;
        }
    stoneCount = 0;
    candCount = 0;

    turn = 1;
    gameOver = 0;
    winner = 0;
}

// Проверка выстроено ли 5 в ряд
int check_win(int x, int y, int player) {
    // Проверяем 4 направления: горизонталь, вертикаль, 2 диагонали
    int dirs[4][2] = {{1,0},{0,1},{1,1},{1,-1}};

    for (int d = 0; d < 4; d++) {
        int count = 1;

        for (int s = -1; s <= 1; s += 2) {
            int nx = x + dirs[d][0] * s;
            int ny = y + dirs[d][1] * s;

            while (nx >= 0 && nx < SIZE && ny >= 0 && ny < SIZE && board[nx][ny] == player) {
                count++;
                nx += dirs[d][0] * s;
                ny += dirs[d][1] * s;
            }
        }

        if (count >= WIN) return 1;
    }

    return 0;
}

// Проверка заполнено ли поле полностью (ничья)
int is_full() {
    return stoneCount == SIZE * SIZE;
}

// Позиция первого элемента >= value в отсортированном массиве (бинарный поиск)
int lower_bound(const int* a, int n, int value) {
    int lo = 0, hi = n;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (a[mid] < value) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

// Вставляет value в отсортированный массив
void sorted_insert(int* a, int* n, int value) {
    int pos = lower_bound(a, *n, value);
    memmove(a + pos + 1, a + pos, (*n - pos) * sizeof(int));
    a[pos] = value;
    (*n)++;
}

// Удаляет value из отсортированного массива, если он там есть
void sorted_remove(int* a, int* n, int value) {
    int pos = lower_bound(a, *n, value);
    if (pos == *n || a[pos] != value) return;
    memmove(a + pos, a + pos + 1, (*n - pos - 1) * sizeof(int));
    (*n)--;
}

// Ставит фишку и обновляет счётчики соседей и список кандидатов.
// Меняется только окрестность 5x5 вокруг хода, поэтому обновление O(1) по размеру поля
void set_stone(int x, int y, int player) {
    int idx = x * SIZE + y;
    board[x][y] = player;
    if (near2[x][y] > 0) sorted_remove(cand, &candCount, idx);
    int n = stoneCount;
    sorted_insert(stoneList, &n, idx);
    stoneCount = n;

    for (int dx = -2; dx <= 2; dx++) {
        for (int dy = -2; dy <= 2; dy++) {
            int nx = x + dx;
            int ny = y + dy;
            if (nx < 0 || nx >= SIZE || ny < 0 || ny >= SIZE) continue;
            if (abs(dx) <= 1 && abs(dy) <= 1) near1[nx][ny]++;
            near2[nx][ny]++;
            if (near2[nx][ny] == 1 && board[nx][ny] == 0)
                sorted_insert(cand, &candCount, nx * SIZE + ny);
        }
    }
}

// Лёгкий режим бота
void bot_easy() {
    int cnt = SIZE * SIZE - stoneCount;

    if (cnt > 0) {
        // r-я по счёту пустая клетка в порядке обхода поля:
        // проходим занятые клетки по возрастанию и сдвигаем позицию за каждую
        int r = rand() % cnt;
        int pos = r;
        for (int k = 0; k < stoneCount && stoneList[k] <= pos; k++)
            pos++;
        int x = pos / SIZE;
        int y = pos % SIZE;
        set_stone(x, y, -1);
        if (check_win(x, y, -1)) {
            winner = -1;
            gameOver = 1;
        }
    }
}

// Делает пробный ход, проверяет победу и откатывается обратно
int move_wins(int x, int y, int player) {
    // Пробный ход: ставим, проверяем победу, откатываем
    if (board[x][y] != 0) return 0;
    board[x][y] = player;
    int win = check_win(x, y, player);
    board[x][y] = 0;
    return win;
}

// Считает длину самой длинной цепочки после хода в (x, y)
int longest_line_after_move(int x, int y, int player) {
    // Оценка силы клетки: какая максимальная линия получится после хода
    int dirs[4][2] = {{1,0},{0,1},{1,1},{1,-1}};
    int best = 1;

    for (int d = 0; d < 4; d++) {
        int count = 1;
        for (int s = -1; s <= 1; s += 2) {
            int nx = x + dirs[d][0] * s;
            int ny = y + dirs[d][1] * s;
            while (nx >= 0 && nx < SIZE && ny >= 0 && ny < SIZE && board[nx][ny] == player) {
                count++;
                nx += dirs[d][0] * s;
                ny += dirs[d][1] * s;
            }
        }
        if (count > best) best = count;
    }

    return best;
}

// Ставит фишку в (x, y) и проверяет победу
void place_bot_at(int x, int y) {
    set_stone(x, y, -1);
    if (check_win(x, y, -1)) {
        winner = -1;
        gameOver = 1;
    }
}

// Ищет среди кандидатов первый в порядке обхода поля выигрышный ход бота
// и первый ход, блокирующий победу игрока (-1, если такого нет).
// Победный ход и блок всегда стоят рядом с фишкой, поэтому они есть среди кандидатов
void find_win_and_block(int* winIdx, int* blockIdx) {
    *winIdx = -1;
    *blockIdx = -1;
    for (int k = 0; k < candCount; k++) {
        int idx = cand[k];
        int i = idx / SIZE, j = idx % SIZE;
        if (move_wins(i, j, -1)) {
            *winIdx = idx;
            return;
        }
        if (*blockIdx == -1 && move_wins(i, j, 1))
            *blockIdx = idx;
    }
}

// Средний режим бота
void bot_medium() {
    int winIdx, blockIdx;
    find_win_and_block(&winIdx, &blockIdx);
    if (winIdx != -1) {
        place_bot_at(winIdx / SIZE, winIdx % SIZE);
        return;
    }
    if (blockIdx != -1) {
        place_bot_at(blockIdx / SIZE, blockIdx % SIZE);
        return;
    }

    // Кандидаты обходятся в том же порядке, что и клетки в исходном цикле,
    // поэтому rand() вызывается для тех же клеток и бот ходит так же
    int bestX = -1, bestY = -1, bestScore = -1000000;
    for (int k = 0; k < candCount; k++) {
        int i = cand[k] / SIZE, j = cand[k] % SIZE;
        if (near1[i][j] == 0) continue;

        board[i][j] = -1;
        int attack = longest_line_after_move(i, j, -1);
        board[i][j] = 0;

        board[i][j] = 1;
        int defend = longest_line_after_move(i, j, 1);
        board[i][j] = 0;

        int score = attack * 10 + defend * 7 + (rand() % 5);
        if (score > bestScore) {
            bestScore = score;
            bestX = i;
            bestY = j;
        }
    }

    if (bestX != -1) place_bot_at(bestX, bestY);
    else bot_easy();
}

// Оценка комбинации
int evaluate_pattern(int count, int openEnds) {
    if (count >= 5) return 1000000;

    if (count == 4 && openEnds == 2) return 100000;
    if (count == 4 && openEnds == 1) return 20000;

    if (count == 3 && openEnds == 2) return 10000;
    if (count == 3 && openEnds == 1) return 2000;

    if (count == 2 && openEnds == 2) return 500;
    if (count == 2 && openEnds == 1) return 100;

    return 10;
}

// Анализирует силу позиции после хода
int evaluate_position(int x, int y, int player) {
    int dirs[4][2] = {{1,0},{0,1},{1,1},{1,-1}};
    int totalScore = 0;

    for (int d = 0; d < 4; d++) {
        int count = 1;
        int openEnds = 0;

        for (int s = -1; s <= 1; s += 2) {
            int nx = x + dirs[d][0] * s;
            int ny = y + dirs[d][1] * s;

            while (nx >= 0 && nx < SIZE &&
                   ny >= 0 && ny < SIZE &&
                   board[nx][ny] == player) {
                count++;
                nx += dirs[d][0] * s;
                ny += dirs[d][1] * s;
                   }

            if (nx >= 0 && nx < SIZE &&
                ny >= 0 && ny < SIZE &&
                board[nx][ny] == 0) {
                openEnds++;
                }
        }

        totalScore += evaluate_pattern(count, openEnds);
    }

    return totalScore;
}

// Сложный режим бота
void bot_hard() {
    int bestX = -1;
    int bestY = -1;
    int bestScore = -1000000;

    int center = SIZE / 2;

    // Один проход по кандидатам (пустые клетки в радиусе 2 от фишек) вместо трёх по всему полю.
    // Приоритет как в исходной версии: победа, затем блок, затем лучшая оценка
    int winIdx = -1;
    int blockIdx = -1;

    for (int k = 0; k < candCount; k++) {
        int i = cand[k] / SIZE, j = cand[k] % SIZE;

        // Проверка мгновенной победы: первая найденная и есть ответ
        if (move_wins(i, j, -1)) {
            winIdx = cand[k];
            break;
        }

        // Блокировка победы игрока: запоминаем первую, но ищем дальше возможную победу
        if (blockIdx == -1 && move_wins(i, j, 1))
            blockIdx = cand[k];
        if (blockIdx != -1)
            continue;

        // Оценка позиций
        // Атака
        board[i][j] = -1;
        int attackScore = evaluate_position(i, j, -1);
        board[i][j] = 0;

        // Защита
        board[i][j] = 1;
        int defendScore = evaluate_position(i, j, 1);
        board[i][j] = 0;

        // Бонус центра
        int dist = abs(i - center) + abs(j - center);
        int centerBonus = 20 - dist;

        int totalScore =
            attackScore * 2 +
            defendScore +
            centerBonus;

        if (totalScore > bestScore) {
            bestScore = totalScore;
            bestX = i;
            bestY = j;
        }
    }

    if (winIdx != -1)
        place_bot_at(winIdx / SIZE, winIdx % SIZE);
    else if (blockIdx != -1)
        place_bot_at(blockIdx / SIZE, blockIdx % SIZE);
    else if (bestX != -1)
        place_bot_at(bestX, bestY);
    else
        bot_medium();
}

// Выбор режима сложности бота
void bot_mode() {
    if (gameOver) return;
    if (botLevel == 1) bot_easy();
    else if (botLevel == 2) bot_medium();
    else bot_hard();
    if (is_full() && winner == 0) gameOver = 1;
    turn = 1;
}

// Ставит фигуру, проверяет победу, передаёт ход
void place_piece(int x, int y) {
    if (gameOver) return;
    if (x < 0 || x >= SIZE || y < 0 || y >= SIZE) return;
    if (board[x][y] != 0) return;

    set_stone(x, y, turn);

    if (check_win(x, y, turn)) {
        winner = turn;
        gameOver = 1;
        return;
    }

    if (is_full()) {
        gameOver = 1;
        return;
    }

    turn = -turn;

    if (gameMode == 2 && turn == -1)
        bot_mode();
}

// Экран меню выбор режима и сложности бота
void draw_menu() {
    glClearColor(0.91f, 0.90f, 0.87f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    draw_rect(WIDTH / 2 - 230, 30, 460, 90, 0.96f, 0.95f, 0.92f);
    draw_rect_outline(WIDTH / 2 - 230, 30, 460, 90, 0.44f, 0.48f, 0.51f, 1.5f);
    print_text(WIDTH / 2 - 155, 62, "FIVE IN A ROW", 0.34f, 0.40f, 0.44f);
    print_text(WIDTH / 2 - 195, 92, "5 IN A ROW ON ENDLESS FIELD", 0.52f, 0.49f, 0.45f);

    print_text(WIDTH / 2 - 95, 165, "SELECT GAME MODE", 0.34f, 0.40f, 0.44f);

    draw_rect(WIDTH / 2 - 125, 200, 250, 60, 0.60f, 0.64f, 0.67f);
    draw_rect_outline(WIDTH / 2 - 125, 200, 250, 60, 0.44f, 0.48f, 0.51f, 1.5f);
    print_text(WIDTH / 2 - 60, 224, "PVP MODE", 0.96f, 0.96f, 0.94f);
    print_text(WIDTH / 2 - 96, 244, "HUMAN VS HUMAN", 0.52f, 0.49f, 0.45f);

    draw_rect(WIDTH / 2 - 125, 300, 250, 60, 0.60f, 0.64f, 0.67f);
    draw_rect_outline(WIDTH / 2 - 125, 300, 250, 60, 0.44f, 0.48f, 0.51f, 1.5f);
    print_text(WIDTH / 2 - 60, 324, "PVE MODE", 0.96f, 0.96f, 0.94f);
    print_text(WIDTH / 2 - 86, 344, "HUMAN VS BOT", 0.52f, 0.49f, 0.45f);

    if (menuSelected == 2) {
        draw_rect(WIDTH / 2 - 150, 386, 300, 282, 0.96f, 0.95f, 0.92f);
        draw_rect_outline(WIDTH / 2 - 150, 386, 300, 282, 0.44f, 0.48f, 0.51f, 1.5f);
        print_text(WIDTH / 2 - 73, 408, "BOT LEVEL", 0.34f, 0.40f, 0.44f);

        if (botLevel == 1) draw_rect(WIDTH / 2 - 110, 430, 220, 50, 0.50f, 0.55f, 0.58f);
        else draw_rect(WIDTH / 2 - 110, 430, 220, 50, 0.60f, 0.64f, 0.67f);
        draw_rect_outline(WIDTH / 2 - 110, 430, 220, 50, 0.44f, 0.48f, 0.51f, 1.5f);
        print_text(WIDTH / 2 - 20, 450, "EASY", 0.96f, 0.96f, 0.94f);

        if (botLevel == 2) draw_rect(WIDTH / 2 - 110, 510, 220, 50, 0.50f, 0.55f, 0.58f);
        else draw_rect(WIDTH / 2 - 110, 510, 220, 50, 0.60f, 0.64f, 0.67f);
        draw_rect_outline(WIDTH / 2 - 110, 510, 220, 50, 0.44f, 0.48f, 0.51f, 1.5f);
        print_text(WIDTH / 2 - 32, 530, "MEDIUM", 0.96f, 0.96f, 0.94f);

        if (botLevel == 3) draw_rect(WIDTH / 2 - 110, 590, 220, 50, 0.50f, 0.55f, 0.58f);
        else draw_rect(WIDTH / 2 - 110, 590, 220, 50, 0.60f, 0.64f, 0.67f);
        draw_rect_outline(WIDTH / 2 - 110, 590, 220, 50, 0.44f, 0.48f, 0.51f, 1.5f);
        print_text(WIDTH / 2 - 20, 610, "HARD", 0.96f, 0.96f, 0.94f);
    }

    draw_rect(WIDTH / 2 - 250, HEIGHT - 110, 500, 70, 0.96f, 0.95f, 0.92f);
    draw_rect_outline(WIDTH / 2 - 250, HEIGHT - 110, 500, 70, 0.44f, 0.48f, 0.51f, 1.5f);
    print_text(WIDTH / 2 - 225, HEIGHT - 83, "CONTROL: LEFT MOUSE BUTTON - SELECT / PLACE", 0.52f, 0.49f, 0.45f);
    print_text(WIDTH / 2 - 130, HEIGHT - 63, "GOAL: BUILD A LINE OF 5", 0.34f, 0.40f, 0.44f);
}

// Экран игры сетка, фигуры и информационные панели
void draw_game() {
    glClearColor(0.91f, 0.90f, 0.87f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glLineWidth(1.0f);
    glColor3f(0.80f, 0.81f, 0.83f);
    for (int i = 0; i <= SIZE; i++) {
        glBegin(GL_LINES);
        glVertex2i(i * CELL_SIZE, 0);
        glVertex2i(i * CELL_SIZE, HEIGHT);
        glVertex2i(0, i * CELL_SIZE);
        glVertex2i(WIDTH, i * CELL_SIZE);
        glEnd();
    }

    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            if (board[i][j] == 1) {
                glColor3f(0.68f, 0.42f, 0.38f);
                glLineWidth(3);
                glBegin(GL_LINES);
                glVertex2i(i * CELL_SIZE + 5, j * CELL_SIZE + 5);
                glVertex2i(i * CELL_SIZE + CELL_SIZE - 5, j * CELL_SIZE + CELL_SIZE - 5);
                glVertex2i(i * CELL_SIZE + CELL_SIZE - 5, j * CELL_SIZE + 5);
                glVertex2i(i * CELL_SIZE + 5, j * CELL_SIZE + CELL_SIZE - 5);
                glEnd();
            }
            else if (board[i][j] == -1) {
                draw_circle(i * CELL_SIZE + CELL_SIZE / 2,
                           j * CELL_SIZE + CELL_SIZE / 2,
                           CELL_SIZE / 2 - 5,
                           0.42f, 0.56f, 0.54f);

                draw_circle(i * CELL_SIZE + CELL_SIZE / 2,
                           j * CELL_SIZE + CELL_SIZE / 2,
                           CELL_SIZE / 2 - 10,
                           0.91f, 0.90f, 0.87f);
            }
        }
    }

    draw_rect(10, 10, 360, 58, 0.96f, 0.95f, 0.92f);
    draw_rect_outline(10, 10, 360, 58, 0.44f, 0.48f, 0.51f, 1.5f);
    if (!gameOver) {
        if (turn == 1)
            print_text(24, 33, "TURN: X (PLAYER 1)", 0.68f, 0.42f, 0.38f);
        else
            print_text(24, 33, "TURN: O (PLAYER 2)", 0.42f, 0.56f, 0.54f);
        print_text(24, 52, "TARGET: 5 IN A ROW", 0.34f, 0.40f, 0.44f);
    }

    if (gameOver) {
        draw_rect(WIDTH / 2 - 150, HEIGHT - 90, 300, 60, 0.50f, 0.55f, 0.58f);
        draw_rect_outline(WIDTH / 2 - 150, HEIGHT - 90, 300, 60, 0.44f, 0.48f, 0.51f, 1.5f);

        if (winner == 1)
            print_text(WIDTH / 2 - 70, HEIGHT - 55, "X WINS", 0.68f, 0.42f, 0.38f);
        else if (winner == -1)
            print_text(WIDTH / 2 - 70, HEIGHT - 55, "O WINS", 0.68f, 0.42f, 0.38f);
        else
            print_text(WIDTH / 2 - 40, HEIGHT - 55, "DRAW", 0.34f, 0.40f, 0.44f);

        draw_rect(WIDTH / 2 - 190, HEIGHT - 155, 380, 42, 0.96f, 0.95f, 0.92f);
        draw_rect_outline(WIDTH / 2 - 190, HEIGHT - 155, 380, 42, 0.44f, 0.48f, 0.51f, 1.5f);
        print_text(WIDTH / 2 - 155, HEIGHT - 128, "LEFT CLICK TO RETURN TO MENU", 0.52f, 0.49f, 0.45f);
    }
}

int main() {
    srand(time(NULL));

    if (!glfwInit()) return -1;

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Five in a Row", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, WIDTH, HEIGHT, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);

    init_board();

    while (!glfwWindowShouldClose(window)) {
        // x, y - позиция курсора в окне
        double x, y;
        glfwGetCursorPos(window, &x, &y);
        int lmb = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);

        if (menu) {
            draw_menu();

            if (lmb == GLFW_PRESS && !mousePressed) {
                // Защита от многократной обработки зажатой кнопки мыши
                mousePressed = 1;

                if (x >= WIDTH / 2 - 125 && x <= WIDTH / 2 + 125 && y >= 200 && y <= 260) {
                    gameMode = 1;
                    menu = 0;
                    init_board();
                }
                else if (x >= WIDTH / 2 - 125 && x <= WIDTH / 2 + 125 && y >= 300 && y <= 360) {
                    menuSelected = 2;
                }
                else if (menuSelected == 2 && x >= WIDTH / 2 - 110 && x <= WIDTH / 2 + 110 && y >= 430 && y <= 480) {
                    botLevel = 1;
                    botSelected = 1;
                    gameMode = 2;
                    menu = 0;
                    init_board();
                }
                else if (menuSelected == 2 && x >= WIDTH / 2 - 110 && x <= WIDTH / 2 + 110 && y >= 510 && y <= 560) {
                    botLevel = 2;
                    botSelected = 2;
                    gameMode = 2;
                    menu = 0;
                    init_board();
                }
                else if (menuSelected == 2 && x >= WIDTH / 2 - 110 && x <= WIDTH / 2 + 110 && y >= 590 && y <= 640) {
                    botLevel = 3;
                    botSelected = 3;
                    gameMode = 2;
                    menu = 0;
                    init_board();
                }
            }
            else if (lmb == GLFW_RELEASE)
                mousePressed = 0;
        }
        else {
            draw_game();

            if (!gameOver && lmb == GLFW_PRESS && !mousePressed) {
                mousePressed = 1;
                place_piece((int)x / CELL_SIZE, (int)y / CELL_SIZE);
            }
            else if (lmb == GLFW_RELEASE)
                mousePressed = 0;

            if (gameOver && lmb == GLFW_PRESS && !mousePressed) {
                mousePressed = 1;
                menu = 1;
                init_board();
            }
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
