# Лабораторна робота №2: FreeRTOS TLS (Thread Local Storage)

* **Виконав:** Сеник Ярослав
* **Варіант:** 18

---

## 1. Опис проєкту

Проєкт демонструє використання механізму локального сховища потоків (Thread Local Storage, TLS) в операційній системі реального часу FreeRTOS на емуляторі під ОС Windows (порт WIN32-MSVC).

Реалізовано $N_{tasks} = 3$ незалежних задач-воркерів (`vWorker`), кожен з яких зберігає власний контекст та профіль виконання у виділених слотах TLS ядра FreeRTOS без використання глобальних змінних.

### Параметри варіанта №18
* **Кількість задач ($N_{tasks}$):** 3
* **Режим роботи (Mode):** `MODE_DELAY`
* **Розмір буфера пакетного виводу (Burst $B$):** 8 рядків
* **Слоти TLS:**
  * Слот $V_1 = 0$: покажчик на структуру контексту `task_context_t` (`name`, `iter`, `checksum`, `delayTicksOrCycles`, `seed`, `lines[BURST_B][96]`, `bcnt`).
  * Слот $V_2 = 1$: покажчик на структуру профілю `profile_t` (`mode`, `burstMode`, `baseDelayMs`, `baseCycles`, `step`).
* **Базова затримка ($Base$):** 90 мс
* **Крок затримки ($Step$):** 7 мс
* **Затримка $i$-ї задачі:** $Delay_i = 90 + (7 \cdot i)$ мс
* **Кількість ітерацій ($TotalIters$):** $10 \cdot 3 + 18 = 48$ ітерацій на задачу
* **Контрольна сума:** CRC-8 SAE J1850 на основі номера ітерації та seed

---

## 2. Структура коду

* `FreeRTOSConfig.h` — увімкнено підтримку TLS та встановлено ліміт покажчиків:
  ```c
  #define configNUM_THREAD_LOCAL_STORAGE_POINTERS 5
  ```
* `main.c` — перемикання режиму роботи на запуск blinky-демо:
  ```c
  #define mainCREATE_SIMPLE_BLINKY_DEMO_ONLY 1
  ```
* `main_blinky.c` — реалізація логіки лабораторної роботи:
  * динамічне виділення пам'яті через `pvPortMalloc` та збереження покажчиків у TLS через `vTaskSetThreadLocalStoragePointer`;
  * читання покажчиків контексту та профілю через `xTaskGetThreadLocalStoragePointer`;
  * обчислення контрольної суми CRC-8 SAE J1850;
  * пакетний вивід результатів (`burst_flush`) кожні 8 записів;
  * коректне звільнення виділеної пам'яті (`vPortFree`), скидання TLS-покажчиків та завершення задач через `vTaskDelete`.

---

## 3. Інструкція із запуску проєкту

### Вимоги:
* Visual Studio 2022 (з компонентом Desktop development with C++)
* FreeRTOS Kernel (демо-оточення WIN32-MSVC)

### Покроковий запуск:
1. Завантажити ядро FreeRTOS або використати існуюче:
   ```cmd
   git clone https://github.com/FreeRTOS/FreeRTOS.git
   cd FreeRTOS
   git submodule update --init --recursive
   ```
2. Помістити файли `FreeRTOSConfig.h`, `main.c` та `main_blinky.c` у директорію:
   `FreeRTOS/Demo/WIN32-MSVC/`
3. Відкрити файл рішення `FreeRTOS/Demo/WIN32-MSVC/WIN32.sln` у Visual Studio.
4. Зібрати та запустити проєкт комбінацією клавіш `Ctrl + F5` або `F5`.
5. У консолі спостерігати пакетний вивід виконання задач із TLS.
