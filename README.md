# Лабораторна робота №4: Синхронізація у FreeRTOS (Counting Semaphore + Mutex)

* **Виконав:** Сеник Ярослав
* **Варіант:** 18 («Counting + Mutex комбіновано — пул ресурсів + спільна консоль»)
* **Префікс задач:** `SEYA_`

---

## 1. Опис проєкту

Проєкт демонструє комбіноване використання двох ключових примітивів синхронізації операційної системи реального часу FreeRTOS під Windows (симулятор WIN32-MSVC):
1. **Підраховувальний семафор (`xSemaphoreCreateCounting`)** — керує пулом обмежених спільних ресурсів місткістю $N = 2$ слоти (`maxCount = 2`, `initialCount = 2`).
2. **М'ютекс (`xSemaphoreCreateMutex`)** — забезпечує взаємне виключення при зверненні до спільного виводу (`stdout`/консоль), запобігаючи перемішуванню рядків від паралельних задач.

---

## 2. Параметри та архітектура (Варіант 18)

* **Кількість задач:** 5 паралельних задач-воркерів (`SEYA_W0` .. `SEYA_W4`).
* **Пул ресурсів:** ліміт одночасного доступу — максимум 2 задачі.
* **Тривалість роботи з ресурсом:** $T_{work} = 600 + (id \cdot 150)$ мс:
  * `SEYA_W0`: 600 мс
  * `SEYA_W1`: 750 мс
  * `SEYA_W2`: 900 мс
  * `SEYA_W3`: 1050 мс
  * `SEYA_W4`: 1200 мс
* **Пауза між запитами:** 500 мс.
* **Потокобезпечний вивід:** функція `safe_printf(const char *fmt, ...)` із захопленням `xConsoleMutex` (`portMAX_DELAY`) та гарантованим викликом `fflush(stdout)`.

---

## 3. Приклад виводу симуляції

```text
============================================================
 FreeRTOS Lab 4: Counting Semaphore + Mutex (Variant 18)
 Student: Yaroslav Senyk (Senyk Yaroslav | SEYA_)
 Resource Pool: 2 slots, Workers: 5 (SEYA_W0..SEYA_W4)
 Synchronization: Counting Sem (Pool) + Mutex (Console)
============================================================

[Task SEYA_W0] [WAITING] Requesting resource slot... (Tick: 2)
[Task SEYA_W0] [ACQUIRED] Slot acquired! (Work: 600ms, Free slots remaining: 1) at tick 2
[Task SEYA_W1] [WAITING] Requesting resource slot... (Tick: 2)
[Task SEYA_W1] [ACQUIRED] Slot acquired! (Work: 750ms, Free slots remaining: 0) at tick 2
[Task SEYA_W2] [WAITING] Requesting resource slot... (Tick: 2)
[Task SEYA_W3] [WAITING] Requesting resource slot... (Tick: 2)
[Task SEYA_W4] [WAITING] Requesting resource slot... (Tick: 2)
[Task SEYA_W0] [RELEASED] Slot released! (Free slots now: 1) at tick 602
[Task SEYA_W2] [ACQUIRED] Slot acquired! (Work: 900ms, Free slots remaining: 0) at tick 602
[Task SEYA_W1] [RELEASED] Slot released! (Free slots now: 1) at tick 752
[Task SEYA_W3] [ACQUIRED] Slot acquired! (Work: 1050ms, Free slots remaining: 0) at tick 752
[Task SEYA_W0] [WAITING] Requesting resource slot... (Tick: 1102)
[Task SEYA_W1] [WAITING] Requesting resource slot... (Tick: 1252)
[Task SEYA_W2] [RELEASED] Slot released! (Free slots now: 1) at tick 1502
[Task SEYA_W4] [ACQUIRED] Slot acquired! (Work: 1200ms, Free slots remaining: 0) at tick 1502
[Task SEYA_W3] [RELEASED] Slot released! (Free slots now: 1) at tick 1802
[Task SEYA_W0] [ACQUIRED] Slot acquired! (Work: 600ms, Free slots remaining: 0) at tick 1803
```

---

## 4. Структура файлів

* `main_blinky.c` (та `Blinky_Demo/main_blinky.c`) — реалізація логіки пулу ресурсів, задач-воркерів, обгортки `safe_printf`.
* `main.c` — перемикання режиму роботи (`#define mainCREATE_SIMPLE_BLINKY_DEMO_ONLY 1`).
* `FreeRTOSConfig.h` — конфігураційні параметри ядра FreeRTOS.

---

## 5. Інструкція із запуску проєкту

1. Завантажити ядро FreeRTOS (демо-оточення WIN32-MSVC):
   ```cmd
   git clone https://github.com/FreeRTOS/FreeRTOS.git
   cd FreeRTOS
   git submodule update --init --recursive
   ```
2. Скопіювати файли `main_blinky.c`, `main.c` та `FreeRTOSConfig.h` у директорію `FreeRTOS/Demo/WIN32-MSVC/`.
3. Відкрити `WIN32.sln` у Visual Studio 2022 / 2026.
4. Запустити проєкт (`Ctrl + F5` або `F5`).
