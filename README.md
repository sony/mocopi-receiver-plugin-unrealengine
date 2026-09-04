# mocopi Receiver Plugin for Unreal Engine

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](./LICENSE)
[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.5+-blue.svg)](https://www.unrealengine.com/)

mocopi Receiver Plugin for Unreal Engine is a plugin for receiving motion data transmitted from the mocopi app and applying it to 3D avatars in Unreal Engine in real-time.

## License Notice
- This project is licensed under the Apache License 2.0 - see the [LICENSE](./LICENSE) file for details.
- Notwithstanding the foregoing, this repository does not include the mocopi logo or application icons. Use of these assets requires entering into a separate mocopi Logo and Icon License Agreement. ([here](https://www.sony.co.jp/en/Products/mocopi-dev/en/others/LogoGuideline.html))

## Overview

**mocopi** is a motion capture system that captures full-body motion data using a smartphone app or PC app combined with mocopi sensors. By using this Unreal Engine plugin, you can easily apply captured motion data to 3D avatars in your Unreal Engine applications.

### Key Features

- **Real-time Motion Capture**: Directly receive motion data from mocopi app via UDP through Live Link
- **Avatar Integration**: Apply motion data to Unreal Engine characters with automatic bone mapping
- **Cross-Platform Support**: Works on Windows and macOS
- **Easy Integration**: Simple Live Link setup for quick configuration and customization

## Supported Platforms

| Platform | Supported Versions |
|----------|-------------------|
| **Windows** | Windows 10/11 |
| **macOS** | macOS 10.15+ |

> **Note**: Android, iOS, and Linux are not currently supported.

## Requirements

### Development Environment
- **Unreal Engine**: Unreal Engine 5.5 or later
- **Network**: Local network connection between mocopi app and Unreal Engine application

> **Windows packaging support**: `PackagePluginWindows.bat` targets Unreal Engine 5.5, 5.6, 5.7, and 5.8.

### Runtime Requirements
- mocopi app (Mobile version: Android/iOS, or PC version: Windows)
- UDP communication connection between mocopi app and Unreal Engine application
- Maximum motion data frame rate: Depends on mocopi app settings

## Installation and Building

### Building the Plugin

1. Ensure you have Unreal Engine 5.5 or later installed
2. Run the appropriate build script for your platform:

#### Windows
```bash
BuildScripts/PackagePluginWindows.bat
```

The Windows script packages separate plugin builds for Unreal Engine 5.5 through 5.8.

#### macOS
```bash
chmod +x BuildScripts/PackagePluginMac.sh
BuildScripts/PackagePluginMac.sh
```

This will create a packaged plugin in the output directory.

### Reliability tuning

The mocopi source defaults to the **Smooth** preset: a 120 ms Engine Time offset with a 120-frame Live Link buffer. Packet Timestamp Recovery uses the capture timestamp embedded in each mocopi packet, allowing Live Link to interpolate between the surrounding real poses when intermediate UDP packets are missing. This follows the same timestamp-buffering principle used by Sony's Unity receiver.

| Preset | Offset | Buffer | Purpose |
| --- | ---: | ---: | --- |
| **Realtime** | 40 ms | 60 frames | Minimum practical latency for clean networks |
| **Smooth (Recommended)** | 120 ms | 120 frames | Balanced event and production use |
| **Reliable** | 200 ms | 160 frames | Maximum resilience for crowded or unstable wireless environments |
| **Custom** | User-defined | User-defined | Fine-grained manual tuning |

Select the mocopi source in **Window > Virtual Production > Live Link** to adjust:

- **Buffer Size (Frames)** and **Engine Time Offset** for the delay-versus-resilience tradeoff
- **UDP Receive Buffer Size** and **Connection Timeout** for burst and dropout tolerance
- **Use Packet Timestamp Recovery** to reconstruct smooth playback timing across short packet-loss gaps
- **Rotation Smoothing Strength** and **Translation Smoothing Strength** for pose filtering
- duplicate/out-of-order rejection and live received/lost/rejected frame diagnostics

Higher offsets and smoothing strengths improve stability at the cost of responsiveness. Selecting a preset applies all related reliability values; manually changing a controlled value marks the preset as **Custom**.

#### Network simulation test panel

The **Mocopi Network Simulation** category can intentionally impair motion packets with deterministic random loss, burst loss, jitter, duplicates, and reordering. Skeleton-definition packets are never impaired, and simulation is forced off in Shipping builds.

For a repeatable event-style test, enable simulation and start with seed `1337`, 5% random loss, a three-frame burst every 250 frames, and 50 ms maximum jitter. The diagnostics show intentionally dropped, delayed, and duplicated packets alongside the received/lost/rejected counters. Always disable simulation after testing.

### Installing to Your Project

1. Copy the built plugin to your Unreal Engine project's `Plugins` directory
2. Open your Unreal Engine project
3. Go to **Edit > Plugins** and enable "mocopi Live Link"
4. Restart the Unreal Engine editor when prompted

## Quick Start

For detailed setup instructions, please refer to the following video tutorials:

### 1. Plugin Setup
**mocopi for Unreal Engine Tutorial #1 Plugin Setup**  
[![Tutorial 1](https://img.youtube.com/vi/uG3puUXEIjo/0.jpg)](https://www.youtube.com/watch?v=uG3puUXEIjo)

### 2. Motion Recording
**mocopi for Unreal Engine Tutorial: #2 Record your motion**  
[![Tutorial 2](https://img.youtube.com/vi/b_CaiUisSRM/0.jpg)](https://www.youtube.com/watch?v=b_CaiUisSRM)

### 3. Live Retargeting
**mocopi for Unreal Engine Tutorial: #3 Live Retargeting**  
[![Tutorial 3](https://img.youtube.com/vi/MKT2p6xV5XU/0.jpg)](https://www.youtube.com/watch?v=MKT2p6xV5XU)

### 4. UEFN / LiveLink Hub
**UEFN / LiveLink Hub tutorial**  
[![UEFN Tutorial](https://img.youtube.com/vi/aFRzeHRmlY0/0.jpg)](https://www.youtube.com/watch?v=aFRzeHRmlY0)

### Network Setup

Please ensure that the smartphone running the mocopi app and your Unreal Engine application are connected to the same local network. The plugin communicates via UDP on port 12351 by default.

## Sample Projects

This repository includes sample scenes and assets demonstrating basic usage:

- **Basic Live Link Sample**: Shows fundamental Live Link setup and character control
- **Advanced Integration**: Demonstrates custom bone mapping and data processing
- **Sample Characters**: Pre-configured character Blueprints and Animation Blueprints

To check the samples, please refer to the plugin's Content folder after installation.

## Troubleshooting

### Common Issues

**Q: Motion data is not being received**
- Please verify that both devices are connected to the same network
- Please check UDP port configuration (default: 12351)
- Please confirm that the mocopi app is transmitting data

**Q: Avatar movement appears jerky**
- Please check network latency and stability
- Please verify frame rate settings in the mocopi app
- Select the **Reliable** preset for crowded venues or unstable wireless environments
- For Custom tuning, keep **Use Packet Timestamp Recovery** enabled and increase **Engine Time Offset** in small steps
- Increase rotation or translation smoothing only as much as needed, because stronger smoothing adds response lag

For additional troubleshooting, please see the [FAQ](https://www.sony.co.jp/en/Products/mocopi-dev/en/documents/ReceiverPlugin/UnrealEngine/TroubleShoot.html).

## Support

For technical support and questions, please join the following Discord server:

**Discord**: https://discord.gg/k55wY45y5N

## Resources

- **mocopi Official Developer Site**: [https://sony.net/mocopi-dev/](https://sony.net/mocopi-dev/)
- **Documentation**: [Unreal Engine Plugin Guide](https://www.sony.co.jp/en/Products/mocopi-dev/en/documents/ReceiverPlugin/UnrealEngine/AboutPlugin.html)

---

**Copyright © 2026 Sony Corporation. All rights reserved.**
