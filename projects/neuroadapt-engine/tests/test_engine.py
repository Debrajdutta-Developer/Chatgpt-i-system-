import unittest
from src.engine import (
    Matrix,
    NeuralPolicyNetwork,
    ExperienceReplayBuffer,
    TabularQLearning,
    GridWorldEnvironment,
    RewardGuardrail,
    relu
)

class TestNeuroAdaptEngine(unittest.TestCase):
    
    def test_matrix_creation_and_indexing(self):
        m = Matrix(2, 3, 1.5)
        self.assertEqual(m.rows, 2)
        self.assertEqual(m.cols, 3)
        self.assertEqual(m[0][0], 1.5)
        self.assertEqual(m[1][2], 1.5)

    def test_matrix_multiplication_precision(self):
        a = Matrix.from_list([[1.0, 2.0], [3.0, 4.0]])
        b = Matrix.from_list([[2.0, 0.0], [1.0, 2.0]])
        c = a.matmul(b)
        self.assertEqual(c[0][0], 4.0)
        self.assertEqual(c[0][1], 4.0)
        self.assertEqual(c[1][0], 10.0)
        self.assertEqual(c[1][1], 8.0)

    def test_matrix_transpose(self):
        a = Matrix.from_list([[1.0, 2.0, 3.0], [4.0, 5.0, 6.0]])
        at = a.transpose()
        self.assertEqual(at.rows, 3)
        self.assertEqual(at.cols, 2)
        self.assertEqual(at[0][1], 4.0)
        self.assertEqual(at[2][0], 3.0)

    def test_neural_network_forward_pass(self):
        net = NeuralPolicyNetwork(input_dim=4, hidden_dim=5, output_dim=2, seed=42)
        out = net.forward([1.0, 0.0, 1.0, 0.0])
        self.assertEqual(len(out), 2)
        self.assertIsInstance(out[0], float)
        self.assertIsInstance(out[1], float)

    def test_experience_replay_buffer(self):
        buf = ExperienceReplayBuffer(capacity=5)
        for i in range(10):
            buf.push(([float(i)], i, float(i), [float(i+1)], False))
        self.assertEqual(len(buf), 5)
        sample = buf.sample(3)
        self.assertEqual(len(sample), 3)

    def test_tabular_q_learning_updates(self):
        agent = TabularQLearning(actions=[0, 1])
        state = (0, 0)
        next_state = (0, 1)
        q_val_before = agent.get_q(state, 0)
        self.assertEqual(q_val_before, 0.0)
        agent.update(state, 0, 10.0, next_state, False)
        q_val_after = agent.get_q(state, 0)
        self.assertNotEqual(q_val_after, 0.0)

    def test_grid_world_environment_dynamics(self):
        env = GridWorldEnvironment(grid_size=4)
        obs = env.reset()
        self.assertEqual(obs, [0.0, 0.0, 3.0, 3.0])
        next_obs, reward, done, _ = env.step(1) # Move down
        self.assertEqual(next_obs[1], 1.0)
        self.assertEqual(reward, -0.1)
        self.assertFalse(done)

    def test_reward_guardrail_valid_and_invalid(self):
        guardrail = RewardGuardrail(min_reward=-10.0, max_reward=10.0)
        # Should not raise
        guardrail.validate(5.0)
        guardrail.validate(-5.0)
        
        with self.assertRaises(RuntimeError):
            guardrail.validate(float('nan'))
            
        with self.assertRaises(RuntimeError):
            guardrail.validate(float('inf'))
            
        with self.assertRaises(RuntimeError):
            guardrail.validate(15.0)

    def test_relu_activation(self):
        self.assertEqual(relu(5.0), 5.0)
        self.assertEqual(relu(-2.5), 0.0)
        self.assertEqual(relu(0.0), 0.0)

    def test_matrix_addition(self):
        a = Matrix.from_list([[1.0, 2.0], [3.0, 4.0]])
        b = Matrix.from_list([[10.0, 20.0], [30.0, 40.0]])
        c = a.add(b)
        self.assertEqual(c[0][0], 11.0)
        self.assertEqual(c[1][1], 44.0)

if __name__ == '__main__':
    unittest.main()
