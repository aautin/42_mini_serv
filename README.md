# 42_mini_serv

# Mini Server (mini_serv.c)

## Overview
This project is a simple **TCP server** written in C. It listens for incoming connections, accepts multiple clients, and broadcasts messages to all connected clients. It uses **`select()`** for handling multiple clients in a non-blocking manner.

## Features
- Accepts multiple clients
- Uses **`select()`** to handle multiple connections
- Notifies all clients when someone **joins** or **leaves**
- Manages client messages in a dynamic buffer

---

## How It Works
### 1. **Server Initialization**
- The server creates a **TCP socket** (`socket()`)
- It binds to `127.0.0.1:<PORT>` (`bind()`)
- It starts listening for connections (`listen()`)

### 2. **Accepting Clients**
- When a new client connects (`accept()`), the server:
  - Creates a new **client struct** (`client_lst`)
  - Assigns an index to the client
  - Sends a message to all clients: _"server: client X just arrived"_

### 3. **Reading Client Messages**
- When a client sends a message:
  - It’s stored in a buffer (`str_join()`)
  - Messages are extracted (`extract_message()`)
  - The message is sent to all other clients

### 4. **Handling Client Disconnections**
- If a client disconnects:
  - It’s removed from the list (`pop_client()`)
  - A message is sent to all clients: _"server: client X just left"_
