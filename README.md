# Lingo OS

**Lingo OS** is an experimental 64-bit operating system focused on natural-language interaction.

The goal is to let ordinary people control the system with everyday phrases such as:

- `create folder Projects`
- `show files`
- `go to Projects`

instead of only traditional commands.

## Status

Early research / hobby kernel (v1.5).

### Implemented

- 64-bit kernel (Limine bootloader)
- Physical + virtual memory management
- GDT, IDT, interrupts, keyboard (Shift/Caps)
- PIT timer and basic preemptive scheduling
- Interactive shell with natural-language phrases
- In-memory filesystem (folders + files, read/write)
- Process structures, context switch, kill/yield
- Dual-mode foundation: Ring 0 / Ring 3, syscalls (`int 0x80`)
- ELF loader basics + user-mode demo (`userdemo` / `runuser`)

### Not yet

- Persistent disk filesystem
- Full multi-user isolation
- Real ELF app ecosystem
- Networking / GUI

## Build & run

```bash
git clone https://github.com/vinyaspoojary318-cloud/New-OS.git
cd New-OS
# follow Limine C template setup, then:
make
make run
```

## Prompt

```text
lingo:/>
```

## Name

**Lingo OS** — natural language (“lingo”) for everyday system control.

## License

Experimental project — use and learn freely.
