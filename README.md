# Real-Time C++ Chat Server 

I wanted to understand how chat apps like WhatsApp actually work under the hood, so I challenged myself to build one from scratch.

This project is a multi-user chat server built using raw C++ and basic networking (Winsock). It lets people connect, register, and send messages instantly.

## How It Works (Architecture)
Here is the blueprint of how the clients, server, and database all talk to each other:

[**System Architecture**](architecture_diagram.jpeg)

## Coolest Features

- **Talk to Everyone:** Multiple people can connect and chat at the exact same time.
- **Built from Scratch:** No easy libraries used. I wrote the logic to manage how data packets move.
- **It Remembers You:** I added a simple database (SQLite) so users can register and log back in later.
- **Private Messaging:** You can talk to the whole group or send private DMs using commands.
- **Color-Coded CLI:** The console uses different colors for private messages vs public chats.

## Tech Stack
- Language: C++
- Networking: Winsock2
- Database: SQLite3

## How to Try It
1.  Download/Clone the code.
2.  Open it in Visual Studio 2022.
3.  Build and run the Server project first.
4.  Run multiple instances of the Client project to simulate different users.

## WHY THIS PROJECT IS DIFFERENT
Most chat tutorials rely on heavy external libraries or just echo messages back and forth. I built this project to demonstrate:
1. Low-level networking (using raw Winsock headers).
2. Complex state management (handling multi-threaded race conditions).
3. Full-stack integration (connecting C++ backend logic to a persistent SQLite database).
