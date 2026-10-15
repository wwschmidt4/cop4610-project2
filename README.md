# Elevator

This project extends the Linux kernel with a simulated pet elevator system. It includes system call tracing, a custom kernel timer module, and an elevator implementation using kernel threads, linked lists, mutexes, and a scheduling algorithm. The elevator transports pets between floors while enforcing capacity, weight, and movement constraints.

## Group Members
- Will Schmidt: cws22b@fsu.edu
- Aleksandar Stavreski: as21em@fsu.edu
- Carter Venclauskas: cv24i@fsu.edu

## Division of Labor

### Part 1: System Call Tracing
- **Responsibilities**: Write C programs to trace and compare system calls.
- **Assigned to**: Will Schmidt

### Part 2: Timer Kernel Module
- **Responsibilities**: [Description]
- **Assigned to**: Jane Smith

### Part 3a: Adding System Calls
- **Responsibilities**: [Description]
- **Assigned to**: Alex Brown

### Part 3b: Kernel Compilation
- **Responsibilities**: [Description]
- **Assigned to**: Alex Brown, Jane Smith

### Part 3c: Threads
- **Responsibilities**: [Description]
- **Assigned to**: Alex Brown, Jane Smith

### Part 3d: Linked List
- **Responsibilities**: [Description]
- **Assigned to**: Jane Smith

### Part 3e: Mutexes
- **Responsibilities**: [Description]
- **Assigned to**: John Doe

### Part 3f: Scheduling Algorithm
- **Responsibilities**: [Description]
- **Assigned to**: Alex Brown, John Doe

## File Listing
```
elevator/
├── Makefile
├── part1/
│   ├── empty.c
│   ├── empty.trace
│   ├── part1.c
│   ├── part1.trace
│   └── Makefile
├── part2/
│   ├── src/
│   └── Makefile
├── part3/
│   ├── src/
│   ├── tests/
│   ├── Makefile
│   └── sys_call.c
├── Makefile
└── README.md

```
# How to Compile & Execute

### Requirements
- **Compiler**: `gcc`

## Part 1

### Compilation
```bash
make
```
This will build the empty and part1 executables in the part1/ directory.
### Execution
```bash
make run
```
This will run both executables: empty first, followed by part1.

## Part 2

### Compilation
For a C/C++ example:
```bash
make
```
This will build the executable in ...
### Execution
```bash
make run
```
This will run the program ...


## Part 3

### Compilation
For a C/C++ example:
```bash
make
```
This will build the executable in ...
### Execution
```bash
make run
```
This will run the program ...

## Development Log
Each member records their contributions here.

### [Member 1]

| Date       | Work Completed / Notes |
|------------|------------------------|
| 2026-10-15 | Implemented part 1     |
| YYYY-MM-DD | [Description of task]  |
| YYYY-MM-DD | [Description of task]  |

### [Member 2]

| Date       | Work Completed / Notes |
|------------|------------------------|
| YYYY-MM-DD | [Description of task]  |
| YYYY-MM-DD | [Description of task]  |
| YYYY-MM-DD | [Description of task]  |


### [Member 3]

| Date       | Work Completed / Notes |
|------------|------------------------|
| YYYY-MM-DD | [Description of task]  |
| YYYY-MM-DD | [Description of task]  |
| YYYY-MM-DD | [Description of task]  |


## Meetings
Document in-person meetings, their purpose, and what was discussed.

| Date       | Attendees            | Topics Discussed | Outcomes / Decisions |
|------------|----------------------|------------------|-----------------------|
| YYYY-MM-DD | [Names]              | [Agenda items]   | [Actions/Next steps]  |
| YYYY-MM-DD | [Names]              | [Agenda items]   | [Actions/Next steps]  |
| YYYY-MM-DD | [Names]              | [Agenda items]   | [Actions/Next steps]  |



## Bugs
- **Bug 1**: This is bug 1.
- **Bug 2**: This is bug 2.
- **Bug 3**: This is bug 3.
