# NeuroAdapt Engine

## Overview
NeuroAdapt Engine is a deterministic, zero-dependency reinforcement learning and neural-policy runtime built entirely in pure Python 3.12. Designed for AI systems engineers and local robotics researchers working on resource-constrained embedded devices, the engine eliminates heavy external C++ bindings (such as PyTorch or TensorFlow) and floating-point non-determinism. It provides robust tabular Q-learning solvers, custom pure-Python linear algebra matrix engines, deep Q-networks (DQN) with experience replay buffers, Monte Carlo policy evaluation, and strict safety guardrails that monitor reward signals for anomalies, NaNs, and extreme negative feedback jumps.

## Architecture
The architecture of NeuroAdapt Engine is structured around modular, side-effect-free components communicating through well-defined data structures:
- **Matrix Engine (`src/engine.py`)**: Implements 2D matrix storage, multiplication, addition, transpose, vector dot products, and activation functions (ReLU, Sigmoid, Tanh) entirely with standard library `float` primitives.
- **Neural Policy Network & DQN (`src/engine.py`)**: Implements a multi-layer perceptron (MLP) with configurable hidden dimensions, weight initialization, forward passes, mean squared error loss calculations, and standard backpropagation with gradient descent updates.
- **Experience Replay Buffer (`src/engine.py`)**: A fixed-capacity FIFO circular buffer storing transition tuples `(state, action, reward, next_state, done)` with uniform random mini-batch sampling.
- **Tabular Q-Learning Solver (`src/engine.py`)**: Manages discrete state quantization, epsilon-greedy action selection, and tabular Bellman updates with customizable discount factors and decay schedules.
- **Dynamic Reward Guardrail (`src/engine.py`)**: Intercepts reward signals before any policy weight or Q-table update occurs, instantly throwing deterministic runtime exceptions on `NaN`, `inf`, or out-of-bounds feedback spikes.
- **CLI Interface (`src/cli.py`)**: Exposes subcommands for tabular training, DQN simulation loops, Monte Carlo evaluations, and weight checkpoint serialization.

## Algorithms and Data Structures
- **Pure Python Linear Algebra**: Matrices are represented as nested lists of standard floats. Matrix multiplication is executed via standard $O(M 	imes N 	imes K)$ triple loops, ensuring exact reproducibility without hardware-specific FPUs or BLAS quirks.
- **Deep Q-Network (DQN)**: Uses a target network decoupled from the policy network, synchronized periodically. Backpropagation calculates analytical gradients for weight matrices and biases.
- **Tabular Q-Learning**: State spaces are discretized into fixed-resolution grid bins. Q-values are stored in a nested dictionary mapping state tuples to action-value vectors.
- **Monte Carlo Policy Evaluation**: Simulates rollouts over fixed horizons to estimate cumulative discounted returns for deterministic policy verification.

## Invariants
1. **Zero External Dependencies**: Relies exclusively on Python 3.12 standard library modules (`math`, `random`, `struct`, `argparse`, `json`, `os`, `unittest`, `tempfile`).
2. **Deterministic Execution**: Given fixed random seeds, all policy updates, weight initializations, and simulation trajectories are bitwise identical.
3. **Safety Guardrail Enforced**: Any reward value containing `NaN`, `Infinity`, or exceeding absolute bounds halts training instantly and raises a `RuntimeError`.
4. **Serialization Roundtrip Integrity**: All policy weights, Q-tables, and replay buffer transitions serialize cleanly to binary disk formats using custom struct serialization and JSON manifests.

## Build and Run
No compilation or installation of third-party packages is required. Ensure Python 3.12 is installed.

```bash
# Run tabular Q-learning training simulation
python3 src/cli.py train-tabular --episodes 100 --output q_table.bin

# Run Deep Q-Network simulation loop
python3 src/cli.py train-dqn --episodes 50 --output dqn_weights.bin

# Evaluate policy via Monte Carlo rollouts
python3 src/cli.py evaluate --weights dqn_weights.bin --episodes 10
```

## Testing
The repository includes comprehensive unit and integration test suites adhering to strict assertion counts. To run all tests:

```bash
python3 -m unittest discover -s tests
```

Each test file contains at least 10 explicit independent assertions verifying matrix math, state discretization, guardrail tripping, serialization roundtrips, and full environment loop dynamics.

## Security and Privacy
NeuroAdapt Engine runs entirely locally without network calls, telemetry, or external sockets. All data remains in memory or in explicitly specified local files. The strict reward guardrails prevent runaway agent behavior from corrupting local memory or executing unsafe actuator commands.

## Performance Characteristics
- **Memory Footprint**: Extremely lightweight (< 15 MB RAM overhead), suitable for microcontrollers and embedded Linux targets.
- **Computation**: Pure-Python matrix operations trade raw execution speed for absolute determinism and transparency. Recommended for state dimensions under 64.

## Limitations
- **Scaling**: Pure Python loops for matrix multiplication are not optimized for high-dimensional deep learning (e.g., image processing or large LLMs).
- **State Space**: Continuous state spaces require manual discretization into fixed-width bins for tabular solvers.
