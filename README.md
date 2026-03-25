# smart-car
## Greenhouse Inspection Rover — “Measure. Monitor. Grow”

<p align="center">
  <img src="images/logo.jpg" width="600">
</p>

---

## Overview

The **Greenhouse Inspection Rover** is a non-contact intelligent inspection and micro-environment intervention platform designed for **strawberry greenhouse** scenarios. It integrates mobility, environmental sensing, visual inspection, local alerting, desktop visualization, and operator takeover into a single rover system.

The rover is intended to support continuous monitoring of strawberry maturity, leaf health, and local environmental abnormalities inside greenhouse environments. By reducing repetitive manual inspection, lowering the risk of contact damage to delicate crops, and providing more traceable data for decision-making, the system aims to make greenhouse inspection more structured, efficient, and practical.

Rather than presenting the rover as a fully autonomous replacement for greenhouse labour, this project explores how a low-cost robotic platform can restructure routine inspection into a more repeatable, traceable, and data-supported workflow.

## Project Story

Strawberry cultivation is highly sensitive to environmental conditions and crop status. In greenhouse production, changes in temperature, light, humidity, and plant health can directly affect fruit quality, ripening progress, and final yield. However, traditional inspection still relies heavily on manual patrol. Growers or operators must walk through greenhouse aisles, check fruit maturity, observe whether leaves show yellowing or dark spots, and identify local environmental abnormalities based on experience.

This traditional approach has several limitations. First, it is labour-intensive and time-consuming. Second, manual inspection is often difficult to standardize and depends strongly on individual experience. Third, strawberries are delicate crops: close inspection, touching, or moving leaves and fruits can easily cause bruising, contamination, or accidental damage, which directly affects product quality.

The **Greenhouse Inspection Rover** was developed to address these practical problems in a strawberry greenhouse context. Rather than being just a smart car with sensors, it is intended as an integrated inspection platform. The rover combines **automatic line-following patrol and obstacle avoidance** for routine movement, **temperature sensing and threshold-based feedback** for local environmental judgment, **OpenCV-based fruit and leaf inspection** for non-contact crop observation, **buzzer and light alerts** for local warning, **GUI-based desktop visualization** for system monitoring, and **infrared remote control with pan-tilt adjustment** for manual takeover and close-up review.

This design reflects a key observation: greenhouse problems are often **local rather than uniform**. One area may be slightly hotter, another may receive insufficient light, a few plants may show leaf abnormalities, and fruit in one row may ripen faster than those in another. Fixed observation and occasional manual checking are often not detailed or continuous enough to capture these differences. A mobile rover, by contrast, can move across the greenhouse, observe local variations, and generate image and sensor records with greater consistency.

From this perspective, the value of the project lies not in any single function, but in how multiple functions work together as one inspection workflow. The rover is not only a moving platform, but a prototype system that connects **mobile inspection, environmental sensing, visual recognition, abnormal warning, human review, and data visualization**. Its purpose is to reduce repetitive manual work, lower contact-related crop damage, and support earlier and more data-supported intervention in greenhouse management.

In short, this project asks a practical question: **Can a lightweight, mobile, and expandable platform replace part of the traditional high-frequency manual inspection process without increasing crop damage risk?** The Greenhouse Inspection Rover is our answer to that question.

**Measure. Monitor. Grow.**

## Why It Matters

The significance of the **Greenhouse Inspection Rover** lies in its ability to turn greenhouse inspection from a largely experience-based manual activity into a more structured workflow built on patrol, sensing, image capture, warning, and review.

- It reduces repetitive manual greenhouse patrol.
- It lowers the risk of damaging delicate strawberry fruits and leaves during inspection.
- It improves the ability to detect local environmental abnormalities and crop-status changes earlier.
- It supports non-contact inspection through image-based fruit and leaf observation.
- It combines automatic patrol with human takeover, making the system practical rather than purely demonstrational.
- It creates timestamped, reviewable inspection records through sensor logging, image capture, and GUI-based visualization.

More broadly, the project proposes a new inspection model for strawberry greenhouse management: **automatic patrol as the main process, manual review for suspicious cases, and data records to support decisions**. In this sense, the rover is not just a prototype vehicle, but a practical smart-agriculture inspection concept.

---

## Key Objectives

- **Non-Contact Crop Inspection:** Inspect strawberry fruits and leaves without direct contact, reducing the risk of bruising, contamination, or accidental damage.
- **Mobile Greenhouse Patrol:** Perform routine patrol along greenhouse aisles through automatic line-following and basic obstacle avoidance.
- **Micro-Environment Monitoring:** Measure local environmental conditions, especially temperature, and identify abnormal zones through configurable threshold logic.
- **Visual Recognition:** Use OpenCV-based methods to detect fruit maturity and leaf-colour abnormalities for preliminary crop-status assessment.
- **Local Warning and Human Review:** Provide buzzer/light alerts for abnormal conditions and support manual takeover through infrared remote control and pan-tilt camera adjustment.
- **Visualization and Traceability:** Present system status, environmental data, and inspection results through a GUI, enabling more traceable and data-supported greenhouse management.

---

## System Photos / CAD Renders

This section presents visual documentation of the rover, including real prototype photos. These figures help reviewers understand the mechanical structure, sensor placement, wiring layout, and overall system arrangement.

<p align="center">
  <img src="images/rover_1.jpg" width="700"><br>
  <em>Figure 1. Rover Prototype (Photo): Side view of the greenhouse inspection rover.</em>
</p>

<p align="center">
  <img src="images/rover_2.jpg" width="700"><br>
  <em>Figure 2. Rover Top View (Photo): Top view showing the controller board, wiring, and sensor layout.</em>
</p>

<p align="center">
  <img src="images/rover_3.jpg" width="700"><br>
  <em>Figure 3. Rover Additional View (Photo): Another angle of the rover showing structural details.</em>
</p>

---

## Table of Contents

- [Bill of Materials (BOM)](#bill-of-materials-bom)
- [System Functional Requirements](#system-functional-requirements)
- [Extended Features](#extended-features)
- [User Workflow and Operating Modes](#user-workflow-and-operating-modes)
- [Software Architecture](#software-architecture)
- [Repository Structure and Key Classes / Modules](#repository-structure-and-key-classes--modules)
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

## Bill of Materials (BOM)

This section lists the main hardware modules used to implement the non-contact strawberry greenhouse inspection platform, including mobility, sensing, warning, visual inspection, and operator-control functions.

#### **Raspberry Pi 4 Model B (4GB recommended)**

Acts as the main controller for sensing, image capture, logging, GUI interaction, and higher-level task coordination.

#### **Motor Expansion Board**
[Schematic PDF](schematics/motor_expansion_board_schematic_v3.pdf)

Provides motor driving capability for rover movement and low-level motion control.

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

Supports basic line-following behaviour for structured patrol paths.

#### **IR Receiver + IR Remote Controller**

Enables remote manual control for targeted inspection and testing.

#### **USB Camera + Pan-Tilt Servo Gimbal**

Provides visual inspection capability and adjustable camera orientation for targeted observation. Basic colour-based visual analysis is used for fruit and leaf inspection tasks.

#### **Breadboard**

Used for temporary wiring, testing, and modular integration during development.

#### **Voltmeter Module**

Used for battery or power monitoring to support low-voltage warning and safer operation.

---

## System Functional Requirements

The greenhouse inspection rover shall provide functional capabilities that support non-contact strawberry inspection, local environmental monitoring, abnormal-condition warning, and practical operator review in greenhouse patrol scenarios.

### Environmental Sensing
- The system shall measure temperature, humidity, and light intensity at configurable intervals (time-based or distance-based).
- The system shall support sensor calibration offsets and basic filtering (e.g., moving average) to reduce noise.
- The system shall detect invalid readings (out-of-range / disconnected sensor) and log a fault event.

### Visual Inspection (Camera)
- The system shall capture still images of plants at configurable intervals and store them with timestamps.
- The system shall support event-triggered image capture, for example when an environmental parameter exceeds a threshold.
- The system shall allow the operator to manually trigger image capture during teleoperation.
- The system shall support basic colour-based recognition for fruit maturity and leaf abnormality screening.

### Patrol and Movement
- The system shall provide a manual control mode (teleoperation) for testing and targeted inspection.
- The system shall provide a patrol mode that moves along greenhouse aisles and stops at sampling points.
- The system shall implement basic obstacle handling: stop, wait, and/or reroute depending on available sensors.
- The system shall support line-following behaviour for structured greenhouse patrol paths.

### Data Logging and Reporting
- The system shall log sensor readings and mission metadata into a local file or database (e.g., CSV/SQLite).
- The system shall store captured images with consistent naming (timestamp + optional location tag).
- The system shall generate a mission summary including min/avg/max values, alert counts, and image references.
- The system shall support traceable review of inspection records through logged data and visual outputs.

### Alerts and Thresholds
- The system shall support configurable threshold-based alerts for temperature, humidity, and light.
- The system shall support trend-based alerts (e.g., temperature rising continuously for N minutes).
- The system shall record all alerts with timestamps and associated sensor values.
- The system shall trigger audible and/or visual warning signals when abnormal conditions are detected.

### Safety and Fail-Safe Behavior
- The system shall include an emergency stop function (hardware button or software command).
- The rover shall automatically stop the motors when critical faults occur (sensor failure, low battery, or loss of control signal).
- The system shall monitor battery voltage and provide a low-battery warning and safe stop/return behaviour.

### Usability
- The system shall provide a simple workflow: start mission → monitor status → review logs/images → export report.
- The system shall provide clear status indication (LED/buzzer/on-screen messages) for states such as running, alert, and error.
- The system shall support GUI-based monitoring and operator takeover for suspicious cases.

---

## Extended Features

To make the greenhouse inspection rover more practical and closer to a complete smart-agriculture system, the following extended features are proposed. These features are modular and can be implemented in phases.

### Multi-Point Patrol & Location Tagging
- Route-based patrol missions: define aisles and checkpoints, and stop automatically for sampling.
- Distance/time-triggered sampling: record readings every N seconds or every M metres.
- Location tagging: associate each record with an aisle ID (e.g., A1–A10) and checkpoint ID.

### Smart Alerts and Event Handling
- Threshold alerts: configurable upper/lower limits for temperature, humidity, and light.
- Trend alerts: detect continuous rise/fall over a time window to provide earlier warning.
- Event actions: when an alert occurs, the rover can pause, take close-up photos, and mark an event in the log.

### Plant Growth Tracking with Vision (Optional)
- Scheduled photo capture at fixed locations for long-term comparison.
- Simple visual metrics (rule-based):
  - greenness index / colour shift (early yellowing detection)
  - estimated leaf area change
  - suspicious spot / high-contrast area detection for disease hints
- Before–after comparison: “today vs. last week” quick review at the same checkpoint.

### Closed-Loop Environmental Actuation (Optional)
Add relay-controlled devices to form a basic closed-loop system:
- fan / ventilation
- humidifier (mist)
- supplemental lighting

Example policy table (if–then rules):
- humidity below threshold → mist for 10 s → wait 2 min → re-check
- light below threshold during daytime → enable LED strip for X minutes

### Remote Dashboard & Operator Interface
- Local web dashboard hosted on the Pi:
  - real-time sensor values, alert list, mission status
  - latest captured images (gallery)
  - export CSV/report button
- Mobile-friendly control panel:
  - start/stop patrol, return home, manual capture, emergency stop

### Reliability and Safety Enhancements
- Battery monitoring with low-battery warning and safe return/stop.
- Watchdog and fail-safe stop if the control loop hangs or communication is lost.
- Obstacle avoidance using ultrasonic/ToF sensors, with stop or reroute logic.

### Deployment Roadmap
Because practical agricultural robotic impact is usually phased rather than immediate, the rover’s development path is also designed as a gradual roadmap:
- **Phase 1:** manual control + sensing + basic logging
- **Phase 2:** structured patrol + event-triggered capture + threshold alerts
- **Phase 3:** GUI/dashboard + multi-point patrol + richer reporting
- **Phase 4:** optional closed-loop environmental response and higher-level vision analysis

This phased approach reflects the idea that useful deployment should begin with structured, low-risk, and economically clear inspection tasks before moving toward more complex autonomy.

---

## User Workflow and Operating Modes

This section describes how an operator interacts with the greenhouse inspection rover and how the rover behaves under different operating modes. The design focuses on a simple workflow with clear state transitions and safe stopping behaviour.

### Typical User Workflow
1. **Power On & Setup:** The operator powers on the rover, confirms battery level, and checks that the sensors and camera are detected.
2. **Select Mode:** Choose one of the operating modes: Manual Inspection, Patrol Mission, or Data Review.
3. **Start Mission:** The rover begins moving (or waits for teleoperation commands). Sensor sampling and logging start automatically.
4. **Monitoring During Operation:** The operator observes live sensor values, GUI status, and camera preview (optional). Alerts are displayed if thresholds or trends are exceeded.
5. **Event Handling (If Any Alert Occurs):** The rover pauses (optional), captures extra close-up images, and records an event marker in the log.
6. **Manual Review / Takeover:** If needed, the operator takes over via infrared remote control and adjusts the pan-tilt camera for closer inspection.
7. **Mission End:** The rover stops at the end of the route or returns to the start point (optional).
8. **Review & Export:** The operator reviews the mission summary, images, and exports CSV/report files for greenhouse management records.

### Operating Modes
- **Mode A — Manual Inspection (Teleoperation)**  
  Purpose: debugging, targeted inspection, and close-up image capture.  
  Features enabled: manual drive, manual photo capture, pan-tilt review, live sensor readout.

- **Mode B — Patrol Mission (Semi/Auto)**  
  Purpose: routine scheduled patrol along greenhouse aisles.  
  Features enabled: autonomous sampling, event-triggered capture, alert generation, summary report.

- **Mode C — Stationary Monitoring (Optional)**  
  Purpose: use the rover as a temporary monitoring station.  
  Features enabled: high-frequency sensing, trend alerts, periodic photo capture.

- **Mode D — Data Review / Maintenance**  
  Purpose: maintenance and dataset management.  
  Functions: view logs/images, clean storage, calibration offsets, sensor diagnostics.

**State Machine (Recommended):**  
Idle → Self-check → Manual / Patrol / Stationary → Alert-handling → Manual Review / Return / Stop → Report

Safety rule: any critical fault triggers Safe Stop and requires operator confirmation before resuming.

---

## Software Architecture

The greenhouse inspection rover software is designed as a modular system so that sensing, movement, vision, logging, alert handling, and GUI visualization can evolve together without becoming tightly coupled. This reflects the central engineering idea behind the project: practical capability depends not only on the existence of modules, but on whether they can coordinate reliably in real inspection tasks.

### High-Level Architecture
Core modules:
- **Mission Manager (State Machine / FSM):** Controls overall rover states and mission flow (Idle → Self-check → Patrol → Alert-handling → Review → Return → Report).
- **Sensor Manager:** Reads temperature/humidity/light (and optional sensors) at a fixed rate, applies filtering, and publishes data.
- **Vision Module:** Handles camera capture, file naming, storage, and OpenCV-based colour recognition for fruit/leaf inspection.
- **Motion Control Module:** Provides low-level motor control and high-level motion primitives (forward/stop/turn).
- **Navigation Module:** Implements patrol behaviour (checkpoints, line-following, obstacle stop/avoid, return-to-home).
- **Alert Manager:** Evaluates thresholds/trends and triggers alerts and event actions (pause + extra photos + local warning).
- **Data Logger:** Writes sensor data, events, and mission metadata into CSV/SQLite; manages image indexing.
- **UI / GUI Dashboard:** Desktop interface for monitoring system status, reviewing results, and supporting operator takeover.

### Data Flow
- Sensors produce periodic readings → Sensor Manager
- Readings are filtered and timestamped → sent to Logger + Alert Manager
- Alert Manager may trigger event actions (pause + warning + photo) via Mission Manager
- Vision Module stores images and returns file references → stored by Logger
- GUI reads current status and logged outputs for visualization and operator review
- At mission end, the Report Generator summarizes statistics and exports outputs

### Concurrency Model (Recommended)
- Thread/Task 1: Sensor sampling (typically 1–5 Hz)
- Thread/Task 2: Motion & navigation loop (50–100 Hz control loop; obstacle checks 5–20 Hz)
- Thread/Task 3: Camera capture and OpenCV processing (periodic or event-driven)
- Thread/Task 4: GUI / communication (remote control, status updates, result visualization)

This separation prevents camera or disk I/O from blocking motor safety control.

### Key Design Principles
- **Loose coupling:** Modules communicate through well-defined interfaces or message queues.
- **Fail-safe first:** Any critical fault triggers Safe Stop in the Mission Manager.
- **Configuration-driven:** Thresholds, sampling rates, and route checkpoints are stored in config files (YAML/JSON).
- **Testability:** Sensor stubs and simulated inputs enable unit testing without hardware.
- **Practical inspectability:** The system should support real operator review, not only automatic execution.
- **Phased deployability:** Structured, low-risk functions are prioritised before more complex autonomous behaviours.

---

## Repository Structure and Key Classes / Modules

The repository is organised to separate documentation assets, hardware references, and implementation code.

- `images/` — project images, logo, rover photos, and visual illustrations used in the README
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
- data logging module
- GUI / remote-control module

This structure supports modular development and makes it easier to extend the rover from a prototype into a more complete greenhouse inspection platform.

---

## User Case UML / Sequence Diagram

This section describes the main user cases and the corresponding sequence of interactions between the operator and the rover subsystems. The diagrams clarify responsibilities across sensing, motion, vision, logging, alerts, and manual review.

### Main User Cases (Use Case List)
- **Start Patrol Mission**  
  Actor: Operator  
  Goal: Start an inspection mission and collect environmental data/images automatically.

- **Manual Inspection & Close-Up Review**  
  Actor: Operator  
  Goal: Teleoperate the rover to a target plant and inspect fruit or leaves without direct contact.

- **Alert Event Handling**  
  Actor: Rover (system) + Operator  
  Goal: Detect abnormal environmental or visual conditions, capture evidence, and notify the operator.

- **End Mission & Export Report**  
  Actor: Operator  
  Goal: Stop the mission, review results, and export logs/images for record keeping.

### Sequence Diagram — Patrol Mission (UC-1)
Participants: Operator, GUI, MissionManager, SensorManager, AlertManager, VisionModule, DataLogger, MotorControl, ObstacleSensor (optional)

Sequence (text form):
- Operator → GUI: Select Patrol Mode and press Start.
- GUI → MissionManager: `startMission(patrol)`
- MissionManager → SelfCheck: battery/sensor/camera status verification
- MissionManager → MotorControl: `beginPatrol()`
- Loop (while patrol is running):
  - SensorManager → DataLogger: `log(sensor_readings, timestamp, location)`
  - SensorManager → AlertManager: `evaluate(readings)`
  - VisionModule → DataLogger: `log(image_path, timestamp, location)`
  - AlertManager → MissionManager: if abnormal → `triggerEvent(alertType)`
  - MissionManager → VisionModule: `captureImage(eventTag)`
  - ObstacleSensor → Navigation/MotorControl: if obstacle → `stop()` / `wait()` / `reroute()`
- Operator → GUI: Review alert / take over manually if necessary
- Operator → GUI: Press Stop (or route ends).
- GUI → MissionManager: `stopMission()`
- MissionManager → DataLogger: `finalizeMission()`
- DataLogger → ReportGenerator: `generateSummary()`
- GUI → Operator: Display summary + provide export.

_TODO: Insert UML sequence diagram figure (Mermaid or image)._

---

## Circuit / Wiring Diagram

This section should summarize how sensing, control, actuation, warning modules, and camera interfaces are wired to the Raspberry Pi and motor-control hardware.

Recommended contents:
- GPIO mapping table
- power distribution path
- motor driver connections
- sensor input connections (digital / analog via PCF8591)
- buzzer / LED status outputs
- camera and pan-tilt servo connection overview
- IR receiver / remote-control interface

Current reference schematics are provided in the `schematics/` folder. A future revision of this README can include a consolidated system-level wiring figure for faster reproduction.

---

## Data Logging, Alerts and Reporting

The rover is intended to support traceable inspection rather than one-time observation. For that reason, logging and reporting are treated as core system functions.

### Suggested Logged Data
- timestamp
- mission ID
- optional checkpoint / aisle ID
- temperature / humidity / light readings
- alert type and threshold status
- fruit / leaf inspection result (if available)
- image filename or path
- battery level / low-voltage event
- operator mode and mission state

### Suggested Storage Formats
- **CSV:** simple, portable, and easy to inspect manually
- **SQLite:** better for larger missions, structured queries, and long-term record management

### Alert Records
Each alert entry should include:
- timestamp
- alert category
- measured value
- threshold or trend condition
- rover state at the time of alert
- whether an extra image was captured
- whether manual review was triggered

### Report Output
A mission report may include:
- mission duration
- number of checkpoints visited
- min/avg/max values for each sensed parameter
- number and type of alerts
- list of captured images
- brief inspection notes if supported
- summary of suspicious fruit/leaf observations

This reporting structure helps turn patrol activity into a reviewable greenhouse management record.

---

## Latency and Performance Notes

The rover is not intended for high-speed autonomous navigation. Its performance target is stable inspection rather than aggressive motion.

Recommended operating assumptions:
- sensor sampling: 1–5 Hz
- obstacle checking: 5–20 Hz
- motion control loop: 50–100 Hz
- image capture: periodic or event-triggered
- GUI update: lightweight, non-blocking
- logging: asynchronous where possible to avoid blocking motion control

Design considerations:
- motor safety control should always have higher priority than image storage
- camera and file I/O should not freeze the patrol loop
- alert detection should be lightweight enough to run during normal inspection
- GUI refresh should not interfere with sensing or motion control
- sampling frequency should be matched to rover speed and greenhouse layout

The practical target is not maximum computational throughput, but stable, low-risk patrol performance under repeated greenhouse operation.

---

## Validation and Test Plan

The rover should be validated in stages, from individual modules to full patrol missions.

### Module-Level Tests
- sensor reading accuracy and repeatability
- motor forward/stop/turn behaviour
- line-tracking response
- buzzer / LED alert output
- camera capture and file saving
- GUI status display
- battery-voltage monitoring
- fruit / leaf colour-recognition baseline performance

### Integration Tests
- simultaneous sensing + movement + logging
- event-triggered image capture during patrol
- remote manual override during semi-auto operation
- alert generation under threshold violations
- GUI-based monitoring during patrol
- safe stop behaviour under simulated faults

### Scenario Tests
- routine greenhouse patrol along a fixed route
- checkpoint-based data capture
- manual close-up inspection of a selected plant
- abnormal-condition handling (e.g., sensor threshold exceeded)
- suspicious leaf / fruit detection workflow
- low-battery warning and controlled stop

### Success Criteria
- data and image timestamps are recorded correctly
- no critical module blocks the safety stop path
- patrol missions can be repeated with consistent results
- GUI outputs remain readable and useful during operation
- logs are usable for post-mission review and decision support

---

## Risk Assessment and Safety Features

As a mobile robotic platform operating near plants, wiring, and operators, the rover should prioritise safe and predictable behaviour.

### Key Risks
- collision with greenhouse structures or plants
- sensor failure or invalid readings
- low battery during patrol
- software freeze or communication loss
- false alerts or missed alerts
- cable looseness or unstable hardware mounting
- misclassification in basic visual recognition

### Mitigation Features
- emergency stop function
- fail-safe motor stop on critical error
- battery-voltage monitoring and warning
- watchdog or timeout for control-loop failure
- threshold validation and fault logging for abnormal sensor values
- modular wiring and documented schematics for easier troubleshooting
- operator takeover and manual review for uncertain cases

### Safety Principle
If the system is uncertain, it should prefer to stop safely and request review rather than continue moving or make overconfident decisions.

This principle aligns with the project’s broader design logic: stable and low-risk deployment is more important than demonstrating maximum autonomy.

---

## Acknowledgements

This project was developed as a multidisciplinary student engineering effort combining embedded systems, sensing, motion control, computer vision, and robotics documentation. The team acknowledges the use of Raspberry Pi-based prototyping tools, open hardware modules, and publicly available robotics development resources that supported rapid implementation and testing.

---

## Authors and Contributions

- **Huichuan Zheng** — Overall project planning, code integration/merging, and main program implementation.
- **Rui Wang** — Motor driving module, IR remote configuration, and program implementation.
- **Yukun Shi** — Temperature-sensor intelligent control system configuration and programming, plus IR line-tracking implementation.
- **Xinge Rao** — README documentation and camera configuration.

---

## License (Third-Party Libraries)

This repository may depend on third-party libraries, drivers, and hardware support packages. Please review the license terms of any external code, Python packages, C/C++ libraries, OpenCV dependencies, or vendor schematics used in the project before redistribution.

_Recommended follow-up: list specific libraries and their licenses here._

---

## Future Work

Future development of the Greenhouse Inspection Rover should continue to improve its role as a non-contact inspection platform for strawberry greenhouse management.

Potential next steps include:
- more reliable multi-point patrol in real greenhouse aisles
- improved fruit-maturity and leaf-abnormality recognition with richer image datasets
- integration of additional environmental sensing such as humidity and light intensity
- stronger GUI functions for mission replay, image review, and trend visualization
- more refined checkpoint-based inspection and data logging
- improved obstacle handling in practical greenhouse environments
- optional closed-loop micro-environment response through relay-controlled devices

In line with the project story, the most meaningful future progress will come not simply from adding more features, but from improving **practical deployment**, **inspection consistency**, and **non-contact decision support**.

---

## Contact Us

For questions, suggestions, or collaboration related to this project, please open an issue in this repository.

---

## Last Updated
2026-03-23