# A Non-Contact Intelligent Inspection and Micro-Environment Intervention Platform for Strawberry Greenhouses

<p align="center">
  <img src="images/logo.jpg" width="600">
</p>

---

## Overview

This project presents a **non-contact intelligent inspection and micro-environment intervention platform for strawberry greenhouses**. It integrates mobility, environmental sensing, visual inspection, local alerting, desktop visualization, and operator takeover into a unified system.

The platform is designed to support continuous monitoring of strawberry maturity, leaf health, and local environmental abnormalities inside greenhouse environments. By reducing repetitive manual inspection, lowering the risk of contact damage to delicate crops, and providing more traceable data for decision-making, the system aims to make greenhouse inspection more structured, efficient, and practical.

Rather than presenting the system as a fully autonomous replacement for greenhouse labor, this project explores how a low-cost robotic platform can restructure routine inspection into a more repeatable, traceable, and data-supported workflow.

## Background and Motivation

Strawberry cultivation is highly sensitive to environmental conditions and crop status. In greenhouse production, changes in temperature, light, humidity, and plant health can directly affect fruit quality, ripening progress, and final yield. However, traditional inspection still relies heavily on manual patrol. Growers or operators must walk through greenhouse aisles, check fruit maturity, observe whether leaves show yellowing or dark spots, and identify local environmental abnormalities based on experience.

This traditional approach has several limitations. First, it is labor-intensive and time-consuming. Second, manual inspection is difficult to standardize and depends strongly on individual experience. Third, strawberries are delicate crops: close inspection, touching, or moving leaves and fruits can easily cause bruising, contamination, or accidental damage, which directly affects product quality.

To address these practical problems in strawberry greenhouse scenarios, this project was developed as an integrated inspection platform rather than a simple sensor-equipped smart car. The system combines **automatic line-following patrol and obstacle avoidance** for routine movement, **temperature sensing and threshold-based feedback** for local environmental judgment, **OpenCV-based fruit and leaf inspection** for non-contact crop observation, **buzzer and LED alerts** for local warning, **GUI-based desktop visualization** for system monitoring, and **infrared remote control with pan-tilt adjustment** for manual takeover and close-up review.

This design reflects a key observation: greenhouse problems are often **local rather than uniform**. One area may be slightly hotter, another may receive insufficient light, a few plants may show leaf abnormalities, and fruit in one row may ripen faster than those in another. Fixed observation points and occasional manual checks are often not detailed or continuous enough to capture these differences. A mobile platform, by contrast, can move across the greenhouse, observe local variations, and generate image and sensor records with greater consistency.

From this perspective, the value of the project lies not in any single function, but in how multiple functions operate together as one inspection workflow. The system is not only a moving platform, but also a prototype that connects **mobile inspection, environmental sensing, visual recognition, abnormal warning, human review, and data visualization**. Its purpose is to reduce repetitive manual work, lower contact-related crop damage, and support earlier and more data-supported intervention in greenhouse management.

In short, this project addresses a practical question: **Can a lightweight, mobile, and expandable platform replace part of the traditional high-frequency manual inspection process without increasing crop-damage risk?** This project is our answer to that question.

## Application Value

The significance of this platform lies in its ability to transform greenhouse inspection from a largely experience-based manual activity into a more structured workflow built on patrol, sensing, image capture, warning, and review.

- It reduces repetitive manual greenhouse patrol.
- It lowers the risk of damaging delicate strawberry fruits and leaves during inspection.
- It improves the ability to detect local environmental abnormalities and crop-status changes earlier.
- It supports non-contact inspection through image-based fruit and leaf observation.
- It combines automatic patrol with human takeover, making the system practical rather than purely demonstrational.
- It creates timestamped, reviewable inspection records through sensor logging, image capture, and GUI-based visualization.

More broadly, the project proposes a new inspection model for strawberry greenhouse management: **automatic patrol as the primary process, manual review for suspicious cases, and data records to support decisions**. In this sense, the platform is not just a prototype vehicle, but a practical smart-agriculture inspection concept.

---

## Key Objectives

- **Non-Contact Crop Inspection:** Inspect strawberry fruits and leaves without direct contact, reducing the risk of bruising, contamination, or accidental damage.
- **Mobile Greenhouse Patrol:** Perform routine patrol along greenhouse aisles through automatic line-following and basic obstacle avoidance.
- **Micro-Environment Monitoring:** Measure local environmental conditions, especially temperature, and identify abnormal zones through configurable threshold logic.
- **Visual Recognition:** Use OpenCV-based methods to detect fruit maturity and leaf-color abnormalities for preliminary crop-status assessment.
- **Local Warning and Human Review:** Provide buzzer and LED alerts for abnormal conditions and support manual takeover through infrared remote control and pan-tilt camera adjustment.
- **Visualization and Traceability:** Present system status, environmental data, and inspection results through a GUI, enabling more traceable and data-supported greenhouse management.

---

## System Photos / CAD Renders

This section presents visual documentation of the platform, including real prototype photos. These figures help reviewers understand the mechanical structure, sensor placement, wiring layout, and overall system arrangement.

<p align="center">
  <img src="images/rover_1.jpg" width="700"><br>
  <em>Figure 1. Prototype side view of the greenhouse inspection platform.</em>
</p>

<p align="center">
  <img src="images/rover_2.jpg" width="700"><br>
  <em>Figure 2. Top view showing the controller board, wiring, and sensor layout.</em>
</p>

<p align="center">
  <img src="images/rover_3.jpg" width="700"><br>
  <em>Figure 3. Additional structural view of the prototype platform.</em>
</p>

---

## Table of Contents

- [Main Libraries and Dependencies](#main-libraries-and-dependencies)
- [Module-Level Libraries and Dependencies](#module-level-libraries-and-dependencies)
- [Bill of Materials (BOM)](#bill-of-materials-bom)
- [System Functional Requirements](#system-functional-requirements)
- [Extended Features](#extended-features)
- [User Workflow and Operating Modes](#user-workflow-and-operating-modes)
- [Software Architecture](#software-architecture)
- [Repository Structure and Key Classes / Modules](#repository-structure-and-key-classes--modules)
- [Build and Run](#build-and-run)
- [User Case UML / Sequence Diagram](#user-case-uml--sequence-diagram)
- [Circuit / Wiring Diagram](#circuit--wiring-diagram)
- [Data Logging, Alerts and Reporting](#data-logging-alerts-and-reporting)
- [Latency and Performance Notes](#latency-and-performance-notes)
- [Validation and Test Plan](#validation-and-test-plan)
- [Risk Assessment and Safety Features](#risk-assessment-and-safety-features)
- [Acknowledgements](#acknowledgements)
- [Authors and Contributions](#authors-and-contributions)
- [License (Third-Party Libraries)](#license-third-party-libraries)
- [Future Work](#future-work)
- [Contact Us](#contact-us)
- [Last Updated](#last-updated)

---

## Main Libraries and Dependencies

This project is implemented in C++ and relies on the following main libraries, frameworks, and system interfaces:

- **libgpiod v2**  
  Used for GPIO access and hardware control on Raspberry Pi, including motor control, buzzer output, LED indicators, line-tracking sensors, and obstacle-detection sensors.

- **Qt5 (Widgets / Core / Gui)**  
  Used to build the desktop GUI for system monitoring, status display, control interaction, camera-frame rendering, and visualization of inspection results.

- **OpenCV**  
  Used for camera capture, image preprocessing, HSV color-space conversion, fruit detection, leaf detection, color-based analysis, contour extraction, annotation drawing, preview display, and image conversion for the GUI.

- **LIRC (`lirc_client`)**  
  Used for infrared remote-control input, including IR receiver initialization, remote-code decoding, and command mapping for vehicle motion and pan-tilt control.

- **Linux i2c-dev interface**  
  Used for communication with the PCA9685 PWM controller that drives the pan-tilt gimbal servos over I2C.

- **C++ Standard Library**  
  Used throughout the project for multithreading, synchronization, callbacks, timing, containers, sorting, numeric processing, and general application logic.

- **POSIX / Linux system calls and interfaces**  
  Used for low-level system tasks such as directory creation, file-descriptor control, `epoll`, `eventfd`, `timerfd`, `read`, `write`, `close`, `ioctl`, and thread-safe event handling on Linux.

---

## Module-Level Libraries and Dependencies

### Camera / Vision Module

The camera subsystem is implemented as a dedicated C++ service (`CameraService`) for real-time image acquisition, target detection, preview display, and event-based image saving.

**Libraries and interfaces used**
- **OpenCV**  
  Used for camera input (`cv::VideoCapture`), frame preprocessing, HSV masking, morphology operations, contour extraction, fruit detection, leaf detection, annotation drawing, preview display, and image saving.
- **C++ Standard Library**  
  Used for multithreading, synchronization, callbacks, containers, timing control, sorting, and detection-result publishing.
- **POSIX / Linux system calls**  
  Used for checking and creating the local image-save directory.

### Infrared Remote Module

The infrared remote module is implemented as a dedicated C++ service (`IrRemote`) for decoding remote-control input and publishing typed commands to the internal DDS-style message bus.

**Libraries and interfaces used**
- **LIRC (`lirc_client`)**  
  Used for infrared receiver initialization, remote configuration loading, code reading, and cleanup.
- **POSIX / Linux system interfaces**  
  Used for `epoll`, `eventfd`, `read`, `write`, and `close` to implement blocking, event-driven IR listening and controlled thread shutdown.
- **C++ Standard Library**  
  Used for threading, atomic state control, callback handling, and debounce timing.
- **LocalDdsBus / VehicleTopics**  
  Used as the project’s internal DDS-style communication layer so that the IR module publishes typed motion and gimbal commands without directly controlling the hardware.

### Motor Control Module

The motor-control subsystem is implemented as a differential-drive driver based on **libgpiod v2**. It controls the left and right motors independently and generates software PWM through a dedicated worker thread.

**Libraries and interfaces used**
- **libgpiod v2**  
  Used for Raspberry Pi GPIO line access, output-line configuration, and setting motor direction and PWM pin states.
- **POSIX / Linux system interfaces**  
  Used for `epoll`, `eventfd`, `timerfd`, `read`, `write`, and `close` to implement event-driven software PWM without busy waiting.
- **C++ Standard Library**  
  Used for threading, atomic state control, mutex protection, speed clamping, fixed-size arrays, and runtime error handling.
- **IMotorDriver interface**  
  Used as an internal abstraction layer so that the motor driver implementation can be replaced or extended more easily in the future.

### Qt GUI Module

The desktop control interface is implemented with **Qt5** and provides a user-friendly panel for camera preview, vehicle motion control, gimbal control, and temperature monitoring.

**Libraries and frameworks used**
- **Qt5 Widgets**  
  Used to build the main window and interactive controls, including buttons, labels, spin boxes, layouts, and group boxes.
- **Qt5 Core**  
  Used for the signal-slot mechanism, timers, and string handling in the GUI event loop.
- **Qt5 Gui**  
  Used for image display components such as `QImage` and `QPixmap`.
- **OpenCV**  
  Used together with Qt to convert `cv::Mat` camera frames into displayable GUI images.
- **C++ Standard Library**  
  Used for time durations, strings, and general application logic inside the façade layer.
- **LocalDdsBus / VehicleTopics / SystemFacade**  
  Used as the internal coordination layer so that the GUI publishes typed commands instead of directly controlling low-level hardware.

### Gimbal Module

The pan-tilt gimbal subsystem is implemented as a dedicated servo-control service based on a **PCA9685 PWM driver over Linux I2C**. It receives typed commands through the internal DDS-style bus and converts them into pan/tilt servo movements.

**Libraries and interfaces used**
- **Linux i2c-dev interface**  
  Used for low-level communication with the PCA9685 servo controller through `/dev/i2c-1`.
- **POSIX / Linux system interfaces**  
  Used for `open`, `read`, `write`, `close`, `ioctl`, and `usleep` during I2C device access and PWM-controller configuration.
- **C++ Standard Library**  
  Used for mutex-based synchronization, I2C payload construction, numeric calculations, state management, and runtime error handling.
- **LocalDdsBus / VehicleTopics**  
  Used as the internal DDS-style communication layer so that gimbal commands from the GUI and IR remote module can be handled in a unified way.

**Hardware notes**
- **PWM controller:** PCA9685  
- **I2C device:** `/dev/i2c-1`  
- **Default I2C address:** `0x40`  
- **Servo channels:** tilt = channel 0, pan = channel 1

---

## Bill of Materials (BOM)

This section lists the main hardware modules used to implement the non-contact strawberry greenhouse inspection platform, including mobility, sensing, warning, visual inspection, and operator-control functions.

#### **Raspberry Pi 4 Model B (4GB recommended)**

Acts as the main controller for sensing, image capture, logging, GUI interaction, and higher-level task coordination.

#### **Motor Expansion Board**
[Schematic PDF](schematics/motor_expansion_board_schematic_v3.pdf)

Provides motor-driving capability for platform movement and low-level motion control.

#### **Sensor Kit**

Includes environmental sensing and auxiliary modules used for routine inspection tasks.

#### **Analog Temperature Sensor Module**

Used for local temperature measurement and threshold-based monitoring.

#### **PCF8591 ADC/DAC Module**

Provides analog-to-digital conversion for analog sensor input and enables flexible sensor integration.

#### **Active Buzzer**

Used for audible alerts during abnormal conditions or fault events.

#### **Joystick Module**

Supports manual interaction and debugging during teleoperation or bench testing.

#### **RGB LED Module**

Provides simple visual state indication such as normal, warning, or fault status.

#### **T-Type GPIO Expansion Board (T-Cobbler)**

Simplifies prototyping and wiring between the Raspberry Pi and peripheral modules.

#### **IR Line Tracking Module**

Supports basic line-following behavior for structured patrol paths.

#### **IR Receiver + IR Remote Controller**

Enables remote manual control for targeted inspection and testing.

#### **USB Camera + Pan-Tilt Servo Gimbal**

Provides visual inspection capability and adjustable camera orientation for targeted observation. Basic color-based visual analysis is used for fruit and leaf inspection tasks.

#### **Breadboard**

Used for temporary wiring, testing, and modular integration during development.

#### **Voltmeter Module**

Used for battery or power monitoring to support low-voltage warning and safer operation.

---

## System Functional Requirements

The platform shall provide functional capabilities that support non-contact strawberry inspection, local environmental monitoring, abnormal-condition warning, and practical operator review in greenhouse patrol scenarios.

### Environmental Sensing
- The system shall measure temperature, humidity, and light intensity at configurable intervals (time-based or distance-based).
- The system shall support sensor calibration offsets and basic filtering (e.g., moving average) to reduce noise.
- The system shall detect invalid readings (out-of-range or disconnected sensor) and log a fault event.

### Visual Inspection (Camera)
- The system shall capture still images of plants at configurable intervals and store them with timestamps.
- The system shall support event-triggered image capture, for example when an environmental parameter exceeds a threshold.
- The system shall allow the operator to manually trigger image capture during teleoperation.
- The system shall support basic color-based recognition for fruit maturity and leaf abnormality screening.

### Patrol and Movement
- The system shall provide a manual control mode (teleoperation) for testing and targeted inspection.
- The system shall provide a patrol mode that moves along greenhouse aisles and stops at sampling points.
- The system shall implement basic obstacle handling: stop, wait, and/or reroute depending on available sensors.
- The system shall support line-following behavior for structured greenhouse patrol paths.

### Data Logging and Reporting
- The system shall log sensor readings and mission metadata into a local file or database (e.g., CSV or SQLite).
- The system shall store captured images with consistent naming (timestamp plus optional location tag).
- The system shall generate a mission summary including min/avg/max values, alert counts, and image references.
- The system shall support traceable review of inspection records through logged data and visual outputs.

### Alerts and Thresholds
- The system shall support configurable threshold-based alerts for temperature, humidity, and light.
- The system shall support trend-based alerts (e.g., temperature rising continuously for N minutes).
- The system shall record all alerts with timestamps and associated sensor values.
- The system shall trigger audible and/or visual warning signals when abnormal conditions are detected.

### Safety and Fail-Safe Behavior
- The system shall include an emergency stop function (hardware button or software command).
- The platform shall automatically stop the motors when critical faults occur (sensor failure, low battery, or loss of control signal).
- The system shall monitor battery voltage and provide a low-battery warning and safe stop/return behavior.

### Usability
- The system shall provide a simple workflow: start mission → monitor status → review logs/images → export report.
- The system shall provide clear status indication (LED, buzzer, or on-screen messages) for states such as running, alert, and error.
- The system shall support GUI-based monitoring and operator takeover for suspicious cases.

---

## Extended Features

To make the platform more practical and closer to a complete smart-agriculture system, the following extended features are proposed. These features are modular and can be implemented in phases.

### Multi-Point Patrol and Location Tagging
- Route-based patrol missions: define aisles and checkpoints, and stop automatically for sampling.
- Distance/time-triggered sampling: record readings every N seconds or every M meters.
- Location tagging: associate each record with an aisle ID (e.g., A1–A10) and checkpoint ID.

### Smart Alerts and Event Handling
- Threshold alerts: configurable upper and lower limits for temperature, humidity, and light.
- Trend alerts: detect continuous rise or fall over a time window to provide earlier warning.
- Event actions: when an alert occurs, the platform can pause, take close-up photos, and mark an event in the log.

### Plant Growth Tracking with Vision (Optional)
- Scheduled photo capture at fixed locations for long-term comparison.
- Simple visual metrics (rule-based):
  - greenness index / color shift (early yellowing detection)
  - estimated leaf area change
  - suspicious spot / high-contrast area detection for disease hints
- Before-and-after comparison: “today vs. last week” quick review at the same checkpoint.

### Closed-Loop Environmental Actuation (Optional)
Add relay-controlled devices to form a basic closed-loop system:
- fan / ventilation
- humidifier (mist)
- supplemental lighting

Example policy rules:
- humidity below threshold → mist for 10 s → wait 2 min → re-check
- light below threshold during daytime → enable LED strip for X minutes

### Remote Dashboard and Operator Interface
- Local web dashboard hosted on the Pi:
  - real-time sensor values, alert list, mission status
  - latest captured images (gallery)
  - export CSV/report button
- Mobile-friendly control panel:
  - start/stop patrol, return home, manual capture, emergency stop

### Reliability and Safety Enhancements
- Battery monitoring with low-battery warning and safe return/stop.
- Watchdog and fail-safe stop if the control loop hangs or communication is lost.
- Obstacle avoidance using ultrasonic or ToF sensors, with stop or reroute logic.

### Deployment Roadmap
Because practical agricultural robotic impact is usually phased rather than immediate, the platform’s development path is also designed as a gradual roadmap:
- **Phase 1:** manual control + sensing + basic logging
- **Phase 2:** structured patrol + event-triggered capture + threshold alerts
- **Phase 3:** GUI/dashboard + multi-point patrol + richer reporting
- **Phase 4:** optional closed-loop environmental response and higher-level vision analysis

This phased approach reflects the idea that useful deployment should begin with structured, low-risk, and economically clear inspection tasks before moving toward more complex autonomy.

---

## User Workflow and Operating Modes

This section describes how an operator interacts with the platform and how the system behaves under different operating modes. The design focuses on a simple workflow with clear state transitions and safe stopping behavior.

### Typical User Workflow
1. **Power On and Setup:** The operator powers on the platform, confirms battery level, and checks that the sensors and camera are detected.
2. **Select Mode:** Choose one of the operating modes: Manual Inspection, Patrol Mission, or Data Review.
3. **Start Mission:** The platform begins moving (or waits for teleoperation commands). Sensor sampling and logging start automatically.
4. **Monitoring During Operation:** The operator observes live sensor values, GUI status, and camera preview (optional). Alerts are displayed if thresholds or trends are exceeded.
5. **Event Handling:** If any alert occurs, the platform pauses (optional), captures extra close-up images, and records an event marker in the log.
6. **Manual Review / Takeover:** If needed, the operator takes over via infrared remote control and adjusts the pan-tilt camera for closer inspection.
7. **Mission End:** The platform stops at the end of the route or returns to the start point (optional).
8. **Review and Export:** The operator reviews the mission summary, images, and exports CSV/report files for greenhouse management records.

### Operating Modes
- **Mode A — Manual Inspection (Teleoperation)**  
  Purpose: debugging, targeted inspection, and close-up image capture.  
  Features enabled: manual drive, manual photo capture, pan-tilt review, live sensor readout.

- **Mode B — Patrol Mission (Semi-Auto)**  
  Purpose: routine scheduled patrol along greenhouse aisles.  
  Features enabled: autonomous sampling, event-triggered capture, alert generation, summary report.

- **Mode C — Stationary Monitoring (Optional)**  
  Purpose: use the platform as a temporary monitoring station.  
  Features enabled: high-frequency sensing, trend alerts, periodic photo capture.

- **Mode D — Data Review / Maintenance**  
  Purpose: maintenance and dataset management.  
  Functions: view logs/images, clean storage, calibration offsets, sensor diagnostics.

**Recommended State Machine**  
Idle → Self-check → Manual / Patrol / Stationary → Alert Handling → Manual Review / Return / Stop → Report

Safety rule: any critical fault triggers Safe Stop and requires operator confirmation before resuming.

---

## Software Architecture

The platform software is designed as a modular system so that sensing, movement, vision, logging, alert handling, and GUI visualization can evolve together without becoming tightly coupled. This reflects the central engineering idea behind the project: practical capability depends not only on the existence of modules, but on whether they can coordinate reliably in real inspection tasks.

### High-Level Architecture

Core modules:
- **Mission Manager (State Machine / FSM):** Controls overall system states and mission flow (Idle → Self-check → Patrol → Alert Handling → Review → Return → Report).
- **Sensor Manager:** Reads temperature, humidity, light, and optional sensors at a fixed rate, applies filtering, and publishes data.
- **Vision Module:** Handles camera capture, file naming, storage, and OpenCV-based color recognition for fruit/leaf inspection.
- **Motion Control Module:** Provides low-level motor control and high-level motion primitives (forward, stop, turn).
- **Navigation Module:** Implements patrol behavior (checkpoints, line-following, obstacle stop/avoid, return-to-home).
- **Alert Manager:** Evaluates thresholds/trends and triggers alerts and event actions (pause + extra photos + local warning).
- **Data Logger:** Writes sensor data, events, and mission metadata into CSV/SQLite and manages image indexing.
- **UI / GUI Dashboard:** Desktop interface for monitoring system status, reviewing results, and supporting operator takeover.
- **Infrared Remote Module:** Receives and decodes IR remote-control input through LIRC, applies debounce logic, and publishes motion/gimbal commands to the internal DDS-style bus.
- **Gimbal Module:** Controls the pan-tilt servos through PCA9685 over Linux I2C and executes typed gimbal commands from the internal bus.

### Data Flow
- Sensors produce periodic readings → Sensor Manager
- Readings are filtered and timestamped → sent to Logger + Alert Manager
- Alert Manager may trigger event actions (pause + warning + photo) via Mission Manager
- Vision Module stores images and returns file references → stored by Logger
- GUI reads current status and logged outputs for visualization and operator review
- At mission end, the Report Generator summarizes statistics and exports outputs

### Recommended Concurrency Model
- Thread/Task 1: Sensor sampling (typically 1–5 Hz)
- Thread/Task 2: Motion and navigation loop (50–100 Hz control loop; obstacle checks 5–20 Hz)
- Thread/Task 3: Camera capture and OpenCV processing (periodic or event-driven)
- Thread/Task 4: GUI / communication (remote control, status updates, result visualization)

This separation prevents camera or disk I/O from blocking motor safety control.

### Key Design Principles
- **Loose coupling:** Modules communicate through well-defined interfaces or message queues.
- **Fail-safe first:** Any critical fault triggers Safe Stop in the Mission Manager.
- **Configuration-driven:** Thresholds, sampling rates, and route checkpoints are stored in config files (YAML/JSON).
- **Testability:** Sensor stubs and simulated inputs enable unit testing without hardware.
- **Practical inspectability:** The system should support real operator review, not only automatic execution.
- **Phased deployability:** Structured, low-risk functions are prioritized before more complex autonomous behaviors.

---

## Repository Structure and Key Classes / Modules

The repository is organized to separate documentation assets, hardware references, and implementation code.

- `images/` — project images, logo, platform photos, and visual illustrations used in the README
- `include/` — header files and interface definitions for C/C++ modules
- `schematics/` — wiring diagrams, module schematics, and hardware reference files
- `src/` — main source code for sensing, motion, patrol logic, logging, GUI control, and inspection functions
- `tests/` — test programs or validation scripts for individual modules
- `CMakeLists.txt` — project build configuration
- `README.md` — project overview, documentation, and system description

Recommended logical module grouping inside `src/` / `include/`:
- sensing module
- motion control module
- patrol/navigation module
- alert and event module
- camera / image capture module
- OpenCV inspection module
- infrared remote module
- gimbal control module
- data logging module
- GUI / remote-control module

This structure supports modular development and makes it easier to extend the platform from a prototype into a more complete greenhouse inspection system.

---
## Build and Run

This project uses **CMake** as the build system.

### Build Commands

```bash
cd smartcar
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```
---

## User Case UML / Sequence Diagram

This section describes the main user cases and the corresponding sequence of interactions between the operator and the platform subsystems. The diagrams clarify responsibilities across sensing, motion, vision, logging, alerts, and manual review.

### Main User Cases

- **Start Patrol Mission**  
  Actor: Operator  
  Goal: Start an inspection mission and collect environmental data and images automatically.

- **Manual Inspection and Close-Up Review**  
  Actor: Operator  
  Goal: Teleoperate the platform to a target plant and inspect fruit or leaves without direct contact.

- **Alert Event Handling**  
  Actor: Platform (system) + Operator  
  Goal: Detect abnormal environmental or visual conditions, capture evidence, and notify the operator.

- **End Mission and Export Report**  
  Actor: Operator  
  Goal: Stop the mission, review results, and export logs and images for record keeping.

### Sequence Diagram — Patrol Mission (UC-1)

Participants: Operator, GUI, MissionManager, SensorManager, AlertManager, VisionModule, DataLogger, MotorControl, ObstacleSensor (optional)

Sequence:
- Operator → GUI: Select Patrol Mode and press Start.
- GUI → MissionManager: `startMission(patrol)`
- MissionManager → SelfCheck: battery / sensor / camera status verification
- MissionManager → MotorControl: `beginPatrol()`
- Loop (while patrol is running):
  - SensorManager → DataLogger: `log(sensor_readings, timestamp, location)`
  - SensorManager → AlertManager: `evaluate(readings)`
  - VisionModule → DataLogger: `log(image_path, timestamp, location)`
  - AlertManager → MissionManager: if abnormal → `triggerEvent(alertType)`
  - MissionManager → VisionModule: `captureImage(eventTag)`
  - ObstacleSensor → Navigation / MotorControl: if obstacle → `stop()` / `wait()` / `reroute()`
- Operator → GUI: Review alert / take over manually if necessary
- Operator → GUI: Press Stop (or route ends)
- GUI → MissionManager: `stopMission()`
- MissionManager → DataLogger: `finalizeMission()`
- DataLogger → ReportGenerator: `generateSummary()`
- GUI → Operator: Display summary and provide export

---

## Circuit / Wiring Diagram

This section summarizes how sensing, control, actuation, warning modules, and camera interfaces are wired to the Raspberry Pi and motor-control hardware.

<p align="center">
  <img src="images/gpio_wiring_table.png" width="900"><br>
  <em>Figure 4. GPIO wiring table: hardware-to-Raspberry Pi GPIO mapping used in the current prototype.</em>
</p>

### Wiring Notes

- **Buzzer** → GPIO17
- **Red LED** → GPIO20
- **Green LED** → GPIO21
- **Left Motor PWMA** → GPIO18
- **Left Motor AIN1** → GPIO22
- **Left Motor AIN2** → GPIO27
- **Right Motor PWMB** → GPIO13
- **Right Motor BIN1** → GPIO25
- **Right Motor BIN2** → GPIO24
- **Left Line Tracking Sensor** → GPIO23
- **Right Line Tracking Sensor** → GPIO26
- **Left Obstacle Sensor** → GPIO16
- **Right Obstacle Sensor** → GPIO12

---