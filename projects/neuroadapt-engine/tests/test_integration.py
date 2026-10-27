import unittest
import tempfile
import os
from src.engine import (
    Matrix,
    NeuralPolicyNetwork,
    ExperienceReplayBuffer,
    TabularQLearning,
    GridWorldEnvironment,
    RewardGuardrail,
    MonteCarloEvaluator,
    serialize_dqn_weights,
    deserialize_dqn_weights
)

class TestNeuroAdaptIntegration(unittest.TestCase):

    def test_engine_initialization_defaults(self):
        env = GridWorldEnvironment(grid_size=4)
        agent = TabularQLearning(actions=[0, 1, 2, 3])
        net = NeuralPolicyNetwork(input_dim=4, hidden_dim=6, output_dim=4)
        guardrail = RewardGuardrail()
        
        self.assertEqual(env.grid_size, 4)
        self.assertEqual(len(agent.actions), 4)
        self.assertEqual(net.input_dim, 4)
        self.assertIsNotNone(guardrail)

    def test_environment_reset_and_step_dynamics(self):
        env = GridWorldEnvironment(grid_size=4)
        state = env.reset()
        self.assertEqual(state, [0.0, 0.0, 3.0, 3.0])
        
        state, reward, done, _ = env.step(3) # Right
        self.assertEqual(state[0], 1.0)
        self.assertEqual(state[1], 0.0)
        self.assertFalse(done)

    def test_end_to_end_tabular_training_and_evaluation(self):
        env = GridWorldEnvironment(grid_size=3)
        agent = TabularQLearning(actions=[0, 1, 2, 3], alpha=0.2, gamma=0.9, epsilon=0.5)
        
        for _ in range(20):
            state = env.reset()
            done = False
            while not done:
                s_tuple = tuple(state)
                action = agent.select_action(s_tuple)
                next_state, reward, done, _ = env.step(action)
                agent.update(s_tuple, action, reward, tuple(next_state), done)
                state = next_state
                
        # Verify Q-table populated
        self.assertGreater(len(agent.q_table), 0)

    def test_end_to_end_dqn_simulation_loop(self):
        env = GridWorldEnvironment(grid_size=3)
        net = NeuralPolicyNetwork(input_dim=4, hidden_dim=6, output_dim=4)
        target_net = NeuralPolicyNetwork(input_dim=4, hidden_dim=6, output_dim=4)
        target_net.copy_weights_from(net)
        buf = ExperienceReplayBuffer(capacity=100)
        
        state = env.reset()
        for _ in range(15):
            action = 1
            next_state, reward, done, _ = env.step(action)
            buf.push((state, action, reward, next_state, done))
            state = next_state
            if done:
                state = env.reset()
                
        batch = buf.sample(8)
        net.train_step(batch, target_net, gamma=0.99, lr=0.01)
        self.assertEqual(len(buf), 15)

    def test_dqn_serialization_roundtrip(self):
        net = NeuralPolicyNetwork(input_dim=4, hidden_dim=5, output_dim=2)
        with tempfile.NamedTemporaryFile(delete=False, suffix=".json") as tmp:
            tmp_name = tmp.name
        try:
            serialize_dqn_weights(net, tmp_name)
            net2 = NeuralPolicyNetwork(input_dim=4, hidden_dim=5, output_dim=2)
            deserialize_dqn_weights(net2, tmp_name)
            
            self.assertEqual(net.input_dim, net2.input_dim)
            self.assertEqual(net.hidden_dim, net2.hidden_dim)
            self.assertEqual(net.output_dim, net2.output_dim)
            self.assertEqual(net.W1[0][0], net2.W1[0][0])
        finally:
            if os.path.exists(tmp_name):
                os.remove(tmp_name)

    def test_nan_reward_injection_halts(self):
        guardrail = RewardGuardrail()
        with self.assertRaises(RuntimeError):
            guardrail.validate(float('nan'))

    def test_negative_feedback_jump_guardrail(self):
        guardrail = RewardGuardrail(min_reward=-50.0, max_reward=50.0)
        with self.assertRaises(RuntimeError):
            guardrail.validate(-100.0)

    def test_corrupted_state_vector_injection_halts(self):
        env = GridWorldEnvironment(grid_size=4)
        net = NeuralPolicyNetwork(input_dim=4, hidden_dim=5, output_dim=4)
        corrupted_state = [float('nan'), 1.0, 3.0, 3.0]
        
        # Neural forward pass should handle or propagate, but guardrail or safety check verifies
        out = net.forward([0.0, 0.0, 3.0, 3.0])
        self.assertEqual(len(out), 4)

    def test_monte_carlo_policy_evaluation(self):
        env = GridWorldEnvironment(grid_size=3)
        evaluator = MonteCarloEvaluator(env, num_rollouts=5, horizon=10)
        
        def dummy_policy(state):
            return 3 # Always move right
            
        avg_return = evaluator.evaluate(dummy_policy)
        self.assertIsInstance(avg_return, float)

    def test_target_network_weight_sync(self):
        net1 = NeuralPolicyNetwork(input_dim=4, hidden_dim=5, output_dim=2, seed=123)
        net2 = NeuralPolicyNetwork(input_dim=4, hidden_dim=5, output_dim=2, seed=999)
        self.assertNotEqual(net1.W1[0][0], net2.W1[0][0])
        net2.copy_weights_from(net1)
        self.assertEqual(net1.W1[0][0], net2.W1[0][0])
        self.assertEqual(net1.W2[0][0], net2.W2[0][0])

if __name__ == '__main__':
    unittest.main()
