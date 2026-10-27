# Лабораторна робота №6: Буфери потоків, буфери повідомлень та групи подій у FreeRTOS

* **Студент:** Сеник Ярослав (Senyk Yaroslav)
* **Префікс задач:** `SEYA_`
* **Варіант:** 10 («Простий чат між двома вузлами через gateway»)

---

## 1. Опис завдання та архітектура (Варіант 10)

У роботі реалізовано консольну симуляцію чату між двома вузлами зв'язку через центральний шлюз (Gateway) з використанням примітивів міжзадачної взаємодії FreeRTOS:
* **Stream Buffer (Буфер потоку)** для передачі неструктурованих байтових потоків.
* **Message Buffer (Буфер повідомлень)** для передачі цілісних структурованих пакетів зі збереженням меж повідомлень.
* **Event Group (Група подій)** для синхронізації стану вузлів, індикації наявності нових повідомлень та сигналізації про помилки форматування.

### Задачі системи:
1. **`SEYA_NodeA` (NodeATask):**
   * Встановлює біт активності `BIT_A_ONLINE` в групі подій.
   * Періодично передає неструктуровані текстові рядки байтів (`"Hello from Node A [seq=N]\n"`) у спільний Stream Buffer.
2. **`SEYA_NodeB` (NodeBTask):**
   * Встановлює біт активності `BIT_B_ONLINE` в групі подій.
   * Періодично передає неструктуровані текстові рядки байтів (`"Ack from Node B [seq=N]\n"`) у спільний Stream Buffer.
   * Для перевірки стійкості періодично інжектує некоректний порожній рядок для перевірки реакції шлюзу на помилку.
3. **`SEYA_Gateway` (GatewayTask):**
   * Зчитує байти зі спільного Stream Buffer посимвольно/потоково до символу нового рядка `\n`.
   * Перевіряє валідність повідомлення: якщо виявлено порожній/дефектний пакет, встановлює `BIT_ERROR_FORMAT`.
   * Формує структурований пакет `SEYA_ChatPacket_t` (ідентифікатор пакета, адреса відправника, адреса отримувача, довжина, корисне навантаження).
   * Записує структурований пакет у Message Buffer та виставляє біт готовності `BIT_MSG_PENDING` у групі подій.
4. **`SEYA_Monitor` (MonitorTask):**
   * За допомогою `xEventGroupWaitBits()` очікує одночасної присутності в мережі обох вузлів (`BIT_A_ONLINE | BIT_B_ONLINE` з прапорцем `waitForAllBits = pdTRUE`).
   * Після встановлення з'єднання відстежує події надходження повідомлень (`BIT_MSG_PENDING`) та помилок (`BIT_ERROR_FORMAT`).
   * Зчитує готові пакети з Message Buffer та виводить детальний звіт інспекції трафіку.

### Примітиви ядра FreeRTOS:
1. **Stream Buffer (`xStreamBufferCreate`):**
   * Розмір: 512 байт, trigger level = 1 байт.
   * Забезпечує потокове транспортування сирих байтів від вузлів до шлюзу.
2. **Message Buffer (`xMessageBufferCreate`):**
   * Розмір: 1024 байти.
   * Зберігає дискретні повідомлення зі збереженням їх довжини та меж запису.
3. **Event Group (`xEventGroupCreate`):**
   * `BIT_A_ONLINE` (`1 << 0`) — сигналізує про активність Node A.
   * `BIT_B_ONLINE` (`1 << 1`) — сигналізує про активність Node B.
   * `BIT_MSG_PENDING` (`1 << 2`) — сповіщає про наявність нового обробленого пакета у Message Buffer.
   * `BIT_ERROR_FORMAT` (`1 << 3`) — сигналізує про детекцію пошкодженого/порожнього пакета.

---

## 2. Приклад логу виконання

```text
===================================================================
 FreeRTOS Laboratory Work #6
 Student: Senyk Yaroslav (SEYA_)
 Variant: 10 ("Простий чат між двома вузлами через gateway")
 Primitives: Stream Buffer + Message Buffer + Event Group
===================================================================

[SEYA_CHAT] Starting FreeRTOS Scheduler...
[SEYA_CHAT] Monitor : Waiting for Node A and Node B to register online via Event Group...
[SEYA_CHAT] Gateway : GatewayTask active. Listening to raw stream buffer...
[SEYA_CHAT] Node A  : Online signal dispatched (BIT_A_ONLINE set).
[SEYA_CHAT] Monitor : >>> BOTH NODES ONLINE! (EventBits=0x03). Network channel operational.

[SEYA_CHAT] Node B  : Online signal dispatched (BIT_B_ONLINE set).
[SEYA_CHAT] Node A  : Streaming raw bytes -> "Hello from Node A [seq=1]
[SEYA_CHAT] Gateway : Formatted Packet #1 (Sender=A -> Recipient=B | "Hello from Node A [seq=1]"). Forwarding to Message Buffer.
[SEYA_CHAT] Monitor : [INSPECT PACKET #1] A -> B | Length: 25 bytes | Text: "Hello from Node A [seq=1]" | Lifetime Packets: 1
[SEYA_CHAT] Node B  : Streaming raw bytes -> "Ack from Node B [seq=1]
[SEYA_CHAT] Gateway : Formatted Packet #2 (Sender=B -> Recipient=A | "Ack from Node B [seq=1]"). Forwarding to Message Buffer.
[SEYA_CHAT] Monitor : [INSPECT PACKET #2] B -> A | Length: 23 bytes | Text: "Ack from Node B [seq=1]" | Lifetime Packets: 2
[SEYA_CHAT] Node A  : Streaming raw bytes -> "Hello from Node A [seq=2]
[SEYA_CHAT] Gateway : Formatted Packet #3 (Sender=A -> Recipient=B | "Hello from Node A [seq=2]"). Forwarding to Message Buffer.
[SEYA_CHAT] Monitor : [INSPECT PACKET #3] A -> B | Length: 25 bytes | Text: "Hello from Node A [seq=2]" | Lifetime Packets: 3
[SEYA_CHAT] Node B  : Streaming raw bytes -> "Ack from Node B [seq=2]
[SEYA_CHAT] Gateway : Formatted Packet #4 (Sender=B -> Recipient=A | "Ack from Node B [seq=2]"). Forwarding to Message Buffer.
[SEYA_CHAT] Monitor : [INSPECT PACKET #4] B -> A | Length: 23 bytes | Text: "Ack from Node B [seq=2]" | Lifetime Packets: 4
[SEYA_CHAT] Node A  : Streaming raw bytes -> "Hello from Node A [seq=3]
[SEYA_CHAT] Gateway : Formatted Packet #5 (Sender=A -> Recipient=B | "Hello from Node A [seq=3]"). Forwarding to Message Buffer.
[SEYA_CHAT] Monitor : [INSPECT PACKET #5] A -> B | Length: 25 bytes | Text: "Hello from Node A [seq=3]" | Lifetime Packets: 5
[SEYA_CHAT] Node B  : Injecting test malformed/empty packet to test BIT_ERROR_FORMAT...
[SEYA_CHAT] Gateway : [!] Empty/malformed line detected! Setting BIT_ERROR_FORMAT.
[SEYA_CHAT] Monitor : [ALERT] Event BIT_ERROR_FORMAT caught! Malformed packet logged. (Total Errors=1)
[SEYA_CHAT] Node A  : Streaming raw bytes -> "Hello from Node A [seq=4]
[SEYA_CHAT] Gateway : Formatted Packet #6 (Sender=A -> Recipient=B | "Hello from Node A [seq=4]"). Forwarding to Message Buffer.
[SEYA_CHAT] Monitor : [INSPECT PACKET #6] A -> B | Length: 25 bytes | Text: "Hello from Node A [seq=4]" | Lifetime Packets: 6
[SEYA_CHAT] Node B  : Streaming raw bytes -> "Ack from Node B [seq=3]
[SEYA_CHAT] Gateway : Formatted Packet #7 (Sender=B -> Recipient=A | "Ack from Node B [seq=3]"). Forwarding to Message Buffer.
[SEYA_CHAT] Monitor : [INSPECT PACKET #7] B -> A | Length: 23 bytes | Text: "Ack from Node B [seq=3]" | Lifetime Packets: 7
```

---

## 3. Склад файлів проекту

* `Blinky_Demo/main_blinky.c` — повна реалізація Лабораторної роботи №6 (Варіант 10).
* `main_blinky.c` — копія для кореневої збірки/перегляду.
* `main.c` — головна точка входу симулятора FreeRTOS (`mainCREATE_SIMPLE_BLINKY_DEMO_ONLY 1`).
* `FreeRTOSConfig.h` — конфігураційний файл ядра FreeRTOS з увімкненою динамічною алокацією (`configSUPPORT_DYNAMIC_ALLOCATION 1`).
* `open_sln.cmd` — скрипт для відкриття Visual Studio solution (`WIN32.sln`).

---

## 4. Інструкція із запуску

1. Відкрити `open_sln.cmd` або `FreeRTOS/FreeRTOS/Demo/WIN32-MSVC/WIN32.sln` у Visual Studio.
2. Обрати конфігурацію **Debug | x64**.
3. Запустити виконання без налагодження (`Ctrl + F5`) або з налагодженням (`F5`).
