
# Lightweight Container Runtime with Kernel Monitoring

## Project Overview
Done by :-
      Sumukh D Hegde         Santhosh M
      PES1UG24AM293          PES1UG24AM252


      
This project is a simple container runtime built using C and Linux system programming concepts. The goal was to understand how containers actually work under the hood instead of just using tools like Docker.

Along with that, a kernel module is implemented to monitor container processes from kernel space.

---

## What this project does

* Runs containers using Linux namespaces (`clone`)
* Uses a supervisor process to manage containers
* Communicates between CLI and supervisor using UNIX sockets
* Tracks container processes inside the kernel
* Allows basic commands like start, stop, and list containers

---

## Features

### Container Execution

Each container runs in an isolated environment using:

* PID namespace
* UTS namespace
* Mount namespace

---

### Supervisor Design

A central supervisor process:

* Receives commands from CLI
* Starts and stops containers
* Keeps track of container states
<img width="1320" height="460" alt="image" src="https://github.com/user-attachments/assets/27626a22-1b82-485d-a43e-9dabca00ef3f" />
<img width="1477" height="330" alt="image" src="https://github.com/user-attachments/assets/df59c3dc-9846-47fa-ab62-6ca1b3922f6f" />
<img width="1477" height="439" alt="image" src="https://github.com/user-attachments/assets/2ce2e736-eb8a-44a9-a56b-8c37fffa219c" />
<img width="1477" height="439" alt="image" src="https://github.com/user-attachments/assets/f1392906-6b62-4e67-bad4-e64935643366" />

---

### CLI Commands

```bash
./engine start <id> <rootfs> "<command>"
./engine ps
./engine stop <id>
```

---

### Kernel Module

The kernel module:

* Creates `/dev/container_monitor`
* Registers container PIDs using `ioctl`
* Continuously monitors processes
* Prints logs using `dmesg`
<img width="1477" height="330" alt="image" src="https://github.com/user-attachments/assets/d30844a7-8800-4fcc-9ecd-9bd798c6bd76" />
<img width="1477" height="259" alt="image" src="https://github.com/user-attachments/assets/692c78ec-f12f-46df-af51-4c741d78f2d6" />


---

## Project Structure

```
engine.c              → main runtime
monitor.c             → kernel module
monitor_ioctl.h       → ioctl definitions
Makefile              → build kernel module
logs/                 → container logs (optional)
rootfs-alpha/         → minimal filesystem
```

---

## How to Run

### 1. Compile engine

```bash
gcc engine.c -o engine -lpthread

```

---

### 2. Build kernel module

```bash
make
```

---

### 3. Load module

```bash
sudo insmod monitor.ko
```

---

### 4. Start supervisor

```bash
sudo ./engine supervisor
```

---

### 5. Start container

```bash
sudo ./engine start alpha ../rootfs-alpha "sleep 100"
```

---

### 6. List containers

```bash
sudo ./engine ps
```

---

### 7. Stop container

```bash
sudo ./engine stop alpha
```

---

### 8. Check kernel logs

```bash
sudo dmesg | grep monitor
```

---

## Example Output

```
[monitor] Registered PID: 22307
[monitor] Monitoring PID: 22307
```

---

## What I learned

* How containers actually work internally (namespaces, chroot, etc.)
* How user space and kernel space communicate using ioctl
* Basics of writing a kernel module
* Process management in Linux

---

## Limitations

* Uses a minimal root filesystem (Alpine)
* Memory monitoring depends on workload
* Not as feature-rich as Docker (intentionally simple)

---

## Conclusion

This project helped me understand the low-level working of containers and how Linux manages processes and isolation. It also gave hands-on experience with kernel modules, which was a new and interesting part.

---
# os-jackfruit
