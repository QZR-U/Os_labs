# Лабораторна робота №5: Прямі сповіщення задач та програмні таймери у FreeRTOS

* **Студент:** Сеник Ярослав (Senyk Yaroslav)
* **Префікс задач:** `SEYA_`
* **Варіант:** 12 («Планувальник задач з дедлайнами» / EDF Deadline Scheduler)

---

## 1. Опис завдання та архітектура (Варіант 12)

У роботі реалізовано консольну симуляцію планувальника задач з дедлайнами (EDF-like) з використанням прямих сповіщень задач (**Direct-to-Task Notifications**) та програмних таймерів (**Software Timers**) у симуляторі FreeRTOS під Windows (порт WIN32-MSVC).

### Задачі системи:
1. **`SEYA_GenTask` (GeneratorTask):**
   * Періодично формує нові завдання (Job ID, час виконання та відносний дедлайн).
   * Передає інформацію планувальнику `SEYA_SchedTask` за допомогою `xTaskNotify(..., value, eSetValueWithOverwrite)`.
2. **`SEYA_SchedTask` (SchedulerTask):**
   * Отримує сповіщення про нові задачі та розпаковує Job ID і дедлайн.
   * Зводить та запускає одноразовий таймер дедлайну (`xTimerChangePeriod` + `xTimerStart`).
   * Будить виконавця `SEYA_WorkerTask`, надсилаючи Job ID через `xTaskNotify()`.
   * Відстежує завершення робіт та настання дедлайнів за допомогою `ulTaskNotifyTake()` у режимі лічильного семафора.
3. **`SEYA_WorkerTask` (WorkerTask):**
   * Симулює корисне навантаження задачі через `vTaskDelay()`.
   * Якщо задача встигає виконатися до дедлайну, зупиняє таймер дедлайну (`xTimerStop`) та надсилає сповіщення про успішне виконання `SEYA_LogTask` і токен завершення `SEYA_SchedTask`.
   * Якщо дедлайн сплив, задача фіксує завершення із запізненням.
4. **`SEYA_LogTask` (LoggerTask):**
   * Отримує статусні події (`SEYA_EVENT_JOB_MET`, `SEYA_EVENT_DEADLINE_MISSED`, `SEYA_EVENT_JOB_LATE`).
   * Виводить детальну статистику з префіксом `[SEYA_EDF]`, включно з кількістю успішних робіт, пропущених дедлайнів та Success Rate (%).

### Програмні таймери:
1. **Періодичний таймер (`SEYA_PeriodicTimer`, період 1200 мс):**
   * В колбеку `prvPeriodicTimerCallback` пробуджує `SEYA_GenTask` через `xTaskNotifyGive()`.
2. **Одноразовий таймер дедлайну (`SEYA_DeadlineTimer`, One-Shot):**
   * Запускається на час дедлайну кожної задачі.
   * У разі спрацювання до завершення WorkerTask надсилає пряме сповіщення `SEYA_LogTask` із кодом `SEYA_EVENT_DEADLINE_MISSED` та токен планувальнику `SEYA_SchedTask`.

### Прямі сповіщення (Direct-to-Task Notifications):
* **Value Carrier:** передача Job ID (старші 16 біт) та відносного дедлайну (молодші 16 біт) у 32-бітному значенні повідомлення через `xTaskNotify()`.
* **Counting Semaphore:** підрахунок та облік завершених/пропущених робіт у планувальнику через `ulTaskNotifyTake(pdTRUE, portMAX_DELAY)`.

---

## 2. Приклад логу виконання

```text
===================================================================
 FreeRTOS Laboratory Work #5
 Student: Senyk Yaroslav (SEYA_)
 Variant: 12 ("Планувальник задач з дедлайнами" / EDF)
===================================================================

[SEYA_EDF] Starting FreeRTOS Scheduler...
[SEYA_EDF] LoggerTask online and logging execution statistics.
[SEYA_EDF] SchedulerTask ready to schedule jobs and track deadlines.
[SEYA_EDF] GeneratorTask initialized and waiting for timer triggers.
[SEYA_EDF] Generator : Created Job #1 | Workload=400 ms | Relative Deadline=750 ms
[SEYA_EDF] Scheduler : Scheduled Job #1 (Deadline: 750 ms). Arming one-shot timer.
[SEYA_EDF] WorkerTask ready to execute jobs.
[SEYA_EDF] Worker    : >>> Executing Job #1 (Simulated workload: 400 ms)...
[SEYA_EDF] Logger    : [MET] Job #1 COMPLETED ON TIME! (Exec=400 ms <= Deadline=750 ms)
[SEYA_EDF] Logger    : [STATS] Met: 1 | Missed: 0 | Total: 1 | Success Rate: 100.0%
[SEYA_EDF] Scheduler : Accounting event processed via ulTaskNotifyTake(). Total tallied events=1
[SEYA_EDF] Generator : Created Job #2 | Workload=850 ms | Relative Deadline=500 ms
[SEYA_EDF] Scheduler : Scheduled Job #2 (Deadline: 500 ms). Arming one-shot timer.
[SEYA_EDF] Worker    : >>> Executing Job #2 (Simulated workload: 850 ms)...
[SEYA_EDF] Logger    : [!] *** MISSED DEADLINE *** Job #2! (Deadline 500 ms expired)
[SEYA_EDF] Logger    : [STATS] Met: 1 | Missed: 1 | Total: 2 | Success Rate: 50.0%
[SEYA_EDF] Scheduler : Accounting event processed via ulTaskNotifyTake(). Total tallied events=2
[SEYA_EDF] Logger    : [LATE] Job #2 finished overdue workload (850 ms) after deadline.
[SEYA_EDF] Generator : Created Job #3 | Workload=300 ms | Relative Deadline=600 ms
[SEYA_EDF] Scheduler : Scheduled Job #3 (Deadline: 600 ms). Arming one-shot timer.
[SEYA_EDF] Worker    : >>> Executing Job #3 (Simulated workload: 300 ms)...
[SEYA_EDF] Logger    : [MET] Job #3 COMPLETED ON TIME! (Exec=300 ms <= Deadline=600 ms)
[SEYA_EDF] Logger    : [STATS] Met: 2 | Missed: 1 | Total: 3 | Success Rate: 66.7%
[SEYA_EDF] Scheduler : Accounting event processed via ulTaskNotifyTake(). Total tallied events=3
[SEYA_EDF] Generator : Created Job #4 | Workload=900 ms | Relative Deadline=450 ms
[SEYA_EDF] Scheduler : Scheduled Job #4 (Deadline: 450 ms). Arming one-shot timer.
[SEYA_EDF] Worker    : >>> Executing Job #4 (Simulated workload: 900 ms)...
[SEYA_EDF] Logger    : [!] *** MISSED DEADLINE *** Job #4! (Deadline 450 ms expired)
[SEYA_EDF] Logger    : [STATS] Met: 2 | Missed: 2 | Total: 4 | Success Rate: 50.0%
[SEYA_EDF] Scheduler : Accounting event processed via ulTaskNotifyTake(). Total tallied events=4
[SEYA_EDF] Logger    : [LATE] Job #4 finished overdue workload (900 ms) after deadline.
[SEYA_EDF] Generator : Created Job #5 | Workload=500 ms | Relative Deadline=800 ms
[SEYA_EDF] Scheduler : Scheduled Job #5 (Deadline: 800 ms). Arming one-shot timer.
[SEYA_EDF] Worker    : >>> Executing Job #5 (Simulated workload: 500 ms)...
[SEYA_EDF] Logger    : [MET] Job #5 COMPLETED ON TIME! (Exec=500 ms <= Deadline=800 ms)
[SEYA_EDF] Logger    : [STATS] Met: 3 | Missed: 2 | Total: 5 | Success Rate: 60.0%
[SEYA_EDF] Scheduler : Accounting event processed via ulTaskNotifyTake(). Total tallied events=5
```

---

## 3. Склад файлів проекту

* `Blinky_Demo/main_blinky.c` — повна реалізація Лабораторної роботи №5 (Варіант 12).
* `main_blinky.c` — копія для кореневої збірки/перегляду.
* `main.c` — головна точка входу симулятора FreeRTOS (`mainCREATE_SIMPLE_BLINKY_DEMO_ONLY 1`).
* `FreeRTOSConfig.h` — конфігураційний файл ядра FreeRTOS з увімкненими таймерами (`configUSE_TIMERS 1`).
* `open_sln.cmd` — скрипт для швидкого відкриття Visual Studio solution (`WIN32.sln`).

---

## 4. Інструкція із запуску

1. Відкрити `open_sln.cmd` або `FreeRTOS/FreeRTOS/Demo/WIN32-MSVC/WIN32.sln` у Visual Studio.
2. Обрати конфігурацію **Debug | x64**.
3. Запустити виконання без налагодження (`Ctrl + F5`) або з налагодженням (`F5`).
