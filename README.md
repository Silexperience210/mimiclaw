# LilyClaw: Pocket AI Assistant on a $15 Chip

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![DeepWiki](https://img.shields.io/badge/DeepWiki-mimiclaw-blue.svg)](https://deepwiki.com/memovai/mimiclaw)
[![Discord](https://img.shields.io/badge/Discord-mimiclaw-5865F2?logo=discord&logoColor=white)](https://discord.gg/r8ZxSvB8Yr)
[![X](https://img.shields.io/badge/X-@ssslvky-black?logo=x)](https://x.com/ssslvky)
[![Flash](https://img.shields.io/badge/⚡_Web_Flasher-LilyClaw-ff4500?style=for-the-badge)](https://silexperience210.github.io/lilyclaw/)

**[English](README.md) | [中文](README_CN.md)**

<p align="center">
  <img src="assets/banner.png" alt="MimiClaw" width="480" />
</p>

**The world's first AI assistant(OpenClaw) on a $15 chip. No Linux. No Node.js. Just pure C**

LilyClaw turns a tiny ESP32-S3 Lilygo Tdisplay S3 board into a personal AI assistant. Plug it into USB power, connect to WiFi, and talk to it through Telegram — it handles any task you throw at it and evolves over time with local memory — all on a chip the size of a thumb.

## Meet LilyClaw

- **Tiny** — No Linux, no Node.js, no bloat — just pure C
- **Handy** — Message it from Telegram, it handles the rest
- **Loyal** — Learns from memory, remembers across reboots
- **Energetic** — USB power, 0.5 W, runs 24/7
- **Lovable** — One ESP32-S3 board, $5, nothing else

## How It Works

![](assets/mimiclaw.png)

You send a message on Telegram. The ESP32-S3 picks it up over WiFi, feeds it into an agent loop — Claude thinks, calls tools, reads memory — and sends the reply back. Everything runs on a single $5 chip with all your data stored locally on flash.

## Quick Start

### What You Need

- An **ESP32-S3 dev board** with 16 MB flash and 8 MB PSRAM (e.g. Xiaozhi AI board, ~$10)
- A **USB Type-C cable**
- A **Telegram bot token** — talk to [@BotFather](https://t.me/BotFather) on Telegram to create one
- An **Anthropic API key** — from [console.anthropic.com](https://console.anthropic.com)

### Web Flash (easy — no tools needed)

> **[⚡ Flash LilyClaw from your browser](https://silexperience210.github.io/lilyclaw/)** — plug USB, click flash, done.

Works with Chrome/Edge. Flashes the latest firmware to your ESP32-S3 in 30 seconds. After flashing, connect to the `LilyClaw-Setup` WiFi and configure everything from your phone at `192.168.4.1`.

### Install (advanced — build from source)

```bash
# You need ESP-IDF v5.5+ installed first:
# https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/get-started/

git clone https://github.com/memovai/mimiclaw.git
cd mimiclaw

idf.py set-target esp32s3
```

### Configure

LilyClaw uses a **two-layer config** system: build-time defaults in `mimi_secrets.h`, with runtime overrides via the serial CLI. CLI values are stored in NVS flash and take priority over build-time values.

```bash
cp main/mimi_secrets.h.example main/mimi_secrets.h
```

Edit `main/mimi_secrets.h`:

```c
#define MIMI_SECRET_WIFI_SSID       "YourWiFiName"
#define MIMI_SECRET_WIFI_PASS       "YourWiFiPassword"
#define MIMI_SECRET_TG_TOKEN        "123456:ABC-DEF1234ghIkl-zyx57W2v1u123ew11"
#define MIMI_SECRET_API_KEY         "sk-ant-api03-xxxxx"
#define MIMI_SECRET_SEARCH_KEY      ""              // optional: Brave Search API key
#define MIMI_SECRET_PROXY_HOST      ""              // optional: e.g. "10.0.0.1"
#define MIMI_SECRET_PROXY_PORT      ""              // optional: e.g. "7897"
```

Then build and flash:

```bash
# Clean build (required after any mimi_secrets.h change)
idf.py fullclean && idf.py build

# Find your serial port
ls /dev/cu.usb*          # macOS
ls /dev/ttyACM*          # Linux

# Flash and monitor (replace PORT with your port)
# USB adapter: likely /dev/cu.usbmodem11401 (macOS) or /dev/ttyACM0 (Linux)
idf.py -p PORT flash monitor
```

### CLI Commands

Connect via serial to configure or debug. **Config commands** let you change settings without recompiling — just plug in a USB cable anywhere.

**Runtime config** (saved to NVS, overrides build-time defaults):

```
mimi> wifi_set MySSID MyPassword   # change WiFi network
mimi> set_tg_token 123456:ABC...   # change Telegram bot token
mimi> set_api_key sk-ant-api03-... # change Anthropic API key
mimi> set_model claude-sonnet-4-5  # change LLM model
mimi> set_proxy 127.0.0.1 7897  # set HTTP proxy
mimi> clear_proxy                  # remove proxy
mimi> set_search_key BSA...        # set Brave Search API key
mimi> config_show                  # show all config (masked)
mimi> config_reset                 # clear NVS, revert to build-time defaults
```

**Debug & maintenance:**

```
mimi> wifi_status              # am I connected?
mimi> memory_read              # see what the bot remembers
mimi> memory_write "content"   # write to MEMORY.md
mimi> heap_info                # how much RAM is free?
mimi> session_list             # list all chat sessions
mimi> session_clear 12345      # wipe a conversation
mimi> restart                  # reboot
```

## Memory

LilyClaw stores everything as plain text files you can read and edit:

| File | What it is |
|------|------------|
| `SOUL.md` | The bot's personality — edit this to change how it behaves |
| `USER.md` | Info about you — name, preferences, language |
| `MEMORY.md` | Long-term memory — things the bot should always remember |
| `2026-02-05.md` | Daily notes — what happened today |
| `tg_12345.jsonl` | Chat history — your conversation with the bot |

## Hardware Variants

| Version | Board | Features |
|---------|-------|----------|
| **v1.0** | Any ESP32-S3 (16MB flash, 8MB PSRAM) | Telegram + AI + tools |
| **v1.2** | LilyGo T-Display S3 | + Ecran + boutons + deep sleep |
| **v1.3** | T-Display S3 + HC-SR04 + 4 servos | + Corps physique animé |

### v1.3 Wiring — HC-SR04 + Servos

Build from source with v1.3 features:

```bash
idf.py menuconfig  # → MimiClaw Configuration → Enable both options
idf.py build && idf.py -p PORT flash
```

Or use the [Web Flasher](https://silexperience210.github.io/lilyclaw/) and select **v1.3**.

#### Wiring Diagram

```
                    T-Display S3
                   ┌────────────┐
                   │  USB-C     │
                   │            │
          GPIO 16 ─┤            ├─ GPIO 18  ──→ Servo Tete H
          GPIO 17 ─┤            ├─ GPIO 10  ──→ Servo Tete V
                   │            ├─ GPIO 11  ──→ Servo Pince G
                   │            ├─ GPIO 12  ──→ Servo Pince D
                   │     GND ───┤            ├─── GND
                   │     5V  ───┤            ├─── 5V
                   └────────────┘

      HC-SR04                          SG90 Servos (x4)
    ┌──────────┐                      ┌──────────┐
    │ VCC ─── 5V                      │ Rouge ─ 5V
    │ TRIG ── GPIO 16                 │ Marron ─ GND
    │ ECHO ── GPIO 17                 │ Orange ─ GPIO signal
    │ GND ─── GND                     └──────────┘
    └──────────┘
```

#### Pin Assignment

| Component | Pin | GPIO |
|-----------|-----|------|
| HC-SR04 TRIG | Trigger | **GPIO 16** |
| HC-SR04 ECHO | Echo | **GPIO 17** |
| Servo tete horizontal | Gauche/Droite | **GPIO 18** |
| Servo tete vertical | Haut/Bas | **GPIO 10** |
| Servo pince gauche | Ouvrir/Fermer | **GPIO 11** |
| Servo pince droite | Ouvrir/Fermer | **GPIO 12** |

#### Notes

- **Alimentation** : Les servos SG90 et le HC-SR04 fonctionnent en 5V. Utilisez le pin 5V de l'ESP32-S3 (alimenté par USB). Si les 4 servos bougent simultanément, un condensateur 470µF sur le rail 5V évite les brownouts.
- **HC-SR04 ECHO** : Le signal ECHO est en 5V. L'ESP32-S3 tolère 3.3V max sur ses GPIO. Ajoutez un **diviseur de tension** (2 résistances : 1kΩ + 2kΩ) sur ECHO, ou utilisez un module HC-SR04 **3.3V** (RCWL-1601).
- **Servos** : Modèles SG90 ou MG90S recommandés. Le signal PWM est 3.3V, compatible directement.
- Au boot, la tête se centre (90°) et les pinces se ferment (0°).
- L'IA peut contrôler les servos via Telegram avec les tools `move_head`, `move_claw`, `animate`, `read_distance`.

## Tools

LilyClaw uses Anthropic's tool use protocol — Claude can call tools during a conversation and loop until the task is done (ReAct pattern).

| Tool | Description |
|------|-------------|
| `web_search` | Search the web via Brave Search API for current information |
| `get_current_time` | Fetch current date/time via HTTP and set the system clock |
| `move_head` | Move the robot head (horizontal/vertical 0-180°) *(v1.3)* |
| `move_claw` | Open/close claws (left/right/both, 0-180°) *(v1.3)* |
| `read_distance` | Read ultrasonic distance sensor (cm) *(v1.3)* |
| `animate` | Play body animation: wave, nod_yes, nod_no, celebrate, think, sleep *(v1.3)* |

To enable web search, set a [Brave Search API key](https://brave.com/search/api/) via `MIMI_SECRET_SEARCH_KEY` in `mimi_secrets.h`.

## Also Included

- **WebSocket gateway** on port 18789 — connect from your LAN with any WebSocket client
- **OTA updates** — flash new firmware over WiFi, no USB needed
- **Dual-core** — network I/O and AI processing run on separate CPU cores
- **HTTP proxy** — CONNECT tunnel support for restricted networks
- **Tool use** — ReAct agent loop with Anthropic tool use protocol

## For Developers

Technical details live in the `docs/` folder:

- **[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)** — system design, module map, task layout, memory budget, protocols, flash partitions
- **[docs/TODO.md](docs/TODO.md)** — feature gap tracker and roadmap

## License

MIT

## Acknowledgments

Inspired by [OpenClaw](https://github.com/openclaw/openclaw) and [Nanobot](https://github.com/HKUDS/nanobot). MimiClaw reimplements the core AI agent architecture for embedded hardware — no Linux, no server, just a $5 chip.

