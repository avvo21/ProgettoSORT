# Real-Time Cyclic Executive (Progetto SORT)

A deterministic, frame-based **Cyclic Executive** real-time scheduler implemented in C++11 using POSIX real-time threads (`pthread`), `SCHED_FIFO` priority-based scheduling, and CPU core affinity.

> **Academic Project Notice**  
> This project was developed as part of the **Real-Time Operating Systems** (*Sistemi Operativi in Tempo Reale*) course taught by **Prof. Stefano Caselli** at the **University of Parma** (*Università degli Studi di Parma*).  
> **Authors**: Andrea Veronesi, Gabriele Incardona

---

## Overview

In hard real-time systems, predictability and determinism are paramount. A **Cyclic Executive** statically schedules periodic tasks into discrete time slots called **frames** (minor cycles) within a **hyperperiod** (major cycle).

This project implements an object-oriented Cyclic Executive engine in C++ with support for:
- Pre-allocated periodic real-time tasks.
- Static frame-based scheduling.
- Dynamic **slack stealing** for aperiodic task servicing.
- Runtime **deadline miss detection** and error handling.
- Deterministic CPU execution via core affinity and POSIX real-time priorities (`SCHED_FIFO`).

---

## Architecture & How It Works

### 1. Thread-per-Task Model
- Each periodic task (and the aperiodic task) runs in its own dedicated `std::thread` wrapped in a state machine (`IDLE`, `PENDING`, `RUNNING`).
- Tasks wait on condition variables (`std::condition_variable`) when idle, avoiding the runtime overhead of thread recreation and ensuring instantaneous activation.

### 2. Deterministic Scheduling & Real-Time Priorities
- The **Executive Thread** runs at the highest real-time priority (`rt::priority::rt_max`).
- At the start of each frame:
  - If an aperiodic task is requested and sufficient **slack time** exists, the aperiodic task is promoted to a high priority (`rt_max - 1`) for the duration of the slack.
  - Periodic tasks allocated to the current frame are released by notifying their condition variables and assigning descending real-time priorities (`--p`). Under Linux's `SCHED_FIFO`, this strictly guarantees sequential, ordered execution.
- The executive thread then sleeps until the beginning of the next frame using `std::this_thread::sleep_until()`.

### 3. Slack Stealing for Aperiodic Tasks
- During initialization, the executive calculates the available **slack** for each frame:
  $$\text{Slack}_k = \text{Frame Length} - \sum_{i \in \text{Frame}_k} \text{WCET}_i$$
- When an aperiodic job is requested:
  - If the current frame has available slack ($\text{Slack}_k > 0$), the aperiodic task is scheduled immediately during that slack interval.
  - Once the slack interval expires, its priority drops to `rt_min`, allowing periodic tasks to execute without risking deadline misses.

### 4. Deadline Miss Detection
At each frame boundary, the executive inspects task states:
- If a task from the preceding frame is still in `RUNNING` state, it has overshot its deadline.
- If a task is still in `PENDING` state, it failed to start within its allocated frame.
- Overruns are logged, and policy hooks can abort or track overrunning tasks.

### 5. CPU Core Affinity
To eliminate context-switch latency and cache invalidation caused by multi-core migration, all threads are pinned to a single designated CPU core using `pthread_setaffinity_np` (via the `rt::affinity` wrapper).

### 6. Calibrated Workload Simulation (`busy_wait`)
A calibrated busy-waiting routine (`busy_wait`) consumes precise CPU cycles to realistically emulate task execution up to each task's Worst-Case Execution Time (WCET).

---

## Project Structure

```text
.
├── CMakeLists.txt        # CMake build configuration
├── Makefile              # GNU Make build configuration
├── executive.h           # Executive class declaration and API
├── executive.cpp         # Executive implementation (dispatching, slack, deadline checks)
├── busy_wait.h           # Workload generator interface
├── busy_wait.cpp         # CPU calibration and busy-wait implementation
├── application_1.cpp     # Demo 1: Basic periodic schedule execution
├── application_2.cpp     # Demo 2: Overrun scenario & deadline miss detection
├── application_3.cpp     # Demo 3: Aperiodic request handling with slack stealing
└── rt/                   # Real-time POSIX wrappers
    ├── Makefile          # Sub-make for rt library
    ├── affinity.h        # CPU affinity wrapper interface
    ├── priority.h        # SCHED_FIFO priority wrapper interface
    └── rt_pthread.cpp    # POSIX pthread and sched implementation
```

---

## Example Applications

- **`application_1`**: Demonstrates a clean periodic task set executed across 5 frames with specific WCETs per task.
- **`application_2`**: Simulates a periodic task exceeding its WCET (every 5 cycles), triggering and verifying the deadline miss detection mechanism.
- **`application_3`**: Demonstrates an aperiodic task (`task_ap`) requested on-demand by periodic task 4, demonstrating slack stealing without perturbing periodic deadlines.

---

## Building and Running

### Prerequisites
- Linux OS with real-time support (POSIX Pthreads, `SCHED_FIFO`).
- C++11 compliant compiler (`g++` / `clang++`).
- Build tool: `make` or `cmake` (>= 3.10).

### Build with Make

```bash
make
```

### Build with CMake

```bash
mkdir build && cd build
cmake ..
make
```

### Running

Because real-time priority assignment (`SCHED_FIFO`) and CPU affinity require elevated privileges, execute the binaries with `sudo` (or grant the `CAP_SYS_NICE` capability to the executable):

```bash
# Example: Running Application 1
sudo ./application_1

# Example: Running Application 2 (Deadline Miss Demo)
sudo ./application_2

# Example: Running Application 3 (Slack Stealing Demo)
sudo ./application_3
```
