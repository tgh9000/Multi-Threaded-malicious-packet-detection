# Multi-Threaded Malicious Packet Detection

A multithreaded, libpcap-based intrusion detection tool written in C. It sniffs live traffic on a network interface, parses each packet's headers in parallel across a fixed pool of worker threads, and flags three categories of malicious activity in real time:

- **SYN flooding** — tracks SYN packets and the number of distinct source IPs they came from.
- **ARP cache poisoning** — flags unsolicited ARP replies.
- **Blacklisted URL access** — inspects HTTP `Host:` headers for requests to Google or Facebook.

On exit (`Ctrl+C`), it prints an intrusion detection report summarizing everything it saw.

This project was built for the CS241 (Operating Systems/Systems Programming) coursework. The accompanying [`cs241_report.pdf`](./cs241_report.pdf) is the original design/testing writeup this README expands on — this document restructures and elaborates on that report alongside the actual source.

```
Intrusion Detection Report:
10 SYN packets detected from 1 different IPs (syn attack)
0 ARP responses (cache poisoning)
0 URL Blacklist violations (0 google 0 facebook)
```

---

## Table of contents

- [How it works, at a glance](#how-it-works-at-a-glance)
- [Project layout](#project-layout)
- [Architecture](#architecture)
  - [Why a thread pool](#why-a-thread-pool)
  - [The producer/consumer queue](#the-producerconsumer-queue)
  - [Packet lifecycle](#packet-lifecycle)
  - [Signal handling and graceful shutdown](#signal-handling-and-graceful-shutdown)
  - [Synchronisation and memory safety](#synchronisation-and-memory-safety)
  - [Detection logic](#detection-logic)
  - [The IP set (binary search tree)](#the-ip-set-binary-search-tree)
- [Building and running](#building-and-running)
  - [Running in the provided container](#running-in-the-provided-container)
  - [Command-line options](#command-line-options)
- [Testing](#testing)
- [Known limitations / future work](#known-limitations--future-work)
- [References](#references)

---

## How it works, at a glance

```
 libpcap (pcap_loop)                 main thread                         worker pool (10 threads)
 ─────────────────────              ─────────────────                   ──────────────────────────
 packet arrives on wire  ──────▶   packet_handler()
                                       │
                                       ▼
                                   dispatch()
                                    - deep-copies header + payload onto the heap
                                    - enqueue() under queueLock
                                    - pthread_cond_signal(queueNotEmpty)
                                       │
                                       ▼
                              ┌─────────────────┐        dequeue() (blocks on
                              │  shared FIFO     │  ───▶  queueNotEmpty when empty)
                              │  packet queue    │                  │
                              └─────────────────┘                  ▼
                                                              analyse(header, data, verbose)
                                                               - classify ARP / IPv4+TCP
                                                               - update global counters under
                                                                 per-category mutexes
                                                               - free(header); free(data)

 Ctrl+C (SIGINT) ──▶ handle_sigint() sets got_sigint, calls
                      pcap_breakloop() ──▶ pcap_loop() returns ──▶ dispatchShutdown()
                      (sets shutdownFlag, broadcasts queueNotEmpty, joins all workers)
                      ──▶ print_analysis()
```

The main thread's only job is capturing packets as fast as `libpcap` delivers them and handing off a private copy to the queue — it never blocks on analysis. A fixed pool of worker threads pulls from that queue and does all the actual header parsing and detection logic, so CPU-bound parsing is spread across cores instead of serialising everything behind the capture loop.

## Project layout

```
.
├── cs241_report.pdf                 # Original design & testing report (LaTeX, PDF)
├── cs241podman/
│   ├── Dockerfile                   # Alpine-based build/test environment
│   └── buildAlpine.sh               # podman build helper
├── runContainer.sh                  # Build the image and drop into a shell with NET_RAW
├── test functions.sh                # hping3 commands used for SYN-flood testing
└── root-folder/                     # Mounted as /root inside the container
    ├── makeCode.sh                  # make clean && make
    ├── runCode.sh                   # Runs the built binary with -v 1
    ├── runCodeVal.sh                # Runs the binary under valgrind
    ├── sendArp.sh                   # Runs the ARP-poisoning test script
    └── workspace/
        ├── .bashrc
        └── skeleton/
            ├── src/
            │   ├── main.c            # CLI argument parsing, entry point
            │   ├── sniff.c / .h      # libpcap capture loop, SIGINT handling, reporting
            │   ├── dispatch.c / .h   # thread pool + FIFO queue (producer/consumer)
            │   ├── analysis.c / .h   # per-packet header parsing & detection logic
            │   ├── globals.c / .h    # shared counters (protected by per-category mutexes)
            │   ├── tree.c / .h       # binary search tree used as a set of seen source IPs
            │   ├── queue.h           # (unused scaffold; see Known limitations)
            │   └── Makefile
            └── test/
                └── arp-poison.py     # scapy script that sends unsolicited ARP replies
```

## Architecture

### Why a thread pool

The design goal is to keep up with high packet rates without dropping traffic. Two things drove the shape of the solution (see §2.1 of the report):

1. **Separate capture from analysis.** If a single thread both read packets off the wire and parsed them, any slow packet (e.g. one requiring a TCP payload scan for blacklisted hosts) would stall capture and risk the kernel/libpcap dropping packets during a burst. Instead, `sniff()` runs `pcap_loop()` on the main thread purely to pull packets and hand them off — it is always ready for the next one.
2. **A fixed-size worker pool, not one thread per packet.** Creating and destroying a thread per packet would add scheduling overhead with no real parallelism benefit, since the OS can only run as many threads concurrently as there are cores. `dispatchInit()` spins up a constant `workerCount` (10) threads once, up front, and they live for the lifetime of the program, pulling from a shared queue.

A **FIFO queue** (not a priority queue) is used deliberately: every packet costs roughly the same amount of work to analyse, so there's no benefit to reordering — FIFO gives fairness and predictable latency, and avoids the bookkeeping overhead of a priority structure. Because workers pull independently from one shared queue, load balances itself naturally: a worker that just finished a cheap ARP packet immediately grabs the next job rather than sitting idle while another worker is busy, which is a property you'd otherwise have to design explicitly into a round-robin or per-thread-queue scheme.

### The producer/consumer queue

`dispatch.c` implements a classic bounded-by-nothing (unbounded, heap-allocated) singly linked FIFO:

```c
typedef struct packet_node {
    struct pcap_pkthdr *header;
    uint8_t *data;
    int verbose;
    struct packet_node *next;
} packet_node;
```

- `enqueue()` (called only from `dispatch()`, i.e. the main/capture thread) locks `queueLock`, appends the node at `queueTail`, and signals `queueNotEmpty` so one sleeping worker wakes up.
- `dequeue()` (called by worker threads) locks `queueLock` and waits on `queueNotEmpty` in a `while (queueHead == NULL && !shutdownFlag)` loop — the standard pattern for condition variables, which guards against spurious wakeups and against two workers racing to drain the same signal. It returns `NULL` only once the queue is empty **and** `shutdownFlag` has been set, which is how workers know to exit their loop.

Note `queue.h` in the repo defines a `Queue`/`Node`-based list type that isn't actually used anywhere — the real queue implementation lives inline in `dispatch.c` as `packet_node`. See [Known limitations](#known-limitations--future-work).

### Packet lifecycle

1. `pcap_loop()` invokes `packet_handler()` once per captured packet, handing it a pointer to `libpcap`'s **internal, reused buffer**.
2. `packet_handler()` immediately calls `dispatch()`, which **deep-copies** both the `pcap_pkthdr` and the packet bytes (`caplen` bytes) onto the heap with `malloc`/`memcpy` before enqueueing. This copy is mandatory: `libpcap` reuses the same buffer for the next packet, so without copying, a worker thread could read a packet that's already been partially overwritten by the time it gets scheduled. This is the single most important correctness property of the queue design, and the report calls it out explicitly as the fix for a would-be use-after-overwrite bug.
3. A worker thread's `dequeue()` pulls the node off the queue, calls `analyse(header, data, verbose)` to classify and count it, and then **frees `header`, `data`, and the node itself**. Every packet that enters the queue is matched by exactly one `analyse()` call and one free — this is what keeps the program leak-free under Valgrind (see [Testing](#testing)).

### Signal handling and graceful shutdown

The spec requires that `Ctrl+C` stop *new* packets from being accepted but let *already-queued* packets finish processing before the program exits and prints its report. This rules out just calling `exit()` from a signal handler. Two separate synchronisation signals are used for this (report §2.3):

- **`SIGINT`** (user-facing): `handle_sigint()` is a minimal, async-signal-safe handler — it only sets a `volatile sig_atomic_t got_sigint` flag and calls `pcap_breakloop()`. This unblocks `pcap_loop()` on the main thread without doing any unsafe work (like mutex locking) inside the signal handler itself. Once `pcap_loop()` returns, `sniff()` calls `dispatchShutdown()`, which:
  1. Locks `queueLock` and sets `shutdownFlag = 1`.
  2. Calls `pthread_cond_broadcast(&queueNotEmpty)` — unlike `enqueue()`'s single `pthread_cond_signal`, this wakes **every** sleeping worker so they can all re-check the shutdown condition, not just one.
  3. `pthread_join()`s every worker thread, which blocks until each one has drained the remaining queue and returned from `dequeue() == NULL`.
  4. Only after all workers have exited does `print_analysis()` run — guaranteeing the final report reflects every packet that was ever enqueued, not just the ones processed before the signal arrived.
- **`queueNotEmpty`** (internal, pthread condition variable): used by the queue itself to coordinate "a new packet arrived" (single signal) vs. "we're shutting down" (broadcast), as described above.

### Synchronisation and memory safety

- **`queueLock`** (one mutex) protects the shared queue (`queueHead`/`queueTail`) and `shutdownFlag` from concurrent access by the single producer (main thread) and multiple consumers (workers).
- **Per-category locks** (`arpLock`, `synLock`, `URLLock` in `analysis.c`) protect the global counters in `globals.c` (`arpNum`, `synNum`/`treeSize`/`tree`, `urlNum`/`googleNum`/`faccebookNum`). These are deliberately **three separate mutexes instead of one global lock**: an ARP packet being counted on one worker thread never has to wait behind a SYN packet being counted on another, so detection categories can be updated fully in parallel.
- **No shared ownership**: each `packet_node`'s `header`/`data` are allocated by exactly one `dispatch()` call and freed by exactly one worker after `analyse()` returns — there is no reference counting and no risk of double-free because ownership transfers cleanly from producer to a single consumer.
- Error handling around packet capture/thread creation is present so failures are reported rather than silently corrupting state (e.g. `pcap_open_live` and `pcap_loop` failures are logged and cause a clean exit).

### Detection logic

All detection happens in `analyse()` (`analysis.c`), called once per dequeued packet:

1. **Ethernet layer**: read `ether_type` to decide whether the payload is ARP (`0x0806`) or IPv4 (`0x0800`); anything else is ignored.
2. **ARP cache poisoning**: for ARP packets, check `ar_op == 2` (ARP *reply*, as opposed to a request). Every ARP reply increments `arpNum` under `arpLock`. (This mirrors how `test/arp-poison.py` works: it sends an unsolicited `op=2` reply for an IP nobody asked about.)
3. **IPv4 / TCP**: for IPv4 packets, the IP header is parsed to get `saddr`/`daddr`, then the IP header length (`ihl * 4`) is used to find the start of the TCP header.
   - **SYN flood detection**: if `TH_SYN` is set and `TH_ACK` is *not* (i.e. a client-initiated connection attempt, not part of a handshake's ACK), `synNum` is incremented and the source IP is inserted into the IP-set tree (`insert_unique`) under `synLock`. The final report pairs `synNum` (total SYN packets seen) with `treeSize` (distinct source IPs) — a large gap between the two with a single repeating source is a classic single-source SYN flood signature; a count where `treeSize` ≈ `synNum` indicates a distributed/spoofed-source flood.
   - **Blacklisted URL detection**: only for packets with TCP destination port 80 (HTTP). The code locates the TCP payload (skipping past the TCP header using `th_off * 4`), copies up to 1499 bytes of it into a local buffer, null-terminates it, and does a plain-text search for the `Host: ` header line (terminated by `\r\n`). If the extracted hostname exactly matches `www.google.co.uk` or `www.facebook.com`, it increments `urlNum` plus the matching per-site counter (`googleNum`/`faccebookNum`) under `URLLock` and prints a `badURL()` violation notice immediately (not just at the final report) — the test transcripts in the report show this as the `====... Blacklisted URL violation detected ...====` block printed live as traffic is seen.

Because this is a plaintext `Host:` header match on port 80, it only catches unencrypted HTTP requests — see [Known limitations](#known-limitations--future-work).

### The IP set (binary search tree)

`tree.c`/`tree.h` implement an unbalanced binary search tree keyed on `struct in_addr` (compared as raw 32-bit values via `compare_ip`). It's used as a **set** of distinct SYN-packet source IPs:

- `insert_unique()` walks down the tree comparing IPs; if the IP is already present it does nothing (no duplicate node, no size change); otherwise it allocates a new leaf and increments the global `treeSize`.
- The report's rationale (§2.4): membership-check-and-insert is the only operation ever needed (no deletion, no iteration), and a BST gives **O(log n)** average-case insert/lookup versus **O(n)** for a linear list — important when a flood test pushes through 100,000 SYN packets and the set may need to grow to ~100,000 unique entries.
- The report notes this as an unbalanced tree and flags switching it to a **self-balancing tree (e.g. AVL)** as future work, since an adversarial or already-sorted stream of source IPs would degrade it to O(n) per insert (effectively a linked list) — plausible if traffic arrives from sequential/incrementing spoofed addresses.

## Building and running

The code depends on `libpcap` and POSIX threads, and uses raw sockets for live capture — it needs root or `CAP_NET_RAW`/`CAP_NET_ADMIN` to actually open an interface.

```sh
cd root-folder/workspace/skeleton/src
make clean
make              # builds root-folder/workspace/skeleton/build/idsniff
```

Run it (needs capture privileges):

```sh
sudo ./../build/idsniff -i <interface> -v 1
```

### Running in the provided container

The repo ships a self-contained Alpine Linux test environment (via `podman`) with `libpcap`, `hping3`, `scapy`, and `valgrind` preinstalled:

```sh
./runContainer.sh
```

This builds the `alpinebuild` image from `cs241podman/Dockerfile` and drops you into a shell with `root-folder/` mounted at `/root` and `--cap-add=NET_RAW` so packet capture works without full `--privileged`. From inside the container:

```sh
bash makeCode.sh          # make clean && make
bash runCode.sh lo        # runs idsniff -i lo -v 1 (pass the interface as $1)
bash runCodeVal.sh        # same, but under valgrind --leak-check=full
bash sendArp.sh           # runs test/arp-poison.py in a second shell to trigger ARP detection
```

### Command-line options

```
Usage: idsniff [OPTIONS]...

  -i [interface]   Specify network interface to sniff (default: eth0)
  -v                Enable verbose mode. Useful for debugging
```

(Long forms `--interface`/`--verbose` are also registered via `getopt_long`, though the current build's verbose flag is just passed through to the packet handler rather than changing output.)

Press `Ctrl+C` to stop capturing; the program finishes any packets already queued and then prints the Intrusion Detection Report.

## Testing

The methodology below is summarized from the project report (§3); see `cs241_report.pdf` for the full write-up with captured terminal/Valgrind screenshots.

| Scenario | Tool / command | What it verifies |
|---|---|---|
| **Small-scale SYN test** | `hping3 -c 10 -d 120 -S -w 64 -p 80 -i u100 localhost` | 10 SYN packets from one source are correctly counted, and the single source collapses to 1 entry in the IP tree. Also confirms `Ctrl+C` triggers the report correctly. |
| **High-volume / distributed SYN test** | `hping3 -c 100000 -d 120 -S -w 64 -p 80 -i u10 --rand-source localhost` | Sustained high packet rate doesn't drop packets (100,000 in → 100,000 counted) and the unique-IP tree scales to ~100,000 entries (99,977 observed — hping3's randomised source occasionally repeats an address). |
| **ARP cache poisoning** | `test/arp-poison.py` (scapy), run 3×, via `sendArp.sh` | Unsolicited ARP replies (`op=2`) are correctly identified and counted, independent of SYN/URL counters. |
| **Blacklisted URL detection** | `wget www.google.co.uk` / `wget www.facebook.com` | Both the google and facebook counters increment correctly and independently, and a live violation notice is printed per request. |
| **Stress + memory safety** | `hping3 --flood -d 120 -S -w 64 -p 80 localhost` for 10s, under `valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes` | No packet loss under flood conditions and, critically, **0 bytes definitely/indirectly/possibly lost** — confirming every `malloc`'d header/data/node in the dispatch queue is freed exactly once by a worker thread, and the BST nodes (which are deliberately never freed, since they represent the running IP set until program exit) show up only as "still reachable", not leaked. |

All test commands referenced above are collected in `test functions.sh` (top-level hping3 commands) and `root-folder/*.sh` (container-side build/run/valgrind/ARP helpers).

## Known limitations / future work

- **Unbalanced BST for the IP set.** As noted above, a pathological (e.g. sequential) stream of source IPs degrades inserts to O(n). The report proposes converting it to an AVL tree.
- **Fixed worker count (10), not CPU-aware.** The pool size is a compile-time constant (`#define workerCount 10` in `dispatch.c`) rather than being derived from `sysconf(_SC_NPROCESSORS_ONLN)` or configurable via a CLI flag.
- **Plaintext HTTP only.** Blacklisted-URL detection greps for a `Host:` header in a port-80 TCP payload; it cannot see into TLS/HTTPS traffic (port 443), so it will miss the overwhelming majority of real-world traffic to google.com/facebook.com today. It also only matches the exact hostnames `www.google.co.uk` and `www.facebook.com`, not other subdomains/TLDs.
- **No IPv6 support.** Only `ETHERTYPE_IP` (IPv4, `0x0800`) and ARP (`0x0806`) are handled.
- **`queue.h` is dead code.** A separate `Queue`/`Node`/`list.h`-based structure exists in the tree but isn't wired into `dispatch.c`, which implements its own inline linked list instead. It can be removed or the dispatch queue could be refactored to use it.
- **Unbounded queue.** The dispatch queue has no maximum size/backpressure — under sustained capture faster than the worker pool can drain, memory usage grows unbounded rather than applying flow control.
- **No automated test suite.** Verification is manual, via the shell scripts and external tools (`hping3`, `scapy`, `wget`, `valgrind`) described above, rather than unit tests.

## References

From the original report:

1. M. Aboelwafa, A. Abdelkader, and A. Hamza. *A dynamic and scalable parallel network intrusion detection system.* Future Generation Computer Systems, 124:152–166, 2021.
2. F. Faraz. *A simple packet sniffer in c using libpcap.* https://gist.github.com/fffaraz/7f9971463558e9ea9545, 2014.
3. Jianmin Wang, Dongbo Chen, and Yaqiang Zhang. *Packet capture based on multi-core binding technology in linux.* Applied Mechanics and Materials, 48:902–906, 2011.
4. Shuguang Wen and Gaogang Xie. *libpcap-mt: A multi-threaded packet capture library.* In Proceedings of the Network Operations and Management Symposium, 2011.
