# Лабораторна робота №3: Черги та тайм-аути у FreeRTOS

* **Виконав:** Сеник Ярослав
* **Варіант:** 18 $\to$ Варіант 8 ($18 \pmod{10} = 8$)
* **Префікс задач:** `SEYA_`

---

## 1. Опис проєкту

Проєкт демонструє роботу з механізмом міжзадачної взаємодії через черги повідомлень (`xQueueCreate`, `xQueueSend`, `xQueueReceive`) та обробку граничних умов за допомогою тайм-аутів в операційній системі реального часу FreeRTOS (симулятор під ОС Windows / MSVC).

Реалізовано дві незалежні задачі:
1. `SEYA_Producer` — генератор повідомлень фіксованої довжини (формат `"M%03u"`), що відправляє їх у чергу з таймаутом відправки 100 мс та циклічно змінює інтервал між ітераціями.
2. `SEYA_Consumer` — приймач повідомлень, що очікує нові дані з черги з таймаутом блокування 200 мс.

### Параметри варіанта №8 (18):
* **Кількість задач:** 2 (`SEYA_Producer`, `SEYA_Consumer`)
* **Тип елемента черги:** `char[5]` (5-байтний буфер рядка з термінатором `\0`, наприклад `"M001"`, `"M002"`)
* **Розмір черги (`QUEUE_LEN`):** 5 елементів
* **Тайм-аут відправки (`xQueueSend`):** 100 мс (`pdMS_TO_TICKS(100)`)
* **Тайм-аут отримання (`xQueueReceive`):** 200 мс (`pdMS_TO_TICKS(200)`)
* **Змінна затримка Producer:** циклічне перемикання значень затримок `{ 80ms, 120ms, 180ms, 260ms, 320ms }`.

---

## 2. Аналіз синхронізації та виникнення таймаутів

Коли затримка виробника (`Producer`) менша за таймаут споживача (80 мс, 120 мс, 180 мс $\le$ 200 мс):
- Споживач прокидається та успішно зчитує повідомлення.

Коли затримка виробника перевищує таймаут споживача (260 мс, 320 мс $>$ 200 мс):
- Споживач вичерпує ліміт очікування у 200 мс та фіксує подію тайм-ауту:
  ```text
  [Consumer | SEYA] Timeout waiting for data at tick 582
  ```
- Після завершення паузи у 260 мс (тік 642) Producer відправляє повідомлення `M005`, яке Consumer успішно отримує на наступній ітерації.

---

## 3. Приклад виводу симуляції

```text
============================================================
 FreeRTOS Lab 3: Queues & Timeouts (Variant 18 -> 8)
 Student: Yaroslav Senyk (Senyk Yaroslav | SEYA_)
 Queue length: 5, Item size: 5 bytes (char[5])
 Send timeout: 100 ms, Receive timeout: 200 ms
============================================================

[Producer | SEYA] Sent: 'M001' at tick 2 (next delay: 80ms)
[Consumer | SEYA] Got: 'M001' at tick 2 (waiting: 0)
[Producer | SEYA] Sent: 'M002' at tick 82 (next delay: 120ms)
[Consumer | SEYA] Got: 'M002' at tick 82 (waiting: 80)
[Producer | SEYA] Sent: 'M003' at tick 202 (next delay: 180ms)
[Consumer | SEYA] Got: 'M003' at tick 202 (waiting: 120)
[Producer | SEYA] Sent: 'M004' at tick 382 (next delay: 260ms)
[Consumer | SEYA] Got: 'M004' at tick 382 (waiting: 180)
[Consumer | SEYA] Timeout waiting for data at tick 582
[Producer | SEYA] Sent: 'M005' at tick 642 (next delay: 320ms)
[Consumer | SEYA] Got: 'M005' at tick 642 (waiting: 60)
[Consumer | SEYA] Timeout waiting for data at tick 842
[Producer | SEYA] Sent: 'M006' at tick 962 (next delay: 80ms)
[Consumer | SEYA] Got: 'M006' at tick 962 (waiting: 120)
```

---

## 4. Структура файлів

* `main_blinky.c` (та `Blinky_Demo/main_blinky.c`) — реалізація логіки черг, задач виробника та споживача, обробка таймаутів.
* `main.c` — перемикання у режим blinky-демо (`#define mainCREATE_SIMPLE_BLINKY_DEMO_ONLY 1`).
* `FreeRTOSConfig.h` — конфігурація ядра FreeRTOS.

---

## 5. Запуск проєкту

1. Завантажити ядро FreeRTOS (демо-оточення WIN32-MSVC):
   ```cmd
   git clone https://github.com/FreeRTOS/FreeRTOS.git
   cd FreeRTOS
   git submodule update --init --recursive
   ```
2. Скопіювати файли `main_blinky.c`, `main.c` та `FreeRTOSConfig.h` у директорію `FreeRTOS/Demo/WIN32-MSVC/`.
3. Відкрити `WIN32.sln` у Visual Studio 2022.
4. Запустити проєкт (`Ctrl + F5` або `F5`).
