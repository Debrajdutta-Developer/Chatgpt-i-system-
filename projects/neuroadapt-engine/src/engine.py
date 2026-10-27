import math
import random
import struct
import json

class Matrix:
    def __init__(self, rows: int, cols: int, fill: float = 0.0):
        self.rows = rows
        self.cols = cols
        self.data = [[fill for _ in range(cols)] for _ in range(rows)]

    @staticmethod
    def from_list(lst: list[list[float]]) -> 'Matrix':
        r = len(lst)
        c = len(lst[0]) if r > 0 else 0
        m = Matrix(r, c)
        m.data = [row[:] for row in lst]
        return m

    def __getitem__(self, idx):
        return self.data[idx]

    def __setitem__(self, idx, val):
        self.data[idx] = val

    def transpose(self) -> 'Matrix':
        res = Matrix(self.cols, self.rows)
        for i in range(self.rows):
            for j in range(self.cols):
                res.data[j][i] = self.data[i][j]
        return res

    def matmul(self, other: 'Matrix') -> 'Matrix':
        if self.cols != other.rows:
            raise ValueError(f"Matrix dimension mismatch for multiplication: {self.cols} != {other.rows}")
        res = Matrix(self.rows, other.cols, 0.0)
        for i in range(self.rows):
            for k in range(self.cols):
                aik = self.data[i][k]
                if aik == 0.0:
                    continue
                other_k = other.data[k]
                res_i = res.data[i]
                for j in range(other.cols):
                    res_i[j] += aik * other_k[j]
        return res

    def add(self, other: 'Matrix') -> 'Matrix':
        if self.rows != other.rows or self.cols != other.cols:
            raise ValueError("Matrix dimensions must match for addition")
        res = Matrix(self.rows, self.cols)
        for i in range(self.rows):
            for j in range(self.cols):
                res.data[i][j] = self.data[i][j] + other.data[i][j]
        return res

    def apply(self, func):
        res = Matrix(self.rows, self.cols)
        for i in range(self.rows):
            for j in range(self.cols):
                res.data[i][j] = func(self.data[i][j])
        return res


def relu(x: float) -> float:
    return x if x > 0.0 else 0.0

def relu_derivative(x: float) -> float:
    return 1.0 if x > 0.0 else 0.0


class NeuralPolicyNetwork:
    def __init__(self, input_dim: int, hidden_dim: int, output_dim: int, seed: int = 42):
        random.seed(seed)
        self.input_dim = input_dim
        self.hidden_dim = hidden_dim
        self.output_dim = output_dim

        # Initialize weights with Xavier/He style scaling
        scale_w1 = math.sqrt(2.0 / input_dim)
        scale_w2 = math.sqrt(2.0 / hidden_dim)

        self.W1 = Matrix(input_dim, hidden_dim)
        for i in range(input_dim):
            for j in range(hidden_dim):
                self.W1[i][j] = random.gauss(0.0, scale_w1)

        self.b1 = Matrix(1, hidden_dim, 0.0)

        self.W2 = Matrix(hidden_dim, output_dim)
        for i in range(hidden_dim):
            for j in range(output_dim):
                self.W2[i][j] = random.gauss(0.0, scale_w2)

        self.b2 = Matrix(1, output_dim, 0.0)

    def forward(self, state: list[float]) -> list[float]:
        x_mat = Matrix.from_list([state])
        h_in = x_mat.matmul(self.W1).add(self.b1)
        self.last_h_pre = h_in
        self.last_h = h_in.apply(relu)
        
        out_in = self.last_h.matmul(self.W2).add(self.b2)
        self.last_out = out_in
        return out_in[0]

    def copy_weights_from(self, other: 'NeuralPolicyNetwork'):
        for i in range(self.W1.rows):
            for j in range(self.W1.cols):
                self.W1[i][j] = other.W1[i][j]
        for i in range(self.b1.rows):
            for j in range(self.b1.cols):
                self.b1[i][j] = other.b1[i][j]
        for i in range(self.W2.rows):
            for j in range(self.W2.cols):
                self.W2[i][j] = other.W2[i][j]
        for i in range(self.b2.rows):
            for j in range(self.b2.cols):
                self.b2[i][j] = other.b2[i][j]

    def train_step(self, batch: list[tuple], target_net: 'NeuralPolicyNetwork', gamma: float, lr: float):
        # Mini-batch SGD Q-learning step
        for state, action, reward, next_state, done in batch:
            current_q = self.forward(state)
            target_q = list(current_q)
            
            if done:
                target_q[action] = reward
            else:
                next_q_target = target_net.forward(next_state)
                max_next_q = max(next_q_target)
                target_q[action] = reward + gamma * max_next_q

            # Compute error (prediction - target) for gradient descent on output layer
            # Loss = 0.5 * (current_q - target_q)^2
            # dLoss/dOutput = current_q - target_q
            output_grads = [current_q[i] - target_q[i] for i in range(self.output_dim)]

            # Backpropagate to W2, b2, W1, b1
            # dW2 = last_h.T @ dOutput
            for h_idx in range(self.hidden_dim):
                h_val = self.last_h[0][h_idx]
                for o_idx in range(self.output_dim):
                    grad = output_grads[o_idx]
                    self.W2[h_idx][o_idx] -= lr * grad * h_val

            for o_idx in range(self.output_dim):
                self.b2[0][o_idx] -= lr * output_grads[o_idx]

            # Backprop to hidden layer
            hidden_grads = [0.0] * self.hidden_dim
            for h_idx in range(self.hidden_dim):
                s = 0.0
                for o_idx in range(self.output_dim):
                    s += output_grads[o_idx] * self.W2[h_idx][o_idx]
                pre_val = self.last_h_pre[0][h_idx]
                hidden_grads[h_idx] = s * relu_derivative(pre_val)

            for i_idx in range(self.input_dim):
                i_val = state[i_idx]
                for h_idx in range(self.hidden_dim):
                    self.W1[i_idx][h_idx] -= lr * hidden_grads[h_idx] * i_val

            for h_idx in range(self.hidden_dim):
                self.b1[0][h_idx] -= lr * hidden_grads[h_idx]


class ExperienceReplayBuffer:
    def __init__(self, capacity: int = 1000):
        self.capacity = capacity
        self.buffer = []
        self.position = 0

    def push(self, transition: tuple):
        if len(self.buffer) < self.capacity:
            self.buffer.append(None)
        self.buffer[self.position] = transition
        self.position = (self.position + 1) % self.capacity

    def sample(self, batch_size: int) -> list[tuple]:
        return random.sample(self.buffer, min(batch_size, len(self.buffer)))

    def __len__(self):
        return len(self.buffer)


class TabularQLearning:
    def __init__(self, actions: list[int], alpha: float = 0.1, gamma: float = 0.99, epsilon: float = 1.0, epsilon_decay: float = 0.99):
        self.actions = actions
        self.alpha = alpha
        self.gamma = gamma
        self.epsilon = epsilon
        self.epsilon_decay = epsilon_decay
        self.q_table = {}

    def get_q(self, state: tuple, action: int) -> float:
        if state not in self.q_table:
            self.q_table[state] = [0.0 for _ in self.actions]
        return self.q_table[state][action]

    def select_action(self, state: tuple) -> int:
        if random.random() < self.epsilon:
            return random.choice(self.actions)
        if state not in self.q_table:
            self.q_table[state] = [0.0 for _ in self.actions]
        q_vals = self.q_table[state]
        max_val = max(q_vals)
        best_actions = [a for a, q in enumerate(q_vals) if q == max_val]
        return random.choice(best_actions)

    def update(self, state: tuple, action: int, reward: float, next_state: tuple, done: bool):
        current_q = self.get_q(state, action)
        if done:
            max_next_q = 0.0
        else:
            if next_state not in self.q_table:
                self.q_table[next_state] = [0.0 for _ in self.actions]
            max_next_q = max(self.q_table[next_state])
        
        new_q = current_q + self.alpha * (reward + self.gamma * max_next_q - current_q)
        self.q_table[state][action] = new_q
        self.epsilon = max(0.01, self.epsilon * self.epsilon_decay)


class GridWorldEnvironment:
    def __init__(self, grid_size: int = 4):
        self.grid_size = grid_size
        self.state = [0.0, 0.0]
        self.goal = [float(grid_size - 1), float(grid_size - 1)]

    def reset(self) -> list[float]:
        self.state = [0.0, 0.0]
        return self._get_obs()

    def _get_obs(self) -> list[float]:
        # 4-dim state vector: [agent_x, agent_y, goal_x, goal_y]
        return [self.state[0], self.state[1], self.goal[0], self.goal[1]]

    def step(self, action: int) -> tuple[list[float], float, bool, dict]:
        # actions: 0=Up, 1=Down, 2=Left, 3=Right
        if action == 0:
            self.state[1] = max(0.0, self.state[1] - 1.0)
        elif action == 1:
            self.state[1] = min(float(self.grid_size - 1), self.state[1] + 1.0)
        elif action == 2:
            self.state[0] = max(0.0, self.state[0] - 1.0)
        elif action == 3:
            self.state[0] = min(float(self.grid_size - 1), self.state[0] + 1.0)

        done = (self.state == self.goal)
        reward = 10.0 if done else -0.1
        return self._get_obs(), reward, done, {}


class RewardGuardrail:
    def __init__(self, min_reward: float = -1000.0, max_reward: float = 1000.0):
        self.min_reward = min_reward
        self.max_reward = max_reward

    def validate(self, reward: float):
        if math.isnan(reward) or math.isinf(reward):
            raise RuntimeError(f"[Guardrail Violation] Reward signal is NaN or Infinite: {reward}")
        if reward < self.min_reward or reward > self.max_reward:
            raise RuntimeError(f"[Guardrail Violation] Reward {reward} out of bounds [{self.min_reward}, {self.max_reward}]")


class MonteCarloEvaluator:
    def __init__(self, env: GridWorldEnvironment, num_rollouts: int = 10, horizon: int = 50):
        self.env = env
        self.num_rollouts = num_rollouts
        self.horizon = horizon

    def evaluate(self, policy_fn) -> float:
        total_returns = 0.0
        for _ in range(self.num_rollouts):
            state = self.env.reset()
            episode_return = 0.0
            discount = 1.0
            for _ in range(self.horizon):
                action = policy_fn(state)
                next_state, reward, done, _ = self.env.step(action)
                episode_return += discount * reward
                discount *= 0.99
                state = next_state
                if done:
                    break
            total_returns += episode_return
        return total_returns / self.num_rollouts


def serialize_dqn_weights(net: NeuralPolicyNetwork, filepath: str):
    data = {
        "input_dim": net.input_dim,
        "hidden_dim": net.hidden_dim,
        "output_dim": net.output_dim,
        "W1": net.W1.data,
        "b1": net.b1.data,
        "W2": net.W2.data,
        "b2": net.b2.data
    }
    with open(filepath, "w") as f:
        json.dump(data, f)


def deserialize_dqn_weights(net: NeuralPolicyNetwork, filepath: str):
    with open(filepath, "r") as f:
        data = json.load(f)
    net.input_dim = data["input_dim"]
    net.hidden_dim = data["hidden_dim"]
    net.output_dim = data["output_dim"]
    net.W1 = Matrix.from_list(data["W1"])
    net.b1 = Matrix.from_list(data["b1"])
    net.W2 = Matrix.from_list(data["W2"])
    net.b2 = Matrix.from_list(data["b2"])
